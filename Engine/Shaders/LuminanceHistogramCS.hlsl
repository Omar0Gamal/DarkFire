#include "Shared/Bindings.h"

Texture2D<float4> g_TextureHDR : register(t0);
RWStructuredBuffer<uint> g_HistogramBuffer : register(u0);

cbuffer CBHistogramBuffer : register(b0)
{
	CBLuminanceHistogram g_Histogram;
};

groupshared uint s_histogram[64];

[numthreads(16, 16, 1)]
void main(uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID, uint groupIndex : SV_GroupIndex)
{
	if (groupIndex < 64)
	{
		s_histogram[groupIndex] = 0;
	}
	GroupMemoryBarrierWithGroupSync();

	uint2 pixelCoord = groupID.xy * 16u + groupThreadID.xy;
	if (pixelCoord.x < g_Histogram.dimensions.x && pixelCoord.y < g_Histogram.dimensions.y)
	{
		float3 color = g_TextureHDR[pixelCoord].rgb;
		float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
		if (luma > 0.0001f)
		{
			float logLuma = log2(luma);
			float norm = saturate((logLuma - g_Histogram.evRange.x) * g_Histogram.evRange.z);
			uint binIndex = clamp(uint(norm * 64.0f), 0u, 63u);
			InterlockedAdd(s_histogram[binIndex], 1u);
		}
	}
	GroupMemoryBarrierWithGroupSync();

	if (groupIndex < 64)
	{
		InterlockedAdd(g_HistogramBuffer[groupIndex], s_histogram[groupIndex]);
	}
}
