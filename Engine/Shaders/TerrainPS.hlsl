#include "Lighting.hlsli"
#include "Atmosphere.hlsli"

cbuffer CBTerrainBuffer : register(b6)
{
	CBTerrain g_Terrain;
};

struct PSOutput
{
	float4 color           : SV_Target0;
	uint   entityId        : SV_Target1;
	float4 normalRoughness : SV_Target2;
	float4 albedoMetallic  : SV_Target3;
};

float terrainHash(float2 p)
{
	p = frac(p * float2(123.34f, 456.21f));
	p += dot(p, p + 45.32f);
	return frac(p.x * p.y);
}

float terrainNoise(float2 p)
{
	float2 i = floor(p);
	float2 f = frac(p);
	float2 u = f * f * (3.0f - 2.0f * f);

	float a = terrainHash(i);
	float b = terrainHash(i + float2(1.0f, 0.0f));
	float c = terrainHash(i + float2(0.0f, 1.0f));
	float d = terrainHash(i + float2(1.0f, 1.0f));

	return lerp(lerp(a, b, u.x), lerp(c, d, u.x), u.y);
}

PSOutput main(PSInput input)
{
	float3 N = normalize(input.normalWS);
	float3 V = normalize(g_View.cameraPosition - input.positionWS);

	float sandHeight = g_Terrain.heightParams.x;
	float cliffSlopeThreshold = g_Terrain.heightParams.y;
	float snowHeight = g_Terrain.heightParams.z;

	float y = input.positionWS.y;
	float Ny = saturate(N.y);

	// 1. Beach Sand Layer (warm golden-beige, micro-grain, roughness 0.75)
	float grain = terrainNoise(input.positionWS.xz * 12.0f) * 0.08f;
	float3 sandColor = sRGBToLinear(float3(0.86f, 0.74f, 0.52f)) + grain;
	float sandRoughness = 0.75f;

	// 2. Lush Grass Layer (organic green, macro/micro variation, roughness 0.65)
	float grassMacro = terrainNoise(input.positionWS.xz * 0.25f);
	float grassMicro = terrainNoise(input.positionWS.xz * 3.5f) * 0.08f;
	float3 grassToneLow = sRGBToLinear(float3(0.18f, 0.38f, 0.09f));
	float3 grassToneHigh = sRGBToLinear(float3(0.32f, 0.54f, 0.18f));
	float3 grassColor = lerp(grassToneLow, grassToneHigh, grassMacro) + grassMicro;
	float grassRoughness = 0.65f;

	// 3. Triplanar Cliff Rock (world-space XYZ triplanar striations & crevices, dark basalt/granite, roughness 0.55)
	float3 triplanarWeights = pow(abs(N), 4.0f);
	triplanarWeights /= max(triplanarWeights.x + triplanarWeights.y + triplanarWeights.z, 0.0001f);

	float striationX = sin(input.positionWS.y * 3.2f + input.positionWS.z * 0.8f) * 0.08f;
	float striationZ = sin(input.positionWS.y * 3.2f + input.positionWS.x * 0.8f) * 0.08f;
	float rockTexX = terrainNoise(input.positionWS.zy * 0.8f) + striationX;
	float rockTexY = terrainNoise(input.positionWS.xz * 0.8f);
	float rockTexZ = terrainNoise(input.positionWS.xy * 0.8f) + striationZ;
	float rockPattern = triplanarWeights.x * rockTexX + triplanarWeights.y * rockTexY + triplanarWeights.z * rockTexZ;

	float3 rockBase = sRGBToLinear(float3(0.24f, 0.22f, 0.21f));
	float3 rockHigh = sRGBToLinear(float3(0.38f, 0.35f, 0.33f));
	float3 cliffColor = lerp(rockBase, rockHigh, rockPattern);
	float cliffRoughness = 0.55f;

	// 4. Alpine Snow Layer (crisp white-blue, low roughness 0.28)
	float snowNoise = terrainNoise(input.positionWS.xz * 1.5f) * 0.04f;
	float3 snowColor = sRGBToLinear(float3(0.92f, 0.95f, 0.99f)) + snowNoise;
	float snowRoughness = 0.28f;

	// Blending weights
	float sandBlend = 1.0f - smoothstep(sandHeight - 0.5f, sandHeight + 1.8f, y);

	float snowAltitude = smoothstep(snowHeight - 2.5f, snowHeight + 3.0f, y);
	float snowSlope = smoothstep(cliffSlopeThreshold * 0.75f, cliffSlopeThreshold * 0.95f, Ny);
	float snowBlend = snowAltitude * snowSlope;

	float cliffBlend = 1.0f - smoothstep(cliffSlopeThreshold - 0.12f, cliffSlopeThreshold + 0.06f, Ny);

	float3 albedo = lerp(grassColor, sandColor, sandBlend);
	float roughness = lerp(grassRoughness, sandRoughness, sandBlend);

	albedo = lerp(albedo, snowColor, snowBlend);
	roughness = lerp(roughness, snowRoughness, snowBlend);

	albedo = lerp(albedo, cliffColor, cliffBlend);
	roughness = lerp(roughness, cliffRoughness, cliffBlend);
	float metallic = 0.0f;

	// Micro-relief normal perturbation for rocky cliff slopes
	if (cliffBlend > 0.01f)
	{
		float2 rockGrad = float2(
			terrainNoise(input.positionWS.xz * 2.0f + float2(0.05f, 0.0f)) - terrainNoise(input.positionWS.xz * 2.0f - float2(0.05f, 0.0f)),
			terrainNoise(input.positionWS.xz * 2.0f + float2(0.0f, 0.05f)) - terrainNoise(input.positionWS.xz * 2.0f - float2(0.0f, 0.05f))
		);
		N = normalize(N + float3(rockGrad.x, 0.0f, rockGrad.y) * (0.35f * cliffBlend));
	}

	float2 screenUV = input.positionCS.xy * g_View.invViewportSize;
	float ao = g_TextureSSAO.SampleLevel(g_SamplerLinearClamp, screenUV, 0).r;

	float3 F0 = float3(0.04f, 0.04f, 0.04f);
	float3 directLighting = evaluateDirectLighting(
		N,
		V,
		input.positionWS,
		input.positionCS.xy,
		albedo,
		roughness,
		metallic,
		F0,
		ao);

	float3 litColor = applyAtmosphericFog(directLighting, input.positionWS, g_View.cameraPosition, input.positionCS.xy);

	PSOutput output;
	output.color = float4(litColor, 1.0f);
	output.entityId = input.entityId;
	output.normalRoughness = float4(N * 0.5f + 0.5f, roughness);
	output.albedoMetallic = float4(albedo, metallic);

	return output;
}
