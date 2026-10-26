#include "Common.hlsli"
#include "BRDF.hlsli"
#include "Shared/Bindings.h"

Texture2D g_TextureNeighborMax : HLSL_REG_T(DF_SLOT_TEX_NEIGHBOR_MAX);

cbuffer CBMotionBlurBuffer : HLSL_REG_B(DF_SLOT_CB_MOTION_BLUR)
{
	CBMotionBlur g_MotionBlur;
};

struct VSOutput
{
	float4 positionCS : SV_Position;
	float2 texcoord   : TEXCOORD0;
};

float4 main(VSOutput input) : SV_Target
{
	float2 screenPos = input.positionCS.xy;
	float2 uv = input.texcoord;

	float3 centerColor = g_TextureHDR.SampleLevel(g_SamplerPointClamp, uv, 0).rgb;
	if (g_MotionBlur.motionBlurParams.w < 0.5f)
	{
		return float4(centerColor, 1.0f);
	}

	float2 vMaxUV = g_TextureNeighborMax.SampleLevel(g_SamplerLinearClamp, uv, 0).xy;
	float2 vMaxPx = vMaxUV * g_MotionBlur.screenResolution.xy * g_MotionBlur.motionBlurParams.x;
	float vMaxMag = length(vMaxPx);

	// Early out: subpixel velocity requires zero blur
	if (vMaxMag < 0.5f)
	{
		return float4(centerColor, 1.0f);
	}

	// Clamp to configured max radius
	float maxRadius = max(g_MotionBlur.motionBlurParams.y, 1.0f);
	if (vMaxMag > maxRadius)
	{
		vMaxPx = (vMaxPx / vMaxMag) * maxRadius;
		vMaxMag = maxRadius;
	}

	float2 vMaxDir = vMaxPx / vMaxMag;
	float centerDepth = g_TextureDepth.SampleLevel(g_SamplerPointClamp, uv, 0).r;
	float2 centerVel = g_TextureVelocity.SampleLevel(g_SamplerPointClamp, uv, 0).xy * g_MotionBlur.screenResolution.xy * g_MotionBlur.motionBlurParams.x;

	float noise = InterleavedGradientNoise(screenPos);
	float totalWeight = 1.0f;
	float3 totalColor = centerColor;

	const int kSamples = 12;
	[unroll]
	for (int i = 0; i < kSamples; ++i)
	{
		float t = ((float(i) + noise) / float(kSamples) - 0.5f) * 2.0f;
		float2 sampleOffsetPx = vMaxDir * (t * vMaxMag);
		float2 sampleUV = uv + sampleOffsetPx * g_MotionBlur.screenResolution.zw;

		if (any(sampleUV < 0.001f) || any(sampleUV > 0.999f))
		{
			continue;
		}

		float sampleDepth = g_TextureDepth.SampleLevel(g_SamplerPointClamp, sampleUV, 0).r;
		float3 sampleCol = g_TextureHDR.SampleLevel(g_SamplerLinearClamp, sampleUV, 0).rgb;
		float2 sampleVel = g_TextureVelocity.SampleLevel(g_SamplerPointClamp, sampleUV, 0).xy * g_MotionBlur.screenResolution.xy * g_MotionBlur.motionBlurParams.x;

		// McGuire depth weighting (Reversed-Z: higher depth is closer to camera)
		float depthDiff = sampleDepth - centerDepth;
		float isForeground = saturate(depthDiff * 800.0f);
		float isBackground = saturate(-depthDiff * 800.0f);

		float weightFg = isForeground * saturate(length(sampleVel) - abs(t * vMaxMag));
		float weightBg = isBackground * saturate(length(centerVel) - abs(t * vMaxMag));
		float weightIn = (1.0f - isForeground) * (1.0f - isBackground);

		float sampleWeight = max(0.001f, weightFg + weightBg + weightIn);
		totalColor += sampleCol * sampleWeight;
		totalWeight += sampleWeight;
	}

	return float4(totalColor / totalWeight, 1.0f);
}
