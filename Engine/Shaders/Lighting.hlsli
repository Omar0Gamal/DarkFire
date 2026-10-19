#ifndef DARKFIRE_LIGHTING_HLSLI
#define DARKFIRE_LIGHTING_HLSLI

#include "Common.hlsli"

// Normal Mapping with Bitangent fix: cross(N, T) * T.w
float3 computePerturbedNormal(float3 N, float4 tangent, float2 texcoord)
{
	float3 T = normalize(tangent.xyz - dot(tangent.xyz, N) * N);
	float3 B = cross(N, T) * tangent.w;
	float3x3 TBN = float3x3(T, B, N);

	float3 normalMap = g_TextureNormal.Sample(g_SamplerLinearWrap, texcoord).xyz * 2.0f - 1.0f;
	return normalize(mul(normalMap, TBN));
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

// PBR Lighting Evaluation
float3 evaluateDirectLighting(
	float3 N,
	float3 V,
	float3 worldPos,
	float3 albedo,
	float roughness,
	float metallic,
	float3 F0)
{
	float3 Lo = float3(0.0f, 0.0f, 0.0f);

	for (uint i = 0; i < g_Pass.lightCount; ++i)
	{
		LightData light = g_Lights[i];
		int type = (int)light.positionType.w;

		float3 L = float3(0.0f, 1.0f, 0.0f);
		float attenuation = 1.0f;

		if (type == 0) // Directional
		{
			L = normalize(-light.directionRange.xyz);
		}
		else if (type == 1) // Point
		{
			float3 toLight = light.positionType.xyz - worldPos;
			float dist = length(toLight);
			float range = light.directionRange.w;
			if (dist > range) continue;

			L = normalize(toLight);
			float attenDist = clamp(1.0f - dist / range, 0.0f, 1.0f);
			attenuation = attenDist * attenDist / max(dist * dist, 0.01f);
		}
		else if (type == 2) // Spot
		{
			float3 toLight = light.positionType.xyz - worldPos;
			float dist = length(toLight);
			float range = light.directionRange.w;
			if (dist > range) continue;

			L = normalize(toLight);
			float cosAngle = dot(-L, normalize(light.directionRange.xyz));
			float innerCos = light.spotAnglesPad.x;
			float outerCos = light.spotAnglesPad.y;
			float spotFactor = clamp((cosAngle - outerCos) / max(innerCos - outerCos, 0.0001f), 0.0f, 1.0f);

			float attenDist = clamp(1.0f - dist / range, 0.0f, 1.0f);
			attenuation = spotFactor * attenDist * attenDist / max(dist * dist, 0.01f);
		}

		float3 H = normalize(V + L);
		float3 radiance = light.colorIntensity.rgb * light.colorIntensity.a * attenuation;

		float NDF = DistributionGGX(N, H, roughness);
		float G = GeometrySmith(N, V, L, roughness);
		float3 F = fresnelSchlick(max(dot(H, V), 0.0f), F0);

		float3 numerator = NDF * G * F;
		float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f;
		float3 specular = numerator / denominator;

		float3 kS = F;
		float3 kD = float3(1.0f, 1.0f, 1.0f) - kS;
		kD *= 1.0f - metallic;

		float NdotL = max(dot(N, L), 0.0f);
		Lo += (kD * albedo * INV_PI + specular) * radiance * NdotL;
	}

	// Ambient lighting computed ONCE outside the per-light loop!
	float3 ambient = g_Pass.ambientLightColor.rgb * g_Pass.ambientLightColor.a * albedo;
	return ambient + Lo;
}

#endif // DARKFIRE_LIGHTING_HLSLI
