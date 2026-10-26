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
	// Fully supports negative terrain elevation without artificial clamping to y = 0
	float b = max(g_Pass.fogParams.x, 0.0001f);
	float y0 = cameraPos.y;
	float y1 = worldPos.y;
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

	// Performance optimization: skip 8-tap shadow raymarch if fog factor is negligible
	if (fogFactor < 0.0001f)
	{
		return litColor;
	}

	// 8-step temporally stable volumetric sunbeam raymarch along cameraPos -> worldPos
	float temporalOffset = float(g_Frame.frameIndex & 15u) * 0.0625f;
	float jitter = (any(screenPos != float2(0.0f, 0.0f)))
		? frac(InterleavedGradientNoise(screenPos) + temporalOffset)
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
		float3 sunDir = safeNormalize(-g_Lights[0].directionRange.xyz);
		float cosTheta = dot(V, sunDir);
		float g = 0.76f;
		float g2 = g * g;
		// Henyey-Greenstein phase function (Mie aerosol forward scattering g = 0.76f)
		float denom = 1.0f + g2 - 2.0f * g * cosTheta;
		float hgPhase = (1.0f / (4.0f * PI)) * ((1.0f - g2) / max(denom * sqrt(max(denom, 0.0001f)), 0.0001f));

		// Soft-compressed phase peak: smooth asymptotic roll-off eliminates 29-degree flat plateau
		float rawPeak = 4.0f * PI * hgPhase;
		float softPeak = rawPeak / (1.0f + rawPeak / 3.5f);

		// Directional sun in-scattering scaled in physical sync with light 0 intensity
		float sunIntensity = max(g_Lights[0].colorIntensity.a, 0.0f);
		float3 sunInscatter = g_Lights[0].colorIntensity.rgb * sunIntensity * softPeak * g_Pass.fogParams.y;
		float twilightFade = smoothstep(-0.02f, 0.08f, sunDir.y);
		fogCol += sunInscatter * twilightFade * sunRayVisibility;
	}

	return lerp(litColor, fogCol, fogFactor);
}

#endif // DARKFIRE_ATMOSPHERE_HLSLI
