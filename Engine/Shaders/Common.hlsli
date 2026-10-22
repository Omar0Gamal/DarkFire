#ifndef DARKFIRE_COMMON_HLSLI
#define DARKFIRE_COMMON_HLSLI

#pragma pack_matrix(row_major)

#include "Shared/Bindings.h"

// Constant Buffer Declarations
cbuffer CBFrame : register(b0)
{
	CBPerFrame g_Frame;
};

cbuffer CBView : register(b1)
{
	CBPerView g_View;
};

cbuffer CBObject : register(b2)
{
	CBPerObject g_Object;
};

cbuffer CBMaterial : register(b3)
{
	CBPerMaterial g_Material;
};

cbuffer CBPass : register(b4)
{
	CBPerPass g_Pass;
};

// Texture Declarations
Texture2D g_TextureAlbedo             : register(t0);
Texture2D g_TextureNormal             : register(t1);
Texture2D g_TextureMetallicRoughness  : register(t2);
Texture2D g_TextureEmissive           : register(t3);
Texture2D g_TextureOcclusion          : register(t4);
StructuredBuffer<LightData> g_Lights  : register(t8);
Texture2D g_TextureDepth              : register(t9);
Texture2D g_TextureBloom              : register(t10);
Texture2D g_TextureSSAO               : register(t11);

// Sampler Declarations
SamplerState g_SamplerLinearWrap  : register(s0);
SamplerState g_SamplerLinearClamp : register(s1);
SamplerState g_SamplerPointClamp  : register(s2);
SamplerState g_SamplerAnisoWrap   : register(s3);

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

#endif // DARKFIRE_COMMON_HLSLI
