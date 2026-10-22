Texture2D g_TextureSource : register(t0);
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
	float2 ts = g_TexelSize;

	// 13-tap downsampling filter (Jimenez 2014)
	float3 a = g_TextureSource.SampleLevel(g_SamplerLinearClamp, uv + float2(-2.0f * ts.x, -2.0f * ts.y), 0).rgb;
	float3 b = g_TextureSource.SampleLevel(g_SamplerLinearClamp, uv + float2( 0.0f,        -2.0f * ts.y), 0).rgb;
	float3 c = g_TextureSource.SampleLevel(g_SamplerLinearClamp, uv + float2( 2.0f * ts.x, -2.0f * ts.y), 0).rgb;

	float3 d = g_TextureSource.SampleLevel(g_SamplerLinearClamp, uv + float2(-1.0f * ts.x, -1.0f * ts.y), 0).rgb;
	float3 e = g_TextureSource.SampleLevel(g_SamplerLinearClamp, uv + float2( 1.0f * ts.x, -1.0f * ts.y), 0).rgb;

	float3 f = g_TextureSource.SampleLevel(g_SamplerLinearClamp, uv + float2(-2.0f * ts.x,  0.0f       ), 0).rgb;
	float3 g = g_TextureSource.SampleLevel(g_SamplerLinearClamp, uv, 0).rgb;
	float3 h = g_TextureSource.SampleLevel(g_SamplerLinearClamp, uv + float2( 2.0f * ts.x,  0.0f       ), 0).rgb;

	float3 i = g_TextureSource.SampleLevel(g_SamplerLinearClamp, uv + float2(-1.0f * ts.x,  1.0f * ts.y), 0).rgb;
	float3 j = g_TextureSource.SampleLevel(g_SamplerLinearClamp, uv + float2( 1.0f * ts.x,  1.0f * ts.y), 0).rgb;

	float3 k = g_TextureSource.SampleLevel(g_SamplerLinearClamp, uv + float2(-2.0f * ts.x,  2.0f * ts.y), 0).rgb;
	float3 l = g_TextureSource.SampleLevel(g_SamplerLinearClamp, uv + float2( 0.0f,         2.0f * ts.y), 0).rgb;
	float3 m = g_TextureSource.SampleLevel(g_SamplerLinearClamp, uv + float2( 2.0f * ts.x,  2.0f * ts.y), 0).rgb;

	float3 box0 = (d + e + i + j) * 0.25f;
	float3 box1 = (a + b + f + g) * 0.25f;
	float3 box2 = (b + c + g + h) * 0.25f;
	float3 box3 = (f + g + k + l) * 0.25f;
	float3 box4 = (g + h + l + m) * 0.25f;

	float3 result = box0 * 0.5f + (box1 + box2 + box3 + box4) * 0.125f;
	return float4(result, 1.0f);
}
