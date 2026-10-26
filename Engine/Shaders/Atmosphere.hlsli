#ifndef DARKFIRE_ATMOSPHERE_HLSLI
#define DARKFIRE_ATMOSPHERE_HLSLI

#include "Shadows.hlsli"

// Analytic Integrated Exponential Height Fog with 3D Froxel Volumetric Fog Volume
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

	// View-space slice depth reconstruction
	float4 viewPosH = mul(float4(worldPos, 1.0f), g_View.viewMatrix);
	float viewDepth = abs(viewPosH.z);

	// 1. Evaluate 3D Froxel Volumetric Fog Volume (Single 3D Trilinear Sample)
	float nearZ = 0.5f;
	float farZ = 65.0f;
	float sliceW = log(max(viewDepth, nearZ) / nearZ) / max(log(farZ / nearZ), 0.0001f);
	float2 screenUV = screenPos * g_View.invViewportSize;

	if (any(screenUV > 0.0001f) && sliceW >= 0.0f)
	{
		float3 froxelUVW = float3(screenUV, saturate(sliceW));
		float4 froxelSample = g_TextureVolumetricFog.SampleLevel(g_SamplerLinearClamp, froxelUVW, 0);
		// Note: Unbound or clear texture returns (0, 0, 0, 0). Valid accumulated transmittance is in (0.001, 1.0].
		if (froxelSample.a > 0.001f && (any(froxelSample.rgb > 0.0001f) || froxelSample.a < 0.999f))
		{
			// Pre-integrated volume: RGB = in-scattered radiance, A = accumulated transmittance
			return litColor * froxelSample.a + froxelSample.rgb;
		}
	}

	// 2. Analytic closed-form exponential height fog fallback
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

	float3 fogCol = g_Pass.fogColorDensity.rgb;
	if (g_Pass.lightCount > 0 && (int)g_Lights[0].positionType.w == 0)
	{
		float3 sunDir = safeNormalize(-g_Lights[0].directionRange.xyz);
		float cosTheta = dot(V, sunDir);
		float g = 0.76f;
		float g2 = g * g;
		float denom = 1.0f + g2 - 2.0f * g * cosTheta;
		float hgPhase = (1.0f / (4.0f * PI)) * ((1.0f - g2) / max(denom * sqrt(max(denom, 0.0001f)), 0.0001f));

		float rawPeak = 4.0f * PI * hgPhase;
		float softPeak = rawPeak / (1.0f + rawPeak / 3.5f);

		float sunIntensity = max(g_Lights[0].colorIntensity.a, 0.0f);
		float3 sunInscatter = g_Lights[0].colorIntensity.rgb * sunIntensity * softPeak * g_Pass.fogParams.y;
		float twilightFade = smoothstep(-0.02f, 0.08f, sunDir.y);
		fogCol += sunInscatter * twilightFade;
	}

	return lerp(litColor, fogCol, fogFactor);
}

#endif // DARKFIRE_ATMOSPHERE_HLSLI
