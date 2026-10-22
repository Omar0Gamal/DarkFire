Texture2D g_TextureRawSSAO : register(t0);
Texture2D g_TextureDepth   : register(t9);
SamplerState g_SamplerPointClamp : register(s2);
SamplerState g_SamplerLinearClamp : register(s1);

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

float4 main(FullscreenVSOutput input) : SV_Target
{
	float centerDepth = g_TextureDepth.SampleLevel(g_SamplerPointClamp, input.texcoord, 0).r;
	if (centerDepth <= 0.00001f)
	{
		return float4(1.0f, 1.0f, 1.0f, 1.0f);
	}

	float totalAO = 0.0f;
	float totalWeight = 0.0f;

	// 4x4 bilateral filter box [-1.5, 1.5]
	for (int x = -2; x <= 2; ++x)
	{
		for (int y = -2; y <= 2; ++y)
		{
			float2 offset = float2(x, y) * g_TexelSize;
			float2 sampleUV = input.texcoord + offset;

			float sampleDepth = g_TextureDepth.SampleLevel(g_SamplerPointClamp, sampleUV, 0).r;
			float sampleAO = g_TextureRawSSAO.SampleLevel(g_SamplerLinearClamp, sampleUV, 0).r;

			// Depth difference weight
			float depthDiff = abs(centerDepth - sampleDepth);
			float weight = 1.0f / (1.0f + depthDiff * 400.0f);

			totalAO += sampleAO * weight;
			totalWeight += weight;
		}
	}

	float finalAO = totalWeight > 0.0001f ? (totalAO / totalWeight) : 1.0f;
	return float4(finalAO, finalAO, finalAO, 1.0f);
}
