#include "Shared/Bindings.h"

Texture2D<float>   g_InputDepth  : register(t0);
RWTexture2D<float> g_OutputDepth : register(u0);

cbuffer CBHiZ : register(b0)
{
	float2 g_InvOutputSize; // 1.0 / OutputDimensions
	float2 g_Pad;
};

// Conservative Reversed-Z Depth Downsampling:
// Takes 4 texels from input depth and computes the minimum (farthest from near plane)
// to guarantee rays marching through the pyramid do not tunnel through foreground objects.
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
	uint2 outCoord = dispatchThreadID.xy;
	uint2 inCoord = outCoord * 2;

	float d0 = g_InputDepth.Load(int3(inCoord, 0));
	float d1 = g_InputDepth.Load(int3(inCoord + int2(1, 0), 0));
	float d2 = g_InputDepth.Load(int3(inCoord + int2(0, 1), 0));
	float d3 = g_InputDepth.Load(int3(inCoord + int2(1, 1), 0));

	float minDepth = min(min(d0, d1), min(d2, d3));
	g_OutputDepth[outCoord] = minDepth;
}
