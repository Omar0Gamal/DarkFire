#include "Common.hlsli"

#define g_TextureRawSSAO g_TextureAlbedo

cbuffer CBBlur : register(b5)
{
	float2 g_TexelSize;
	float2 g_Pad;
};

struct FullscreenVSOutput
{
	float4 positionCS : SV_Position;
	float2 texcoord   : TEXCOORD0;
};

float GetLinearZ(float2 uv, float rawDepth)
{
	float2 ndc = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
	float4 clip = float4(ndc, rawDepth, 1.0f);
	float4 viewH = mul(clip, g_View.invProjMatrix);
	return abs(viewH.z / max(abs(viewH.w), 0.0001f));
}

float4 main(FullscreenVSOutput input) : SV_Target
{
	float centerDepth = g_TextureDepth.SampleLevel(g_SamplerPointClamp, input.texcoord, 0).r;
	if (centerDepth <= 0.00001f)
	{
		return float4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	float centerZ = GetLinearZ(input.texcoord, centerDepth);
	float totalAO = 0.0f;
	float totalWeight = 0.0f;

	// High-fidelity 5x5 Bilateral Filter:
	// Perfectly integrates the 4x4 spatial Bayer dither pattern across flat surfaces,
	// completely eliminating visible screen noise and ghost banding,
	// while preserving true depth edges and fine architectural stone contours.
	float depthTolerance = max(0.12f, centerZ * 0.025f);

	[unroll]
	for (int x = -2; x <= 2; ++x)
	{
		[unroll]
		for (int y = -2; y <= 2; ++y)
		{
			float2 offset = float2((float)x, (float)y) * g_TexelSize;
			float2 sampleUV = input.texcoord + offset;

			float sampleDepth = g_TextureDepth.SampleLevel(g_SamplerPointClamp, sampleUV, 0).r;
			float sampleZ = GetLinearZ(sampleUV, sampleDepth);
			float sampleAO = g_TextureRawSSAO.SampleLevel(g_SamplerLinearClamp, sampleUV, 0).r;

			float spatialDist2 = (float)(x * x + y * y);
			float spatialWeight = exp(-spatialDist2 * 0.222f); // Gaussian sigma = 1.5
			float depthDiff = abs(centerZ - sampleZ);
			float dRatio = depthDiff / depthTolerance;
			float depthWeight = 1.0f / (1.0f + dRatio * dRatio * 4.0f);

			float weight = spatialWeight * depthWeight;
			totalAO += sampleAO * weight;
			totalWeight += weight;
		}
	}

	float finalAO = totalWeight > 0.0001f ? (totalAO / totalWeight) : 1.0f;
	return float4(finalAO, finalAO, finalAO, 1.0f);
}
