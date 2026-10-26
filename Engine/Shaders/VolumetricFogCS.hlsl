#include "Common.hlsli"
#include "Shadows.hlsli"
#include "Shared/Bindings.h"

RWTexture3D<float4> g_OutputScatteringExtinction : register(u0);
Texture3D<float4>   g_TexturePrevHistory         : register(t0);

cbuffer CBVolumetricFogBuffer : HLSL_REG_B(DF_SLOT_CB_VOLUMETRIC_FOG)
{
	CBVolumetricFog g_Fog;
};

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	if (id.x >= (uint)g_Fog.volumeGridSize.x || id.y >= (uint)g_Fog.volumeGridSize.y || id.z >= (uint)g_Fog.volumeGridSize.z)
	{
		return;
	}

	// View-space slice depth reconstruction: logarithmic distribution
	float sliceT = (float(id.z) + 0.5f) / g_Fog.volumeGridSize.z;
	float viewZ = g_Fog.fogDepths.x * pow(max(g_Fog.fogDepths.y / g_Fog.fogDepths.x, 1.0f), sliceT);

	// Reconstruct world space cell position analytically from view coordinates
	float2 ndcXY = (float2(id.xy) + 0.5f) / g_Fog.volumeGridSize.xy * float2(2.0f, -2.0f) + float2(-1.0f, 1.0f);
	float3 viewPos;
	viewPos.x = ndcXY.x * viewZ / max(g_View.projMatrix[0][0], 0.0001f);
	viewPos.y = ndcXY.y * viewZ / max(g_View.projMatrix[1][1], 0.0001f);
	viewPos.z = viewZ;
	float3 worldPos = mul(float4(viewPos, 1.0f), g_View.invViewMatrix).xyz;

	float3 camToCell = worldPos - g_View.cameraPosition;
	float dist = length(camToCell);
	float3 V = camToCell / max(dist, 0.0001f);

	// Exponential height density
	float height = worldPos.y - g_Fog.fogParams.w;
	float density = g_Fog.fogParams.x * exp(-max(g_Fog.fogParams.y, 0.0001f) * height);
	density = max(density, 0.0f);

	float3 scattering = float3(0.0f, 0.0f, 0.0f);

	// 1. Directional Sun In-Scattering
	if (g_Pass.lightCount > 0 && (int)g_Lights[0].positionType.w == 0)
	{
		float3 L = safeNormalize(-g_Lights[0].directionRange.xyz);
		float shadowVis = sampleVolumetricShadow(worldPos);

		// Henyey-Greenstein Mie aerosol phase function (g = 0.76)
		float cosTheta = dot(V, L);
		float g = 0.76f;
		float g2 = g * g;
		float denom = 1.0f + g2 - 2.0f * g * cosTheta;
		float hgPhase = (1.0f / (4.0f * PI)) * ((1.0f - g2) / max(denom * sqrt(max(denom, 0.0001f)), 0.0001f));
		float softPeak = (4.0f * PI * hgPhase) / (1.0f + (4.0f * PI * hgPhase) / 3.5f);

		float sunIntensity = max(g_Lights[0].colorIntensity.a, 0.0f);
		float3 sunRadiance = g_Lights[0].colorIntensity.rgb * sunIntensity;
		scattering += sunRadiance * (density * softPeak * g_Fog.fogParams.z * shadowVis);
	}

	// 2. Dynamic Point & Spot Lights In-Scattering (Clustered or top dynamic lights)
	for (uint i = 1; i < min(g_Pass.lightCount, 16u); ++i)
	{
		LightData light = g_Lights[i];
		int lType = (int)light.positionType.w;
		float3 lightPos = light.positionType.xyz;
		float3 toLight = lightPos - worldPos;
		float lightDist = length(toLight);
		float range = light.directionRange.w;

		if (lightDist < range && range > 0.001f)
		{
			float3 L = toLight / lightDist;
			float atten = saturate(1.0f - lightDist / range);
			atten = atten * atten;

			if (lType == 2) // Spot light cone attenuation
			{
				float3 spotDir = light.directionRange.xyz;
				float cosAngle = dot(-L, spotDir);
				float cosInner = light.spotAnglesPad.x;
				float cosOuter = light.spotAnglesPad.y;
				float spotAtten = saturate((cosAngle - cosOuter) / max(cosInner - cosOuter, 0.0001f));
				atten *= spotAtten * spotAtten;
			}

			// Isotropic forward-biased phase for local lights (g = 0.40)
			float cosTheta = dot(V, L);
			float g = 0.40f;
			float g2 = g * g;
			float denom = 1.0f + g2 - 2.0f * g * cosTheta;
			float localPhase = (1.0f / (4.0f * PI)) * ((1.0f - g2) / max(denom * sqrt(max(denom, 0.0001f)), 0.0001f));

			float3 lightRadiance = light.colorIntensity.rgb * light.colorIntensity.a * atten;
			scattering += lightRadiance * (density * localPhase * 0.5f);
		}
	}

	// Extinction coefficient = scattering + absorption
	float extinction = density * 1.25f;

	// Temporal History Reprojection
	float4 prevClip = mul(float4(worldPos, 1.0f), g_Fog.prevViewProj);
	float3 prevUVW = float3(
		(prevClip.x / prevClip.w) * 0.5f + 0.5f,
		(-prevClip.y / prevClip.w) * 0.5f + 0.5f,
		log(max(prevClip.w, g_Fog.fogDepths.x) / g_Fog.fogDepths.x) / max(log(g_Fog.fogDepths.y / g_Fog.fogDepths.x), 0.0001f)
	);

	float4 currentScatterExt = float4(scattering, extinction);
	if (all(prevUVW >= 0.001f) && all(prevUVW <= 0.999f))
	{
		float4 prevSample = g_TexturePrevHistory.SampleLevel(g_SamplerLinearClamp, prevUVW, 0);
		currentScatterExt = lerp(currentScatterExt, prevSample, g_Fog.fogDepths.z);
	}

	g_OutputScatteringExtinction[id] = currentScatterExt;
}
