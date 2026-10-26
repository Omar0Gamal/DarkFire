#ifndef DARKFIRE_SHADOWS_HLSLI
#define DARKFIRE_SHADOWS_HLSLI

#include "BRDF.hlsli"

// 16-Tap Vogel Spiral Kernel
static const float2 s_vogelDisk16[16] = {
	float2( 0.0000f,  0.2500f), float2(-0.2443f,  0.2229f),
	float2( 0.1654f, -0.3245f), float2( 0.2030f,  0.3705f),
	float2(-0.4398f, -0.1179f), float2( 0.3164f, -0.3771f),
	float2( 0.0863f,  0.5432f), float2(-0.5052f, -0.2526f),
	float2( 0.5369f, -0.2737f), float2(-0.1627f,  0.6402f),
	float2(-0.4398f, -0.5284f), float2( 0.6974f, -0.0610f),
	float2(-0.4735f,  0.5643f), float2(-0.1299f, -0.7488f),
	float2( 0.7303f,  0.2872f), float2(-0.7583f,  0.2849f)
};

// Sample single cascade slice from the 2x2 shadow atlas
float sampleCascadeSlice(float3 biasedPos, int cascadeIndex, float2 screenPos, float NdotL)
{
	float4 lightPosH;
	float2 quadOffset;
	if (cascadeIndex == 0)
	{
		lightPosH = mul(float4(biasedPos, 1.0f), g_Shadow.lightViewProj);
		quadOffset = float2(0.0f, 0.0f);
	}
	else if (cascadeIndex == 1)
	{
		lightPosH = mul(float4(biasedPos, 1.0f), g_Shadow.cascadeViewProj1);
		quadOffset = float2(0.5f, 0.0f);
	}
	else if (cascadeIndex == 2)
	{
		lightPosH = mul(float4(biasedPos, 1.0f), g_Shadow.cascadeViewProj2);
		quadOffset = float2(0.0f, 0.5f);
	}
	else
	{
		lightPosH = mul(float4(biasedPos, 1.0f), g_Shadow.cascadeViewProj3);
		quadOffset = float2(0.5f, 0.5f);
	}

	float3 shadowCoord = lightPosH.xyz / lightPosH.w;
	float2 uv = shadowCoord.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

	if (uv.x < 0.001f || uv.x > 0.999f || uv.y < 0.001f || uv.y > 0.999f || shadowCoord.z < 0.0f || shadowCoord.z > 1.0f)
	{
		return 1.0f;
	}

	float atlasScale = (g_Shadow.shadowParams.z < 0.00035f) ? 0.5f : 1.0f;
	float2 atlasUV = (uv * 0.5f + quadOffset) * atlasScale;

	// Softness filter: invSize is for full 2048x2048 atlas. Each cascade quadrant is 1024x1024.
	float invSize = g_Shadow.shadowParams.z;
	float softness = max(g_Shadow.shadowParams.w, 0.8f);
	float filterRadius = (invSize * 2.0f) * softness * (1.0f + float(cascadeIndex) * 0.35f);

	// Slope-scaled & filter-radius-aware receiver depth bias (prevents wide PCF self-shadowing on curved surfaces)
	float slopeFactor = sqrt(max(0.0f, 1.0f - NdotL * NdotL)) / max(NdotL, 0.1f);
	float kernelBias = filterRadius * 0.35f * (1.0f + min(slopeFactor, 2.5f));
	float depthBias = (g_Shadow.shadowParams.x + g_Shadow.shadowParams.x * min(slopeFactor, 3.5f)) * (1.0f + float(cascadeIndex) * 0.5f) + kernelBias;
	float compareDepth = shadowCoord.z - depthBias;

	// Interleaved gradient noise for per-pixel random rotation (eliminates checkerboard/stair-stepping banding)
	float noise = InterleavedGradientNoise(screenPos);
	float phi = noise * 6.2831853f;
	float sinP, cosP;
	sincos(phi, sinP, cosP);
	float2x2 rotMat = float2x2(cosP, -sinP, sinP, cosP);

	// Fast-path early out for unoccluded or deeply shadowed fragments
	float centerShadow = g_TextureShadowMap.SampleCmpLevelZero(g_SamplerShadow, atlasUV, compareDepth);
	float2 dExt = mul(float2(filterRadius, filterRadius), rotMat);
	float tap1 = g_TextureShadowMap.SampleCmpLevelZero(g_SamplerShadow, atlasUV + dExt, compareDepth);
	float tap2 = g_TextureShadowMap.SampleCmpLevelZero(g_SamplerShadow, atlasUV - dExt, compareDepth);
	if (centerShadow > 0.999f && tap1 > 0.999f && tap2 > 0.999f)
	{
		return 1.0f;
	}
	if (centerShadow < 0.001f && tap1 < 0.001f && tap2 < 0.001f)
	{
		return 0.0f;
	}

	// Optional PCSS (Percentage-Closer Soft Shadows) Contact-Hardening Blocker Search (g_Pass.fogParams.z > 0.5f)
	if (g_Pass.fogParams.z > 0.5f && cascadeIndex <= 2)
	{
		float searchRadius = filterRadius * 1.6f;
		float blockerSum = 0.0f;
		float blockerCount = 0.0f;
		[unroll]
		for (int b = 0; b < 8; ++b)
		{
			float2 bOffset = mul(s_vogelDisk16[b * 2], rotMat) * searchRadius;
			float shadowDepth = g_TextureShadowMap.SampleLevel(g_SamplerPointClamp, atlasUV + bOffset, 0).r;
			if (shadowDepth < compareDepth)
			{
				blockerSum += shadowDepth;
				blockerCount += 1.0f;
			}
		}

		if (blockerCount < 0.5f)
		{
			return 1.0f;
		}
		float avgBlockerDepth = blockerSum / blockerCount;
		float penumbraRatio = saturate((compareDepth - avgBlockerDepth) / max(avgBlockerDepth, 0.01f)) * 22.0f;
		filterRadius *= lerp(0.35f, 2.25f, saturate(penumbraRatio));
	}

	float shadow = 0.0f;
	[unroll]
	for (int i = 0; i < 16; ++i)
	{
		float2 rotatedOffset = mul(s_vogelDisk16[i], rotMat) * filterRadius;
		shadow += g_TextureShadowMap.SampleCmpLevelZero(g_SamplerShadow, atlasUV + rotatedOffset, compareDepth);
	}

	return shadow / 16.0f;
}

// Short-Range Screen-Space Micro Contact Shadows (8 steps, 12cm world range)
float evaluateContactShadow(float3 worldPos, float3 L, float2 screenPos)
{
	if (g_Pass.fogParams.w < 0.5f) // Contact shadows disabled
	{
		return 1.0f;
	}

	float4 startClip = mul(float4(worldPos, 1.0f), g_View.viewProjMatrix);
	float3 endWS = worldPos + L * 0.14f;
	float4 endClip = mul(float4(endWS, 1.0f), g_View.viewProjMatrix);
	if (startClip.w <= 0.05f || endClip.w <= 0.05f)
	{
		return 1.0f;
	}

	float3 startNDC = startClip.xyz / startClip.w;
	float3 endNDC = endClip.xyz / endClip.w;

	float3 startUVZ = float3(startNDC.xy * float2(0.5f, -0.5f) + 0.5f, startNDC.z);
	float3 endUVZ = float3(endNDC.xy * float2(0.5f, -0.5f) + 0.5f, endNDC.z);
	float3 stepUVZ = (endUVZ - startUVZ) / 8.0f;

	float jitter = InterleavedGradientNoise(screenPos);
	float3 rayUVZ = startUVZ + stepUVZ * (0.5f + jitter * 0.5f);

	// Reversed-Z depth thickness tolerance around ray depth
	float thicknessZ = abs(startUVZ.z - endUVZ.z) * 1.75f + 0.00015f;

	[unroll]
	for (int s = 0; s < 8; ++s)
	{
		if (any(rayUVZ.xy < 0.001f) || any(rayUVZ.xy > 0.999f))
		{
			break;
		}
		float sceneZ = g_TextureDepth.SampleLevel(g_SamplerPointClamp, rayUVZ.xy, 0).r;
		// In Reversed-Z, larger Z is closer to camera (occluder in front of ray)
		float deltaZ = sceneZ - rayUVZ.z;
		if (deltaZ > 0.00004f && deltaZ < thicknessZ)
		{
			return lerp(0.15f, 1.0f, float(s) / 8.0f);
		}
		rayUVZ += stepUVZ;
	}

	return 1.0f;
}

// 16-Tap Vogel Disk Hardware PCF Shadow Evaluation with 4-Cascade CSM & 10% Blend Band
float evaluateDirectionalShadow(float3 worldPos, float3 N, float3 L, float2 screenPos)
{
	if (g_Shadow.shadowExtra.z < 0.5f) // Shadow disabled
	{
		return 1.0f;
	}

	float3 geomN = N;
	if (g_Material.materialFlags & (DF_MAT_FLAG_TWO_SIDED | DF_MAT_FLAG_CLOTH | DF_MAT_FLAG_TRANSMISSION))
	{
		// Orient thin/two-sided geometry normal towards the incoming light direction L
		geomN = (dot(N, L) >= 0.0f) ? N : -N;
	}

	float rawNdotL = dot(geomN, L);
	if (rawNdotL <= 0.0f && !(g_Material.materialFlags & (DF_MAT_FLAG_TRANSMISSION | DF_MAT_FLAG_TWO_SIDED | DF_MAT_FLAG_CLOTH)))
	{
		return 1.0f - g_Shadow.shadowExtra.x;
	}

	float NdotL = saturate(rawNdotL);

	if (g_Shadow.shadowExtra.w > 1.5f) // Multi-cascade CSM enabled (4 cascades)
	{
		float4 viewPosH = mul(float4(worldPos, 1.0f), g_View.viewMatrix);
		float viewDepth = abs(viewPosH.z);

		int cascadeIdx = 0;
		float nextSplit = g_Shadow.cascadeSplits.x;
		float prevSplit = 0.0f;

		if (viewDepth > g_Shadow.cascadeSplits.z)
		{
			cascadeIdx = 3;
			nextSplit = g_Shadow.cascadeSplits.w;
			prevSplit = g_Shadow.cascadeSplits.z;
		}
		else if (viewDepth > g_Shadow.cascadeSplits.y)
		{
			cascadeIdx = 2;
			nextSplit = g_Shadow.cascadeSplits.z;
			prevSplit = g_Shadow.cascadeSplits.y;
		}
		else if (viewDepth > g_Shadow.cascadeSplits.x)
		{
			cascadeIdx = 1;
			nextSplit = g_Shadow.cascadeSplits.y;
			prevSplit = g_Shadow.cascadeSplits.x;
		}

		float normalBiasScale = g_Shadow.shadowParams.y * (1.0f + float(cascadeIdx) * 0.5f);
		float normalBias = normalBiasScale * sqrt(max(0.0f, 1.0f - NdotL * NdotL));
		float3 biasedPos = worldPos + geomN * normalBias;

		float shadowTerm = sampleCascadeSlice(biasedPos, cascadeIdx, screenPos, NdotL);

		// 10% cross-cascade blend band
		float blendBand = max((nextSplit - prevSplit) * 0.10f, 0.5f);
		float distToNext = nextSplit - viewDepth;
		if (distToNext < blendBand && cascadeIdx < 3)
		{
			float normalBiasNext = g_Shadow.shadowParams.y * (1.0f + float(cascadeIdx + 1) * 0.5f) * sqrt(max(0.0f, 1.0f - NdotL * NdotL));
			float3 biasedPosNext = worldPos + geomN * normalBiasNext;
			float shadowNext = sampleCascadeSlice(biasedPosNext, cascadeIdx + 1, screenPos, NdotL);
			float alpha = 1.0f - saturate(distToNext / blendBand);
			shadowTerm = lerp(shadowTerm, shadowNext, alpha);
		}

		return lerp(1.0f - g_Shadow.shadowExtra.x, 1.0f, shadowTerm);
	}
	else // Single cascade / Spot light shadow
	{
		float normalBias = g_Shadow.shadowParams.y * sqrt(max(0.0f, 1.0f - NdotL * NdotL));
		float3 biasedPos = worldPos + geomN * normalBias;

		float4 lightPosH = mul(float4(biasedPos, 1.0f), g_Shadow.lightViewProj);
		float3 shadowCoord = lightPosH.xyz / lightPosH.w;
		float2 uv = shadowCoord.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

		if (uv.x < 0.001f || uv.x > 0.999f || uv.y < 0.001f || uv.y > 0.999f || shadowCoord.z < 0.0f || shadowCoord.z > 1.0f)
		{
			return 1.0f;
		}

		float slopeFactor = sqrt(max(0.0f, 1.0f - NdotL * NdotL)) / max(NdotL, 0.1f);
		float depthBias = g_Shadow.shadowParams.x * (1.0f + min(slopeFactor, 3.5f));
		float compareDepth = shadowCoord.z - depthBias;

		float invSize = g_Shadow.shadowParams.z;
		float softness = max(g_Shadow.shadowParams.w, 0.8f);
		float filterRadius = invSize * softness * 2.0f;

		float noise = InterleavedGradientNoise(screenPos);
		float phi = noise * 6.2831853f;
		float sinP, cosP;
		sincos(phi, sinP, cosP);
		float2x2 rotMat = float2x2(cosP, -sinP, sinP, cosP);

		float shadow = 0.0f;
		[unroll]
		for (int i = 0; i < 16; ++i)
		{
			float2 rotatedOffset = mul(s_vogelDisk16[i], rotMat) * filterRadius;
			shadow += g_TextureShadowMap.SampleCmpLevelZero(g_SamplerShadow, uv + rotatedOffset, compareDepth);
		}

		float shadowTerm = shadow / 16.0f;
		return lerp(1.0f - g_Shadow.shadowExtra.x, 1.0f, shadowTerm);
	}
}

// Fast single-tap shadow map visibility query for volumetric fog & ambient sky occlusion
float sampleVolumetricShadow(float3 posWS)
{
	if (g_Shadow.shadowExtra.z < 0.5f)
	{
		return 1.0f;
	}

	float4 lightPosH;
	float2 quadOffset = float2(0.0f, 0.0f);
	bool isMultiCascade = (g_Shadow.shadowExtra.w > 1.5f);

	if (isMultiCascade)
	{
		float4 viewPosH = mul(float4(posWS, 1.0f), g_View.viewMatrix);
		float viewDepth = abs(viewPosH.z);
		if (viewDepth > g_Shadow.cascadeSplits.z)
		{
			lightPosH = mul(float4(posWS, 1.0f), g_Shadow.cascadeViewProj3);
			quadOffset = float2(0.5f, 0.5f);
		}
		else if (viewDepth > g_Shadow.cascadeSplits.y)
		{
			lightPosH = mul(float4(posWS, 1.0f), g_Shadow.cascadeViewProj2);
			quadOffset = float2(0.0f, 0.5f);
		}
		else if (viewDepth > g_Shadow.cascadeSplits.x)
		{
			lightPosH = mul(float4(posWS, 1.0f), g_Shadow.cascadeViewProj1);
			quadOffset = float2(0.5f, 0.0f);
		}
		else
		{
			lightPosH = mul(float4(posWS, 1.0f), g_Shadow.lightViewProj);
			quadOffset = float2(0.0f, 0.0f);
		}
	}
	else
	{
		lightPosH = mul(float4(posWS, 1.0f), g_Shadow.lightViewProj);
	}

	float3 shadowCoord = lightPosH.xyz / lightPosH.w;
	float2 uv = shadowCoord.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
	if (uv.x < 0.001f || uv.x > 0.999f || uv.y < 0.001f || uv.y > 0.999f || shadowCoord.z < 0.0f || shadowCoord.z > 1.0f)
	{
		return 1.0f;
	}

	float atlasScale = (g_Shadow.shadowParams.z < 0.00035f) ? 0.5f : 1.0f;
	float2 atlasUV = isMultiCascade ? ((uv * 0.5f + quadOffset) * atlasScale) : uv;
	return g_TextureShadowMap.SampleCmpLevelZero(g_SamplerShadow, atlasUV, shadowCoord.z - 0.0025f);
}

// Spot Light Shadow Evaluation in Dynamic Shadow Atlas
float evaluateSpotShadow(LightData light, float3 worldPos, float3 N, float3 L, float2 screenPos)
{
	if (light.spotAnglesPad.z < 0.5f) return 1.0f;

	float NdotL = saturate(dot(N, L));
	float normalBias = g_Shadow.shadowParams.y * sqrt(max(0.0f, 1.0f - NdotL * NdotL));
	float3 biasedPos = worldPos + N * normalBias;

	float4 lightPosH = mul(float4(biasedPos, 1.0f), light.shadowViewProj);
	float3 shadowCoord = lightPosH.xyz / max(abs(lightPosH.w), 0.00001f);
	float2 uv = shadowCoord.xy * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);

	if (any(uv < 0.001f) || any(uv > 0.999f) || shadowCoord.z < 0.0f || shadowCoord.z > 1.0f)
	{
		return 1.0f;
	}

	float2 atlasUV = uv * light.shadowAtlasRect.zw + light.shadowAtlasRect.xy;
	float invSize = g_Shadow.shadowParams.z;
	float filterRadius = invSize * 1.5f;
	float depthBias = g_Shadow.shadowParams.x * 2.0f;
	float compareDepth = shadowCoord.z - depthBias;

	float noise = InterleavedGradientNoise(screenPos);
	float phi = noise * 6.2831853f;
	float sinP, cosP;
	sincos(phi, sinP, cosP);
	float2x2 rotMat = float2x2(cosP, -sinP, sinP, cosP);

	float shadow = 0.0f;
	[unroll]
	for (int i = 0; i < 8; ++i)
	{
		float2 rotatedOffset = mul(s_vogelDisk16[i * 2], rotMat) * filterRadius;
		shadow += g_TextureShadowMap.SampleCmpLevelZero(g_SamplerShadow, atlasUV + rotatedOffset, compareDepth);
	}
	return shadow * 0.125f;
}

// Omni-directional Point Light Dual-Paraboloid Shadow Evaluation
float evaluatePointShadow(LightData light, float3 worldPos, float3 N, float3 L, float2 screenPos)
{
	if (light.spotAnglesPad.z < 0.5f) return 1.0f;

	float NdotL = saturate(dot(N, L));
	float normalBias = g_Shadow.shadowParams.y * sqrt(max(0.0f, 1.0f - NdotL * NdotL));
	float3 biasedPos = worldPos + N * normalBias;

	float4 lightPos = mul(float4(biasedPos, 1.0f), light.shadowViewProj);
	float dist = length(lightPos.xyz);
	float range = max(light.directionRange.w, 0.1f);
	if (dist > range) return 1.0f;

	float3 P = lightPos.xyz / max(dist, 0.0001f);
	bool isFront = (P.z >= 0.0f);
	float zSign = isFront ? 1.0f : -1.0f;
	P.z *= zSign;

	float2 proj = P.xy / max(P.z + 1.0f, 0.001f);
	float2 uv = proj * float2(0.5f, -0.5f) + float2(0.5f, 0.5f);
	if (any(uv < 0.001f) || any(uv > 0.999f)) return 1.0f;

	float2 tileOffset = isFront ? float2(0.0f, 0.0f) : float2(light.shadowAtlasRect.z * 0.5f, 0.0f);
	float2 atlasUV = uv * float2(light.shadowAtlasRect.z * 0.5f, light.shadowAtlasRect.w) + light.shadowAtlasRect.xy + tileOffset;

	float normDepth = (dist - 0.1f) / (range - 0.1f);
	float compareDepth = normDepth - 0.005f;

	float invSize = g_Shadow.shadowParams.z;
	float filterRadius = invSize * 1.5f;

	float noise = InterleavedGradientNoise(screenPos);
	float phi = noise * 6.2831853f;
	float sinP, cosP;
	sincos(phi, sinP, cosP);
	float2x2 rotMat = float2x2(cosP, -sinP, sinP, cosP);

	float shadow = 0.0f;
	[unroll]
	for (int i = 0; i < 8; ++i)
	{
		float2 rotatedOffset = mul(s_vogelDisk16[i * 2], rotMat) * filterRadius;
		shadow += g_TextureShadowMap.SampleCmpLevelZero(g_SamplerShadow, atlasUV + rotatedOffset, compareDepth);
	}
	return shadow * 0.125f;
}

// Horizon Micro-Shadowing (Chan 2018, Unity HDRP Classic-Sponza MicroShadowing opacity: 0.5)
float computeMicroShadow(float NdotL, float ao)
{
	float aperture = 2.0f * ao * ao;
	float microShadow = saturate(abs(NdotL) + aperture - 1.0f);
	return lerp(1.0f, microShadow, 0.5f);
}

#endif // DARKFIRE_SHADOWS_HLSLI
