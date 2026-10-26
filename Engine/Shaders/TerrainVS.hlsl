#include "Common.hlsli"

cbuffer CBTerrainBuffer : register(b6)
{
	CBTerrain g_Terrain;
};

PSInput main(VSInput input)
{
	PSInput output;

	float4 worldPos = mul(float4(input.position, 1.0f), g_Object.worldMatrix);
	output.positionWS = worldPos.xyz;
	output.positionCS = mul(worldPos, g_View.viewProjMatrix);

	output.normalWS = normalize(mul(float4(input.normal, 0.0f), g_Object.worldMatrix).xyz);
	output.tangentWS = float4(normalize(mul(float4(input.tangent.xyz, 0.0f), g_Object.worldMatrix).xyz), input.tangent.w);
	output.texcoord = input.texcoord;
	output.color = input.color;
	output.entityId = g_Object.entityId;

	return output;
}
