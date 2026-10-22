Texture2D g_TextureLDR : register(t0);
SamplerState g_SamplerLinearClamp : register(s1);

cbuffer CBFXAA : register(b0)
{
	float2 g_TexelSize;
	float2 g_Pad;
};

struct FullscreenVSOutput
{
	float4 positionCS : SV_Position;
	float2 texcoord   : TEXCOORD0;
};

// NVIDIA Fast Approximate Anti-Aliasing (FXAA 3.11 Quality Preset 12)
// Reads precomputed perceptual luma directly from alpha channel!
float4 main(FullscreenVSOutput input) : SV_Target
{
	float2 uv = input.texcoord;

	float4 sampleM = g_TextureLDR.SampleLevel(g_SamplerLinearClamp, uv, 0);
	float3 rgbM = sampleM.rgb;
	float lumaM = sampleM.a;

	float lumaN = g_TextureLDR.SampleLevel(g_SamplerLinearClamp, uv + float2(0.0f, -g_TexelSize.y), 0).a;
	float lumaS = g_TextureLDR.SampleLevel(g_SamplerLinearClamp, uv + float2(0.0f,  g_TexelSize.y), 0).a;
	float lumaE = g_TextureLDR.SampleLevel(g_SamplerLinearClamp, uv + float2( g_TexelSize.x, 0.0f), 0).a;
	float lumaW = g_TextureLDR.SampleLevel(g_SamplerLinearClamp, uv + float2(-g_TexelSize.x, 0.0f), 0).a;

	float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaE, lumaW)));
	float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaE, lumaW)));
	float lumaRange = lumaMax - lumaMin;

	static const float FXAA_EDGE_THRESHOLD_MIN = 0.0312f;
	static const float FXAA_EDGE_THRESHOLD = 0.125f;

	if (lumaRange < max(FXAA_EDGE_THRESHOLD_MIN, lumaMax * FXAA_EDGE_THRESHOLD))
	{
		return float4(rgbM, 1.0f);
	}

	float lumaNW = g_TextureLDR.SampleLevel(g_SamplerLinearClamp, uv + float2(-g_TexelSize.x, -g_TexelSize.y), 0).a;
	float lumaNE = g_TextureLDR.SampleLevel(g_SamplerLinearClamp, uv + float2( g_TexelSize.x, -g_TexelSize.y), 0).a;
	float lumaSW = g_TextureLDR.SampleLevel(g_SamplerLinearClamp, uv + float2(-g_TexelSize.x,  g_TexelSize.y), 0).a;
	float lumaSE = g_TextureLDR.SampleLevel(g_SamplerLinearClamp, uv + float2( g_TexelSize.x,  g_TexelSize.y), 0).a;

	float edgeHorz = abs((lumaNW + lumaNE) - 2.0f * lumaN) + 2.0f * abs((lumaW + lumaE) - 2.0f * lumaM) + abs((lumaSW + lumaSE) - 2.0f * lumaS);
	float edgeVert = abs((lumaNW + lumaSW) - 2.0f * lumaW) + 2.0f * abs((lumaN + lumaS) - 2.0f * lumaM) + abs((lumaNE + lumaSE) - 2.0f * lumaE);
	bool isHorizontal = (edgeHorz >= edgeVert);

	float luma1 = isHorizontal ? lumaN : lumaW;
	float luma2 = isHorizontal ? lumaS : lumaE;
	float grad1 = abs(luma1 - lumaM);
	float grad2 = abs(luma2 - lumaM);

	float stepLength = isHorizontal ? g_TexelSize.y : g_TexelSize.x;
	if (grad1 < grad2)
	{
		stepLength = -stepLength;
	}

	float lumaL = (lumaN + lumaS + lumaE + lumaW) * 0.25f;
	float rangeL = abs(lumaL - lumaM);
	float subPixelBlend = saturate(rangeL / lumaRange);
	float subPixelOffset = (-2.0f * subPixelBlend + 3.0f) * subPixelBlend * subPixelBlend;
	float finalOffset = max(subPixelOffset * 0.75f, 0.0f);

	float2 sampleUV = uv;
	if (isHorizontal)
	{
		sampleUV.y += stepLength * finalOffset;
	}
	else
	{
		sampleUV.x += stepLength * finalOffset;
	}

	float3 filteredRGB = g_TextureLDR.SampleLevel(g_SamplerLinearClamp, sampleUV, 0).rgb;
	return float4(filteredRGB, 1.0f);
}
