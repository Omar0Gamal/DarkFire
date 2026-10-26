#include "Common.hlsli"

cbuffer CBSSAOBuffer : register(b5)
{
	CBSSAO g_SSAO;
};

struct FullscreenVSOutput
{
	float4 positionCS : SV_Position;
	float2 texcoord   : TEXCOORD0;
};

float3 GetViewPos(float2 uv, float depth)
{
	float2 ndc = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
	float4 clip = float4(ndc, depth, 1.0f);
	float4 viewH = mul(clip, g_View.invProjMatrix);
	return viewH.xyz / viewH.w;
}

// 4x4 spatial Bayer dither pattern for noise decorrelation
static const float s_bayer4x4[16] = {
	 0.0f / 16.0f,  8.0f / 16.0f,  2.0f / 16.0f, 10.0f / 16.0f,
	12.0f / 16.0f,  4.0f / 16.0f, 14.0f / 16.0f,  6.0f / 16.0f,
	 3.0f / 16.0f, 11.0f / 16.0f,  1.0f / 16.0f,  9.0f / 16.0f,
	15.0f / 16.0f,  7.0f / 16.0f, 13.0f / 16.0f,  5.0f / 16.0f
};

// GTAO inner horizon integral (Jimenez et al. 2016)
float GTAO_IntegrateHorizon(float h, float gamma)
{
	return -cos(2.0f * h - gamma) + cos(gamma) + 2.0f * h * sin(gamma);
}

float4 main(FullscreenVSOutput input) : SV_Target
{
	float depth = g_TextureDepth.SampleLevel(g_SamplerPointClamp, input.texcoord, 0).r;
	// Far plane / sky in reversed-Z is depth ~ 0.0
	if (depth <= 0.00001f)
	{
		return float4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	float3 viewPos = GetViewPos(input.texcoord, depth);

	// Reconstruct view-space normal using best-of-4 neighbors to prevent halo artifacts
	float2 texel = g_SSAO.invResolution;
	float depthL = g_TextureDepth.SampleLevel(g_SamplerPointClamp, input.texcoord + float2(-texel.x, 0.0f), 0).r;
	float depthR = g_TextureDepth.SampleLevel(g_SamplerPointClamp, input.texcoord + float2( texel.x, 0.0f), 0).r;
	float depthT = g_TextureDepth.SampleLevel(g_SamplerPointClamp, input.texcoord + float2(0.0f, -texel.y), 0).r;
	float depthB = g_TextureDepth.SampleLevel(g_SamplerPointClamp, input.texcoord + float2(0.0f,  texel.y), 0).r;

	float3 vL = GetViewPos(input.texcoord + float2(-texel.x, 0.0f), depthL);
	float3 vR = GetViewPos(input.texcoord + float2( texel.x, 0.0f), depthR);
	float3 vT = GetViewPos(input.texcoord + float2(0.0f, -texel.y), depthT);
	float3 vB = GetViewPos(input.texcoord + float2(0.0f,  texel.y), depthB);

	float3 dX1 = vR - viewPos;
	float3 dX0 = viewPos - vL;
	float3 dY1 = vT - viewPos;
	float3 dY0 = viewPos - vB;
	float3 dX = abs(dX1.z) < abs(dX0.z) ? dX1 : dX0;
	float3 dY = abs(dY1.z) < abs(dY0.z) ? dY1 : dY0;
	float3 crossN = cross(dY, dX);
	float lenN = length(crossN);
	float3 normalVS = lenN > 1e-5f ? (crossN / lenN) : float3(0.0f, 0.0f, -1.0f);

	if (dot(normalVS, -viewPos) < 0.0f)
	{
		normalVS = -normalVS;
	}

	float3 V = -normalize(viewPos);

	// Ground Truth Ambient Occlusion (GTAO Horizon Search)
	uint2 pixCoord = (uint2)input.positionCS.xy % 4u;
	float dither = s_bayer4x4[pixCoord.y * 4u + pixCoord.x];

	float worldRadius = max(g_SSAO.radius, 0.1f);
	float projRadius = (worldRadius / max(abs(viewPos.z), 0.1f)) * g_View.projMatrix[1][1] * 0.5f;
	projRadius = clamp(projRadius, texel.y * 3.0f, 0.25f);

	const uint numSlices = 3u;
	const uint stepsPerSide = 4u;

	float totalVisibility = 0.0f;
	float totalWeight = 0.0f;
	float biasMeters = max(g_SSAO.bias, 0.01f);

	[unroll]
	for (uint s = 0u; s < numSlices; ++s)
	{
		float phi = (float(s) + dither) * (PI / float(numSlices));
		float2 dir2D = float2(cos(phi), sin(phi));

		// Find 3D slice tangent vector orthogonal to V
		float3 sliceSamplePos = GetViewPos(input.texcoord + dir2D * texel * 5.0f, depth);
		float3 sliceDelta = sliceSamplePos - viewPos;
		float3 sliceTangent = sliceDelta - V * dot(sliceDelta, V);
		float lenT = length(sliceTangent);
		if (lenT < 1e-4f) continue;
		sliceTangent /= lenT;

		// Project normal onto slice plane (spanned by sliceTangent and V)
		float nx = dot(normalVS, sliceTangent);
		float ny = dot(normalVS, V);
		float projNormLen = sqrt(nx * nx + ny * ny);
		if (projNormLen < 1e-4f) continue;

		float gamma = clamp(atan2(nx, ny), -0.48f * PI, 0.48f * PI);
		// Unoccluded tangent horizon angles measured from +V towards +sliceTangent (t1_max) and -sliceTangent (t2_max)
		float t1_max = 0.5f * PI + gamma;
		float t2_max = 0.5f * PI - gamma;

		float h1 = t1_max;
		float h2 = t2_max;

		// Side 1 (+dir2D)
		[unroll]
		for (uint i = 1u; i <= stepsPerSide; ++i)
		{
			float stepT = (float(i) - 0.5f + dither * 0.5f) / float(stepsPerSide);
			float2 sampleUV = input.texcoord + dir2D * (projRadius * stepT * stepT);
			if (sampleUV.x < 0.0f || sampleUV.x > 1.0f || sampleUV.y < 0.0f || sampleUV.y > 1.0f) break;

			float sDepth = g_TextureDepth.SampleLevel(g_SamplerPointClamp, sampleUV, 0).r;
			if (sDepth <= 0.00001f) continue;

			float3 sPos = GetViewPos(sampleUV, sDepth);
			float3 sDelta = sPos - viewPos;
			float sDist = length(sDelta);
			if (sDist < worldRadius && sDist > 0.005f)
			{
				float normDist = sDist / worldRadius;
				float distFalloff = saturate(1.0f - normDist * normDist);
				float angleBias = atan(biasMeters / max(sDist, 0.05f));
				float theta = atan2(dot(sDelta, sliceTangent), dot(sDelta, V)) + angleBias;
				// Modulate horizon angle towards unoccluded tangent plane by distance falloff
				theta = lerp(t1_max, theta, distFalloff);
				h1 = min(h1, theta);
			}
		}

		// Side 2 (-dir2D)
		[unroll]
		for (uint j = 1u; j <= stepsPerSide; ++j)
		{
			float stepT = (float(j) - 0.5f + dither * 0.5f) / float(stepsPerSide);
			float2 sampleUV = input.texcoord - dir2D * (projRadius * stepT * stepT);
			if (sampleUV.x < 0.0f || sampleUV.x > 1.0f || sampleUV.y < 0.0f || sampleUV.y > 1.0f) break;

			float sDepth = g_TextureDepth.SampleLevel(g_SamplerPointClamp, sampleUV, 0).r;
			if (sDepth <= 0.00001f) continue;

			float3 sPos = GetViewPos(sampleUV, sDepth);
			float3 sDelta = sPos - viewPos;
			float sDist = length(sDelta);
			if (sDist < worldRadius && sDist > 0.005f)
			{
				float normDist = sDist / worldRadius;
				float distFalloff = saturate(1.0f - normDist * normDist);
				float angleBias = atan(biasMeters / max(sDist, 0.05f));
				float theta = atan2(-dot(sDelta, sliceTangent), dot(sDelta, V)) + angleBias;
				theta = lerp(t2_max, theta, distFalloff);
				h2 = min(h2, theta);
			}
		}

		h1 = clamp(h1, max(0.0f, gamma - 0.5f * PI), t1_max);
		h2 = clamp(h2, max(0.0f, -gamma - 0.5f * PI), t2_max);

		float int1 = GTAO_IntegrateHorizon(h1, gamma);
		float int2 = GTAO_IntegrateHorizon(h2, -gamma);
		float maxInt = max(GTAO_IntegrateHorizon(t1_max, gamma) + GTAO_IntegrateHorizon(t2_max, -gamma), 1e-4f);
		float sliceVis = saturate((int1 + int2) / maxInt);

		totalVisibility += sliceVis * projNormLen;
		totalWeight += projNormLen;
	}

	float visibility = totalWeight > 1e-4f ? (totalVisibility / totalWeight) : 1.0f;
	visibility = saturate(visibility);

	// Contrast and power
	float ao = saturate(1.0f - (1.0f - visibility) * g_SSAO.intensity);
	ao = pow(ao, max(g_SSAO.power, 0.1f));

	return float4(ao, ao, ao, 1.0f);
}
