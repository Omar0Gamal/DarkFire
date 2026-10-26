#include "Shared/Bindings.h"

RWTexture3D<float4> g_OutputIntegratedFog       : register(u0);
Texture3D<float4>   g_TextureScatteringExtinction : register(t0);

cbuffer CBVolumetricFogIntegrateBuffer : register(b0)
{
	CBVolumetricFogIntegrate g_Integrate;
};

[numthreads(8, 8, 1)]
void main(uint3 id : SV_DispatchThreadID)
{
	if (id.x >= (uint)g_Integrate.volumeGridSize.x || id.y >= (uint)g_Integrate.volumeGridSize.y)
	{
		return;
	}

	float3 accumRadiance = float3(0.0f, 0.0f, 0.0f);
	float accumTransmittance = 1.0f;
	float prevSliceZ = g_Integrate.fogDepths.x;

	for (uint z = 0; z < (uint)g_Integrate.volumeGridSize.z; ++z)
	{
		float sliceT = (float(z) + 0.5f) / g_Integrate.volumeGridSize.z;
		float curSliceZ = g_Integrate.fogDepths.x * pow(max(g_Integrate.fogDepths.y / g_Integrate.fogDepths.x, 1.0f), sliceT);
		float stepDist = max(curSliceZ - prevSliceZ, 0.001f);
		prevSliceZ = curSliceZ;

		float4 scatterExt = g_TextureScatteringExtinction[uint3(id.xy, z)];
		float3 inScatter = scatterExt.rgb;
		float extinction = max(scatterExt.a, 0.00001f);

		float stepTransmittance = exp(-extinction * stepDist);
		float3 stepIntegrated = inScatter * ((1.0f - stepTransmittance) / extinction);

		accumRadiance += stepIntegrated * accumTransmittance;
		accumTransmittance *= stepTransmittance;

		g_OutputIntegratedFog[uint3(id.xy, z)] = float4(accumRadiance, accumTransmittance);
	}
}
