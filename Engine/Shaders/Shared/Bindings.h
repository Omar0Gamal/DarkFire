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

// Texture Slots
#define DF_SLOT_TEX_ALBEDO      0
#define DF_SLOT_TEX_NORMAL      1
#define DF_SLOT_TEX_METALLIC_ROUGHNESS 2
#define DF_SLOT_TEX_EMISSIVE    3
#define DF_SLOT_TEX_OCCLUSION   4
#define DF_SLOT_TEX_CUSTOM0     5
#define DF_SLOT_TEX_CUSTOM1     6
#define DF_SLOT_TEX_CUSTOM2     7
#define DF_SLOT_BUF_LIGHTS      8
#define DF_SLOT_TEX_DEPTH       9
#define DF_SLOT_TEX_BLOOM       10

// Sampler Slots
#define DF_SLOT_SMP_LINEAR_WRAP  0
#define DF_SLOT_SMP_LINEAR_CLAMP 1
#define DF_SLOT_SMP_POINT_CLAMP  2
#define DF_SLOT_SMP_ANISO_WRAP   3

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
	float4 baseColorFactor;
	float roughnessFactor;
	float metallicFactor;
	float2 tilingOffset;
	float4 emissiveFactor;
	uint materialFlags;
	float3 pad1;
};

// PerPass Buffer (b4)
struct CBPerPass
{
	float4 ambientLightColor;
	uint lightCount;
	float3 pad2;
};

// Light Data
struct LightData
{
	float4 positionType;   // xyz = position, w = type (0 = Directional, 1 = Point, 2 = Spot)
	float4 directionRange;  // xyz = direction, w = range
	float4 colorIntensity; // rgb = color, a = intensity
	float4 spotAnglesPad;  // x = inner angle cos, y = outer angle cos, zw = padding
};

#ifdef __cplusplus
} // namespace Core::Renderer
#endif

#endif // DARKFIRE_SHADER_BINDINGS_H
