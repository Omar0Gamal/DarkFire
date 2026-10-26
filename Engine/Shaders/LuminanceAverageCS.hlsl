#include "Shared/Bindings.h"

RWStructuredBuffer<float> g_AdaptedLuminance : register(u0); // index 0 = adaptedLuma, index 1 = exposure
StructuredBuffer<uint>    g_HistogramBuffer  : register(t0);

cbuffer CBAverageBuffer : register(b0)
{
	CBLuminanceAverage g_Average;
};

groupshared uint s_hist[64];

[numthreads(64, 1, 1)]
void main(uint threadIdx : SV_GroupIndex)
{
	s_hist[threadIdx] = g_HistogramBuffer[threadIdx];
	GroupMemoryBarrierWithGroupSync();

	if (threadIdx == 0)
	{
		uint totalPixels = g_Average.pixelCount.x;
		uint lowCutoff = uint(float(totalPixels) * 0.05f);
		uint highCutoff = uint(float(totalPixels) * 0.95f);

		uint cumulative = 0;
		float weightedSum = 0.0f;
		float validPixelCount = 0.0f;

		for (uint i = 0; i < 64; ++i)
		{
			uint count = s_hist[i];
			uint nextCumulative = cumulative + count;

			if (nextCumulative > lowCutoff && cumulative < highCutoff)
			{
				uint validInBin = count;
				if (cumulative < lowCutoff) validInBin -= (lowCutoff - cumulative);
				if (nextCumulative > highCutoff) validInBin -= (nextCumulative - highCutoff);

				float ev = g_Average.evRange.x + (float(i) + 0.5f) / 64.0f * (g_Average.evRange.y - g_Average.evRange.x);
				weightedSum += float(validInBin) * ev;
				validPixelCount += float(validInBin);
			}
			cumulative = nextCumulative;
		}

		float avgEV = (validPixelCount > 0.0f) ? (weightedSum / validPixelCount) : 0.0f;
		float targetLuma = exp2(avgEV);

		float prevLuma = g_AdaptedLuminance[0];
		if (prevLuma <= 0.0001f) prevLuma = targetLuma;

		float speed = (targetLuma > prevLuma) ? g_Average.adaptationParams.y : g_Average.adaptationParams.z;
		float alpha = 1.0f - exp(-g_Average.adaptationParams.x * speed);
		float adaptedLuma = prevLuma + (targetLuma - prevLuma) * alpha;

		float targetExposure = g_Average.adaptationParams.w / max(adaptedLuma, 0.001f);
		targetExposure = clamp(targetExposure, g_Average.evRange.z, g_Average.evRange.w);

		g_AdaptedLuminance[0] = adaptedLuma;
		g_AdaptedLuminance[1] = targetExposure;
	}
}
