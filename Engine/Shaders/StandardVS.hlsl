#include "Common.hlsli"

PSInput main(VSInput input)
{
	PSInput output;

#ifdef DF_INSTANCING
	float4x4 world = input.instanceWorld;
	uint entityId = input.instanceEntityId;
#else
	float4x4 world = g_Object.worldMatrix;
	uint entityId = g_Object.entityId;
#endif

	float4 worldPos = mul(float4(input.position, 1.0f), world);
	output.positionWS = worldPos.xyz;
	output.positionCS = mul(worldPos, g_View.viewProjMatrix);

	// Transform normal and tangent to world space
	output.normalWS = normalize(mul(input.normal, (float3x3)world));
	output.tangentWS = float4(normalize(mul(input.tangent.xyz, (float3x3)world)), input.tangent.w);

	output.texcoord = input.texcoord * g_Material.tilingOffset;
	output.color = input.color;
	output.entityId = entityId;

	return output;
}
