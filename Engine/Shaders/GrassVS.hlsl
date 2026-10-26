#include "Common.hlsli"

StructuredBuffer<GrassInstanceData> g_GrassInstances : register(t0);

cbuffer CBFoliageBuffer : register(b6)
{
	CBFoliage g_Foliage;
};

PSInput main(VSInput input, uint instanceId : SV_InstanceID)
{
	GrassInstanceData inst = g_GrassInstances[instanceId];

	float3 worldPos = inst.positionScale.xyz;
	float heightScale = inst.positionScale.w;

	float sinYaw = inst.rotationColor.x;
	float cosYaw = inst.rotationColor.y;
	float widthScale = inst.rotationColor.z;
	float colorVariation = inst.rotationColor.w;

	float3 localPos;
	localPos.x = (input.position.x * cosYaw - input.position.z * sinYaw) * widthScale;
	localPos.z = (input.position.x * sinYaw + input.position.z * cosYaw) * widthScale;
	localPos.y = input.position.y * heightScale;

	float3 posWS = worldPos + localPos;

	float distToCam = length(g_View.cameraPosition - posWS);
	float maxDrawDist = max(g_Foliage.windParams.w, 1.0f);
	float lodScale = smoothstep(maxDrawDist, maxDrawDist * 0.7f, distToCam);

	posWS.y = worldPos.y + localPos.y * lodScale;

	float v = input.texcoord.y;
	float windWeight = v * v;

	float windSpeed = g_Foliage.windParams.x;
	float windStrength = g_Foliage.windParams.y;
	float windFreq = g_Foliage.windParams.z;

	float time = g_Frame.time * windSpeed;
	float wave1 = sin(posWS.x * windFreq + time) * cos(posWS.z * windFreq * 0.85f + time * 0.75f);
	float wave2 = sin(posWS.x * windFreq * 2.2f + time * 1.5f + 1.3f) * 0.45f;
	float windDisp = (wave1 + wave2) * windStrength * windWeight;

	posWS.x += windDisp;
	posWS.z += windDisp * 0.65f;
	posWS.y -= abs(windDisp) * 0.35f;

	PSInput output;
	output.positionWS = posWS;
	output.positionCS = mul(float4(posWS, 1.0f), g_View.viewProjMatrix);

	float3 normRot;
	normRot.x = input.normal.x * cosYaw - input.normal.z * sinYaw;
	normRot.z = input.normal.x * sinYaw + input.normal.z * cosYaw;
	normRot.y = input.normal.y;
	output.normalWS = normalize(normRot);

	output.tangentWS = float4(cosYaw, 0.0f, sinYaw, 1.0f);
	output.texcoord = input.texcoord;
	output.color = float4(colorVariation, 0.0f, 0.0f, lodScale);
	output.entityId = g_Object.entityId;

	return output;
}
