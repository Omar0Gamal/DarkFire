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

	GerstnerWave waves[4];
	waves[0].direction = normalize(float2(0.707f, 0.707f));
	waves[0].amplitude = baseAmp * 0.45f;
	waves[0].frequency = baseFreq * 1.0f;
	waves[0].speed = baseSpeed * 1.0f;
	waves[0].steepness = baseSteep;

	waves[1].direction = normalize(float2(0.866f, -0.500f));
	waves[1].amplitude = baseAmp * 0.28f;
	waves[1].frequency = baseFreq * 1.9f;
	waves[1].speed = baseSpeed * 1.3f;
	waves[1].steepness = baseSteep;

	waves[2].direction = normalize(float2(-0.500f, 0.866f));
	waves[2].amplitude = baseAmp * 0.16f;
	waves[2].frequency = baseFreq * 3.1f;
	waves[2].speed = baseSpeed * 1.6f;
	waves[2].steepness = baseSteep;

	waves[3].direction = normalize(float2(-0.924f, -0.383f));
	waves[3].amplitude = baseAmp * 0.11f;
	waves[3].frequency = baseFreq * 4.4f;
	waves[3].speed = baseSpeed * 2.1f;
	waves[3].steepness = baseSteep;

	float3 displacedP = P;
	float3 normal = float3(0.0f, 1.0f, 0.0f);

	float time = g_Frame.time;

	[unroll]
	for (int i = 0; i < 4; ++i)
	{
		float2 d = waves[i].direction;
		float a = waves[i].amplitude;
		float w = waves[i].frequency;
		float phi = waves[i].speed * w;
		float q = waves[i].steepness / max(w * a * 4.0f, 0.0001f);
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
	}

	normal = normalize(normal);

	PSInput output;
	output.positionWS = displacedP;
	output.positionCS = mul(float4(displacedP, 1.0f), g_View.viewProjMatrix);
	output.normalWS = normal;
	output.tangentWS = float4(1.0f, 0.0f, 0.0f, 1.0f);
	output.texcoord = input.texcoord;
	output.color = float4(1.0f, 1.0f, 1.0f, 1.0f);
	output.entityId = g_Object.entityId;

	return output;
}
