#ifndef DARKFIRE_ATMOSPHERE_HLSLI
#define DARKFIRE_ATMOSPHERE_HLSLI

#include "Shadows.hlsli"

// Analytic Integrated Exponential Height Fog with Geometry-Occluded Volumetric Sun In-Scattering
float3 applyAtmosphericFog(
	float3 litColor,
	float3 worldPos,
	float3 cameraPos,
	float2 screenPos = float2(0.0f, 0.0f))
{
	if (g_Pass.fogColorDensity.a <= 0.00001f)
	{
		return litColor;
	}

	float3 camToFrag = worldPos - cameraPos;
	float dist = length(camToFrag);
	float3 V = camToFrag / max(dist, 0.0001f);

	// Closed-form line integral of rho(y) = rho0 * exp(-b * y) from camera to surface
	float b = max(g_Pass.fogParams.x, 0.0001f);
	float y0 = max(cameraPos.y, 0.0f);
	float y1 = max(worldPos.y, 0.0f);
	float dy = y1 - y0;

	float heightIntegral;
	if (abs(dy * b) < 0.001f)
	{
		heightIntegral = exp(-b * y0) * (1.0f - 0.5f * b * dy + (1.0f / 6.0f) * (b * b * dy * dy));
	}
	else
	{
		heightIntegral = (exp(-b * y0) - exp(-b * y1)) / (b * dy);
	}

	float opticalDepth = g_Pass.fogColorDensity.a * dist * heightIntegral;
	float transmittance = exp(-opticalDepth);
	float fogFactor = saturate(1.0f - transmittance);

	// 8-step jittered volumetric sunbeam raymarch along cameraPos -> worldPos (clamped to min(dist, 65.0f))
	float jitter = (any(screenPos != float2(0.0f, 0.0f)))
		? frac(InterleavedGradientNoise(screenPos) + frac(worldPos.x * 17.3f + worldPos.z * 17.3f))
		: frac(worldPos.x * 17.3f + worldPos.z * 17.3f);

	float marchDist = min(dist, 65.0f);
	float stepLength = marchDist / 8.0f;
	float shadowSum = 0.0f;

	[unroll]
	for (int s = 0; s < 8; ++s)
	{
		float sampleDist = (float(s) + jitter) * stepLength;
		float3 stepPos = cameraPos + V * sampleDist;
		shadowSum += sampleVolumetricShadow(stepPos);
	}
	float sunRayVisibility = shadowSum * 0.125f;

	float3 fogCol = g_Pass.fogColorDensity.rgb * lerp(0.35f, 1.0f, sunRayVisibility);
	if (g_Pass.lightCount > 0 && (int)g_Lights[0].positionType.w == 0)
	{
		float3 sunDir = normalize(-g_Lights[0].directionRange.xyz);
		float cosTheta = dot(V, sunDir);
		float g = 0.76f;
		float g2 = g * g;
		// Henyey-Greenstein phase function (Mie aerosol forward scattering g = 0.76f)
		float denom = 1.0f + g2 - 2.0f * g * cosTheta;
		float hgPhase = (1.0f / (4.0f * PI)) * ((1.0f - g2) / max(denom * sqrt(max(denom, 0.0001f)), 0.0001f));

		// Directional sun in-scattering occluded by scene shadow map along view ray
		float3 sunInscatter = g_Lights[0].colorIntensity.rgb * min(4.0f * PI * hgPhase, 3.5f) * g_Pass.fogParams.y;
		fogCol += sunInscatter * saturate(sunDir.y * 0.5f + 0.5f) * sunRayVisibility;
	}

	return lerp(litColor, fogCol, fogFactor);
}

#endif // DARKFIRE_ATMOSPHERE_HLSLI
