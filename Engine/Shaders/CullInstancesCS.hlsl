//=============================================================================
// CullInstancesCS.hlsl - High-Performance GPU-Driven Frustum & Cascade Culling
//=============================================================================

struct GpuInstanceData
{
	float4x4 world;
	float4x4 prevWorld;
	float3   aabbMin;
	float    pad0;
	float3   aabbMax;
	uint     entityId;
	uint     materialId;
	uint     batchId;
	uint     startIndex;
	uint     indexCount;
};

struct DrawIndexedInstancedIndirectArgs
{
	uint indexCountPerInstance;
	uint instanceCount;
	uint startIndexLocation;
	int  baseVertexLocation;
	uint startInstanceLocation;
};

cbuffer CBCullData : register(b0)
{
	float4 g_FrustumPlanes[6];       // Camera frustum planes (xyz = normal, w = distance)
	float4 g_CascadePlanes[24];      // 4 Cascades x 6 planes
	uint   g_InstanceCount;
	uint   g_CascadeCount;
	uint   g_BatchCount;
	uint   g_Pad0;
};

StructuredBuffer<GpuInstanceData> g_Instances : register(t0);

// UAV outputs
RWStructuredBuffer<DrawIndexedInstancedIndirectArgs> g_CameraIndirectArgs : register(u0);
RWStructuredBuffer<uint>                             g_CameraVisibleInstances : register(u1);
RWStructuredBuffer<DrawIndexedInstancedIndirectArgs> g_CascadeIndirectArgs : register(u2);
RWStructuredBuffer<uint>                             g_CascadeVisibleInstances : register(u3);

// AABB vs Plane intersection test
// Returns true if the bounding box is at least partially in the positive half-space of the plane
bool isAABBOutsidePlane(float3 bMin, float3 bMax, float4 plane)
{
	float3 p;
	p.x = (plane.x >= 0.0f) ? bMax.x : bMin.x;
	p.y = (plane.y >= 0.0f) ? bMax.y : bMin.y;
	p.z = (plane.z >= 0.0f) ? bMax.z : bMin.z;

	return (dot(plane.xyz, p) + plane.w) < 0.0f;
}

// Full 6-plane frustum test for camera
bool testCameraFrustum(float3 bMin, float3 bMax)
{
	[unroll]
	for (uint i = 0; i < 6; ++i)
	{
		if (isAABBOutsidePlane(bMin, bMax, g_FrustumPlanes[i]))
		{
			return false;
		}
	}
	return true;
}

// Full 6-plane frustum test for shadow cascade
bool testCascadeFrustum(float3 bMin, float3 bMax, uint cascadeIndex)
{
	uint offset = cascadeIndex * 6;
	[unroll]
	for (uint i = 0; i < 6; ++i)
	{
		if (isAABBOutsidePlane(bMin, bMax, g_CascadePlanes[offset + i]))
		{
			return false;
		}
	}
	return true;
}

// Transform local AABB to world space
void transformAABB(float3 lMin, float3 lMax, float4x4 world, out float3 wMin, out float3 wMax)
{
	float3 center = (lMin + lMax) * 0.5f;
	float3 extents = (lMax - lMin) * 0.5f;

	float3 wCenter = mul(float4(center, 1.0f), world).xyz;

	// Absolute rotation matrix times extents
	float3x3 m = (float3x3)world;
	float3 wExtents = float3(
		dot(abs(float3(m[0][0], m[1][0], m[2][0])), extents),
		dot(abs(float3(m[0][1], m[1][1], m[2][1])), extents),
		dot(abs(float3(m[0][2], m[1][2], m[2][2])), extents)
	);

	wMin = wCenter - wExtents;
	wMax = wCenter + wExtents;
}

[numthreads(64, 1, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint instanceId = dispatchThreadId.x;
	if (instanceId >= g_InstanceCount)
	{
		return;
	}

	GpuInstanceData inst = g_Instances[instanceId];

	float3 wMin, wMax;
	transformAABB(inst.aabbMin, inst.aabbMax, inst.world, wMin, wMax);

	// 1. Camera View Frustum Culling
	bool cameraVisible = testCameraFrustum(wMin, wMax);
	if (cameraVisible)
	{
		uint batchId = inst.batchId;
		uint writeOffset;
		InterlockedAdd(g_CameraIndirectArgs[batchId].instanceCount, 1, writeOffset);
		// Write instance index to compacted visible buffer
		uint visibleIndex = batchId * g_InstanceCount + writeOffset;
		g_CameraVisibleInstances[visibleIndex] = instanceId;
	}

	// 2. Shadow Cascades Culling
	for (uint c = 0; c < g_CascadeCount && c < 4; ++c)
	{
		bool cascadeVisible = testCascadeFrustum(wMin, wMax, c);
		if (cascadeVisible)
		{
			uint cascadeBatchId = c * g_BatchCount + inst.batchId;
			uint cascadeWriteOffset;
			InterlockedAdd(g_CascadeIndirectArgs[cascadeBatchId].instanceCount, 1, cascadeWriteOffset);
			uint cascadeVisibleIndex = cascadeBatchId * g_InstanceCount + cascadeWriteOffset;
			g_CascadeVisibleInstances[cascadeVisibleIndex] = instanceId;
		}
	}
}
