#include "Lighting.hlsli"

struct PSOutput
{
	float4 color    : SV_Target0;
	uint   entityId : SV_Target1;
};

PSOutput main(PSInput input)
{
	float4 albedoTex = g_TextureAlbedo.Sample(g_SamplerLinearWrap, input.texcoord);
	float4 albedo = albedoTex * g_Material.baseColorFactor * input.color;

	// Alpha test / discard for transparent cutouts
	if (albedo.a < 0.01f)
	{
		discard;
	}

	float3 N = normalize(input.normalWS);
	if (length(input.tangentWS.xyz) > 0.1f)
	{
		N = computePerturbedNormal(N, input.tangentWS, input.texcoord);
	}

	float3 V = normalize(g_View.cameraPosition - input.positionWS);

	float4 mrSample = g_TextureMetallicRoughness.Sample(g_SamplerLinearWrap, input.texcoord);
	float metallic = clamp(mrSample.b * g_Material.metallicFactor, 0.0f, 1.0f);
	float roughness = clamp(mrSample.g * g_Material.roughnessFactor, 0.04f, 1.0f);

	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	F0 = lerp(F0, albedo.rgb, metallic);

	float2 screenUV = input.positionCS.xy * g_View.invViewportSize;
	float ssao = g_TextureSSAO.SampleLevel(g_SamplerLinearClamp, screenUV, 0).r;
	float matOcclusion = g_TextureOcclusion.Sample(g_SamplerLinearWrap, input.texcoord).r;
	float ao = min(ssao, matOcclusion);

	float3 directLighting = evaluateDirectLighting(
		N,
		V,
		input.positionWS,
		albedo.rgb,
		roughness,
		metallic,
		F0,
		ao);

	float4 emissiveTex = g_TextureEmissive.Sample(g_SamplerLinearWrap, input.texcoord);
	float3 emissive = emissiveTex.rgb * g_Material.emissiveFactor.rgb;

	PSOutput output;
	output.color = float4(directLighting + emissive, albedo.a);
	output.entityId = input.entityId;

	return output;
}
