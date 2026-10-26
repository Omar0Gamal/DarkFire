#include "Shared/Bindings.h"

cbuffer CBView : register(b1)
{
	CBPerView g_View;
};

Texture2D g_TextureColor : register(t0);
Texture2D g_TextureDepth : register(t9);
Texture2D g_TextureNormalRoughness : register(t15);

cbuffer CBSSSData : register(b8)
{
	CBSSS g_SSS;
};

SamplerState g_SamplerPointClamp  : register(s2);
SamplerState g_SamplerLinearClamp : register(s1);

struct VSOutput
{
	float4 position : SV_Position;
	float2 uv       : TEXCOORD0;
};

// 7-tap Gaussian weights with center weight
static const float s_weights[4] = { 0.382928f, 0.241732f, 0.060598f, 0.005977f };
static const float s_offsets[4] = { 0.0f, 1.411764f, 3.294117f, 5.176470f };

float4 main(VSOutput input) : SV_Target
{
	float depthCenter = g_TextureDepth.SampleLevel(g_SamplerPointClamp, input.uv, 0).r;
	if (depthCenter <= 0.00001f)
	{
		return g_TextureColor.SampleLevel(g_SamplerPointClamp, input.uv, 0);
	}

	float4 centerColor = g_TextureColor.SampleLevel(g_SamplerPointClamp, input.uv, 0);
	float radius = g_SSS.sssParams.x;
	float maxDepthDiff = max(g_SSS.sssParams.y, 0.0001f);
	float2 dir = g_SSS.sssDirection.xy * (g_View.invViewportSize * radius);

	float4 totalColor = centerColor * s_weights[0];
	float totalWeight = s_weights[0];

	[unroll]
	for (int i = 1; i < 4; ++i)
	{
		float2 uvOffset = dir * s_offsets[i];

		// Positive tap
		float2 uvPos = input.uv + uvOffset;
		float depthPos = g_TextureDepth.SampleLevel(g_SamplerPointClamp, uvPos, 0).r;
		float weightPos = s_weights[i] * exp(-abs(depthPos - depthCenter) / maxDepthDiff);
		totalColor += g_TextureColor.SampleLevel(g_SamplerLinearClamp, uvPos, 0) * weightPos;
		totalWeight += weightPos;

		// Negative tap
		float2 uvNeg = input.uv - uvOffset;
		float depthNeg = g_TextureDepth.SampleLevel(g_SamplerPointClamp, uvNeg, 0).r;
		float weightNeg = s_weights[i] * exp(-abs(depthNeg - depthCenter) / maxDepthDiff);
		totalColor += g_TextureColor.SampleLevel(g_SamplerLinearClamp, uvNeg, 0) * weightNeg;
		totalWeight += weightNeg;
	}

	return totalColor / max(totalWeight, 0.00001f);
}
