#ifndef DARKFIRE_NORMALMAPPING_HLSLI
#define DARKFIRE_NORMALMAPPING_HLSLI

#include "Common.hlsli"

// Normal Mapping with Bitangent fix, strength scaling, and OpenGL/DirectX flip
float3 computePerturbedNormal(float3 N, float4 tangent, float2 texcoord)
{
	if (!(g_Material.materialFlags & DF_MAT_FLAG_HAS_NORMAL_MAP))
	{
		return N;
	}

	float3 T = normalize(tangent.xyz - dot(tangent.xyz, N) * N);
	float3 B = cross(N, T) * tangent.w;
	float3x3 TBN = float3x3(T, B, N);

	float4 normalSample = g_TextureNormal.Sample(g_SamplerAnisoWrap, texcoord);
	float2 normalXY = normalSample.xy * 2.0f - 1.0f;
	// Support both BC5 (2-channel with reconstructed Z) and standard 3-channel normal maps
	float normalZ = (normalSample.z > 0.01f) ? (normalSample.z * 2.0f - 1.0f) : sqrt(saturate(1.0f - dot(normalXY, normalXY)));
	float3 normalMap = float3(normalXY, normalZ);
	if (g_Material.materialFlags & DF_MAT_FLAG_FLIP_NORMAL_Y)
	{
		normalMap.y = -normalMap.y;
	}
	normalMap.xy *= g_Material.normalScale;
	return normalize(mul(normalMap, TBN));
}

#endif // DARKFIRE_NORMALMAPPING_HLSLI
