#ifndef DARKFIRE_LIGHTING_HLSLI
#define DARKFIRE_LIGHTING_HLSLI

#include "Common.hlsli"
#include "BRDF.hlsli"
#include "Shadows.hlsli"
#include "NormalMapping.hlsli"

// Octahedral coordinate encoding for DDGI probe lookup
float2 octEncode(float3 n)
{
	n /= (abs(n.x) + abs(n.y) + abs(n.z));
	float2 oct = (n.z >= 0.0f) ? n.xy : ((1.0f - abs(n.yx)) * signNotZero(n.xy));
	return oct * 0.5f + 0.5f;
}

// Sample DDGI Irradiance Atlas with Chebyshev Depth Visibility Testing
float3 sampleDDGI(float3 worldPos, float3 N)
{
	if (g_DDGI.ddgiParams.w < 0.5f)
	{
		return float3(0.0f, 0.0f, 0.0f);
	}

	float3 gridOrigin = g_DDGI.gridOriginSpacing.xyz;
	float probeSpacing = max(g_DDGI.gridOriginSpacing.w, 0.001f);
	float3 probeCounts = max(g_DDGI.gridCountsExtents.xyz, float3(1.0f, 1.0f, 1.0f));
	float normalBias = max(g_DDGI.gridCountsExtents.w, 0.05f);

	float3 biasedPos = worldPos + N * normalBias;
	float3 gridPos = (biasedPos - gridOrigin) / probeSpacing + (probeCounts * 0.5f - 0.5f);

	// Smooth boundary falloff at probe volume edges (eliminates hard object clipping seams)
	float3 distFromMin = gridPos;
	float3 distFromMax = (probeCounts - 1.0f) - gridPos;
	if (any(distFromMin < 0.0f) || any(distFromMax < 0.0f))
	{
		return float3(0.0f, 0.0f, 0.0f);
	}
	float3 minDist = min(distFromMin, distFromMax);
	float edgeFade = smoothstep(0.0f, 1.0f, saturate(min(minDist.x, min(minDist.y, minDist.z)) * 2.0f));

	int3 baseProbe = clamp(int3(floor(gridPos)), int3(0, 0, 0), int3(probeCounts) - int3(2, 2, 2));
	float3 alpha = saturate(gridPos - float3(baseProbe));

	float3 totalRadiance = float3(0.0f, 0.0f, 0.0f);
	float totalWeight = 0.0f;
	float2 octUV = octEncode(N);

	// Hoist atlas dimensions out of the probe iteration loop
	float depthWidth = 512.0f, depthHeight = 256.0f;
	g_TextureDDGIDepth.GetDimensions(depthWidth, depthHeight);
	float2 invDepthAtlasSize = 1.0f / float2(depthWidth, depthHeight);

	float radWidth = 512.0f, radHeight = 256.0f;
	g_TextureDDGIRadiance.GetDimensions(radWidth, radHeight);
	float2 invRadAtlasSize = 1.0f / float2(radWidth, radHeight);

	[unroll]
	for (int z = 0; z <= 1; ++z)
	{
		[unroll]
		for (int y = 0; y <= 1; ++y)
		{
			[unroll]
			for (int x = 0; x <= 1; ++x)
			{
				int3 cornerProbe = baseProbe + int3(x, y, z);
				uint probeIndex = cornerProbe.x + cornerProbe.y * (uint)probeCounts.x + cornerProbe.z * ((uint)probeCounts.x * (uint)probeCounts.y);

				float3 probeWorldPos = gridOrigin + (float3(cornerProbe) - probeCounts * 0.5f + 0.5f) * probeSpacing;
				float3 pointToProbe = probeWorldPos - biasedPos;
				float dist = length(pointToProbe);
				float3 dirToProbe = (dist > 0.001f) ? (pointToProbe / dist) : N;

				float trilinearWeight = ((x == 1) ? alpha.x : (1.0f - alpha.x)) *
				                        ((y == 1) ? alpha.y : (1.0f - alpha.y)) *
				                        ((z == 1) ? alpha.z : (1.0f - alpha.z));

				// Normal / front-facing weight towards probe (McGuire weight crush)
				float normalWeight = saturate(dot(dirToProbe, N) * 0.5f + 0.5f);
				if (normalWeight < 0.2f)
				{
					normalWeight *= (normalWeight * normalWeight) / 0.04f;
				}
				else
				{
					normalWeight = normalWeight * normalWeight + 0.01f;
				}

				// Chebyshev depth visibility test: probe direction towards point
				uint depthProbesPerRow = 16;
				uint depthTileX = probeIndex % depthProbesPerRow;
				uint depthTileY = probeIndex / depthProbesPerRow;
				float2 octDirUV = octEncode(-dirToProbe);
				float2 depthAtlasUV = (float2(depthTileX * 16, depthTileY * 16) + octDirUV * 15.0f + 0.5f) * invDepthAtlasSize;
				float2 depthMoments = g_TextureDDGIDepth.SampleLevel(g_SamplerLinearClamp, depthAtlasUV, 0).rg;

				float mean = depthMoments.x;
				float variance = max(depthMoments.y - mean * mean, 0.05f * mean * mean);
				float d = dist - mean;
				float chebyshevWeight = (d <= 0.0f) ? 1.0f : (variance / (variance + d * d));
				chebyshevWeight = max(chebyshevWeight * chebyshevWeight, 0.0f);

				float weight = trilinearWeight * normalWeight * chebyshevWeight;

				// Sample radiance atlas (8x8 texels per probe) using surface normal N
				uint radProbesPerRow = 32;
				uint radTileX = probeIndex % radProbesPerRow;
				uint radTileY = probeIndex / radProbesPerRow;
				float2 radAtlasUV = (float2(radTileX * 8, radTileY * 8) + octUV * 7.0f + 0.5f) * invRadAtlasSize;
				float3 probeRadiance = g_TextureDDGIRadiance.SampleLevel(g_SamplerLinearClamp, radAtlasUV, 0).rgb;

				totalRadiance += probeRadiance * weight;
				totalWeight += weight;
			}
		}
	}

	if (totalWeight < 0.0001f)
	{
		return float3(0.0f, 0.0f, 0.0f);
	}

	return (totalRadiance / totalWeight) * edgeFade;
}

// Box-Projected Parallax-Corrected Reflection Helper
float3 boxProjectDirection(float3 worldPos, float3 R, float3 boxMin, float3 boxMax, float3 probePos)
{
	float3 invR = 1.0f / ( signNotZero(R) * max(abs(R), 1e-5f) );
	float3 tMin = (boxMin - worldPos) * invR;
	float3 tMax = (boxMax - worldPos) * invR;
	float3 tFar = max(tMin, tMax);
	float t = min(tFar.x, min(tFar.y, tFar.z));
	float3 hitPos = worldPos + R * max(t, 0.0f);
	return safeNormalize(hitPos - probePos);
}

float3 boxProjectDirection(float3 dir, float3 worldPos, float3 boxCenter, float3 boxHalfExtents)
{
	float3 boxMin = boxCenter - boxHalfExtents;
	float3 boxMax = boxCenter + boxHalfExtents;
	return boxProjectDirection(worldPos, dir, boxMin, boxMax, boxCenter);
}

float3 boxProjectDirectionWithFalloff(float3 worldPos, float3 R, float3 boxCenter, float3 boxHalfExtents, out float blendWeight)
{
	float3 boxMin = boxCenter - boxHalfExtents;
	float3 boxMax = boxCenter + boxHalfExtents;
	float3 nrdir = safeNormalize(R);
	float3 invR = 1.0f / (signNotZero(nrdir) * max(abs(nrdir), 1e-5f));
	float3 tMin = (boxMin - worldPos) * invR;
	float3 tMax = (boxMax - worldPos) * invR;
	float3 tFar = max(tMin, tMax);
	float t = min(tFar.x, min(tFar.y, tFar.z));
	float3 hitPos = worldPos + nrdir * max(t, 0.0f);

	float3 distToEdge = abs(worldPos - boxCenter) / max(boxHalfExtents, 0.001f);
	float maxDist = max(distToEdge.x, max(distToEdge.y, distToEdge.z));
	blendWeight = (maxDist <= 1.0f) ? 1.0f : saturate(1.0f - (maxDist - 1.0f) / 0.25f);

	return safeNormalize(hitPos - boxCenter);
}

// PBR Lighting Evaluation with Shadows & Pre-Filtered Environment IBL
// Clustered 16x16x24 Grid Coordinate Computation
uint3 getClusterIndex(float2 screenUV, float viewDepth)
{
	uint x = clamp(uint(screenUV.x * 16.0f), 0u, 15u);
	uint y = clamp(uint(screenUV.y * 16.0f), 0u, 15u);
	float logRatio = log(max(viewDepth, g_View.nearZ) / max(g_View.nearZ, 0.001f)) / max(log(g_View.farZ / max(g_View.nearZ, 0.001f)), 0.0001f);
	uint z = clamp(uint(logRatio * 24.0f), 0u, 23u);
	return uint3(x, y, z);
}

float3 evaluateDirectLighting(
	float3 N,
	float3 V,
	float3 worldPos,
	float2 screenPos,
	float3 albedo,
	float roughness,
	float metallic,
	float3 F0,
	float ao = 1.0f,
	uint matFlagsOverride = 0xFFFFFFFFu,
	float sheenOverride = -1.0f,
	float3 tangentWS = float3(0.0f, 0.0f, 0.0f))
{
	uint matFlags = (matFlagsOverride != 0xFFFFFFFFu) ? matFlagsOverride : g_Material.materialFlags;
	float sheenVal = (sheenOverride >= 0.0f) ? sheenOverride : g_Material.sheenFactor;

	float3 Lo = float3(0.0f, 0.0f, 0.0f);
	float directOcclusion = lerp(1.0f, ao, g_Shadow.shadowExtra.y);

	// Clustered light query
	float2 screenUV = screenPos * g_View.invViewportSize;
	float viewDepth = mul(float4(worldPos, 1.0f), g_View.viewMatrix).z;
	uint3 clusterCoord = getClusterIndex(screenUV, viewDepth);
	uint clusterIdx = clusterCoord.x + clusterCoord.y * 16 + clusterCoord.z * (16 * 16);
	uint2 clusterData = g_ClusterGrid[clusterIdx];
	uint clusterOffset = clusterData.x;
	uint activeLightCount = clusterData.y;
	uint totalIterations = min(activeLightCount, 64u);

	for (uint iter = 0; iter < totalIterations; ++iter)
	{
		uint i = g_ClusterLightIndices[clusterOffset + iter];
		LightData light = g_Lights[i];
		int type = (int)light.positionType.w;

		float3 L = float3(0.0f, 1.0f, 0.0f);
		float attenuation = 1.0f;
		float shadowFactor = 1.0f;

		if (type == 0) // Directional
		{
			L = normalize(-light.directionRange.xyz);
			float NdotL_raw = dot(N, L);
			bool needsShadow = (NdotL_raw > -0.05f) || (matFlags & (DF_MAT_FLAG_TRANSMISSION | DF_MAT_FLAG_TWO_SIDED | DF_MAT_FLAG_CLOTH));
			if (needsShadow)
			{
				shadowFactor = evaluateDirectionalShadow(worldPos, N, L, screenPos);
				if (shadowFactor > 0.05f && NdotL_raw > 0.0f)
				{
					shadowFactor *= evaluateContactShadow(worldPos, L, screenPos);
				}
			}
			else
			{
				shadowFactor = 0.0f;
			}
		}
		else if (type == 1) // Point Light (meter-scale physical attenuation)
		{
			float3 toLight = light.positionType.xyz - worldPos;
			float dist = length(toLight);
			float range = light.directionRange.w;
			if (dist > range) continue;

			L = safeNormalize(toLight);
			float distOverRange = dist / max(range, 0.001f);
			float falloff = saturate(1.0f - distOverRange * distOverRange * distOverRange * distOverRange);
			attenuation = (falloff * falloff) / (dist * dist + 0.01f);
		}
		else if (type == 2) // Spot Light (meter-scale attenuation + smoothstep^2 cone)
		{
			float3 toLight = light.positionType.xyz - worldPos;
			float dist = length(toLight);
			float range = light.directionRange.w;
			if (dist > range) continue;

			L = safeNormalize(toLight);
			float cosAngle = dot(-L, safeNormalize(light.directionRange.xyz));
			float innerCos = light.spotAnglesPad.x;
			float outerCos = light.spotAnglesPad.y;
			float spotFactor = saturate((cosAngle - outerCos) / max(innerCos - outerCos, 0.0001f));
			float spotAttenuation = spotFactor * spotFactor;

			float distOverRange = dist / max(range, 0.001f);
			float falloff = saturate(1.0f - distOverRange * distOverRange * distOverRange * distOverRange);
			attenuation = spotAttenuation * (falloff * falloff) / (dist * dist + 0.01f);
		}

		float3 H = safeNormalize(V + L);
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
		float microShadow = computeMicroShadow(dot(N, L), ao);
		float3 directLightRadiance = radiance * (shadowFactor * directOcclusion * microShadow);

		if (matFlags & DF_MAT_FLAG_CLOTH)
		{
			// Charlie Sheen NDF (Estevez & Kulla 2017) + Neubelt Visibility
			float wrapNdotL = saturate((dot(N, L) + 0.35f) / 1.35f);
			float NdotH = saturate(dot(N, H));
			float sinTheta = max(0.001f, sqrt(max(1.0f - NdotH * NdotH, 0.0001f)));
			float alphaCloth = max(roughness * roughness, 0.002f);
			float invAlpha = 1.0f / alphaCloth;
			float sheenNDF = (2.0f + invAlpha) * pow(sinTheta, invAlpha) * (INV_PI * 0.5f);

			float NdotV = max(dot(N, V), 0.001f);
			float NdotL_raw = max(dot(N, L), 0.001f);
			float sheenVis = 1.0f / (4.0f * (NdotL_raw + NdotV - NdotL_raw * NdotV));

			// Tinted cloth sheen (matches fabric weave color without white plastic rim)
			float3 clothSheen = albedo * sheenVal * (sheenNDF * sheenVis);
			float3 clothDiffuse = albedo * INV_PI * wrapNdotL * (1.0f - metallic);

			float backNdotL = max(dot(-N, L), 0.0f);
			clothDiffuse += albedo * INV_PI * (backNdotL * 0.35f) * (1.0f - metallic);

			float3 directSpecular = specular * NdotL_raw * metallic;
			Lo += (clothDiffuse + clothSheen * NdotL_raw + directSpecular) * directLightRadiance;
		}
		else
		{
			float3 directDiffuse = kD * albedo * INV_PI * NdotL;

			if (matFlags & DF_MAT_FLAG_TRANSMISSION)
			{
				float backNdotL = saturate(dot(-N, L));
				float3 transColor = albedo * g_Material.transmissionColor.rgb;
				directDiffuse += transColor * INV_PI * backNdotL * g_Material.transmissionFactor;
			}
			else if (matFlags & DF_MAT_FLAG_TWO_SIDED)
			{
				float backNdotL = max(dot(-N, L), 0.0f);
				directDiffuse += kD * albedo * INV_PI * (backNdotL * 0.5f);
			}

			// Clear-coat evaluation and energy conservation
			float3 ccF = float3(0.0f, 0.0f, 0.0f);
			float3 ccSpec = float3(0.0f, 0.0f, 0.0f);
			if (matFlags & DF_MAT_FLAG_CLEAR_COAT)
			{
				float ccFactor = saturate(g_Material.extraMaterialParams.x > 0.0001f ? g_Material.extraMaterialParams.x : g_Material.transmissionColor.w);
				float ccRoughness = max(g_Material.extraMaterialParams.y, 0.02f);
				float ccNDF = DistributionGGX(N, H, ccRoughness);
				float ccG = GeometrySmith(N, V, L, ccRoughness);
				ccF = fresnelSchlick(max(dot(H, V), 0.0f), float3(0.04f, 0.04f, 0.04f)) * ccFactor;
				ccSpec = (ccNDF * ccG * ccF) / denominator;
			}

			// Anisotropic specular lobe (brushed metals & carbon fiber)
			if (matFlags & DF_MAT_FLAG_ANISOTROPY)
			{
				float aniso = clamp(g_Material.extraMaterialParams.z, -0.99f, 0.99f);
				float at = max(roughness * roughness * (1.0f + aniso), 0.001f);
				float ab = max(roughness * roughness * (1.0f - aniso), 0.001f);
				float3 T, B;
				if (any(abs(tangentWS) > 0.001f))
				{
					T = normalize(tangentWS - dot(tangentWS, N) * N);
					B = cross(N, T);
				}
				else
				{
					float3 upVec = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
					T = normalize(cross(upVec, N));
					B = cross(N, T);
				}
				if (abs(g_Material.extraMaterialParams.w) > 0.001f)
				{
					float cosRot = cos(g_Material.extraMaterialParams.w);
					float sinRot = sin(g_Material.extraMaterialParams.w);
					float3 rotT = T * cosRot + B * sinRot;
					float3 rotB = -T * sinRot + B * cosRot;
					T = rotT;
					B = rotB;
				}
				float anisoNDF = DistributionAnisotropicGGX(N, H, T, B, at, ab);
				float anisoG = GeometrySmithAnisotropic(N, V, L, T, B, at, ab);
				specular = (anisoNDF * anisoG * F);
			}

			// Energy conservation: clearcoat attenuates base layer by (1.0 - ccF)
			float3 directBase = (directDiffuse + specular * NdotL) * (1.0f - ccF);
			Lo += (directBase + ccSpec * NdotL) * directLightRadiance;
		}
	}

	// 1. Pre-Convolved Environment & Hemispherical Diffuse IBL (Zero Screen-Space Noise)
	float3 kD_env = (1.0f - F0) * (1.0f - metallic);
	float3 diffuseAO = multiBounceAO(ao, albedo);
	// In shadow areas, occlude outdoor sky ambient while maintaining natural diffuse fill
	float primarySunVis = sampleVolumetricShadow(worldPos + N * 0.15f);
	float skyShadowOcclusion = lerp(0.35f, 1.0f, primarySunVis);

	// Bent-Normal Diffuse IBL: bias normal towards open courtyard sky in crevices and corners
	float3 bentN = N;
	if (ao < 0.99f)
	{
		float pullWeight = max(N.y, 0.0f) * (1.0f - ao) * 0.35f;
		bentN = normalize(lerp(N, float3(0.0f, 1.0f, 0.0f), pullWeight));
	}

	float skyWeight = saturate(bentN.y * 0.5f + 0.5f);
	float3 skyHemi = lerp(g_Pass.skyGroundColor.rgb, lerp(g_Pass.skyHorizonColor.rgb, g_Pass.skyZenithColor.rgb, saturate(bentN.y)), skyWeight);

	// Box-project normal when sampling diffuse probe / lowest prefiltered mip
	float3 N_probe = bentN;
	float diffProbeBlend = 1.0f;
	bool hasProbe = (g_Pass.shGridMax.w > 0.5f);
	if (hasProbe)
	{
		float3 boxCenter = g_Pass.shGridMax.xyz;
		float3 boxHalfExtents = max(g_Pass.probeBoxExtents.xyz, float3(0.5f, 0.5f, 0.5f));
		N_probe = normalize(lerp(bentN, boxProjectDirectionWithFalloff(worldPos, bentN, boxCenter, boxHalfExtents, diffProbeBlend), 0.65f));
	}

	// Sample diffuse irradiance: prioritize local probe (Slot 18), fallback to specular probe mip 5 (Slot 17), then sky (Slot 12)
	float2 diffEqUV = float2(atan2(N_probe.z, N_probe.x) * (0.5f * INV_PI) + 0.5f, clamp(0.5f - asin(clamp(N_probe.y, -1.0f, 1.0f)) * INV_PI, 0.001f, 0.999f));
	float3 skyDiffuseSample = (g_Pass.ambientLightColor.a > 0.0001f)
		? g_TextureSkyEquirect.SampleLevel(g_SamplerLinearWrap, diffEqUV, 5.0f).rgb
		: skyHemi;
	float3 skyDiffuse = skyDiffuseSample * g_Pass.ambientLightColor.a * albedo * kD_env * diffuseAO * skyShadowOcclusion;
	if (matFlags & DF_MAT_FLAG_CLOTH)
	{
		skyDiffuse = skyDiffuseSample * g_Pass.ambientLightColor.a * albedo * diffuseAO * skyShadowOcclusion;
	}

	float3 diffuseIBL = skyDiffuse;
	if (hasProbe)
	{
		float3 probeDiffuseSample = g_TextureIrradianceIBL.SampleLevel(g_SamplerLinearWrap, diffEqUV, 0.0f).rgb;
		float probeIntensity = max(g_Pass.probeBoxExtents.w, 0.0f);
		float3 probeDiffuse = probeDiffuseSample * (probeIntensity * 0.20f) * albedo * kD_env * diffuseAO;
		if (matFlags & DF_MAT_FLAG_CLOTH)
		{
			probeDiffuse = probeDiffuseSample * (probeIntensity * 0.20f) * albedo * diffuseAO;
		}
		diffuseIBL = lerp(skyDiffuse, probeDiffuse, diffProbeBlend);
	}

	// Dynamic DDGI Irradiance Probe Volume Integration
	if (g_DDGI.ddgiParams.w > 0.5f)
	{
		float3 ddgiIrradiance = sampleDDGI(worldPos, bentN);
		if (any(ddgiIrradiance > 0.0001f))
		{
			float3 ddgiDiffuse = ddgiIrradiance * albedo * kD_env * diffuseAO;
			// If a high-fidelity baked irradiance probe is active, preserve its authentic occlusion and blend DDGI softly as secondary bounce
			float ddgiWeight = hasProbe ? saturate(g_DDGI.ddgiParams.x * 0.20f) : saturate(g_DDGI.ddgiParams.x);
			diffuseIBL = lerp(diffuseIBL, ddgiDiffuse, ddgiWeight);
		}
	}

	// 2. Pre-Filtered Specular IBL (Split-Sum with 6-level roughness mip chain)
	float3 R = reflect(-V, N);
	float3 R_lookup = R;
	float specProbeBlend = 1.0f;
	if (hasProbe)
	{
		float3 boxCenter = g_Pass.shGridMax.xyz;
		float3 boxHalfExtents = max(g_Pass.probeBoxExtents.xyz, float3(0.5f, 0.5f, 0.5f));
		float3 R_box = boxProjectDirectionWithFalloff(worldPos, R, boxCenter, boxHalfExtents, specProbeBlend);
		R_lookup = normalize(lerp(R, R_box, specProbeBlend));
	}

	float rWeight = saturate(R_lookup.y * 0.5f + 0.5f);
	float3 skyHemiSpec = lerp(g_Pass.skyGroundColor.rgb, lerp(g_Pass.skyHorizonColor.rgb, g_Pass.skyZenithColor.rgb, saturate(R_lookup.y)), rWeight) * g_Pass.ambientLightColor.a;

	float2 eqUV = float2(atan2(R_lookup.z, R_lookup.x) * (0.5f * INV_PI) + 0.5f, clamp(0.5f - asin(clamp(R_lookup.y, -1.0f, 1.0f)) * INV_PI, 0.001f, 0.999f));
	float3 envRadiance = (g_Pass.ambientLightColor.a > 0.0001f)
		? g_TextureSkyEquirect.SampleLevel(g_SamplerLinearWrap, eqUV, roughness * 5.0f).rgb * g_Pass.ambientLightColor.a
		: skyHemiSpec;

	if (hasProbe)
	{
		float3 localProbeSample = g_TextureSpecularIBL.SampleLevel(g_SamplerLinearWrap, eqUV, roughness * 5.0f).rgb;
		float3 localRadiance = localProbeSample * max(g_Pass.probeBoxExtents.w, 0.0f);
		envRadiance = lerp(envRadiance, localRadiance, specProbeBlend);
	}

	float NoV = max(dot(N, V), 0.0f);
	float2 envBRDF = envBRDFApprox(roughness, NoV);
	// Frostbite directional specular occlusion (Lagarde & de Rousiers 2014)
	float specOcclusion = computeSpecularOcclusion(NoV, ao, roughness);
	float horizOcclusion = computeHorizonOcclusion(R, N);

	// Multi-scatter GGX energy compensation (Fdez-AgÃ¼era 2019) with roughness-attenuated Fresnel grazing peak
	// For dielectrics, quadratic roughness attenuation prevents artificial wet/plastic sheen on dry stone & brick walls
	float Ess = envBRDF.x + envBRDF.y;
	float3 F_ms = F0 * (1.0f / max(Ess, 0.001f) - 1.0f);
	float grazingFresnel = saturate(50.0f * F0.g) * pow(saturate(1.0f - roughness), 1.5f);
	float3 singleScatter = F0 * envBRDF.x + grazingFresnel * envBRDF.y;
	float3 specularIBL = envRadiance * singleScatter * (1.0f + F_ms) * (specOcclusion * horizOcclusion);

	// Screen-Space Reflections (Hi-Z SSR) with Roughness Cutoff & Vignette Confidence
	if (g_SSR.ssrExtra.x > 0.5f && roughness <= g_SSR.ssrParams.z)
	{
		float4 ssrSample = g_TextureSSR.SampleLevel(g_SamplerLinearClamp, screenUV, 0);
		float confidence = ssrSample.a * g_SSR.ssrParams.w;
		if (confidence > 0.001f)
		{
			float roughnessFade = saturate((g_SSR.ssrParams.z - roughness) / max(g_SSR.ssrParams.z * 0.5f, 0.01f));
			float ssrWeight = saturate(confidence * roughnessFade);
			specularIBL = lerp(specularIBL, ssrSample.rgb * singleScatter * (1.0f + F_ms), ssrWeight);
		}
	}
	if (matFlags & DF_MAT_FLAG_CLOTH)
	{
		specularIBL = float3(0.0f, 0.0f, 0.0f);
	}

	if (matFlags & DF_MAT_FLAG_CLEAR_COAT)
	{
		float ccFactor = saturate(g_Material.extraMaterialParams.x > 0.0001f ? g_Material.extraMaterialParams.x : g_Material.transmissionColor.w);
		float ccRoughness = max(g_Material.extraMaterialParams.y, 0.02f);
		float3 ccEnv = (g_Pass.ambientLightColor.a > 0.0001f)
			? g_TextureSkyEquirect.SampleLevel(g_SamplerLinearWrap, eqUV, ccRoughness * 5.0f).rgb * g_Pass.ambientLightColor.a
			: skyHemiSpec;
		if (hasProbe)
		{
			float3 ccProbe = g_TextureSpecularIBL.SampleLevel(g_SamplerLinearWrap, eqUV, ccRoughness * 5.0f).rgb * max(g_Pass.probeBoxExtents.w, 0.0f);
			ccEnv = lerp(ccEnv, ccProbe, specProbeBlend);
		}
		float2 ccBRDF = envBRDFApprox(ccRoughness, NoV);
		float3 ccF_env = fresnelSchlickRoughness(NoV, float3(0.04f, 0.04f, 0.04f), ccRoughness) * ccFactor;
		float3 ccSpecIBL = ccEnv * (0.04f * ccBRDF.x + ccBRDF.y) * ccFactor * (specOcclusion * horizOcclusion);

		// Attenuate base IBL by (1.0 - ccF_env)
		diffuseIBL *= (1.0f - ccF_env);
		specularIBL = specularIBL * (1.0f - ccF_env) + ccSpecIBL;
	}

	return diffuseIBL + specularIBL + Lo;
}

#endif // DARKFIRE_LIGHTING_HLSLI
