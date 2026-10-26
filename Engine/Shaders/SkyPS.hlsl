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

// Earth Rayleigh & Mie physical scattering constants
static const float3 kRayleighScattering = float3(5.802e-6f, 13.558e-6f, 33.100e-6f); // Wavelength-dependent (m^-1)
static const float  kRayleighScaleHeight = 8000.0f; // 8 km
static const float3 kMieScattering = float3(3.996e-6f, 3.996e-6f, 3.996e-6f);      // Aerosol scattering (m^-1)
static const float  kMieScaleHeight = 1200.0f;     // 1.2 km
static const float  kMieG = 0.78f;                 // Mie forward-scattering asymmetry

// Triangular dither noise to eliminate gradient banding (B7)
float SkyDither(float2 pixelCoord)
{
	float r0 = frac(sin(dot(pixelCoord, float2(12.9898f, 78.233f))) * 43758.5453f);
	float r1 = frac(sin(dot(pixelCoord + 0.5f, float2(39.346f, 11.135f))) * 43758.5453f);
	return (r0 + r1 - 1.0f) * (0.5f / 255.0f);
}

// Rayleigh phase function
float RayleighPhase(float cosTheta)
{
	return (3.0f / (16.0f * PI)) * (1.0f + cosTheta * cosTheta);
}

// Henyey-Greenstein Mie phase function
float MiePhase(float cosTheta, float g)
{
	float g2 = g * g;
	float denom = 1.0f + g2 - 2.0f * g * cosTheta;
	return (3.0f * (1.0f - g2) / (8.0f * PI * (2.0f + g2))) * ((1.0f + cosTheta * cosTheta) / max(denom * sqrt(max(denom, 0.0001f)), 0.0001f));
}

// Analytic sun transmission (airmass extinction)
float3 ComputeSunExtinction(float sunElevation)
{
	// Relative optical path length (Kasten & Young 1989 airmass model)
	float sinElev = max(sunElevation, 0.0f);
	float airmass = 1.0f / (sinElev + 0.15f * pow(max(sunElevation + 0.05f, 0.001f), -1.253f));
	float3 opticalDepth = (kRayleighScattering * kRayleighScaleHeight + kMieScattering * kMieScaleHeight) * airmass;
	return exp(-opticalDepth);
}

PSOutput main(FullscreenVSOutput input)
{
	// Unproject screen UV to world ray direction (reversed-Z depth = 0.0 is the far plane)
	float2 ndc = input.texcoord * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
	float4 clipPos = float4(ndc, 0.0f, 1.0f);
	float4 worldPosH = mul(clipPos, g_View.invViewProjMatrix);
	float3 worldPos = worldPosH.xyz / worldPosH.w;
	float3 rayDir = normalize(worldPos - g_View.cameraPosition);

	float elevation = rayDir.y;
	float3 skyColor = float3(0.0f, 0.0f, 0.0f);

	if (g_Environment.skyType == 1u) // SkyboxTexture (Equirectangular HDR)
	{
		float2 eqUV = float2(atan2(rayDir.z, rayDir.x) * (0.5f * INV_PI) + 0.5f, 0.5f - asin(clamp(rayDir.y, -1.0f, 1.0f)) * INV_PI);
		skyColor = g_TextureSkyEquirect.SampleLevel(g_SamplerLinearWrap, eqUV, 0).rgb * g_Environment.skyIntensity;
	}
	else if (g_Environment.skyType == 2u) // CustomColor
	{
		skyColor = g_Environment.skyZenith.rgb * g_Environment.skyIntensity;
	}
	else // Procedural Physical Sky (Rayleigh + Mie Scattering & Sun In-scattering)
	{
		float3 sunDir = (g_Environment.sunDirection.w > 0.5f)
			? normalize(g_Environment.sunDirection.xyz)
			: float3(0.0f, 1.0f, 0.0f);

		float sunElevation = sunDir.y;
		float3 sunTransmittance = ComputeSunExtinction(sunElevation);
		float3 directSunColor = g_Environment.sunColor.rgb * sunTransmittance;

		float cosTheta = dot(rayDir, sunDir);

		if (elevation >= 0.0f)
		{
			// Upper atmosphere optical path
			float airmassV = 1.0f / max(elevation + 0.04f, 0.02f);
			float3 tauView = (kRayleighScattering * kRayleighScaleHeight + kMieScattering * kMieScaleHeight) * airmassV;
			float3 extinctionV = exp(-tauView);
			float3 inscatterFactor = (1.0f - extinctionV);

			float pR = RayleighPhase(cosTheta);
			float pM = MiePhase(cosTheta, kMieG);

			// Single-scattered atmospheric radiance
			float3 rayleighRadiance = kRayleighScattering * kRayleighScaleHeight * pR;
			float3 mieRadiance = kMieScattering * kMieScaleHeight * pM;
			float3 physicalSky = (rayleighRadiance + mieRadiance) * directSunColor * inscatterFactor * 15.0f;

			// Blend with authored zenith & horizon colors to preserve artist direction
			float t = pow(clamp(elevation, 0.0f, 1.0f), 0.5f);
			float3 authoredSky = lerp(g_Environment.skyHorizon.rgb, g_Environment.skyZenith.rgb, t);

			// Warm horizon tinting at dusk
			float sunsetGlow = saturate(1.0f - abs(sunElevation) * 3.0f);
			float3 sunsetColor = float3(1.0f, 0.45f, 0.15f) * saturate(dot(rayDir.xz, sunDir.xz));
			authoredSky += sunsetColor * (sunsetGlow * 0.45f * exp(-elevation * 5.0f));

			skyColor = (authoredSky * 0.45f + physicalSky * 0.55f) * g_Environment.skyIntensity;
		}
		else
		{
			// Below horizon ground transition
			float t = pow(clamp(-elevation * 2.0f, 0.0f, 1.0f), 0.5f);
			float groundSunLight = saturate(sunElevation * 0.7f + 0.3f);
			float3 groundLit = g_Environment.groundColor.rgb * groundSunLight;
			skyColor = lerp(g_Environment.skyHorizon.rgb * 0.6f, groundLit, t) * g_Environment.skyIntensity;
		}

		// Sun disk and physical corona
		if (g_Environment.sunDirection.w > 0.5f && sunElevation > -0.05f)
		{
			float sunDiskCos = g_Environment.sunColor.a > 0.0f ? g_Environment.sunColor.a : 0.9995f;
			if (cosTheta > sunDiskCos)
			{
				float diskT = (cosTheta - sunDiskCos) / max(1.0f - sunDiskCos, 0.00001f);
				float limbDarkening = sqrt(saturate(diskT));
				skyColor += directSunColor * (10.0f + limbDarkening * 25.0f);
			}
			else
			{
				// Forward-scattering Mie corona surrounding the sun disk
				float coronaCos = sunDiskCos - 0.035f;
				if (cosTheta > coronaCos)
				{
					float coronaT = (cosTheta - coronaCos) / (sunDiskCos - coronaCos);
					float coronaGlow = pow(saturate(coronaT), 2.5f);
					skyColor += directSunColor * coronaGlow * 1.5f;
				}
			}
		}
	}

	// High-frequency triangular dither to eliminate 8-bit / 16-bit banding (B7)
	skyColor += SkyDither(input.positionCS.xy);

	PSOutput output;
	output.color = float4(max(skyColor, 0.0f), 1.0f);
	output.entityId = 0;
	return output;
}
