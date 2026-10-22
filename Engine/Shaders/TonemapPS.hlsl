Texture2D g_TextureHDR   : register(t0);
Texture2D g_TextureBloom : register(t10);
Texture2D g_TextureSSAO  : register(t11);

SamplerState g_SamplerPointClamp  : register(s2);
SamplerState g_SamplerLinearClamp : register(s1);

#include "Shared/Bindings.h"

cbuffer CBPostProcessBuffer : register(b0)
{
	CBPostProcess g_Post;
};

struct FullscreenVSOutput
{
	float4 positionCS : SV_Position;
	float2 texcoord   : TEXCOORD0;
};

// ACES Filmic tonemapper (Narkowicz 2015)
float3 TonemapACES(float3 x)
{
	float a = 2.51f;
	float b = 0.03f;
	float c = 2.43f;
	float d = 0.59f;
	float e = 0.14f;
	return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

// Reinhard tonemapper
float3 TonemapReinhard(float3 x)
{
	return x / (1.0f + x);
}

// Khronos PBR Neutral tonemapper
float3 TonemapNeutral(float3 color)
{
	static const float startCompression = 0.8f - 0.04f;
	static const float desaturation = 0.15f;

	float x = min(color.r, min(color.g, color.b));
	float offset = x < 0.08f ? x - 6.25f * x * x : 0.04f;
	color -= offset;

	float peak = max(color.r, max(color.g, color.b));
	if (peak < startCompression) return color;

	const float d = 1.0f - startCompression;
	float newPeak = 1.0f - d * d / (peak + d - startCompression);
	color *= newPeak / peak;

	float g = 1.0f - 1.0f / (desaturation * (peak - newPeak) + 1.0f);
	return lerp(color, newPeak.xxx, g);
}

// Exact piecewise sRGB Opto-Electronic Transfer Function (OETF)
float3 LinearToSRGBExact(float3 c)
{
	c = max(c, float3(0.0f, 0.0f, 0.0f));
	float3 lo = c * 12.92f;
	float3 hi = 1.055f * pow(c, float3(1.0f / 2.4f, 1.0f / 2.4f, 1.0f / 2.4f)) - 0.055f;
	return float3(
		c.r <= 0.0031308f ? lo.r : hi.r,
		c.g <= 0.0031308f ? lo.g : hi.g,
		c.b <= 0.0031308f ? lo.b : hi.b
	);
}

float4 main(FullscreenVSOutput input) : SV_Target
{
	// Debug views
	if (g_Post.debugView == 1u) // SSAO only
	{
		float ssao = g_TextureSSAO.SampleLevel(g_SamplerLinearClamp, input.texcoord, 0).r;
		return float4(ssao, ssao, ssao, 1.0f);
	}
	if (g_Post.debugView == 2u) // Bloom only
	{
		float3 bloom = g_TextureBloom.SampleLevel(g_SamplerLinearClamp, input.texcoord, 0).rgb;
		return float4(bloom, 1.0f);
	}

	float3 hdr = g_TextureHDR.SampleLevel(g_SamplerPointClamp, input.texcoord, 0).rgb;

	// Composite bloom
	if (g_Post.bloomIntensity > 0.0001f)
	{
		float3 bloom = g_TextureBloom.SampleLevel(g_SamplerLinearClamp, input.texcoord, 0).rgb;
		hdr += bloom * g_Post.bloomIntensity * g_Post.bloomTint.rgb;
	}

	// Exposure
	float3 color = hdr * max(g_Post.exposure, 0.0001f);

	// Color filter / White balance
	color *= g_Post.colorFilter.rgb;

	// Tonemapper selection: 0=ACES, 1=AgX, 2=Reinhard, 3=Neutral, 4=None
	if (g_Post.tonemapper == 0u)
	{
		color = TonemapACES(color);
	}
	else if (g_Post.tonemapper == 2u)
	{
		color = TonemapReinhard(color);
	}
	else if (g_Post.tonemapper == 3u)
	{
		color = TonemapNeutral(color);
	}
	else if (g_Post.tonemapper == 4u)
	{
		color = saturate(color);
	}
	else
	{
		color = TonemapACES(color);
	}

	// Contrast (midtone-centered around 0.5)
	if (abs(g_Post.contrast - 1.0f) > 0.001f)
	{
		color = saturate((color - 0.5f) * g_Post.contrast + 0.5f);
	}

	// Saturation
	if (abs(g_Post.saturation - 1.0f) > 0.001f)
	{
		float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
		color = saturate(lerp(float3(luma, luma, luma), color, g_Post.saturation));
	}

	// Vignette
	if (g_Post.vignetteIntensity > 0.001f)
	{
		float2 uv = input.texcoord - 0.5f;
		float dist = length(uv);
		float vig = smoothstep(0.8f, max(0.8f - g_Post.vignetteSmoothness, 0.1f), dist * (g_Post.vignetteIntensity + 0.5f));
		color *= clamp(vig, 0.0f, 1.0f);
	}

	// Piecewise sRGB OETF
	float3 srgb = LinearToSRGBExact(color);

	// Pack perceptual luma in alpha for single-pass FXAA
	float luma = dot(srgb, float3(0.299f, 0.587f, 0.114f));

	return float4(saturate(srgb), luma);
}
