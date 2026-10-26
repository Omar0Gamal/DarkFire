#ifndef DARKFIRE_BRDF_HLSLI
#define DARKFIRE_BRDF_HLSLI

#include "Common.hlsli"

// Interleaved Gradient Noise for jittering sample kernels
float InterleavedGradientNoise(float2 screenPos)
{
	float3 magic = float3(0.06711056f, 0.00583715f, 52.9829189f);
	return frac(magic.z * frac(dot(screenPos, magic.xy)));
}

// GGX / Trowbridge-Reitz NDF
float DistributionGGX(float3 N, float3 H, float roughness)
{
	float a = roughness * roughness;
	float a2 = a * a;
	float NdotH = max(dot(N, H), 0.0f);
	float NdotH2 = NdotH * NdotH;

	float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
	denom = PI * denom * denom;

	return a2 / max(denom, 0.0000001f);
}

// Anisotropic Trowbridge-Reitz / GGX NDF (Burley 2012 / Heitz 2014)
float DistributionAnisotropicGGX(float3 N, float3 H, float3 T, float3 B, float at, float ab)
{
	float TdotH = dot(T, H);
	float BdotH = dot(B, H);
	float NdotH = dot(N, H);

	float d = (TdotH * TdotH) / max(at * at, 0.00001f) + (BdotH * BdotH) / max(ab * ab, 0.00001f) + NdotH * NdotH;
	return 1.0f / max(PI * at * ab * d * d, 0.0000001f);
}

float GeometrySmithAnisotropic(float3 N, float3 V, float3 L, float3 T, float3 B, float at, float ab)
{
	float NdotV = max(dot(N, V), 0.0001f);
	float NdotL = max(dot(N, L), 0.0001f);
	float TdotV = dot(T, V);
	float BdotV = dot(B, V);
	float TdotL = dot(T, L);
	float BdotL = dot(B, L);

	float lambdaV = NdotL * length(float3(at * TdotV, ab * BdotV, NdotV));
	float lambdaL = NdotV * length(float3(at * TdotL, ab * BdotL, NdotL));
	return 0.5f / max(lambdaV + lambdaL, 0.0000001f);
}

// Schlick-GGX Geometry
float GeometrySchlickGGX(float NdotV, float roughness)
{
	float r = (roughness + 1.0f);
	float k = (r * r) / 8.0f;

	float denom = NdotV * (1.0f - k) + k;
	return NdotV / max(denom, 0.0000001f);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
	float NdotV = max(dot(N, V), 0.0f);
	float NdotL = max(dot(N, L), 0.0f);
	float ggx2 = GeometrySchlickGGX(NdotV, roughness);
	float ggx1 = GeometrySchlickGGX(NdotL, roughness);

	return ggx2 * ggx1;
}

// Fresnel-Schlick
float3 fresnelSchlick(float cosTheta, float3 F0)
{
	return F0 + (1.0f - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

float3 fresnelSchlickRoughness(float cosTheta, float3 F0, float roughness)
{
	return F0 + (max(float3(1.0f - roughness, 1.0f - roughness, 1.0f - roughness), F0) - F0) * pow(clamp(1.0f - cosTheta, 0.0f, 1.0f), 5.0f);
}

// Split-sum BRDF approximation (Karis 2013 / Unreal) with LUT sampling
float2 envBRDFApprox(float roughness, float NoV)
{
	float2 lut = g_TextureBRDFLUT.SampleLevel(g_SamplerLinearClamp, float2(NoV, roughness), 0).rg;
	if (any(lut > 0.0001f))
	{
		return lut;
	}
	const float4 c0 = float4(-1.0f, -0.0275f, -0.572f, 0.022f);
	const float4 c1 = float4( 1.0f,  0.0425f,  1.040f, -0.040f);
	float4 r = roughness * c0 + c1;
	float a004 = min(r.x * r.x, exp2(-9.28f * NoV)) * r.x + r.y;
	return float2(-1.04f, 1.04f) * a004 + r.zw;
}

// Lagarde & de Rousiers 2014 specular occlusion from ambient occlusion & roughness
float computeSpecularOcclusion(float NoV, float ao, float roughness)
{
	return saturate(pow(max(NoV + ao, 0.0f), exp2(-16.0f * roughness - 1.0f)) - 1.0f + ao);
}

// Jimenez 2016 multi-bounce ambient occlusion approximation
float3 multiBounceAO(float visibility, float3 albedo)
{
	float3 a = 2.0404f * albedo - 0.3324f;
	float3 b = -4.7951f * albedo + 0.6417f;
	float3 c = 2.7552f * albedo + 0.6903f;
	float x = saturate(visibility);
	return clamp(((x * a + b) * x + c) * x, float3(x, x, x), float3(1.0f, 1.0f, 1.0f));
}

// Horizon specular occlusion (prevents reflection rays below tangent plane from reflecting ambient light)
float computeHorizonOcclusion(float3 R, float3 N)
{
	float factor = saturate(1.0f + 2.0f * dot(R, N));
	return factor * factor;
}

#endif // DARKFIRE_BRDF_HLSLI
