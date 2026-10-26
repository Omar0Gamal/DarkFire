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

	// Transform normal using cofactor (inverse-transpose) to prevent distortion on non-uniformly scaled objects
#ifdef DF_INSTANCING
	float3 r0 = world[0].xyz;
	float3 r1 = world[1].xyz;
	float3 r2 = world[2].xyz;
	float3x3 normalMatrix = float3x3(
		cross(r1, r2),
		cross(r2, r0),
		cross(r0, r1)
	);
	output.normalWS = normalize(mul(input.normal, normalMatrix));
	output.tangentWS = float4(normalize(mul(input.tangent.xyz, (float3x3)world)), input.tangent.w);
#else
	float3x3 normalMatrix = (float3x3)transpose(g_Object.invWorldMatrix);
	output.normalWS = normalize(mul(input.normal, normalMatrix));
	output.tangentWS = float4(normalize(mul(input.tangent.xyz, (float3x3)world)), input.tangent.w);
#endif

	output.texcoord = input.texcoord * g_Material.tilingOffset.xy + g_Material.tilingOffset.zw;
	output.color = input.color;
	output.entityId = entityId;
	output.curPositionCS = output.positionCS;
	output.prevPositionCS = mul(worldPos, g_View.prevViewProjMatrix);

	return output;
}
