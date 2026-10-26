#include "Shared/Bindings.h"
#include "Common.hlsli"

// Output updated radiance atlas
RWTexture2D<float4> g_OutputRadianceAtlas  : register(u0);

float2 octWrap(float2 v)
{
	return (1.0f - abs(v.yx)) * (v.xy >= 0.0f ? 1.0f : -1.0f);
}

float3 octDecode(float2 f)
{
	f = f * 2.0f - 1.0f;
	float3 n = float3(f.x, f.y, 1.0f - abs(f.x) - abs(f.y));
	float t = saturate(-n.z);
	n.xy += (n.xy >= 0.0f ? -t : t);
	return normalize(n);
}

// 8x8 texels per probe radiance atlas
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID)
{
	uint probeIndex = groupID.z;
	uint3 probeCounts = uint3(g_DDGI.gridCountsExtents.xyz);
	if (probeCounts.x == 0 || probeCounts.y == 0 || probeCounts.z == 0) return;

	// Compute probe 3D grid coordinate (X, Y, Z)
	uint probeZ = probeIndex / (probeCounts.x * probeCounts.y);
	uint rem = probeIndex % (probeCounts.x * probeCounts.y);
	uint probeY = rem / probeCounts.x;
	uint probeX = rem % probeCounts.x;

	if (probeX >= probeCounts.x || probeY >= probeCounts.y || probeZ >= probeCounts.z) return;

	// Calculate world position of this probe
	float3 gridOrigin = g_DDGI.gridOriginSpacing.xyz;
	float spacing = g_DDGI.gridOriginSpacing.w;
	float3 probeWorldPos = gridOrigin + (float3(probeX, probeY, probeZ) - float3(probeCounts) * 0.5f + 0.5f) * spacing;

	// Texel within the 8x8 octahedral footprint
	uint2 texelInProbe = groupThreadID.xy; // [0..7, 0..7]
	float2 octUV = (float2(texelInProbe) + 0.5f) / 8.0f;
	float3 rayDir = octDecode(octUV);

	// Gather incoming indirect bounced radiance along ray direction (bounced sun + ambient sky)
	float3 sunDir = float3(0.0f, 1.0f, 0.0f);
	float3 sunColor = float3(1.0f, 0.95f, 0.88f) * 4.2f;
	if (g_Pass.lightCount > 0 && (int)g_Lights[0].positionType.w == 0)
	{
		sunDir = safeNormalize(-g_Lights[0].directionRange.xyz);
		sunColor = g_Lights[0].colorIntensity.rgb * g_Lights[0].colorIntensity.a;
	}

	// Geometry normal reflecting light along rayDir into probe: N_surf = -rayDir
	// Surface illuminated by sun faces sun: dot(N_surf, sunDir) > 0 => dot(-rayDir, sunDir) > 0
	float sunFacing = max(dot(-rayDir, sunDir), 0.0f);

	// Ground/floor upward indirect bounce: downward probe rays (-rayDir.y > 0) receive bounce from sun-lit horizontal surfaces
	float groundBounce = max(-rayDir.y, 0.0f) * max(sunDir.y, 0.0f);

	// Multi-bounce diffuse ambient fill so shadowed crevices receive gentle warm stone ambient inter-reflection
	float multiBounce = 0.38f * max(sunDir.y, 0.2f);
	float directionalBounce = sunFacing * 0.65f + groundBounce * 0.55f;
	float indirectBounce = saturate(multiBounce + directionalBounce);
	// Data-driven ground/ambient bounce tint from environment pass settings
	float3 groundAlbedo = max(g_Pass.skyGroundColor.rgb, float3(0.05f, 0.05f, 0.05f));
	float3 bouncedSun = sunColor * groundAlbedo * indirectBounce * (INV_PI * 0.5f);

	// Probe ray sky sampling: rays facing upward sample sky radiance smoothly based on elevation
	float skyVisibility = saturate(rayDir.y);
	float phi = atan2(rayDir.z, rayDir.x);
	float theta = acos(clamp(rayDir.y, -1.0f, 1.0f));
	float2 eqUV = float2(phi * (0.5f * INV_PI) + 0.5f, theta * INV_PI);
	float3 skyLight = g_TextureSkyEquirect.SampleLevel(g_SamplerLinearWrap, eqUV, 4.0f).rgb * g_Pass.ambientLightColor.a * skyVisibility;
	float3 gatheredRadiance = (bouncedSun + skyLight) * g_DDGI.ddgiParams.z;

	// Global atlas pixel position
	// Probes arranged in 2D atlas grid: numProbesPerRow = 32 (probeWidth = 8 texels)
	uint numProbesPerRow = 32;
	uint atlasTileX = probeIndex % numProbesPerRow;
	uint atlasTileY = probeIndex / numProbesPerRow;
	uint2 globalAtlasCoord = uint2(atlasTileX * 8 + texelInProbe.x, atlasTileY * 8 + texelInProbe.y);

	// Direct gathered probe radiance
	g_OutputRadianceAtlas[globalAtlasCoord] = float4(gatheredRadiance, 1.0f);
}
