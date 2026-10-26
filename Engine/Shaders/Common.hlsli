//=============================================================================
// DarkFire Engine - Common.hlsli
// Direct3D 11 Shader Pipeline Foundation & Architecture Conventions
//=============================================================================
// GROUND RULE 1: VERIFIED ENGINE CONVENTIONS
// - Coordinate System: Left-Handed (LH), +X Right, +Y Up, +Z Forward (Engine/Core/Math/Math.h)
// - Depth Convention:  Reversed-Z Floating Point (Znear = 1.0, Zfar = 0.0)
// - Depth Format:      DXGI_FORMAT_R32_TYPELESS / D32_FLOAT (SceneRendererInit.cpp:756)
// - View Depth Sign:   Positive Z in front of camera (z_view > 0, w_clip = z_view)
// - World Units:       Meters (1.0 = 1.0 m; camera near = 0.1 m, far = 1000.0 m)
// - Matrix Packing:    Row-Major (/Zpr, D3DCOMPILE_PACK_MATRIX_ROW_MAJOR)
// - mul() Order:       mul(vector, matrix) (row-vector times row-major matrix)
// - Roughness:         Perceptual roughness in [0, 1]; alpha = roughness^2 in NDF
// - Normal Maps:       Cooked: BC5_UNORM (R=X, G=Y). Raw: R8G8B8A8_UNORM Linear
// - Cluster Grid:      16x16x24 View-Space Logarithmic Depth Slices
//=============================================================================

#ifndef DARKFIRE_COMMON_HLSLI
#define DARKFIRE_COMMON_HLSLI

#pragma pack_matrix(row_major)

#include "Shared/Bindings.h"

// Register binding macro glue to enforce single source of truth with C++ Bindings.h
#define HLSL_REG_GLUE(prefix, slot) prefix##slot
#define HLSL_REG_B(slot) register(HLSL_REG_GLUE(b, slot))
#define HLSL_REG_T(slot) register(HLSL_REG_GLUE(t, slot))
#define HLSL_REG_S(slot) register(HLSL_REG_GLUE(s, slot))
#define HLSL_REG_U(slot) register(HLSL_REG_GLUE(u, slot))

// Constant Buffer Declarations
cbuffer CBFrame : HLSL_REG_B(DF_SLOT_CB_PER_FRAME)
{
	CBPerFrame g_Frame;
};

cbuffer CBView : HLSL_REG_B(DF_SLOT_CB_PER_VIEW)
{
	CBPerView g_View;
};

cbuffer CBObject : HLSL_REG_B(DF_SLOT_CB_PER_OBJECT)
{
	CBPerObject g_Object;
};

cbuffer CBMaterial : HLSL_REG_B(DF_SLOT_CB_PER_MATERIAL)
{
	CBPerMaterial g_Material;
};

cbuffer CBPass : HLSL_REG_B(DF_SLOT_CB_PER_PASS)
{
	CBPerPass g_Pass;
};

cbuffer CBShadowBuffer : HLSL_REG_B(DF_SLOT_CB_SHADOW)
{
	CBShadow g_Shadow;
};

// Texture Declarations
Texture2D g_TextureAlbedo             : HLSL_REG_T(DF_SLOT_TEX_ALBEDO);
#define g_TextureHDR g_TextureAlbedo
Texture2D g_TextureNormal             : HLSL_REG_T(DF_SLOT_TEX_NORMAL);
Texture2D g_TextureMetallicRoughness  : HLSL_REG_T(DF_SLOT_TEX_METALLIC_ROUGHNESS);
Texture2D g_TextureEmissive           : HLSL_REG_T(DF_SLOT_TEX_EMISSIVE);
Texture2D g_TextureOcclusion          : HLSL_REG_T(DF_SLOT_TEX_OCCLUSION);
Texture2D g_TextureCustom0            : HLSL_REG_T(DF_SLOT_TEX_CUSTOM0);
#define g_TextureDetail g_TextureCustom0
Texture2D g_TextureCustom1            : HLSL_REG_T(DF_SLOT_TEX_CUSTOM1);
Texture2D g_TextureCustom2            : HLSL_REG_T(DF_SLOT_TEX_CUSTOM2);
StructuredBuffer<LightData> g_Lights  : HLSL_REG_T(DF_SLOT_BUF_LIGHTS);
Texture2D g_TextureDepth              : HLSL_REG_T(DF_SLOT_TEX_DEPTH);
Texture2D g_TextureBloom              : HLSL_REG_T(DF_SLOT_TEX_BLOOM);
Texture2D g_TextureSSAO               : HLSL_REG_T(DF_SLOT_TEX_SSAO);
Texture2D g_TextureSkyEquirect        : HLSL_REG_T(DF_SLOT_TEX_SKY_EQUIRECT);
Texture2D g_TextureShadowMap          : HLSL_REG_T(DF_SLOT_TEX_SHADOW_MAP);
Texture2D g_TextureGBufferEmissiveAO  : HLSL_REG_T(DF_SLOT_TEX_GBUFFER_EMISSIVE_AO);
Texture2D g_TextureNormalRoughness    : HLSL_REG_T(DF_SLOT_TEX_NORMAL_ROUGHNESS);
Texture2D g_TextureAlbedoMetallic     : HLSL_REG_T(DF_SLOT_TEX_ALBEDO_METALLIC);
Texture2D g_TextureSpecularIBL        : HLSL_REG_T(DF_SLOT_TEX_SPECULAR_IBL);
Texture2D g_TextureIrradianceIBL      : HLSL_REG_T(DF_SLOT_TEX_IRRADIANCE_IBL);
Texture2D g_TextureBRDFLUT            : HLSL_REG_T(DF_SLOT_TEX_BRDF_LUT);
Texture3D g_TextureColorLUT           : HLSL_REG_T(DF_SLOT_TEX_COLOR_LUT);
Texture2D g_TextureVelocity           : HLSL_REG_T(DF_SLOT_TEX_VELOCITY);
StructuredBuffer<uint2> g_ClusterGrid : HLSL_REG_T(DF_SLOT_BUF_CLUSTER_GRID);
StructuredBuffer<uint>  g_ClusterLightIndices : HLSL_REG_T(DF_SLOT_BUF_CLUSTER_INDICES);
Texture2D g_TextureSSR                : HLSL_REG_T(DF_SLOT_TEX_SSR);
Texture2D g_TextureHiZDepth           : HLSL_REG_T(DF_SLOT_TEX_HIZ);
Texture2D g_TextureDDGIRadiance       : HLSL_REG_T(DF_SLOT_TEX_DDGI_IRRADIANCE);
Texture2D g_TextureDDGIDepth          : HLSL_REG_T(DF_SLOT_TEX_DDGI_DEPTH);
Texture2D g_TextureSSS                : HLSL_REG_T(DF_SLOT_TEX_SSS);
Texture3D g_TextureVolumetricFog      : HLSL_REG_T(DF_SLOT_TEX_VOLUMETRIC_FOG);

cbuffer CBSSRBuffer : HLSL_REG_B(DF_SLOT_CB_SSR)
{
	CBSSR g_SSR;
};

cbuffer CBDDGIBuffer : HLSL_REG_B(DF_SLOT_CB_DDGI)
{
	CBDDGI g_DDGI;
};

// Sampler Declarations
SamplerState g_SamplerLinearWrap       : HLSL_REG_S(DF_SLOT_SMP_LINEAR_WRAP);
SamplerState g_SamplerLinearClamp      : HLSL_REG_S(DF_SLOT_SMP_LINEAR_CLAMP);
SamplerState g_SamplerPointClamp       : HLSL_REG_S(DF_SLOT_SMP_POINT_CLAMP);
SamplerState g_SamplerAnisoWrap        : HLSL_REG_S(DF_SLOT_SMP_ANISO_WRAP);
SamplerComparisonState g_SamplerShadow : HLSL_REG_S(DF_SLOT_SMP_SHADOW);

// Vertex Input Structure
struct VSInput
{
	float3 position : POSITION;
	float3 normal   : NORMAL;
	float2 texcoord : TEXCOORD0;
	float4 tangent  : TANGENT;
	float4 color    : COLOR;

#ifdef DF_INSTANCING
	row_major float4x4 instanceWorld : INSTANCE_WORLD;
	uint instanceEntityId            : INSTANCE_ID;
#endif
};

// Pixel Input (Vertex Output) Structure
struct PSInput
{
	float4 positionCS : SV_Position;
	float3 positionWS : POSITION;
	float3 normalWS   : NORMAL;
	float2 texcoord   : TEXCOORD0;
	float4 tangentWS  : TANGENT;
	float4 color      : COLOR;
	nointerpolation uint entityId : ENTITY_ID;
	float4 curPositionCS  : TEXCOORD1;
	float4 prevPositionCS : TEXCOORD2;
};

// Math & Color Space Helpers
static const float PI = 3.14159265359f;
static const float INV_PI = 0.31830988618f;

float3 sRGBToLinear(float3 c)
{
	return pow(max(c, float3(0.0f, 0.0f, 0.0f)), float3(2.2f, 2.2f, 2.2f));
}

float3 linearToSRGB(float3 c)
{
	return pow(max(c, float3(0.0f, 0.0f, 0.0f)), float3(1.0f / 2.2f, 1.0f / 2.2f, 1.0f / 2.2f));
}

// Numerical Safety Helpers
float signNotZero(float v)
{
	return (v >= 0.0f) ? 1.0f : -1.0f;
}

float2 signNotZero(float2 v)
{
	return float2(signNotZero(v.x), signNotZero(v.y));
}

float3 signNotZero(float3 v)
{
	return float3(signNotZero(v.x), signNotZero(v.y), signNotZero(v.z));
}

float3 safeNormalize(float3 v, float3 fallback = float3(0.0f, 1.0f, 0.0f))
{
	float lenSq = dot(v, v);
	return (lenSq > 1e-8f) ? (v * rsqrt(lenSq)) : fallback;
}

#endif // DARKFIRE_COMMON_HLSLI
