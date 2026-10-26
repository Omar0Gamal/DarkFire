#ifndef DARKFIRE_SURFACE_HLSLI
#define DARKFIRE_SURFACE_HLSLI

#include "NormalMapping.hlsli"

struct SurfaceData
{
	float4 albedo;
	float3 normalWS;
	float  roughness;
	float  metallic;
	float  matOcclusion;
	float  matAO;
	float3 emissive;
	float3 F0;
};

SurfaceData evaluateSurface(PSInput input, bool isFrontFace)
{
	SurfaceData s;

	float2 uv = input.texcoord;
	float4 albedoTex = g_TextureAlbedo.Sample(g_SamplerAnisoWrap, uv);
	s.albedo = albedoTex * g_Material.baseColorFactor * input.color;

	// Alpha test with derivative sharpening to eliminate distant leaf erosion
	if (g_Material.alphaCutoff > 0.001f)
	{
		clip((s.albedo.a - g_Material.alphaCutoff) / max(fwidth(s.albedo.a), 0.0001f));
	}

	float3 N = normalize(input.normalWS);
	float4 tangentWS = input.tangentWS;
	if (!isFrontFace && (g_Material.materialFlags & DF_MAT_FLAG_TWO_SIDED))
	{
		N = -N;
		tangentWS.w = -tangentWS.w;
	}

	if (length(tangentWS.xyz) > 0.1f)
	{
		N = computePerturbedNormal(N, tangentWS, uv);
	}

	// Micro-detail normal mapping with RNM (Reoriented Normal Mapping)
	if (g_Material.materialFlags & DF_MAT_FLAG_HAS_DETAIL_MAP)
	{
		float2 detailUV = uv * g_Material.detailTilingScale.xy;
		float4 detailSamp = g_TextureDetail.Sample(g_SamplerAnisoWrap, detailUV);
		float2 detailXY = detailSamp.xy * 2.0f - 1.0f;
		float detailZ = (detailSamp.z > 0.01f) ? (detailSamp.z * 2.0f - 1.0f) : sqrt(saturate(1.0f - dot(detailXY, detailXY)));
		float3 detailNormalMap = float3(detailXY, detailZ);
		detailNormalMap.xy *= g_Material.detailTilingScale.z;
		if (length(tangentWS.xyz) > 0.1f)
		{
			float3 T = normalize(tangentWS.xyz - dot(tangentWS.xyz, input.normalWS) * input.normalWS);
			float3 B = cross(input.normalWS, T) * tangentWS.w;
			float3x3 TBN = float3x3(T, B, input.normalWS);
			float3 detailWS = mul(float3(detailNormalMap.xy, 0.0f), TBN);
			N = normalize(N + detailWS);
		}
	}
	s.normalWS = N;

	float4 mrSample = g_TextureMetallicRoughness.Sample(g_SamplerAnisoWrap, uv);
	float metallic = 0.0f;
	float roughness = 0.5f;
	float matOcclusion = 1.0f;

	if (g_Material.materialFlags & DF_MAT_FLAG_MASK_MAP_PACKED)
	{
		metallic = clamp(mrSample.r * g_Material.metallicFactor, 0.0f, 1.0f);
		matOcclusion = mrSample.g;
		float smoothness = mrSample.a;
		roughness = clamp((1.0f - smoothness) * g_Material.roughnessFactor, 0.04f, 1.0f);
	}
	else if (g_Material.materialFlags & DF_MAT_FLAG_ORM_PACKED)
	{
		matOcclusion = mrSample.r;
		roughness = clamp(mrSample.g * g_Material.roughnessFactor, 0.04f, 1.0f);
		metallic = clamp(mrSample.b * g_Material.metallicFactor, 0.0f, 1.0f);
	}
	else
	{
		roughness = clamp(mrSample.g * g_Material.roughnessFactor, 0.04f, 1.0f);
		metallic = clamp(mrSample.b * g_Material.metallicFactor, 0.0f, 1.0f);
		if (g_Material.materialFlags & DF_MAT_FLAG_HAS_OCCLUSION_MAP)
		{
			matOcclusion = g_TextureOcclusion.Sample(g_SamplerAnisoWrap, uv).r;
		}
	}

	// Geometric & Normal Specular Anti-Aliasing (Kaplanyan variance 0.25, clamp 0.18)
	float3 dNdx = ddx(N);
	float3 dNdy = ddy(N);
	float normalVariance = 0.25f * max(dot(dNdx, dNdx), dot(dNdy, dNdy));
	float kernelRoughness = min(2.0f * normalVariance, 0.18f);
	s.roughness = sqrt(saturate(roughness * roughness + kernelRoughness));
	s.metallic = metallic;
	s.matOcclusion = matOcclusion;
	s.matAO = lerp(1.0f, matOcclusion, g_Material.occlusionStrength);

	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	s.F0 = lerp(F0, s.albedo.rgb, s.metallic);

	float4 emissiveTex = (g_Material.materialFlags & DF_MAT_FLAG_HAS_EMISSIVE_MAP)
		? g_TextureEmissive.Sample(g_SamplerAnisoWrap, uv)
		: float4(1.0f, 1.0f, 1.0f, 1.0f);
	s.emissive = clamp(emissiveTex.rgb * g_Material.emissiveFactor.rgb * g_Material.emissiveFactor.a, 0.0f, 50.0f);

	return s;
}

#endif // DARKFIRE_SURFACE_HLSLI
