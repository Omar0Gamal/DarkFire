#include "Common.hlsli"

cbuffer CBWaterBuffer : register(b6)
{
	CBWater g_Water;
};

struct GerstnerWave
{
	float2 direction;
	float amplitude;
	float frequency;
	float speed;
	float steepness;
};

PSInput main(VSInput input)
{
	float4 worldOrigin = mul(float4(input.position, 1.0f), g_Object.worldMatrix);
	float3 P = worldOrigin.xyz;
	float waterLevel = g_Water.waterExtra.x;
	P.y = waterLevel;

	float baseAmp = g_Water.waveParams.x;
	float baseFreq = g_Water.waveParams.y;
	float baseSpeed = g_Water.waveParams.z;
	float baseSteep = g_Water.waveParams.w;

	float windAngle = g_Water.sssColor.w;
	float2 windDir = float2(cos(windAngle), sin(windAngle));

	// Dual-Cascade 8-Wave Spectrum: Swell (0-3) + Wind Chop (4-7)
	GerstnerWave waves[8];
	// Cascade 0: Long oceanic swell
	waves[0].direction = normalize(float2(0.707f, 0.707f));
	waves[0].amplitude = baseAmp * 0.45f;
	waves[0].frequency = baseFreq * 1.0f;
	waves[0].speed = baseSpeed * 1.0f;
	waves[0].steepness = baseSteep;

	waves[1].direction = normalize(float2(0.866f, -0.500f));
	waves[1].amplitude = baseAmp * 0.28f;
	waves[1].frequency = baseFreq * 1.8f;
	waves[1].speed = baseSpeed * 1.3f;
	waves[1].steepness = baseSteep;

	waves[2].direction = normalize(float2(-0.500f, 0.866f));
	waves[2].amplitude = baseAmp * 0.16f;
	waves[2].frequency = baseFreq * 2.8f;
	waves[2].speed = baseSpeed * 1.5f;
	waves[2].steepness = baseSteep;

	waves[3].direction = normalize(float2(-0.924f, -0.383f));
	waves[3].amplitude = baseAmp * 0.11f;
	waves[3].frequency = baseFreq * 3.9f;
	waves[3].speed = baseSpeed * 1.8f;
	waves[3].steepness = baseSteep;

	// Cascade 1: High-frequency wind chop
	waves[4].direction = normalize(windDir + float2(0.15f, -0.20f));
	waves[4].amplitude = baseAmp * 0.07f;
	waves[4].frequency = baseFreq * 6.0f;
	waves[4].speed = baseSpeed * 2.2f;
	waves[4].steepness = baseSteep * 0.9f;

	waves[5].direction = normalize(windDir + float2(-0.25f, 0.15f));
	waves[5].amplitude = baseAmp * 0.04f;
	waves[5].frequency = baseFreq * 9.5f;
	waves[5].speed = baseSpeed * 2.6f;
	waves[5].steepness = baseSteep * 0.9f;

	waves[6].direction = normalize(windDir + float2(0.35f, 0.30f));
	waves[6].amplitude = baseAmp * 0.025f;
	waves[6].frequency = baseFreq * 14.0f;
	waves[6].speed = baseSpeed * 3.1f;
	waves[6].steepness = baseSteep * 0.85f;

	waves[7].direction = normalize(windDir + float2(-0.30f, -0.35f));
	waves[7].amplitude = baseAmp * 0.015f;
	waves[7].frequency = baseFreq * 20.0f;
	waves[7].speed = baseSpeed * 3.6f;
	waves[7].steepness = baseSteep * 0.85f;

	float3 displacedP = P;
	float3 normal = float3(0.0f, 1.0f, 0.0f);
	float time = g_Frame.time;

	float dX_dx = 0.0f;
	float dX_dz = 0.0f;
	float dZ_dx = 0.0f;
	float dZ_dz = 0.0f;

	[unroll]
	for (int i = 0; i < 8; ++i)
	{
		float2 d = waves[i].direction;
		float a = waves[i].amplitude;
		float w = waves[i].frequency;
		float phi = waves[i].speed * w;
		float q = waves[i].steepness / max(w * a * 8.0f, 0.0001f);
		q = min(q, 0.95f);

		float phase = w * dot(d, P.xz) + phi * time;
		float cosP = cos(phase);
		float sinP = sin(phase);

		displacedP.x += q * a * d.x * cosP;
		displacedP.z += q * a * d.y * cosP;
		displacedP.y += a * sinP;

		normal.x -= d.x * (w * a * cosP);
		normal.z -= d.y * (w * a * cosP);
		normal.y -= q * (w * a * sinP);

		// Partial derivatives for Jacobian determinant
		dX_dx -= q * a * d.x * d.x * w * sinP;
		dX_dz -= q * a * d.x * d.y * w * sinP;
		dZ_dx -= q * a * d.y * d.x * w * sinP;
		dZ_dz -= q * a * d.y * d.y * w * sinP;
	}

	normal = normalize(normal);

	// Jacobian determinant: surface pinching factor
	float jacobian = (1.0f + dX_dx) * (1.0f + dZ_dz) - (dX_dz * dZ_dx);

	PSInput output;
	output.positionWS = displacedP;
	output.positionCS = mul(float4(displacedP, 1.0f), g_View.viewProjMatrix);
	output.normalWS = normal;
	output.tangentWS = float4(1.0f, 0.0f, 0.0f, 1.0f);
	output.texcoord = input.texcoord;
	// Pack Jacobian determinant in color.r and wave elevation in color.g
	output.color = float4(jacobian, displacedP.y - waterLevel, 1.0f, 1.0f);
	output.entityId = g_Object.entityId;
	output.curPositionCS = output.positionCS;
	output.prevPositionCS = mul(float4(displacedP, 1.0f), g_View.prevViewProjMatrix);

	return output;
}
