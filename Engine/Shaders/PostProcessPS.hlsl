Texture2D g_TextureHdr : register(t0);
SamplerState g_SamplerLinearClamp : register(s1);
SamplerState g_SamplerPointClamp : register(s2);

cbuffer CBPostProcess : register(b0)
{
	float g_Exposure;
	float g_EnableFXAA;
	float2 g_TexelSize;
};

struct FullscreenVSOutput
{
	float4 positionCS : SV_Position;
	float2 texcoord   : TEXCOORD0;
};

// Narkowicz 2015 ACES filmic tonemap curve
float3 ACESFilm(float3 x)
{
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 TonemapAndGamma(float3 hdr)
{
	float3 color = hdr * (g_Exposure > 0.001f ? g_Exposure : 1.0f);
	float3 ldr = ACESFilm(color);
	return pow(max(ldr, float3(0.0f, 0.0f, 0.0f)), float3(1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f));
}

float GetLuma(float3 rgb)
{
	return dot(rgb, float3(0.299f, 0.587f, 0.114f));
}

// NVIDIA Fast Approximate Anti-Aliasing (FXAA 3.11 Quality)
float3 ApplyFXAA(float2 uv)
{
	float3 rgbM = TonemapAndGamma(g_TextureHdr.SampleLevel(g_SamplerLinearClamp, uv, 0).rgb);
	float3 rgbN = TonemapAndGamma(g_TextureHdr.SampleLevel(g_SamplerLinearClamp, uv + float2(0.0f, -g_TexelSize.y), 0).rgb);
	float3 rgbS = TonemapAndGamma(g_TextureHdr.SampleLevel(g_SamplerLinearClamp, uv + float2(0.0f,  g_TexelSize.y), 0).rgb);
	float3 rgbE = TonemapAndGamma(g_TextureHdr.SampleLevel(g_SamplerLinearClamp, uv + float2( g_TexelSize.x, 0.0f), 0).rgb);
	float3 rgbW = TonemapAndGamma(g_TextureHdr.SampleLevel(g_SamplerLinearClamp, uv + float2(-g_TexelSize.x, 0.0f), 0).rgb);

	float lumaM = GetLuma(rgbM);
	float lumaN = GetLuma(rgbN);
	float lumaS = GetLuma(rgbS);
	float lumaE = GetLuma(rgbE);
	float lumaW = GetLuma(rgbW);

	float lumaMin = min(lumaM, min(min(lumaN, lumaS), min(lumaE, lumaW)));
	float lumaMax = max(lumaM, max(max(lumaN, lumaS), max(lumaE, lumaW)));
	float lumaRange = lumaMax - lumaMin;

	// Edge detection threshold: if contrast is below threshold, skip AA to preserve sharp textures
	static const float FXAA_EDGE_THRESHOLD_MIN = 0.0312f;
	static const float FXAA_EDGE_THRESHOLD = 0.125f;

	if (lumaRange < max(FXAA_EDGE_THRESHOLD_MIN, lumaMax * FXAA_EDGE_THRESHOLD))
	{
		return rgbM;
	}

	// Sample 4 diagonal corners
	float lumaNW = GetLuma(TonemapAndGamma(g_TextureHdr.SampleLevel(g_SamplerLinearClamp, uv + float2(-g_TexelSize.x, -g_TexelSize.y), 0).rgb));
	float lumaNE = GetLuma(TonemapAndGamma(g_TextureHdr.SampleLevel(g_SamplerLinearClamp, uv + float2( g_TexelSize.x, -g_TexelSize.y), 0).rgb));
	float lumaSW = GetLuma(TonemapAndGamma(g_TextureHdr.SampleLevel(g_SamplerLinearClamp, uv + float2(-g_TexelSize.x,  g_TexelSize.y), 0).rgb));
	float lumaSE = GetLuma(TonemapAndGamma(g_TextureHdr.SampleLevel(g_SamplerLinearClamp, uv + float2( g_TexelSize.x,  g_TexelSize.y), 0).rgb));

	// Determine edge orientation (horizontal vs vertical)
	float edgeHorz = abs((lumaNW + lumaNE) - 2.0f * lumaN) + 2.0f * abs((lumaW + lumaE) - 2.0f * lumaM) + abs((lumaSW + lumaSE) - 2.0f * lumaS);
	float edgeVert = abs((lumaNW + lumaSW) - 2.0f * lumaW) + 2.0f * abs((lumaN + lumaS) - 2.0f * lumaM) + abs((lumaNE + lumaSE) - 2.0f * lumaE);
	bool isHorizontal = (edgeHorz >= edgeVert);

	// Determine gradient step direction
	float luma1 = isHorizontal ? lumaN : lumaW;
	float luma2 = isHorizontal ? lumaS : lumaE;
	float grad1 = abs(luma1 - lumaM);
	float grad2 = abs(luma2 - lumaM);

	float stepLength = isHorizontal ? g_TexelSize.y : g_TexelSize.x;
	if (grad1 < grad2)
	{
		stepLength = -stepLength;
	}

	// Sub-pixel low-pass blending
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

	return TonemapAndGamma(g_TextureHdr.SampleLevel(g_SamplerLinearClamp, sampleUV, 0).rgb);
}

float4 main(FullscreenVSOutput input) : SV_Target
{
	float3 finalColor;

	if (g_EnableFXAA > 0.5f)
	{
		finalColor = ApplyFXAA(input.texcoord);
	}
	else
	{
		float4 hdrColor = g_TextureHdr.Sample(g_SamplerPointClamp, input.texcoord);
		finalColor = TonemapAndGamma(hdrColor.rgb);
	}

	return float4(finalColor, 1.0f);
}
