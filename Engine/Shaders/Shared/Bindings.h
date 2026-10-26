#ifndef DARKFIRE_SHADER_BINDINGS_H
#define DARKFIRE_SHADER_BINDINGS_H

#ifndef __cplusplus
#pragma pack_matrix(row_major)
#endif

// Constant Buffer Slots
#define DF_SLOT_CB_PER_FRAME    0
#define DF_SLOT_CB_PER_VIEW     1
#define DF_SLOT_CB_PER_OBJECT   2
#define DF_SLOT_CB_PER_MATERIAL 3
#define DF_SLOT_CB_PER_PASS     4
#define DF_SLOT_CB_POST_PROCESS 5
#define DF_SLOT_CB_SHADOW       5
#define DF_SLOT_CB_TERRAIN      6
#define DF_SLOT_CB_FOLIAGE      6
#define DF_SLOT_CB_WATER        6

// Texture Slots
#define DF_SLOT_TEX_ALBEDO      0
#define DF_SLOT_TEX_NORMAL      1
#define DF_SLOT_TEX_METALLIC_ROUGHNESS 2
#define DF_SLOT_TEX_EMISSIVE    3
#define DF_SLOT_TEX_OCCLUSION   4
#define DF_SLOT_TEX_CUSTOM0     5
#define DF_SLOT_TEX_DETAIL      5
#define DF_SLOT_TEX_CUSTOM1     6
#define DF_SLOT_TEX_CUSTOM2     7
#define DF_SLOT_BUF_LIGHTS      8
#define DF_SLOT_TEX_DEPTH       9
#define DF_SLOT_TEX_BLOOM       10
#define DF_SLOT_TEX_SSAO        11
#define DF_SLOT_TEX_SKY_LUT     12
#define DF_SLOT_TEX_SKY_EQUIRECT 12
#define DF_SLOT_TEX_SHADOW_MAP  13
#define DF_SLOT_TEX_GBUFFER_EMISSIVE_AO 14
#define DF_SLOT_TEX_NORMAL_ROUGHNESS  15
#define DF_SLOT_TEX_ALBEDO_METALLIC   16
#define DF_SLOT_TEX_SPECULAR_IBL      17
#define DF_SLOT_TEX_IRRADIANCE_IBL    18
#define DF_SLOT_TEX_BRDF_LUT          19
#define DF_SLOT_TEX_COLOR_LUT         20
#define DF_SLOT_TEX_VELOCITY          21
#define DF_SLOT_BUF_CLUSTER_GRID      22
#define DF_SLOT_BUF_CLUSTER_INDICES   23
#define DF_SLOT_TEX_SSR               24
#define DF_SLOT_TEX_HIZ               25
#define DF_SLOT_TEX_DDGI_IRRADIANCE   26
#define DF_SLOT_TEX_DDGI_DEPTH        27
#define DF_SLOT_TEX_SSS               28
#define DF_SLOT_TEX_VOLUMETRIC_FOG    29
#define DF_SLOT_TEX_TILE_MAX          30
#define DF_SLOT_TEX_NEIGHBOR_MAX      31

// Constant Buffer Slots (Extended)
#define DF_SLOT_CB_SSR                7
#define DF_SLOT_CB_MOTION_BLUR        7
#define DF_SLOT_CB_DDGI               8
#define DF_SLOT_CB_SSS                8
#define DF_SLOT_CB_VOLUMETRIC_FOG     8

// Sampler Slots
#define DF_SLOT_SMP_LINEAR_WRAP  0
#define DF_SLOT_SMP_LINEAR_CLAMP 1
#define DF_SLOT_SMP_POINT_CLAMP  2
#define DF_SLOT_SMP_ANISO_WRAP   3
#define DF_SLOT_SMP_SHADOW       4

// Material Flags Bitmask
#define DF_MAT_FLAG_HAS_ALBEDO_MAP    (1u << 0)
#define DF_MAT_FLAG_HAS_NORMAL_MAP    (1u << 1)
#define DF_MAT_FLAG_HAS_MR_MAP        (1u << 2)
#define DF_MAT_FLAG_HAS_EMISSIVE_MAP  (1u << 3)
#define DF_MAT_FLAG_HAS_OCCLUSION_MAP (1u << 4)
#define DF_MAT_FLAG_FLIP_NORMAL_Y     (1u << 5) // OpenGL (+Y) vs DirectX (-Y) normal map
#define DF_MAT_FLAG_ORM_PACKED        (1u << 6) // R=Occlusion, G=Roughness, B=Metallic
#define DF_MAT_FLAG_TWO_SIDED         (1u << 7)
#define DF_MAT_FLAG_MASK_MAP_PACKED   (1u << 8) // Unity HDRP: R=Metallic, G=AO, B=DetailMask, A=Smoothness
#define DF_MAT_FLAG_HAS_DETAIL_MAP    (1u << 9) // Secondary micro-detail normal/albedo
#define DF_MAT_FLAG_TRANSMISSION      (1u << 10)// Subsurface leaf transmission
#define DF_MAT_FLAG_CLOTH             (1u << 11)// Charlie cloth BRDF & wrap lighting
#define DF_MAT_FLAG_CLEAR_COAT        (1u << 12)// Dual-layer dielectric clear-coat GGX lobe
#define DF_MAT_FLAG_ANISOTROPY        (1u << 13)// Brushed anisotropic GGX lobe
#define DF_MAT_FLAG_SUBSURFACE        (1u << 14)// Screen-space subsurface scattering diffusion profile

#ifdef __cplusplus
#include "Engine/Core/Math/Math.h"
#include <cstdint>

namespace Core::Renderer
{
	using float4 = Math::Vector4;
	using float3 = Math::Vector3;
	using float2 = Math::Vector2;
	using float4x4 = Math::Matrix;
	using uint = uint32_t;
	struct uint4 { uint32_t x = 0, y = 0, z = 0, w = 0; };
#endif

// PerFrame Buffer (b0)
struct CBPerFrame
{
	float time;
	float deltaTime;
	uint frameIndex;
	float pad0;
};

// PerView Buffer (b1)
struct CBPerView
{
	float4x4 viewMatrix;
	float4x4 projMatrix;
	float4x4 viewProjMatrix;
	float4x4 invViewMatrix;
	float4x4 invProjMatrix;
	float4x4 invViewProjMatrix;
	float3 cameraPosition;
	float nearZ;
	float3 cameraForward;
	float farZ;
	float2 viewportSize;
	float2 invViewportSize;
	float4x4 prevViewProjMatrix;
	float2 jitterOffset;
	float2 prevJitterOffset;
};

// PerObject Buffer (b2)
struct CBPerObject
{
	float4x4 worldMatrix;
	float4x4 invWorldMatrix;
	uint entityId;
	float3 pad0;
};

// PerMaterial Buffer (b3)
struct CBPerMaterial
{
	float4 baseColorFactor;     // 16B: offset 0
	float4 emissiveFactor;      // 16B: offset 16 (rgb = color, a = intensity multiplier)
	float4 tilingOffset;        // 16B: offset 32 (xy = UV tiling, zw = UV offset)
	float  roughnessFactor;     // 4B:  offset 48
	float  metallicFactor;      // 4B:  offset 52
	float  normalScale;         // 4B:  offset 56
	float  occlusionStrength;   // 4B:  offset 60
	float  alphaCutoff;         // 4B:  offset 64
	uint   materialFlags;       // 4B:  offset 68
	float  transmissionFactor;  // 4B:  offset 72
	float  sheenFactor;         // 4B:  offset 76 -> 80B
	float4 transmissionColor;   // 16B: offset 80 (rgb = transmitted tint, w = unused)
	float4 detailTilingScale;   // 16B: offset 96 (xy = detail UV tiling, z = detailNormalScale, w = detailAlbedoScale)
	float4 extraMaterialParams; // 16B: offset 112 (x = clearCoatFactor, y = clearCoatRoughness, z = anisotropyFactor, w = anisotropyRotation)
};

// SSR Buffer (b7)
struct CBSSR
{
	float4 ssrParams;          // x = maxDistance, y = thickness, z = roughnessCutoff, w = intensity
	float4 ssrExtra;           // x = enabled, y = stepCount, zw = pad
};

// SSS Buffer (b8 in subsurface pass)
struct CBSSS
{
	float4 sssParams;          // x = radius, y = maxDepthDiff, zw = pad
	float4 sssDirection;       // xy = direction (1,0 or 0,1), zw = pad
};

// DDGI Buffer (b8 in lighting passes)
struct CBDDGI
{
	float4 gridOriginSpacing;  // xyz = origin, w = probeSpacing
	float4 gridCountsExtents;  // xyz = probeCounts (e.g. 16, 8, 12), w = normalBias
	float4 ddgiParams;         // x = energyPreservation, y = hysteresis, z = intensity, w = enabled
};

// PerPass Buffer (b4)
struct CBPerPass
{
	float4 ambientLightColor;   // 16B: offset 0 (rgb = color, a = intensity)
	float4 skyZenithColor;      // 16B: offset 16
	float4 skyHorizonColor;     // 16B: offset 32
	float4 skyGroundColor;      // 16B: offset 48
	float4 fogColorDensity;     // 16B: offset 64 (rgb = fogColor, a = fogDensity, 0 = disabled)
	float4 fogParams;           // 16B: offset 80 (x = fogHeightFalloff, y = fogSunScattering, z = enablePCSS, w = enableContactShadows)
	float4 shGridMin;           // 16B: offset 96 (x = tessellationFactor, y = minDist, z = maxDist, w = phongStrength)
	float4 shGridMax;           // 16B: offset 112 (xyz = boxProbeCenter, w = enableBoxProbe)
	float4 probeBoxExtents;     // 16B: offset 128 (xyz = boxProbeHalfExtents, w = probeIntensity)
	uint   lightCount;          // 4B:  offset 144
	float3 pad2;                // 12B: offset 148 -> total 160 bytes (multiple of 16)
};

// Shadow Buffer (b5 in opaque/shadow passes)
struct CBShadow
{
	float4x4 lightViewProj;     // 64B: offset 0 (Primary / Cascade 0 matrix)
	float4x4 cascadeViewProj1;  // 64B: offset 64
	float4x4 cascadeViewProj2;  // 64B: offset 128
	float4x4 cascadeViewProj3;  // 64B: offset 192
	float4   cascadeSplits;     // 16B: offset 256 (x, y, z, w = view-space split depths)
	float4   shadowParams;      // 16B: offset 272 (x = depthBias, y = normalBias, z = invShadowMapSize, w = softness)
	float4   shadowExtra;       // 16B: offset 288 (x = shadowStrength, y = directAOInfluence, z = shadowEnabled, w = cascadeCount)
};

// Light Data
struct LightData
{
	float4 positionType;   // xyz = position, w = type (0 = Directional, 1 = Point, 2 = Spot)
	float4 directionRange;  // xyz = direction, w = range
	float4 colorIntensity; // rgb = color, a = intensity
	float4 spotAnglesPad;  // x = inner angle cos, y = outer angle cos, z = hasShadow, w = isPointParaboloid
	float4x4 shadowViewProj;// 64B: Light View-Projection or Light View
	float4 shadowAtlasRect;// 16B: xy = uv offset, zw = uv scale in atlas
};

// PostProcess Buffer (b5 or b0 in post passes)
struct CBPostProcess
{
	float exposure;
	uint tonemapper;       // 0=ACES, 1=AgX, 2=Reinhard, 3=Neutral, 4=None
	float contrast;
	float saturation;
	float vignetteIntensity;
	float vignetteSmoothness;
	float bloomIntensity;
	uint debugView;        // 0=Lit, 1=SSAO, 2=Bloom, 3=Shadows, 4=Normals, 5=Albedo, 6=MetallicRoughness
	float4 colorFilter;
	float4 bloomTint;
	float2 texelSize;
	float gamma;
	float pad0;
};

// FXAA Buffer
struct CBFXAA
{
	float2 texelSize;
	float subpix;
	float edgeThreshold;
	float edgeThresholdMin;
	float3 pad;
};

// SSAO Buffer
struct CBSSAO
{
	float radius;
	float bias;
	float intensity;
	float power;
	uint sampleCount;
	float pad0;
	float2 invResolution;
	float4 kernel[16];
};

// Bloom Buffer
struct CBBloom
{
	float threshold;
	float knee;
	float scatter;
	float intensity;
	float4 tint;
	float2 texelSize;
	float mipLevel;
	float pad;
};

// Environment Buffer
struct CBEnvironment
{
	float4 skyZenith;
	float4 skyHorizon;
	float4 groundColor;
	float4 ambientColor;
	float4 sunDirection; // xyz = dir, w = drawSunDisk
	float4 sunColor;     // rgb = color, a = sunDiskCos
	float skyIntensity;
	float ambientIntensity;
	uint  skyType;       // 0 = ProceduralGradient, 1 = SkyboxTexture, 2 = CustomColor
	float pad0;
};

// CBTerrain Buffer (b6)
struct CBTerrain
{
	float4 heightParams; // x = sandHeight, y = cliffSlopeThreshold, z = snowHeight, w = maxHeight
	float4 worldParams;  // x = worldSize, y = seaFloorDepth, z = uvScale, w = pad
};

// Grass Instance Data (StructuredBuffer<GrassInstanceData> at t0)
struct GrassInstanceData
{
	float4 positionScale; // xyz = worldPos, w = heightScale
	float4 rotationColor; // x = sinYaw, y = cosYaw, z = widthScale, w = colorVariation
};

// CBFoliage Buffer (b6)
struct CBFoliage
{
	float4 windParams;    // x = windSpeed, y = windStrength, z = windFrequency, w = maxDrawDistance
	float4 rootColor;
	float4 tipColor;
};

// CBWater Buffer (b6)
struct CBWater
{
	float4 waveParams;    // x = waveAmplitude, y = waveFrequency, z = waveSpeed, w = waveSteepness
	float4 shallowColor;
	float4 deepColor;
	float4 foamColor;
	float4 opticalParams; // x = depthAbsorption, y = foamDepthThreshold, z = refractionStrength, w = causticsIntensity
	float4 waterExtra;    // x = waterLevel, y = planeSize, z = sssIntensity, w = jacobianThreshold
	float4 sssColor;      // rgb = sssColor, w = windAngle
};

// Motion Blur Buffer
struct CBMotionBlur
{
	float4 motionBlurParams; // x = shutterAngle (0-1), y = maxRadius (px), z = numSamples (12), w = enabled
	float4 screenResolution; // xy = res, zw = invRes
};

// Volumetric Fog Buffer
struct CBVolumetricFog
{
	float4 fogParams;        // x = density, y = heightFalloff, z = sunScattering, w = baseHeight
	float4 fogDepths;        // x = nearZ, y = farZ, z = historyBlend (0.88), w = pad
	float4 volumeGridSize;   // xyz = (160, 90, 64), w = 1.0 / 64.0
	float4x4 prevViewProj;
};

// Volumetric Fog Integration Buffer
struct CBVolumetricFogIntegrate
{
	float4 fogDepths;        // x = nearZ, y = farZ, zw = pad
	float4 volumeGridSize;   // xyz = (160, 90, 64), w = 1.0 / 64.0
};

// Paraboloid Shadow Buffer
struct CBParaboloidShadow
{
	float4x4 lightViewMatrix;
	float4   paraboloidParams; // x = nearZ, y = farZ, z = isBackFacing (+1 front, -1 back), w = pad
	float4   atlasViewport;    // xy = offset, zw = scale
};

// Luminance Histogram Buffer
struct CBLuminanceHistogram
{
	uint4  dimensions; // x = width, y = height, zw = pad
	float4 evRange;    // x = minEV (-10.0), y = maxEV (+10.0), z = invRange, w = pad
};

// Luminance Average & Adaptation Buffer
struct CBLuminanceAverage
{
	float4 adaptationParams; // x = deltaTime, y = speedUp, z = speedDown, w = keyVal (0.18)
	float4 evRange;          // x = minEV, y = maxEV, z = minExposure, w = maxExposure
	uint4  pixelCount;       // x = totalPixels, yzw = pad
};

#ifdef __cplusplus
} // namespace Core::Renderer
#endif

#endif // DARKFIRE_SHADER_BINDINGS_H
