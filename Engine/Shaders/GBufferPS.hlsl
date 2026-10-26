#include "Surface.hlsli"

struct GBufferOutput
{
	float4 albedoMetallic  : SV_Target0; // rgb = Albedo, a = Metallic
	float4 normalRoughness : SV_Target1; // rgb = Normal * 0.5 + 0.5, a = Roughness (negative if Cloth)
	float4 emissiveAO      : SV_Target2; // rgb = Emissive, a = Material AO
	uint   entityId        : SV_Target3; // R32_UINT Entity ID
	float2 velocity        : SV_Target4; // R16G16_FLOAT Screen-space motion vector
};

GBufferOutput main(PSInput input, bool isFrontFace : SV_IsFrontFace)
{
	SurfaceData s = evaluateSurface(input, isFrontFace);

	bool isCloth = (g_Material.materialFlags & DF_MAT_FLAG_CLOTH) != 0;
	float packedRoughness = isCloth ? -max(s.roughness, 0.04f) : max(s.roughness, 0.04f);

	// Screen-space velocity vector (UV space: +X right, +Y down, pure object & camera motion without subpixel jitter)
	float2 curPosNDC = input.curPositionCS.xy / max(input.curPositionCS.w, 0.0001f);
	float2 prevPosNDC = input.prevPositionCS.xy / max(input.prevPositionCS.w, 0.0001f);
	float2 vel = (curPosNDC - prevPosNDC) * float2(0.5f, -0.5f) - (g_View.jitterOffset.xy - g_View.prevJitterOffset.xy);

	GBufferOutput output;
	output.albedoMetallic  = float4(s.albedo.rgb, s.metallic);
	output.normalRoughness = float4(s.normalWS * 0.5f + 0.5f, packedRoughness);
	output.emissiveAO      = float4(s.emissive, s.matAO);
	output.entityId        = input.entityId;
	output.velocity        = vel;
	return output;
}
