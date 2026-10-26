#include "Shared/Bindings.h"

Texture2D<float2>   g_TextureTileMax     : register(t0);
RWTexture2D<float2> g_OutputNeighborMax  : register(u0);

cbuffer CBNeighborMax : register(b0)
{
	uint4 g_TileDimensions; // x = tileCountX, y = tileCountY, zw = pad
};

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	if (id.x >= g_TileDimensions.x || id.y >= g_TileDimensions.y)
	{
		return;
	}

	float2 maxVel = float2(0.0f, 0.0f);
	float maxMagSq = 0.0f;

	[unroll]
	for (int dy = -1; dy <= 1; ++dy)
	{
		[unroll]
		for (int dx = -1; dx <= 1; ++dx)
		{
			int2 coord = clamp(int2(id.xy) + int2(dx, dy), int2(0, 0), int2(g_TileDimensions.xy) - int2(1, 1));
			float2 v = g_TextureTileMax[coord];
			float magSq = dot(v, v);
			if (magSq > maxMagSq)
			{
				maxMagSq = magSq;
				maxVel = v;
			}
		}
	}

	g_OutputNeighborMax[id.xy] = maxVel;
}
