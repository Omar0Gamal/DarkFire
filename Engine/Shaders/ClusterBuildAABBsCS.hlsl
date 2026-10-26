//=============================================================================
// ClusterBuildAABBsCS.hlsl - Builds 16x16x24 View-Space Cluster AABBs
//=============================================================================

struct ClusterAABB
{
	float4 minPoint; // View-space min (xyz), w unused
	float4 maxPoint; // View-space max (xyz), w unused
};

cbuffer CBClusterBuild : register(b0)
{
	float4x4 g_InvProjection;
	float2   g_TileDimensions;  // (viewportWidth / 16, viewportHeight / 16)
	float2   g_ViewportSize;    // (viewportWidth, viewportHeight)
	float    g_NearZ;
	float    g_FarZ;
	float2   g_Pad0;
};

RWStructuredBuffer<ClusterAABB> g_ClusterAABBs : register(u0);

// Unproject screen-space point (0..1 UV, depth in view space) to view-space position
float3 screenToView(float2 screenUV, float viewDepth)
{
	// Convert UV to NDC [-1, 1]
	float4 ndc = float4(screenUV.x * 2.0f - 1.0f, (1.0f - screenUV.y) * 2.0f - 1.0f, 1.0f, 1.0f);
	float4 viewPos = mul(ndc, g_InvProjection);
	viewPos /= viewPos.w;

	// Scale view ray to desired depth
	return viewPos.xyz * (viewDepth / max(viewPos.z, 0.0001f));
}

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
	uint tileX = dispatchThreadId.x;
	uint tileY = dispatchThreadId.y;
	uint sliceZ = dispatchThreadId.z;

	if (tileX >= 16 || tileY >= 16 || sliceZ >= 24)
	{
		return;
	}

	uint clusterIndex = tileX + tileY * 16 + sliceZ * (16 * 16);

	// Logarithmic depth calculation for near and far planes of sliceZ
	// z_slice = near * (far / near)^(slice / 24)
	float depthRatio = g_FarZ / g_NearZ;
	float zNear = g_NearZ * pow(depthRatio, float(sliceZ) / 24.0f);
	float zFar  = g_NearZ * pow(depthRatio, float(sliceZ + 1) / 24.0f);

	// 4 Corners in normalized UV [0..1]
	float2 minUV = float2(float(tileX) / 16.0f, float(tileY) / 16.0f);
	float2 maxUV = float2(float(tileX + 1) / 16.0f, float(tileY + 1) / 16.0f);

	// 8 Corners of the cluster frustum in view space
	float3 corners[8];
	corners[0] = screenToView(float2(minUV.x, minUV.y), zNear);
	corners[1] = screenToView(float2(maxUV.x, minUV.y), zNear);
	corners[2] = screenToView(float2(minUV.x, maxUV.y), zNear);
	corners[3] = screenToView(float2(maxUV.x, maxUV.y), zNear);
	corners[4] = screenToView(float2(minUV.x, minUV.y), zFar);
	corners[5] = screenToView(float2(maxUV.x, minUV.y), zFar);
	corners[6] = screenToView(float2(minUV.x, maxUV.y), zFar);
	corners[7] = screenToView(float2(maxUV.x, maxUV.y), zFar);

	// Compute tight view-space AABB encompassing all 8 corners
	float3 aabbMin = corners[0];
	float3 aabbMax = corners[0];

	[unroll]
	for (int i = 1; i < 8; ++i)
	{
		aabbMin = min(aabbMin, corners[i]);
		aabbMax = max(aabbMax, corners[i]);
	}

	ClusterAABB aabb;
	aabb.minPoint = float4(aabbMin, 0.0f);
	aabb.maxPoint = float4(aabbMax, 0.0f);
	g_ClusterAABBs[clusterIndex] = aabb;
}
