#include "Common.hlsli"

struct ShadowVSOutput
{
	float4 positionCS : SV_Position;
	float2 texcoord   : TEXCOORD0;
};

ShadowVSOutput main(VSInput input)
{
	ShadowVSOutput output;
#ifdef DF_INSTANCING
	float4x4 world = input.instanceWorld;
#else
	float4x4 world = g_Object.worldMatrix;
#endif

	float4 worldPos = mul(float4(input.position, 1.0f), world);
	output.positionCS = mul(worldPos, g_Shadow.lightViewProj);
	output.texcoord = input.texcoord;
	return output;
}
