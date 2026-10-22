Texture2D g_TextureHDR : register(t0);
SamplerState g_SamplerLinearClamp : register(s1);

cbuffer CBBloomBuffer : register(b5)
{
	float g_Threshold;
	float g_Knee;
	float g_Scatter;
	float g_Intensity;
	float4 g_Tint;
	float2 g_TexelSize;
	float g_MipLevel;
	float g_Pad;
};

struct FullscreenVSOutput
{
	float4 positionCS : SV_Position;
	float2 texcoord   : TEXCOORD0;
};

float Luma(float3 c)
{
	return dot(c, float3(0.2126f, 0.7152f, 0.0722f));
}

float3 KarisWeight(float3 c)
{
	return c / (1.0f + Luma(c));
}

float3 SoftThreshold(float3 color, float threshold, float knee)
{
	float brightness = max(color.r, max(color.g, color.b));
	float soft = brightness - threshold + knee;
	soft = clamp(soft, 0.0f, 2.0f * knee);
	soft = (soft * soft) / (4.0f * max(knee, 0.0001f));
	float contribution = max(soft, brightness - threshold) / max(brightness, 0.0001f);
	return color * max(contribution, 0.0f);
}

float4 main(FullscreenVSOutput input) : SV_Target
{
	float2 uv = input.texcoord;
	float2 ts = g_TexelSize;

	// 13-tap Jimenez filter pattern
	float3 a = g_TextureHDR.SampleLevel(g_SamplerLinearClamp, uv + float2(-2.0f * ts.x, -2.0f * ts.y), 0).rgb;
	float3 b = g_TextureHDR.SampleLevel(g_SamplerLinearClamp, uv + float2( 0.0f,        -2.0f * ts.y), 0).rgb;
	float3 c = g_TextureHDR.SampleLevel(g_SamplerLinearClamp, uv + float2( 2.0f * ts.x, -2.0f * ts.y), 0).rgb;

	float3 d = g_TextureHDR.SampleLevel(g_SamplerLinearClamp, uv + float2(-1.0f * ts.x, -1.0f * ts.y), 0).rgb;
	float3 e = g_TextureHDR.SampleLevel(g_SamplerLinearClamp, uv + float2( 1.0f * ts.x, -1.0f * ts.y), 0).rgb;

	float3 f = g_TextureHDR.SampleLevel(g_SamplerLinearClamp, uv + float2(-2.0f * ts.x,  0.0f       ), 0).rgb;
	float3 g = g_TextureHDR.SampleLevel(g_SamplerLinearClamp, uv, 0).rgb;
	float3 h = g_TextureHDR.SampleLevel(g_SamplerLinearClamp, uv + float2( 2.0f * ts.x,  0.0f       ), 0).rgb;

	float3 i = g_TextureHDR.SampleLevel(g_SamplerLinearClamp, uv + float2(-1.0f * ts.x,  1.0f * ts.y), 0).rgb;
	float3 j = g_TextureHDR.SampleLevel(g_SamplerLinearClamp, uv + float2( 1.0f * ts.x,  1.0f * ts.y), 0).rgb;

	float3 k = g_TextureHDR.SampleLevel(g_SamplerLinearClamp, uv + float2(-2.0f * ts.x,  2.0f * ts.y), 0).rgb;
	float3 l = g_TextureHDR.SampleLevel(g_SamplerLinearClamp, uv + float2( 0.0f,         2.0f * ts.y), 0).rgb;
	float3 m = g_TextureHDR.SampleLevel(g_SamplerLinearClamp, uv + float2( 2.0f * ts.x,  2.0f * ts.y), 0).rgb;

	// Apply soft threshold to each sample
	a = SoftThreshold(a, g_Threshold, g_Knee);
	b = SoftThreshold(b, g_Threshold, g_Knee);
	c = SoftThreshold(c, g_Threshold, g_Knee);
	d = SoftThreshold(d, g_Threshold, g_Knee);
	e = SoftThreshold(e, g_Threshold, g_Knee);
	f = SoftThreshold(f, g_Threshold, g_Knee);
	g = SoftThreshold(g, g_Threshold, g_Knee);
	h = SoftThreshold(h, g_Threshold, g_Knee);
	i = SoftThreshold(i, g_Threshold, g_Knee);
	j = SoftThreshold(j, g_Threshold, g_Knee);
	k = SoftThreshold(k, g_Threshold, g_Knee);
	l = SoftThreshold(l, g_Threshold, g_Knee);
	m = SoftThreshold(m, g_Threshold, g_Knee);

	// Karis weighted average over 5 overlapping boxes to kill fireflies
	float3 box0 = (d + e + i + j) * 0.25f;
	float3 box1 = (a + b + f + g) * 0.25f;
	float3 box2 = (b + c + g + h) * 0.25f;
	float3 box3 = (f + g + k + l) * 0.25f;
	float3 box4 = (g + h + l + m) * 0.25f;

	float w0 = 1.0f / (1.0f + Luma(box0));
	float w1 = 1.0f / (1.0f + Luma(box1));
	float w2 = 1.0f / (1.0f + Luma(box2));
	float w3 = 1.0f / (1.0f + Luma(box3));
	float w4 = 1.0f / (1.0f + Luma(box4));

	float3 result = (box0 * 0.5f * w0 + (box1 + box2 + box3 + box4) * 0.125f * (w1 + w2 + w3 + w4) * 0.25f);
	float totalW = 0.5f * w0 + 0.125f * (w1 + w2 + w3 + w4);
	result = totalW > 0.0001f ? (result / totalW) : float3(0.0f, 0.0f, 0.0f);

	return float4(result, 1.0f);
}
