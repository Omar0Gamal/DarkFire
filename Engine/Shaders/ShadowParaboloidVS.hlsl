#include "Common.hlsli"
#include "Shared/Bindings.h"

cbuffer CBParaboloidShadowBuffer : register(b0)
{
	CBParaboloidShadow g_Paraboloid;
};

PSInput main(VSInput input)
{
	float4 worldPos = mul(float4(input.position, 1.0f), g_Object.worldMatrix);
	float4 lightPos = mul(worldPos, g_Paraboloid.lightViewMatrix);

	float L = length(lightPos.xyz);
	float3 P = lightPos.xyz / max(L, 0.0001f);

	float zSign = g_Paraboloid.paraboloidParams.z; // +1.0 for front hemisphere, -1.0 for back hemisphere
	P.z *= zSign;

	// Paraboloid projection: (x / (z + 1), y / (z + 1))
	float2 proj = P.xy / max(P.z + 1.0f, 0.001f);
	float normDepth = (L - g_Paraboloid.paraboloidParams.x) / max(g_Paraboloid.paraboloidParams.y - g_Paraboloid.paraboloidParams.x, 0.001f);

	// Remap to atlas viewport sub-rect
	float2 atlasPos = proj * g_Paraboloid.atlasViewport.zw + g_Paraboloid.atlasViewport.xy;

	PSInput output;
	output.positionCS = float4(atlasPos, saturate(normDepth), 1.0f);
	output.positionWS = worldPos.xyz;
	output.normalWS = input.normal;
	output.texcoord = input.texcoord;
	output.tangentWS = float4(1.0f, 0.0f, 0.0f, 1.0f);
	output.color = float4(normDepth, 0.0f, 0.0f, 1.0f);
	output.entityId = g_Object.entityId;
	output.curPositionCS = output.positionCS;
	output.prevPositionCS = output.positionCS;

	return output;
}
