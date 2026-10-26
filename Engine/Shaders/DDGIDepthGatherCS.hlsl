#include "Shared/Bindings.h"
#include "Common.hlsli"

// Output updated depth atlas
RWTexture2D<float2> g_OutputDepthAtlas  : register(u0);

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

// 16x16 texels per probe Chebyshev depth atlas
[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID, uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID)
{
	uint probeIndex = groupID.z;
	uint3 probeCounts = uint3(g_DDGI.gridCountsExtents.xyz);
	if (probeCounts.x == 0 || probeCounts.y == 0 || probeCounts.z == 0) return;

	uint probeZ = probeIndex / (probeCounts.x * probeCounts.y);
	uint rem = probeIndex % (probeCounts.x * probeCounts.y);
	uint probeY = rem / probeCounts.x;
	uint probeX = rem % probeCounts.x;

	if (probeX >= probeCounts.x || probeY >= probeCounts.y || probeZ >= probeCounts.z) return;

	uint2 texelInProbe = groupThreadID.xy; // [0..15, 0..15]
	float2 octUV = (float2(texelInProbe) + 0.5f) / 16.0f;
	float3 rayDir = octDecode(octUV);

	// Standard bounding probe distance (e.g. max range = probeSpacing * 2.5)
	float spacing = g_DDGI.gridOriginSpacing.w;
	float dist = spacing * 2.0f;
	float distSq = dist * dist;
	float2 newDepth = float2(dist, distSq);

	uint numProbesPerRow = 32;
	uint atlasTileX = probeIndex % numProbesPerRow;
	uint atlasTileY = probeIndex / numProbesPerRow;
	uint2 globalAtlasCoord = uint2(atlasTileX * 16 + texelInProbe.x, atlasTileY * 16 + texelInProbe.y);

	// Direct gathered Chebyshev depth moments
	g_OutputDepthAtlas[globalAtlasCoord] = newDepth;
}
