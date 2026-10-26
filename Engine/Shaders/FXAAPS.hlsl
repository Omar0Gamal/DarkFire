Texture2D g_TextureLDR : register(t0);
SamplerState g_SamplerLinearClamp : register(s1);

cbuffer CBFXAA : register(b0)
{
	float2 g_TexelSize;
	float  g_Subpix;
	float  g_EdgeThreshold;
	float  g_EdgeThresholdMin;
	float3 g_Pad;
};

struct FullscreenVSOutput
{
	float4 positionCS : SV_Position;
	float2 texcoord   : TEXCOORD0;
};

// NVIDIA Fast Approximate Anti-Aliasing (FXAA 3.11 Quality Preset 12)
// Full edge-span search + sub-pixel filtering using perceptual luma from alpha
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

	float edgeThreshold = max(g_EdgeThreshold, 0.063f);
	float edgeThresholdMin = max(g_EdgeThresholdMin, 0.0312f);

	if (lumaRange < max(edgeThresholdMin, lumaMax * edgeThreshold))
	{
		// Optional Contrast Adaptive Sharpening (CAS) on non-edge interior detail (g_Pad.x = sharpness [0..1])
		float sharpness = saturate(g_Pad.x);
		if (sharpness > 0.001f)
		{
			float3 rgbN = g_TextureLDR.SampleLevel(g_SamplerLinearClamp, uv + float2(0.0f, -g_TexelSize.y), 0).rgb;
			float3 rgbS = g_TextureLDR.SampleLevel(g_SamplerLinearClamp, uv + float2(0.0f,  g_TexelSize.y), 0).rgb;
			float3 rgbE = g_TextureLDR.SampleLevel(g_SamplerLinearClamp, uv + float2( g_TexelSize.x, 0.0f), 0).rgb;
			float3 rgbW = g_TextureLDR.SampleLevel(g_SamplerLinearClamp, uv + float2(-g_TexelSize.x, 0.0f), 0).rgb;

			float minG = min(rgbM.g, min(min(rgbN.g, rgbS.g), min(rgbE.g, rgbW.g)));
			float maxG = max(rgbM.g, max(max(rgbN.g, rgbS.g), max(rgbE.g, rgbW.g)));
			float ampG = saturate(min(minG, 1.0f - maxG) / max(maxG, 0.0001f));
			float peak = -1.0f / lerp(8.0f, 5.0f, sharpness);
			float w = sqrt(ampG) * peak;
			float3 casRGB = saturate((rgbM + (rgbN + rgbS + rgbE + rgbW) * w) / (1.0f + 4.0f * w));
			return float4(casRGB, 1.0f);
		}
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

	// 1. Sub-pixel aliasing test
	float lumaL = (lumaN + lumaS + lumaE + lumaW) * 0.25f;
	float rangeL = abs(lumaL - lumaM);
	float subPixelBlend = saturate(rangeL / lumaRange);
	float subPixelOffset = (-2.0f * subPixelBlend + 3.0f) * subPixelBlend * subPixelBlend;
	// Sub-pixel quality in [0.0, 1.0] controls anti-aliasing strength on thin edges and fine details
	float subPixelQuality = clamp(g_Subpix, 0.0f, 1.0f);
	float subpixOffsetFinal = subPixelOffset * subPixelQuality;

	// 2. Full 10-step Edge-Span Search along tangent direction
	float2 edgeUV = uv;
	if (isHorizontal)
	{
		edgeUV.y += stepLength * 0.5f;
	}
	else
	{
		edgeUV.x += stepLength * 0.5f;
	}

	float2 tangentStep = isHorizontal ? float2(g_TexelSize.x, 0.0f) : float2(0.0f, g_TexelSize.y);
	float lumaEdgeTarget = (lumaM + (grad1 < grad2 ? luma2 : luma1)) * 0.5f;
	float lumaGradient = max(grad1, grad2) * 0.25f;

	float2 uvN = edgeUV - tangentStep;
	float2 uvP = edgeUV + tangentStep;

	float lumaEndN = g_TextureLDR.SampleLevel(g_SamplerLinearClamp, uvN, 0).a - lumaEdgeTarget;
	float lumaEndP = g_TextureLDR.SampleLevel(g_SamplerLinearClamp, uvP, 0).a - lumaEdgeTarget;

	bool doneN = abs(lumaEndN) >= lumaGradient;
	bool doneP = abs(lumaEndP) >= lumaGradient;

	static const float s_sampleSteps[8] = { 1.5f, 2.0f, 2.0f, 2.0f, 2.0f, 4.0f, 8.0f, 8.0f };

	[unroll]
	for (int i = 0; i < 8; ++i)
	{
		if (!doneN)
		{
			uvN -= tangentStep * s_sampleSteps[i];
			lumaEndN = g_TextureLDR.SampleLevel(g_SamplerLinearClamp, uvN, 0).a - lumaEdgeTarget;
			doneN = abs(lumaEndN) >= lumaGradient;
		}
		if (!doneP)
		{
			uvP += tangentStep * s_sampleSteps[i];
			lumaEndP = g_TextureLDR.SampleLevel(g_SamplerLinearClamp, uvP, 0).a - lumaEdgeTarget;
			doneP = abs(lumaEndP) >= lumaGradient;
		}
		if (doneN && doneP) break;
	}

	float distN = isHorizontal ? (uv.x - uvN.x) : (uv.y - uvN.y);
	float distP = isHorizontal ? (uvP.x - uv.x) : (uvP.y - uv.y);

	float spanLength = distN + distP;
	float distToNearest = min(distN, distP);
	bool isDirectionN = (distN < distP);
	float lumaEndNearest = isDirectionN ? lumaEndN : lumaEndP;

	float spanOffset = 0.0f;
	if ((lumaM - lumaEdgeTarget < 0.0f) == (lumaEndNearest < 0.0f))
	{
		spanOffset = 0.0f;
	}
	else
	{
		spanOffset = 0.5f - (distToNearest / max(spanLength, 0.0001f));
	}

	float finalOffset = max(subpixOffsetFinal, spanOffset);

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
