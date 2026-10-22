Texture2D g_TextureLowerMip   : register(t0);
Texture2D g_TextureCurrentMip : register(t1);
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

float4 main(FullscreenVSOutput input) : SV_Target
{
	float2 uv = input.texcoord;
	float2 ts = g_TexelSize * max(g_Scatter, 0.1f);

	// 9-tap 3x3 tent filter
	float3 a = g_TextureLowerMip.SampleLevel(g_SamplerLinearClamp, uv + float2(-ts.x, -ts.y), 0).rgb;
	float3 b = g_TextureLowerMip.SampleLevel(g_SamplerLinearClamp, uv + float2( 0.0f, -ts.y), 0).rgb;
	float3 c = g_TextureLowerMip.SampleLevel(g_SamplerLinearClamp, uv + float2( ts.x, -ts.y), 0).rgb;

	float3 d = g_TextureLowerMip.SampleLevel(g_SamplerLinearClamp, uv + float2(-ts.x,  0.0f), 0).rgb;
	float3 e = g_TextureLowerMip.SampleLevel(g_SamplerLinearClamp, uv, 0).rgb;
	float3 f = g_TextureLowerMip.SampleLevel(g_SamplerLinearClamp, uv + float2( ts.x,  0.0f), 0).rgb;

	float3 g = g_TextureLowerMip.SampleLevel(g_SamplerLinearClamp, uv + float2(-ts.x,  ts.y), 0).rgb;
	float3 h = g_TextureLowerMip.SampleLevel(g_SamplerLinearClamp, uv + float2( 0.0f,  ts.y), 0).rgb;
	float3 i = g_TextureLowerMip.SampleLevel(g_SamplerLinearClamp, uv + float2( ts.x,  ts.y), 0).rgb;

	float3 upsample = (a + c + g + i) * (1.0f / 16.0f) +
	                  (b + d + f + h) * (2.0f / 16.0f) +
	                  e               * (4.0f / 16.0f);

	float3 current = g_TextureCurrentMip.SampleLevel(g_SamplerLinearClamp, uv, 0).rgb;
	float3 result = lerp(current, upsample, clamp(g_Scatter, 0.0f, 1.0f));

	return float4(result, 1.0f);
}
