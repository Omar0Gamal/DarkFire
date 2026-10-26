#include "Shared/Bindings.h"
#include "Common.hlsli"
#include "BRDF.hlsli"

struct VSOutput
{
	float4 position : SV_Position;
	float2 uv       : TEXCOORD0;
};

// Reconstruct View-Space position from Reversed-Z depth
float3 reconstructViewPos(float2 uv, float depth)
{
	// Reversed-Z NDC: Z is in [0, 1] where 1 is near plane, 0 is far plane
	float4 clip = float4(uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f, depth, 1.0f);
	float4 viewPos = mul(clip, g_View.invProjMatrix);
	return viewPos.xyz / max(viewPos.w, 0.00001f);
}

// Convert View-Space position to Screen UV and Depth
float3 projectToScreen(float3 viewPos)
{
	float4 clip = mul(float4(viewPos, 1.0f), g_View.projMatrix);
	float3 ndc = clip.xyz / max(clip.w, 0.00001f);
	return float3(ndc.x * 0.5f + 0.5f, -ndc.y * 0.5f + 0.5f, ndc.z);
}

float4 main(VSOutput input) : SV_Target
{
	if (g_SSR.ssrExtra.x < 0.5f)
	{
		return float4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	float depth = g_TextureDepth.SampleLevel(g_SamplerPointClamp, input.uv, 0).r;
	if (depth <= 0.00001f) // Far plane skybox
	{
		return float4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	float4 nr = g_TextureNormalRoughness.SampleLevel(g_SamplerPointClamp, input.uv, 0);
	float3 worldNormal = nr.xyz * 2.0f - 1.0f;
	float roughness = nr.w;

	if (roughness < 0.0f || roughness > g_SSR.ssrParams.z) // Cloth material or exceeds SSR roughness cutoff
	{
		return float4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	float3 worldPos = reconstructViewPos(input.uv, depth); // Actually view-space position
	float3 viewDir = -normalize(worldPos);
	float3 viewNormal = mul(worldNormal, (float3x3)g_View.viewMatrix);
	viewNormal = normalize(viewNormal);

	float3 reflectDir = normalize(reflect(-viewDir, viewNormal));
	if (reflectDir.z > 0.95f) // Heading away behind camera
	{
		return float4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	float maxDist = g_SSR.ssrParams.x;
	float thickness = g_SSR.ssrParams.y;
	int maxSteps = (int)clamp(g_SSR.ssrExtra.y, 16.0f, 64.0f);

	// Dither start offset using interleaved gradient noise
	float noise = InterleavedGradientNoise(input.position.xy);
	float stepSize = maxDist / float(maxSteps);
	float3 rayPos = worldPos + reflectDir * (stepSize * (0.5f + noise * 0.5f));

	bool hit = false;
	float2 hitUV = float2(0.0f, 0.0f);
	float hitDist = 0.0f;

	[loop]
	for (int i = 0; i < maxSteps; ++i)
	{
		float3 screenCoords = projectToScreen(rayPos);
		if (any(screenCoords.xy < 0.001f) || any(screenCoords.xy > 0.999f))
		{
			break; // Traveled offscreen
		}

		float sceneDepth = g_TextureDepth.SampleLevel(g_SamplerPointClamp, screenCoords.xy, 0).r;
		if (sceneDepth > 0.00001f)
		{
			// Reversed-Z: Foreground geometry has LARGER depth than background geometry
			float3 sceneViewPos = reconstructViewPos(screenCoords.xy, sceneDepth);
			float depthDiff = rayPos.z - sceneViewPos.z;

			if (depthDiff > 0.0f && depthDiff < thickness)
			{
				hit = true;
				hitUV = screenCoords.xy;
				hitDist = length(rayPos - worldPos);
				break;
			}
		}

		rayPos += reflectDir * stepSize;
	}

	if (!hit)
	{
		return float4(0.0f, 0.0f, 0.0f, 0.0f);
	}

	// Smooth screen-edge vignette falloff
	float2 edgeCoords = abs(hitUV * 2.0f - 1.0f);
	float edgeFactor = 1.0f - max(edgeCoords.x, edgeCoords.y);
	float edgeWeight = saturate(edgeFactor * 8.0f);

	// Distance and roughness attenuation
	float distWeight = saturate(1.0f - (hitDist / maxDist));
	float roughnessWeight = saturate(1.0f - (roughness / g_SSR.ssrParams.z));
	float confidence = edgeWeight * distWeight * roughnessWeight * g_SSR.ssrParams.w;

	// Reproject hit UV into previous frame using velocity buffer
	float2 reprojectedUV = hitUV;
	float2 vel = g_TextureVelocity.SampleLevel(g_SamplerPointClamp, hitUV, 0).xy;
	reprojectedUV -= vel;
	reprojectedUV = clamp(reprojectedUV, 0.001f, 0.999f);

	float3 hitColor = g_TextureHDR.SampleLevel(g_SamplerLinearClamp, reprojectedUV, 0).rgb;

	// Clamp maximum reflection brightness to prevent fireflies
	hitColor = min(hitColor, float3(8.0f, 8.0f, 8.0f));

	return float4(hitColor * confidence, confidence);
}
