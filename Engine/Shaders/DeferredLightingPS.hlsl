#include "Lighting.hlsli"
#include "Atmosphere.hlsli"

struct FullscreenVSOutput
{
	float4 positionCS : SV_Position;
	float2 texcoord   : TEXCOORD0;
};

float4 main(FullscreenVSOutput input) : SV_Target0
{
	float2 uv = input.texcoord;

	// Reversed-Z depth (0.0 = far plane / skybox)
	float depthZ = g_TextureDepth.SampleLevel(g_SamplerPointClamp, uv, 0).r;
	if (depthZ <= 0.000001f)
	{
		discard;
	}

	float4 nrSample = g_TextureNormalRoughness.SampleLevel(g_SamplerPointClamp, uv, 0);
	if (abs(nrSample.w) < 0.001f)
	{
		discard;
	}

	float4 amSample = g_TextureAlbedoMetallic.SampleLevel(g_SamplerPointClamp, uv, 0);
	float4 emSample = g_TextureGBufferEmissiveAO.SampleLevel(g_SamplerPointClamp, uv, 0); // Slot 14: GBuffer Emissive + MaterialAO

	float3 albedo = amSample.rgb;
	float  metallic = amSample.a;
	float3 N = normalize(nrSample.xyz * 2.0f - 1.0f);
	bool   isCloth = (nrSample.w < 0.0f);
	float  roughness = clamp(abs(nrSample.w), 0.04f, 1.0f);
	float3 emissive = emSample.rgb;
	float  matAO = emSample.a;

	// Reconstruct World Position from Reversed-Z Depth
	float2 ndcXY = uv * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
	float4 worldPosH = mul(float4(ndcXY, depthZ, 1.0f), g_View.invViewProjMatrix);
	float3 worldPos = worldPosH.xyz / max(abs(worldPosH.w), 0.000001f);

	float3 V = normalize(g_View.cameraPosition - worldPos);
	float3 F0 = lerp(float3(0.04f, 0.04f, 0.04f), albedo, metallic);

	float ssao = g_TextureSSAO.SampleLevel(g_SamplerLinearClamp, uv, 0).r;
	float ao = ssao * matAO;

	uint matFlags = isCloth ? (DF_MAT_FLAG_CLOTH | DF_MAT_FLAG_TWO_SIDED) : 0u;
	float sheenVal = isCloth ? 0.45f : 0.0f;

	float3 directLighting = evaluateDirectLighting(
		N,
		V,
		worldPos,
		input.positionCS.xy,
		albedo,
		roughness,
		metallic,
		F0,
		ao,
		matFlags,
		sheenVal);

	float3 litColor = applyAtmosphericFog(directLighting + emissive, worldPos, g_View.cameraPosition, input.positionCS.xy);
	return float4(litColor, 1.0f);
}
