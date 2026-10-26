// DarkFire Engine - ConvolveIBLCS.hlsl
// GGX Importance-Sampled Environment Map Convolution Compute Shader (cs_5_0)

Texture2D<float4> g_InputEnv      : register(t0);
SamplerState      g_LinearSampler : register(s0);

cbuffer CBConvolve : register(b0)
{
	uint2 outputDims;
	float roughness;
	float pad;
};

RWTexture2D<float4> g_OutputMip   : register(u0);

static const float PI = 3.14159265358979323846f;
static const float TWO_PI = 6.28318530717958647692f;

float RadicalInverse_VdC(uint bits)
{
	bits = (bits << 16u) | (bits >> 16u);
	bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
	bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
	bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
	bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
	return float(bits) * 2.3283064365386963e-10f; // / 0x100000000
}

float2 Hammersley(uint i, uint N)
{
	return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float rough)
{
	float a = rough * rough;
	float phi = TWO_PI * Xi.x;
	float cosTheta = sqrt(saturate((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y)));
	float sinTheta = sqrt(saturate(1.0f - cosTheta * cosTheta));

	float3 H;
	H.x = cos(phi) * sinTheta;
	H.y = sin(phi) * sinTheta;
	H.z = cosTheta;

	float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
	float3 tangent = normalize(cross(up, N));
	float3 bitangent = cross(N, tangent);

	return normalize(tangent * H.x + bitangent * H.y + N * H.z);
}

float2 DirToEquirectUV(float3 dir)
{
	float phi = atan2(dir.z, dir.x);
	float theta = asin(clamp(dir.y, -1.0f, 1.0f));
	return float2(phi * (0.5f / PI) + 0.5f, 0.5f - theta / PI);
}

float3 EquirectUVToDir(float2 uv)
{
	float phi = (uv.x - 0.5f) * TWO_PI;
	float theta = (0.5f - uv.y) * PI;
	float cosTheta = cos(theta);
	return float3(cosTheta * cos(phi), sin(theta), cosTheta * sin(phi));
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	if (dispatchThreadId.x >= outputDims.x || dispatchThreadId.y >= outputDims.y)
	{
		return;
	}

	float2 uv = (float2(dispatchThreadId.xy) + 0.5f) / float2(outputDims);
	float3 N = normalize(EquirectUVToDir(uv));

	// If roughness is near zero, copy source directly
	if (roughness < 0.005f)
	{
		float4 src = g_InputEnv.SampleLevel(g_LinearSampler, uv, 0);
		g_OutputMip[dispatchThreadId.xy] = src;
		return;
	}

	float3 R = N;
	float3 V = R;

	float3 prefilteredColor = float3(0.0f, 0.0f, 0.0f);
	float totalWeight = 0.0f;

	const uint kNumSamples = 64u;
	for (uint i = 0u; i < kNumSamples; ++i)
	{
		float2 Xi = Hammersley(i, kNumSamples);
		float3 H = ImportanceSampleGGX(Xi, N, roughness);
		float3 L = normalize(2.0f * dot(V, H) * H - V);

		float NdotL = max(dot(N, L), 0.0f);
		if (NdotL > 0.0f)
		{
			float2 sampleUV = DirToEquirectUV(L);
			float3 envSample = g_InputEnv.SampleLevel(g_LinearSampler, sampleUV, 0).rgb;
			prefilteredColor += envSample * NdotL;
			totalWeight += NdotL;
		}
	}

	prefilteredColor = (totalWeight > 0.0f) ? (prefilteredColor / totalWeight) : g_InputEnv.SampleLevel(g_LinearSampler, uv, 0).rgb;
	g_OutputMip[dispatchThreadId.xy] = float4(prefilteredColor, 1.0f);
}
