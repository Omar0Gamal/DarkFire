Texture2D g_TextureHDR             : register(t0);
Texture2D g_TextureBloom           : register(t10);
Texture2D g_TextureSSAO            : register(t11);
Texture2D g_TextureShadowMap       : register(t13);
Texture2D g_TextureNormalRoughness : register(t15);
Texture2D g_TextureAlbedoMetallic  : register(t16);
Texture3D g_TextureColorLUT        : register(t20);
Texture2D g_TextureVelocity        : register(t21);
Texture2D g_TextureSSR             : register(t24);

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

// ACES Filmic tonemapper with AP1 Gamut Compression (Stephen Hill / ACES fitted)
float3 TonemapACES(float3 color)
{
	static const float3x3 ACESInputMat = float3x3(
		0.59719f, 0.35458f, 0.04823f,
		0.07600f, 0.90834f, 0.01566f,
		0.02840f, 0.13383f, 0.83777f
	);

	static const float3x3 ACESOutputMat = float3x3(
		 1.60475f, -0.53108f, -0.07367f,
		-0.10208f,  1.10813f, -0.00605f,
		-0.00327f, -0.07276f,  1.07602f
	);

	color = mul(ACESInputMat, color);

	// RRT and ODT curve fit
	float3 a = color * (color + 0.0245786f) - 0.000090537f;
	float3 b = color * (0.983729f * color + 0.4329510f) + 0.238081f;
	color = a / b;

	color = mul(ACESOutputMat, color);
	return saturate(color);
}

// AgX tonemapper (Troy Sobotka / Blender 4.x / Three.js)
static const float3x3 AgX_InputMatrix = float3x3(
	0.842479062253094, 0.0784335999999992, 0.079223746654848,
	0.0423282422610123, 0.878468036492047, 0.079161664536979,
	0.0423756549057051, 0.0780507008696803, 0.879737351659843
);

static const float3x3 AgX_OutputMatrix = float3x3(
	1.196879024258300, -0.052896851700204, -0.052971635532578,
	-0.098020881140137, 1.151903129904170, -0.098043450125866,
	-0.099029744079720, -0.098961176844843, 1.151073672641160
);

float3 AgXDefaultContrastApprox(float3 x)
{
	float3 x2 = x * x;
	float3 x4 = x2 * x2;
	return + 15.5f * x4 * x2
	       - 40.14f * x4 * x
	       + 31.96f * x4
	       - 6.868f * x2 * x
	       + 0.4298f * x2
	       + 0.1191f * x
	       - 0.00232f;
}

float3 TonemapAgX(float3 val)
{
	val = max(val, 0.0f);
	val = mul(AgX_InputMatrix, val);

	// AgX Log encoding: min -10.0 EV, max +6.5 EV
	const float minEV = -10.0f;
	const float maxEV = 6.5f;
	val = clamp(log2(max(val, 1e-6f)) * (1.0f / (maxEV - minEV)) - (minEV / (maxEV - minEV)), 0.0f, 1.0f);
	val = clamp(AgXDefaultContrastApprox(val), 0.0f, 1.0f);
	val = mul(AgX_OutputMatrix, val);
	return saturate(val);
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
	if (g_Post.debugView == 3u) // Shadow Map depth only
	{
		float sm = g_TextureShadowMap.SampleLevel(g_SamplerPointClamp, input.texcoord, 0).r;
		return float4(sm, sm, sm, 1.0f);
	}
	if (g_Post.debugView == 4u) // World Normals
	{
		float3 normals = g_TextureNormalRoughness.SampleLevel(g_SamplerPointClamp, input.texcoord, 0).rgb;
		return float4(normals, 1.0f);
	}
	if (g_Post.debugView == 5u) // Albedo
	{
		float3 albedo = g_TextureAlbedoMetallic.SampleLevel(g_SamplerPointClamp, input.texcoord, 0).rgb;
		return float4(albedo, 1.0f);
	}
	if (g_Post.debugView == 6u) // Metallic / Roughness
	{
		float4 nm = g_TextureNormalRoughness.SampleLevel(g_SamplerPointClamp, input.texcoord, 0);
		float4 am = g_TextureAlbedoMetallic.SampleLevel(g_SamplerPointClamp, input.texcoord, 0);
		return float4(0.0f, abs(nm.a), am.a, 1.0f);
	}
	if (g_Post.debugView == 7u) // Motion Vectors / Velocity
	{
		float2 vel = g_TextureVelocity.SampleLevel(g_SamplerPointClamp, input.texcoord, 0).rg;
		return float4(saturate(abs(vel.x) * 50.0f), saturate(abs(vel.y) * 50.0f), saturate(length(vel) * 25.0f), 1.0f);
	}
	if (g_Post.debugView == 8u) // Screen-Space Reflections (SSR)
	{
		float4 ssr = g_TextureSSR.SampleLevel(g_SamplerLinearClamp, input.texcoord, 0);
		return float4(ssr.rgb, 1.0f);
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

	// Contrast (midtone-centered around 0.18 in linear space)
	if (abs(g_Post.contrast - 1.0f) > 0.001f)
	{
		color = max(pow(max(color / 0.18f, 0.0f), g_Post.contrast) * 0.18f, 0.0f);
	}

	// Saturation
	if (abs(g_Post.saturation - 1.0f) > 0.001f)
	{
		float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
		color = max(lerp(float3(luma, luma, luma), color, g_Post.saturation), 0.0f);
	}

	// Vignette (smooth optical falloff)
	if (g_Post.vignetteIntensity > 0.001f)
	{
		float2 uv = input.texcoord - 0.5f;
		float dist = length(uv);
		float inner = max(0.1f, 0.75f - g_Post.vignetteSmoothness * 0.5f);
		float outer = inner + max(g_Post.vignetteSmoothness * 0.5f, 0.1f);
		float vig = 1.0f - smoothstep(inner, outer, dist * (g_Post.vignetteIntensity + 0.5f));
		color *= saturate(vig);
	}

	// Tonemapper selection: 0=ACES, 1=Reinhard, 2=Linear/None, 3=AgX, 4=Neutral
	if (g_Post.tonemapper == 0u)
	{
		color = TonemapACES(color);
	}
	else if (g_Post.tonemapper == 1u)
	{
		color = TonemapReinhard(color);
	}
	else if (g_Post.tonemapper == 2u)
	{
		color = saturate(color);
	}
	else if (g_Post.tonemapper == 3u)
	{
		color = TonemapAgX(color);
	}
	else if (g_Post.tonemapper == 4u)
	{
		color = TonemapNeutral(color);
	}
	else
	{
		color = TonemapACES(color);
	}

	// 3D Color Grading LUT Sampling (32x32x32)
	float3 lutUVW = color * ((32.0f - 1.0f) / 32.0f) + (0.5f / 32.0f);
	float3 lutColor = g_TextureColorLUT.SampleLevel(g_SamplerLinearClamp, lutUVW, 0.0f).rgb;
	if (any(lutColor > 0.0001f))
	{
		color = lutColor;
	}

	// Piecewise sRGB OETF
	float3 srgb = LinearToSRGBExact(color);

	// Triangular noise dither (±0.5 LSB) to eliminate banding in gradients
	float2 p = input.positionCS.xy;
	float r0 = frac(sin(dot(p, float2(12.9898f, 78.233f))) * 43758.5453f);
	float r1 = frac(sin(dot(p + 0.5f, float2(39.346f, 11.135f))) * 43758.5453f);
	float dither = (r0 + r1 - 1.0f) * (1.0f / 255.0f);
	srgb += dither;

	// Pack perceptual luma in alpha for single-pass FXAA
	float luma = dot(srgb, float3(0.299f, 0.587f, 0.114f));

	return float4(saturate(srgb), luma);
}
