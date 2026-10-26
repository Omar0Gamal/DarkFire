// DarkFire Engine - DecalVS.hlsl
// Unit Cube Projected OBB Decal Vertex Shader (vs_5_0)

#include "Common.hlsli"

struct DecalVSOutput
{
	float4 positionCS : SV_Position;
	float4 clipPos    : TEXCOORD0;
};

DecalVSOutput main(VSInput input)
{
	DecalVSOutput output;

	// Transform unit cube vertex [-0.5, 0.5]^3 into world space by decal OBB worldMatrix
	float4 worldPos = mul(float4(input.position, 1.0f), g_Object.worldMatrix);

	// Transform world space position into clip space by camera viewProjMatrix
	output.positionCS = mul(worldPos, g_View.viewProjMatrix);
	output.clipPos = output.positionCS;

	return output;
}
