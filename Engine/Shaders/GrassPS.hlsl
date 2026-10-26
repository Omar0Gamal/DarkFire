#include "Lighting.hlsli"

cbuffer CBFoliageBuffer : register(b6)
{
	CBFoliage g_Foliage;
};

struct PSOutput
{
	float4 color           : SV_Target0;
	uint   entityId        : SV_Target1;
	float4 normalRoughness : SV_Target2;
	float4 albedoMetallic  : SV_Target3;
};

PSOutput main(PSInput input, bool isFrontFace : SV_IsFrontFace)
{
	if (input.color.a < 0.01f)
	{
		discard;
	}

	float3 N = normalize(input.normalWS);
	if (!isFrontFace)
	{
		N = -N;
	}

	float3 V = normalize(g_View.cameraPosition - input.positionWS);

	float v = input.texcoord.y;
	float3 grassColor = lerp(sRGBToLinear(g_Foliage.rootColor.rgb), sRGBToLinear(g_Foliage.tipColor.rgb), v);

	float variation = input.color.r;
	grassColor *= (0.82f + 0.36f * variation);

	float roughness = lerp(0.55f, 0.40f, v);
	float metallic = 0.0f;

	float2 screenUV = input.positionCS.xy * g_View.invViewportSize;
	float ao = g_TextureSSAO.SampleLevel(g_SamplerLinearClamp, screenUV, 0).r;
	ao *= lerp(0.5f, 1.0f, v);

	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	float3 directLighting = evaluateDirectLighting(
		N,
		V,
		input.positionWS,
		input.positionCS.xy,
		grassColor,
		roughness,
		metallic,
		F0,
		ao);

	float3 sunDir = float3(0.577f, 0.577f, -0.577f);
	if (g_Pass.lightCount > 0)
	{
		sunDir = -g_Lights[0].directionRange.xyz;
	}
	float backlight = saturate(dot(-V, sunDir));
	float subsurfaceFactor = pow(backlight, 2.5f) * v * 0.45f;
	float3 subsurfaceLight = sRGBToLinear(float3(0.55f, 0.78f, 0.18f)) * subsurfaceFactor;

	float3 totalLit = directLighting + subsurfaceLight;
	float3 litColor = applyAtmosphericFog(totalLit, input.positionWS, g_View.cameraPosition);

	PSOutput output;
	output.color = float4(litColor, 1.0f);
	output.entityId = input.entityId;
	output.normalRoughness = float4(N * 0.5f + 0.5f, roughness);
	output.albedoMetallic = float4(grassColor, metallic);

	return output;
}
