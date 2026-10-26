#include "Shared/Bindings.h"

Texture2D<float2> g_TextureVelocity : register(t0);
RWTexture2D<float2> g_OutputTileMax  : register(u0);

cbuffer CBTileMax : register(b0)
{
	uint4 g_Dimensions; // x = screenWidth, y = screenHeight, z = tileWidth, w = tileHeight
};

groupshared float2 s_tileVelocities[400]; // 20x20 = 400 threads
groupshared float  s_tileMagnitudes[400];

[numthreads(20, 20, 1)]
void main(uint3 groupID : SV_GroupID, uint3 groupThreadID : SV_GroupThreadID, uint groupIndex : SV_GroupIndex)
{
	uint2 pixelCoord = groupID.xy * 20u + groupThreadID.xy;
	float2 vel = float2(0.0f, 0.0f);
	if (pixelCoord.x < g_Dimensions.x && pixelCoord.y < g_Dimensions.y)
	{
		vel = g_TextureVelocity[pixelCoord];
	}

	float magSq = dot(vel, vel);
	s_tileVelocities[groupIndex] = vel;
	s_tileMagnitudes[groupIndex] = magSq;
	GroupMemoryBarrierWithGroupSync();

	// Parallel reduction within 20x20 tile
	[unroll]
	for (uint s = 256; s > 0; s >>= 1)
	{
		if (groupIndex < s && (groupIndex + s) < 400)
		{
			if (s_tileMagnitudes[groupIndex + s] > s_tileMagnitudes[groupIndex])
			{
				s_tileMagnitudes[groupIndex] = s_tileMagnitudes[groupIndex + s];
				s_tileVelocities[groupIndex] = s_tileVelocities[groupIndex + s];
			}
		}
		GroupMemoryBarrierWithGroupSync();
	}

	if (groupIndex == 0)
	{
		g_OutputTileMax[groupID.xy] = s_tileVelocities[0];
	}
}
