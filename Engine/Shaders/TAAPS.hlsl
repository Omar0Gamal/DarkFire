//=============================================================================
// TAAPS.hlsl - Cinematic Temporal Anti-Aliasing (TAA)
// Karis Ray-AABB Clipping + Depth-Reconstructed Motion Vectors + Karis Weighting
//=============================================================================

#pragma pack_matrix(row_major)

cbuffer CBTAA : register(b0)
{
	float4 g_JitterAndBlend; // xy = current jitter offset (UV space), z = blend factor (e.g. 0.88), w = sharpness
	float4 g_Resolution;     // xy = resolution, zw = 1.0 / resolution
};

#include "Shared/Bindings.h"

cbuffer CBPerViewBuffer : register(b1)
{
	CBPerView g_View;
};

Texture2D   g_CurrentColor  : register(t0);
Texture2D   g_HistoryColor  : register(t1);
Texture2D   g_Velocity      : register(t2);
Texture2D   g_Depth         : register(t3);

SamplerState g_LinearClamp : register(s0);
SamplerState g_PointClamp  : register(s1);

struct PSInput
{
	float4 positionCS : SV_Position;
	float2 texcoord   : TEXCOORD0;
};

// RGB to YCoCg color space
float3 RGBToYCoCg(float3 rgb)
{
	float y  =  0.25f * rgb.r + 0.5f * rgb.g + 0.25f * rgb.b;
	float co =  0.50f * rgb.r                - 0.50f * rgb.b;
	float cg = -0.25f * rgb.r + 0.5f * rgb.g - 0.25f * rgb.b;
	return float3(y, co, cg);
}

// YCoCg to RGB color space
float3 YCoCgToRGB(float3 ycocg)
{
	float y  = ycocg.x;
	float co = ycocg.y;
	float cg = ycocg.z;

	float r = y + co - cg;
	float g = y + cg;
	float b = y - co - cg;
	return max(float3(0.0f, 0.0f, 0.0f), float3(r, g, b));
}

// Ray-AABB intersection in YCoCg color space (Karis / Epic Games method)
// Instead of independent clamping which distorts hue, clips history point along ray towards neighborhood center
float3 ClipToAABB(float3 history, float3 boxMin, float3 boxMax)
{
	float3 center = 0.5f * (boxMin + boxMax);
	float3 extents = 0.5f * (boxMax - boxMin) + 1e-5f;
	float3 offset = history - center;
	float3 repeat = abs(offset / extents);
	float maxComponent = max(repeat.x, max(repeat.y, repeat.z));
	if (maxComponent > 1.0f)
	{
		return center + offset / maxComponent;
	}
	return history;
}

// 5-Tap Catmull-Rom Bicubic Filter (Unreal Engine 4/5 method)
// Eliminates bilinear blur during motion and maintains razor-sharp subpixel details
float3 SampleHistoryCatmullRom(Texture2D tex, SamplerState smp, float2 uv, float2 texSize)
{
	float2 samplePos = uv * texSize;
	float2 tc = floor(samplePos - 0.5f) + 0.5f;
	float2 f = samplePos - tc;
	float2 f2 = f * f;
	float2 f3 = f2 * f;

	float2 w0 = f2 - 0.5f * (f3 + f);
	float2 w1 = 1.5f * f3 - 2.5f * f2 + 1.0f;
	float2 w3 = 0.5f * (f3 - f2);
	float2 w2 = 1.0f - w0 - w1 - w3;

	float2 w12 = w1 + w2;
	float2 tc12 = tc + w2 / max(w12, 1e-5f);

	float3 col = float3(0.0f, 0.0f, 0.0f);
	col += tex.SampleLevel(smp, float2(tc12.x, tc.y - 1.0f) / texSize, 0).rgb * (w12.x * w0.y);
	col += tex.SampleLevel(smp, float2(tc.x - 1.0f, tc12.y) / texSize, 0).rgb * (w0.x * w12.y);
	col += tex.SampleLevel(smp, float2(tc12.x, tc12.y)     / texSize, 0).rgb * (w12.x * w12.y);
	col += tex.SampleLevel(smp, float2(tc.x + 2.0f, tc12.y) / texSize, 0).rgb * (w3.x * w12.y);
	col += tex.SampleLevel(smp, float2(tc12.x, tc.y + 2.0f) / texSize, 0).rgb * (w12.x * w3.y);

	return max(float3(0.0f, 0.0f, 0.0f), col);
}

float4 main(PSInput input) : SV_Target0
{
	float2 uv = input.texcoord;
	float2 invRes = g_Resolution.zw;

	// 1. Closest-Depth Velocity Dilation (3x3 neighborhood)
	// In Reversed-Z, larger depth value is CLOSER to the camera.
	float maxDepth = -1.0f;
	int2 bestOffset = int2(0, 0);

	[unroll]
	for (int dy = -1; dy <= 1; ++dy)
	{
		[unroll]
		for (int dx = -1; dx <= 1; ++dx)
		{
			float d = g_Depth.SampleLevel(g_PointClamp, uv, 0, int2(dx, dy)).r;
			if (d > maxDepth)
			{
				maxDepth = d;
				bestOffset = int2(dx, dy);
			}
		}
	}

	float2 sampleUV = uv + float2(bestOffset) * invRes;
	float2 geomVel = g_Velocity.SampleLevel(g_PointClamp, sampleUV, 0).xy;

	bool isCameraMoving = (g_JitterAndBlend.w > 0.5f);
	float2 velocity = float2(0.0f, 0.0f);
	float speed = 0.0f;

	if (isCameraMoving)
	{
		velocity = geomVel;
		// 2. Camera-Motion Velocity Fallback from Depth Buffer
		// If velocity texture is empty (sky or unwritten forward geometry) during camera motion, reconstruct camera reprojection
		if (dot(velocity, velocity) < 1e-7f && maxDepth > 0.0f)
		{
			float depthVal = maxDepth;
			float4 clipPos = float4(sampleUV.x * 2.0f - 1.0f, (1.0f - sampleUV.y) * 2.0f - 1.0f, depthVal, 1.0f);
			float4 worldPos = mul(clipPos, g_View.invViewProjMatrix);
			worldPos /= max(worldPos.w, 1e-5f);

			float4 prevClip = mul(worldPos, g_View.prevViewProjMatrix);
			prevClip.xyz /= max(prevClip.w, 1e-5f);
			float2 prevUV = float2(prevClip.x * 0.5f + 0.5f, -prevClip.y * 0.5f + 0.5f);

			velocity = (sampleUV - prevUV) - (g_View.jitterOffset.xy - g_View.prevJitterOffset.xy);
		}
		speed = length(velocity * g_Resolution.xy);
	}
	else
	{
		// Camera is stationary: if an object has dynamic motion (e.g. physics body), track it
		float geomSpeed = length(geomVel * g_Resolution.xy);
		if (geomSpeed > 0.05f)
		{
			velocity = geomVel;
			speed = geomSpeed;
		}
		else
		{
			// Pixel is strictly stationary: zero velocity, zero subpixel jitter
			velocity = float2(0.0f, 0.0f);
			speed = 0.0f;
		}
	}

	// Exact TAA History Reprojection
	float2 historyUV = uv - velocity;
	bool offScreen = (historyUV.x < 0.0f || historyUV.x > 1.0f || historyUV.y < 0.0f || historyUV.y > 1.0f);

	// 3. Sample 3x3 Neighborhood in Current Frame (YCoCg Space, Point Sampled for crispness)
	float3 m1 = float3(0.0f, 0.0f, 0.0f);
	float3 m2 = float3(0.0f, 0.0f, 0.0f);
	float3 minColor = float3(1e6f, 1e6f, 1e6f);
	float3 maxColor = float3(-1e6f, -1e6f, -1e6f);

	float3 centerColor = g_CurrentColor.SampleLevel(g_PointClamp, uv, 0).rgb;
	float3 centerYCoCg = RGBToYCoCg(centerColor);

	[unroll]
	for (int y = -1; y <= 1; ++y)
	{
		[unroll]
		for (int x = -1; x <= 1; ++x)
		{
			float3 c = g_CurrentColor.SampleLevel(g_PointClamp, uv + float2(x, y) * invRes, 0).rgb;
			float3 ycocg = RGBToYCoCg(c);
			m1 += ycocg;
			m2 += ycocg * ycocg;
			minColor = min(minColor, ycocg);
			maxColor = max(maxColor, ycocg);
		}
	}

	float3 mu = m1 / 9.0f;
	float3 sigma = sqrt(max(float3(0.0f, 0.0f, 0.0f), (m2 / 9.0f) - (mu * mu)));

	// Variance clipping box: relaxed (2.5 * sigma) when stationary to eliminate edge vibration,
	// and tighter (1.5 * sigma) when moving to eliminate ghost trails
	float boxSigma = (speed < 0.001f) ? 2.5f : 1.5f;
	float3 boxMin = max(minColor, mu - boxSigma * sigma);
	float3 boxMax = min(maxColor, mu + boxSigma * sigma);

	// 4. Sample and Clip Reprojected History
	// When stationary (speed < 0.001), fetch exact history pixel with g_PointClamp (ZERO bilinear accumulation blur)
	// When in motion, use 5-tap Catmull-Rom bicubic reconstruction to avoid linear softness
	float3 historyColor;
	if (speed < 0.001f)
	{
		historyColor = g_HistoryColor.SampleLevel(g_PointClamp, uv, 0).rgb;
	}
	else
	{
		historyColor = SampleHistoryCatmullRom(g_HistoryColor, g_LinearClamp, historyUV, g_Resolution.xy);
	}

	float3 historyYCoCg = RGBToYCoCg(historyColor);
	float3 clampedHistoryYCoCg = ClipToAABB(historyYCoCg, boxMin, boxMax);
	float3 clampedHistory = YCoCgToRGB(clampedHistoryYCoCg);

	// 5. Anti-Ghosting Dynamic Blend
	float baseBlend = offScreen ? 0.0f : ((speed < 0.001f) ? 0.94f : g_JitterAndBlend.z);

	float motionRejection = saturate(speed * 0.20f);
	float blend = lerp(baseBlend, 0.15f, motionRejection);

	// Karis Luminance Weighting in YCoCg space to suppress bright streak trails and fireflies
	float lumaCurrent = max(centerYCoCg.x, 0.0f);
	float lumaHistory = max(clampedHistoryYCoCg.x, 0.0f);
	float wCurrent = 1.0f / (1.0f + lumaCurrent);
	float wHistory = 1.0f / (1.0f + lumaHistory);
	float finalBlend = (blend * wHistory) / max(wCurrent * (1.0f - blend) + blend * wHistory, 1e-4f);

	if (speed < 0.001f)
	{
		finalBlend = max(finalBlend, 0.93f);
	}

	float3 result = lerp(centerColor, clampedHistory, finalBlend);

	return float4(max(float3(0.0f, 0.0f, 0.0f), result), 1.0f);
}
