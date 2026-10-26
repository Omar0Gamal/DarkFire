#include "Surface.hlsli"
#include "Lighting.hlsli"
#include "Atmosphere.hlsli"

struct PSOutput
{
	float4 color           : SV_Target0;
	uint   entityId        : SV_Target1;
	float4 normalRoughness : SV_Target2;
	float4 albedoMetallic  : SV_Target3;
	float2 velocity        : SV_Target4;
};

PSOutput main(PSInput input, bool isFrontFace : SV_IsFrontFace)
{
	SurfaceData s = evaluateSurface(input, isFrontFace);

	float3 V = normalize(g_View.cameraPosition - input.positionWS);
	float2 screenUV = input.positionCS.xy * g_View.invViewportSize;
	float ssao = g_TextureSSAO.SampleLevel(g_SamplerLinearClamp, screenUV, 0).r;
	float ao = ssao * s.matAO;

	float3 directLighting = evaluateDirectLighting(
		s.normalWS,
		V,
		input.positionWS,
		input.positionCS.xy,
		s.albedo.rgb,
		s.roughness,
		s.metallic,
		s.F0,
		ao,
		0xFFFFFFFFu,
		-1.0f,
		input.tangentWS.xyz);

	PSOutput output;
	float3 litColor = applyAtmosphericFog(directLighting + s.emissive, input.positionWS, g_View.cameraPosition, input.positionCS.xy);
	output.color = float4(litColor, s.albedo.a);
	output.entityId = input.entityId;
	output.normalRoughness = float4(s.normalWS * 0.5f + 0.5f, s.roughness);
	output.albedoMetallic = float4(s.albedo.rgb, s.metallic);

	// Screen-space velocity vector (UV space: +X right, +Y down, pure object & camera motion without subpixel jitter)
	float2 curPosNDC = input.curPositionCS.xy / max(input.curPositionCS.w, 0.0001f);
	float2 prevPosNDC = input.prevPositionCS.xy / max(input.prevPositionCS.w, 0.0001f);
	output.velocity = (curPosNDC - prevPosNDC) * float2(0.5f, -0.5f) - (g_View.jitterOffset.xy - g_View.prevJitterOffset.xy);

	return output;
}
