//=============================================================================
// ClusterLightAssignCS.hlsl - Assigns Dynamic Lights to 16x16x24 Cluster Grid
//=============================================================================

struct ClusterAABB
{
	float4 minPoint; // View-space min (xyz)
	float4 maxPoint; // View-space max (xyz)
};

struct LightData
{
	float4 positionType;   // xyz = position, w = type (0 = Directional, 1 = Point, 2 = Spot)
	float4 directionRange; // xyz = direction, w = range
	float4 colorIntensity; // rgb = color, a = intensity
	float4 spotAnglesPad;  // x = inner angle cos, y = outer angle cos, zw = padding
};

cbuffer CBClusterAssign : register(b0)
{
	float4x4 g_ViewMatrix;
	uint     g_LightCount;
	uint     g_MaxIndices;
	float2   g_Pad0;
};

StructuredBuffer<ClusterAABB> g_ClusterAABBs : register(t0);
StructuredBuffer<LightData>   g_Lights       : register(t1);

// UAV outputs
RWStructuredBuffer<uint2> g_ClusterGrid              : register(u0); // (offset, count)
RWStructuredBuffer<uint>  g_ClusterLightIndices       : register(u1); // Compact list of light indices
RWStructuredBuffer<uint>  g_GlobalLightIndexCounter  : register(u2); // Atomic allocation counter

// Sphere vs AABB distance test
bool testSphereAABB(float3 center, float radius, float3 aabbMin, float3 aabbMax)
{
	float3 closest = clamp(center, aabbMin, aabbMax);
	float3 diff = center - closest;
	return dot(diff, diff) <= (radius * radius);
}

// Cone vs AABB test (approximate with bounding sphere + forward plane)
bool testConeAABB(float3 apex, float range, float3 dir, float outerCos, float3 aabbMin, float3 aabbMax)
{
	// 1. Check outer bounding sphere
	if (!testSphereAABB(apex, range, aabbMin, aabbMax))
	{
		return false;
	}

	// 2. Check plane through apex perpendicular to cone axis
	float3 boxCenter = (aabbMin + aabbMax) * 0.5f;
	float3 boxExtents = (aabbMax - aabbMin) * 0.5f;
	float3 toCenter = boxCenter - apex;

	float projected = dot(toCenter, dir);
	float r = dot(boxExtents, abs(dir));

	if (projected + r < 0.0f)
	{
		return false;
	}

	return true;
}

[numthreads(8, 8, 4)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint tileX  = dispatchThreadId.x;
	uint tileY  = dispatchThreadId.y;
	uint sliceZ = dispatchThreadId.z;

	if (tileX >= 16 || tileY >= 16 || sliceZ >= 24)
	{
		return;
	}

	uint clusterIndex = tileX + tileY * 16 + sliceZ * (16 * 16);
	ClusterAABB aabb = g_ClusterAABBs[clusterIndex];
	float3 bMin = aabb.minPoint.xyz;
	float3 bMax = aabb.maxPoint.xyz;

	uint localLightCount = 0;
	uint localIndices[64]; // Max 64 lights per cluster

	for (uint l = 0; l < g_LightCount && localLightCount < 64; ++l)
	{
		LightData light = g_Lights[l];
		uint lightType = uint(light.positionType.w);

		if (lightType == 0) // Directional Light
		{
			localIndices[localLightCount++] = l;
		}
		else if (lightType == 1) // Point Light
		{
			float3 posWS = light.positionType.xyz;
			float radius = light.directionRange.w;
			float3 posVS = mul(float4(posWS, 1.0f), g_ViewMatrix).xyz;

			if (testSphereAABB(posVS, radius, bMin, bMax))
			{
				localIndices[localLightCount++] = l;
			}
		}
		else if (lightType == 2) // Spot Light
		{
			float3 posWS = light.positionType.xyz;
			float range = light.directionRange.w;
			float3 dirWS = light.directionRange.xyz;
			float outerCos = light.spotAnglesPad.y;

			float3 posVS = mul(float4(posWS, 1.0f), g_ViewMatrix).xyz;
			float3 dirVS = normalize(mul(float4(dirWS, 0.0f), g_ViewMatrix).xyz);

			if (testConeAABB(posVS, range, dirVS, outerCos, bMin, bMax))
			{
				localIndices[localLightCount++] = l;
			}
		}
	}

	// Atomically allocate slot range in the global light indices buffer
	uint globalOffset = 0;
	if (localLightCount > 0)
	{
		InterlockedAdd(g_GlobalLightIndexCounter[0], localLightCount, globalOffset);
		// Check against buffer overflow
		if (globalOffset + localLightCount <= g_MaxIndices)
		{
			for (uint i = 0; i < localLightCount; ++i)
			{
				g_ClusterLightIndices[globalOffset + i] = localIndices[i];
			}
			g_ClusterGrid[clusterIndex] = uint2(globalOffset, localLightCount);
		}
		else
		{
			g_ClusterGrid[clusterIndex] = uint2(0, 0);
		}
	}
	else
	{
		g_ClusterGrid[clusterIndex] = uint2(0, 0);
	}
}
