#include "Common.hlsli"

cbuffer CBEnvironmentBuffer : register(b5)
{
	CBEnvironment g_Environment;
};

struct FullscreenVSOutput
{
	float4 positionCS : SV_Position;
	float2 texcoord   : TEXCOORD0;
};

struct PSOutput
{
	float4 color    : SV_Target0;
	uint   entityId : SV_Target1;
};

PSOutput main(FullscreenVSOutput input)
{
	// Unproject screen UV to world ray direction (reversed-Z depth = 0.0 is the far plane)
	float2 ndc = input.texcoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
	float4 clipPos = float4(ndc, 0.0f, 1.0f);
	float4 worldPosH = mul(clipPos, g_View.invViewProjMatrix);
	float3 worldPos = worldPosH.xyz / worldPosH.w;
	float3 rayDir = normalize(worldPos - g_View.cameraPosition);

	float elevation = rayDir.y;
	float3 skyColor;
	if (elevation >= 0.0f)
	{
		float t = pow(clamp(elevation, 0.0f, 1.0f), 0.5f);
		skyColor = lerp(g_Environment.skyHorizon.rgb, g_Environment.skyZenith.rgb, t);
	}
	else
	{
		float t = pow(clamp(-elevation, 0.0f, 1.0f), 0.5f);
		skyColor = lerp(g_Environment.skyHorizon.rgb, g_Environment.groundColor.rgb, t);
	}

	skyColor *= g_Environment.skyIntensity;

	// Sun disk (sunDirection.w > 0.5 means enabled)
	if (g_Environment.sunDirection.w > 0.5f)
	{
		float3 sunDir = normalize(g_Environment.sunDirection.xyz);
		float sunDot = dot(rayDir, sunDir);
		float sunDiskCos = g_Environment.sunColor.a > 0.0f ? g_Environment.sunColor.a : 0.9995f;
		if (sunDot > sunDiskCos)
		{
			float sunGlow = smoothstep(sunDiskCos, 1.0f, sunDot);
			skyColor += g_Environment.sunColor.rgb * (1.0f + sunGlow * 12.0f);
		}
		else
		{
			// Soft corona glow around the sun
			float coronaCos = sunDiskCos - 0.025f;
			if (sunDot > coronaCos)
			{
				float corona = smoothstep(coronaCos, sunDiskCos, sunDot);
				skyColor += g_Environment.sunColor.rgb * corona * 0.6f;
			}
		}
	}

	PSOutput output;
	output.color = float4(skyColor, 1.0f);
	output.entityId = 0;
	return output;
}
