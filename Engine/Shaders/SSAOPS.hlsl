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

// Interleaved Gradient Noise
float InterleavedGradientNoise(float2 screenPos)
{
	float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
	return frac(magic.z * frac(dot(screenPos, magic.xy)));
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

	// Reconstruct view-space normal using best-of-4 neighbors to avoid edge halos
	float2 texel = g_SSAO.invResolution;
	float depthL = g_TextureDepth.SampleLevel(g_SamplerPointClamp, input.texcoord + float2(-texel.x, 0.0f), 0).r;
	float depthR = g_TextureDepth.SampleLevel(g_SamplerPointClamp, input.texcoord + float2( texel.x, 0.0f), 0).r;
	float depthT = g_TextureDepth.SampleLevel(g_SamplerPointClamp, input.texcoord + float2(0.0f, -texel.y), 0).r;
	float depthB = g_TextureDepth.SampleLevel(g_SamplerPointClamp, input.texcoord + float2(0.0f,  texel.y), 0).r;

	float3 vL = GetViewPos(input.texcoord + float2(-texel.x, 0.0f), depthL);
	float3 vR = GetViewPos(input.texcoord + float2( texel.x, 0.0f), depthR);
	float3 vT = GetViewPos(input.texcoord + float2(0.0f, -texel.y), depthT);
	float3 vB = GetViewPos(input.texcoord + float2(0.0f,  texel.y), depthB);

	float3 dX = abs(vR.z - viewPos.z) < abs(vL.z - viewPos.z) ? (vR - viewPos) : (viewPos - vL);
	float3 dY = abs(vB.z - viewPos.z) < abs(vT.z - viewPos.z) ? (vB - viewPos) : (viewPos - vT);
	float3 normalVS = normalize(cross(dX, dY));
	if (normalVS.z > 0.0f) normalVS = -normalVS; // Ensure pointing towards camera

	// Random rotation around normal
	float2 screenPos = input.positionCS.xy;
	float noiseAngle = InterleavedGradientNoise(screenPos) * 2.0f * PI;
	float sinA = sin(noiseAngle);
	float cosA = cos(noiseAngle);

	float3 tangent = normalize(abs(normalVS.y) < 0.99f ? cross(normalVS, float3(0.0f, 1.0f, 0.0f)) : cross(normalVS, float3(1.0f, 0.0f, 0.0f)));
	float3 bitangent = cross(normalVS, tangent);
	float3x3 tbn = float3x3(tangent, bitangent, normalVS);

	float occlusion = 0.0f;
	uint count = min(g_SSAO.sampleCount, 16u);
	if (count == 0u) count = 16u;

	for (uint i = 0; i < count; ++i)
	{
		float3 sampleVec = g_SSAO.kernel[i].xyz;
		// Rotate sample vector around Z (normal)
		float3 rotatedSample = float3(
			sampleVec.x * cosA - sampleVec.y * sinA,
			sampleVec.x * sinA + sampleVec.y * cosA,
			sampleVec.z
		);

		float3 sampleDir = mul(rotatedSample, tbn);
		if (dot(sampleDir, normalVS) < 0.0f) sampleDir = -sampleDir;

		float3 sampleViewPos = viewPos + sampleDir * g_SSAO.radius;

		// Project to screen space
		float4 sampleClip = mul(float4(sampleViewPos, 1.0f), g_View.projMatrix);
		float2 sampleUV = (sampleClip.xy / sampleClip.w) * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

		if (sampleUV.x < 0.0f || sampleUV.x > 1.0f || sampleUV.y < 0.0f || sampleUV.y > 1.0f)
		{
			continue;
		}

		float sampleDepth = g_TextureDepth.SampleLevel(g_SamplerPointClamp, sampleUV, 0).r;
		float3 sampleGeomPos = GetViewPos(sampleUV, sampleDepth);

		// Left-handed reversed-Z: larger Z is farther from camera
		float rangeCheck = smoothstep(0.0f, 1.0f, g_SSAO.radius / max(abs(viewPos.z - sampleGeomPos.z), 0.001f));
		if (sampleGeomPos.z < sampleViewPos.z - g_SSAO.bias)
		{
			occlusion += rangeCheck;
		}
	}

	float ao = 1.0f - (occlusion / float(count)) * g_SSAO.intensity;
	ao = clamp(pow(max(ao, 0.0f), g_SSAO.power), 0.0f, 1.0f);

	return float4(ao, ao, ao, 1.0f);
}
