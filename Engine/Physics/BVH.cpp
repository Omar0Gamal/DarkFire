#include "Engine/Physics/BVH.h"
#include <algorithm>
#include <cmath>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace Physics
{
	namespace
	{
		inline bool RayAABB(
			const Vector3& origin,
			const Vector3& invDir,
			const Vector3& boxMin,
			const Vector3& boxMax,
			float maxDist,
			float& outNear)
		{
			float t1 = (boxMin.x - origin.x) * invDir.x;
			float t2 = (boxMax.x - origin.x) * invDir.x;
			float t3 = (boxMin.y - origin.y) * invDir.y;
			float t4 = (boxMax.y - origin.y) * invDir.y;
			float t5 = (boxMin.z - origin.z) * invDir.z;
			float t6 = (boxMax.z - origin.z) * invDir.z;

			float tmin = std::max(std::max(std::min(t1, t2), std::min(t3, t4)), std::min(t5, t6));
			float tmax = std::min(std::min(std::max(t1, t2), std::max(t3, t4)), std::max(t5, t6));

			if (tmax < 0.0f || tmin > tmax || tmin > maxDist)
			{
				return false;
			}
			outNear = tmin < 0.0f ? 0.0f : tmin;
			return true;
		}

		inline bool RayTriangleMT(
			const Vector3& orig,
			const Vector3& dir,
			const Vector3& v0,
			const Vector3& v1,
			const Vector3& v2,
			float& outT)
		{
			const float EPSILON = 1e-7f;
			Vector3 edge1 = v1 - v0;
			Vector3 edge2 = v2 - v0;
			Vector3 h = dir.Cross(edge2);
			float a = edge1.Dot(h);

			if (std::abs(a) < EPSILON) return false;

			float f = 1.0f / a;
			Vector3 s = orig - v0;
			float u = f * s.Dot(h);
			if (u < 0.0f || u > 1.0f) return false;

			Vector3 q = s.Cross(edge1);
			float v = f * dir.Dot(q);
			if (v < 0.0f || u + v > 1.0f) return false;

			float t = f * edge2.Dot(q);
			if (t > EPSILON)
			{
				outT = t;
				return true;
			}
			return false;
		}

		inline bool AABBOverlap(
			const Vector3& aMin, const Vector3& aMax,
			const Vector3& bMin, const Vector3& bMax)
		{
			if (aMax.x < bMin.x || aMin.x > bMax.x) return false;
			if (aMax.y < bMin.y || aMin.y > bMax.y) return false;
			if (aMax.z < bMin.z || aMin.z > bMax.z) return false;
			return true;
		}
	}

	void BVH::build(const std::vector<Vector3>& vertices, const std::vector<uint32_t>& indices)
	{
		std::vector<Core::CollisionTriangle> tris;
		tris.reserve(indices.size() / 3);

		for (size_t i = 0; i + 2 < indices.size(); i += 3)
		{
			uint32_t i0 = indices[i];
			uint32_t i1 = indices[i + 1];
			uint32_t i2 = indices[i + 2];

			if (i0 < vertices.size() && i1 < vertices.size() && i2 < vertices.size())
			{
				Core::CollisionTriangle t;
				t.v0 = vertices[i0];
				t.v1 = vertices[i1];
				t.v2 = vertices[i2];

				Vector3 e1 = t.v1 - t.v0;
				Vector3 e2 = t.v2 - t.v0;
				Vector3 n = e1.Cross(e2);
				float len = n.Length();
				t.normal = len > 1e-6f ? (n / len) : Vector3(0.0f, 1.0f, 0.0f);

				tris.push_back(t);
			}
		}

		buildFromTriangles(tris);
	}

	void BVH::buildFromTriangles(const std::vector<Core::CollisionTriangle>& triangles)
	{
		m_triangles = triangles;
		m_nodes.clear();
		m_triangleIndices.resize(m_triangles.size());

		if (m_triangles.empty())
		{
			m_boundsMin = Vector3(0.0f, 0.0f, 0.0f);
			m_boundsMax = Vector3(0.0f, 0.0f, 0.0f);
			return;
		}

		for (uint32_t i = 0; i < static_cast<uint32_t>(m_triangles.size()); ++i)
		{
			m_triangleIndices[i] = i;
		}

		m_nodes.emplace_back(); // Root node at 0
		buildNode(0, 0, static_cast<uint32_t>(m_triangles.size()), 0);

		if (!m_nodes.empty())
		{
			m_boundsMin = m_nodes[0].aabbMin;
			m_boundsMax = m_nodes[0].aabbMax;
		}
	}

	void BVH::buildNode(int nodeIndex, uint32_t start, uint32_t count, int depth)
	{
		Vector3 bMin(1e30f, 1e30f, 1e30f);
		Vector3 bMax(-1e30f, -1e30f, -1e30f);
		Vector3 cMin(1e30f, 1e30f, 1e30f);
		Vector3 cMax(-1e30f, -1e30f, -1e30f);

		for (uint32_t i = 0; i < count; ++i)
		{
			const auto& tri = m_triangles[m_triangleIndices[start + i]];
			bMin = Vector3::Min(bMin, Vector3::Min(tri.v0, Vector3::Min(tri.v1, tri.v2)));
			bMax = Vector3::Max(bMax, Vector3::Max(tri.v0, Vector3::Max(tri.v1, tri.v2)));

			Vector3 centroid = (tri.v0 + tri.v1 + tri.v2) * (1.0f / 3.0f);
			cMin = Vector3::Min(cMin, centroid);
			cMax = Vector3::Max(cMax, centroid);
		}

		m_nodes[nodeIndex].aabbMin = bMin;
		m_nodes[nodeIndex].aabbMax = bMax;

		// Leaf conditions
		if (count <= 4 || depth >= 24)
		{
			m_nodes[nodeIndex].firstTriangle = start;
			m_nodes[nodeIndex].triangleCount = count;
			m_nodes[nodeIndex].leftChild = -1;
			m_nodes[nodeIndex].rightChild = -1;
			return;
		}

		// Choose split axis with largest extent of centroids
		Vector3 cExtent = cMax - cMin;
		int axis = 0;
		if (cExtent.y > cExtent.x && cExtent.y > cExtent.z) axis = 1;
		else if (cExtent.z > cExtent.x && cExtent.z > cExtent.y) axis = 2;

		float splitPos = 0.5f * (
			(axis == 0 ? cMin.x + cMax.x : (axis == 1 ? cMin.y + cMax.y : cMin.z + cMax.z))
		);

		// Partition triangles
		uint32_t mid = start;
		for (uint32_t i = start; i < start + count; ++i)
		{
			const auto& tri = m_triangles[m_triangleIndices[i]];
			Vector3 centroid = (tri.v0 + tri.v1 + tri.v2) * (1.0f / 3.0f);
			float coord = (axis == 0 ? centroid.x : (axis == 1 ? centroid.y : centroid.z));
			if (coord < splitPos)
			{
				std::swap(m_triangleIndices[i], m_triangleIndices[mid]);
				++mid;
			}
		}

		// Fallback if partition failed (all on one side)
		if (mid == start || mid == start + count)
		{
			mid = start + count / 2;
		}

		uint32_t leftCount = mid - start;
		uint32_t rightCount = count - leftCount;

		int leftIndex = static_cast<int>(m_nodes.size());
		m_nodes.emplace_back();
		int rightIndex = static_cast<int>(m_nodes.size());
		m_nodes.emplace_back();

		m_nodes[nodeIndex].leftChild = leftIndex;
		m_nodes[nodeIndex].rightChild = rightIndex;
		m_nodes[nodeIndex].firstTriangle = 0;
		m_nodes[nodeIndex].triangleCount = 0;

		buildNode(leftIndex, start, leftCount, depth + 1);
		buildNode(rightIndex, mid, rightCount, depth + 1);
	}

	bool BVH::raycast(
		const Vector3& origin,
		const Vector3& dir,
		float maxDist,
		float& outT,
		Vector3& outNormal,
		uint32_t& outTriangleIndex
	) const
	{
		if (m_nodes.empty() || m_triangles.empty()) return false;

		Vector3 invDir(
			std::abs(dir.x) > 1e-7f ? 1.0f / dir.x : (dir.x >= 0 ? 1e7f : -1e7f),
			std::abs(dir.y) > 1e-7f ? 1.0f / dir.y : (dir.y >= 0 ? 1e7f : -1e7f),
			std::abs(dir.z) > 1e-7f ? 1.0f / dir.z : (dir.z >= 0 ? 1e7f : -1e7f)
		);

		float closestT = maxDist;
		bool hit = false;

		int stack[64];
		int stackPtr = 0;
		stack[stackPtr++] = 0; // Root node

		while (stackPtr > 0)
		{
			int currIdx = stack[--stackPtr];
			const auto& node = m_nodes[currIdx];

			float nearT = 0.0f;
			if (!RayAABB(origin, invDir, node.aabbMin, node.aabbMax, closestT, nearT))
			{
				continue;
			}

			if (node.isLeaf())
			{
				for (uint32_t i = 0; i < node.triangleCount; ++i)
				{
					uint32_t triIdx = m_triangleIndices[node.firstTriangle + i];
					const auto& tri = m_triangles[triIdx];
					float t = 0.0f;
					if (RayTriangleMT(origin, dir, tri.v0, tri.v1, tri.v2, t))
					{
						if (t < closestT)
						{
							closestT = t;
							outNormal = tri.normal;
							outTriangleIndex = triIdx;
							hit = true;
						}
					}
				}
			}
			else
			{
				// Traverse both children
				if (node.leftChild != -1) stack[stackPtr++] = node.leftChild;
				if (node.rightChild != -1) stack[stackPtr++] = node.rightChild;
			}
		}

		if (hit)
		{
			outT = closestT;
		}
		return hit;
	}

	void BVH::queryAABB(
		const Vector3& boxMin,
		const Vector3& boxMax,
		std::vector<uint32_t>& outTriangleIndices
	) const
	{
		if (m_nodes.empty() || m_triangles.empty()) return;

		int stack[64];
		int stackPtr = 0;
		stack[stackPtr++] = 0;

		while (stackPtr > 0)
		{
			int currIdx = stack[--stackPtr];
			const auto& node = m_nodes[currIdx];

			if (!AABBOverlap(node.aabbMin, node.aabbMax, boxMin, boxMax))
			{
				continue;
			}

			if (node.isLeaf())
			{
				for (uint32_t i = 0; i < node.triangleCount; ++i)
				{
					uint32_t triIdx = m_triangleIndices[node.firstTriangle + i];
					const auto& tri = m_triangles[triIdx];

					Vector3 tMin = Vector3::Min(tri.v0, Vector3::Min(tri.v1, tri.v2));
					Vector3 tMax = Vector3::Max(tri.v0, Vector3::Max(tri.v1, tri.v2));

					if (AABBOverlap(tMin, tMax, boxMin, boxMax))
					{
						outTriangleIndices.push_back(triIdx);
					}
				}
			}
			else
			{
				if (node.leftChild != -1) stack[stackPtr++] = node.leftChild;
				if (node.rightChild != -1) stack[stackPtr++] = node.rightChild;
			}
		}
	}

	void BVH::querySphere(
		const Vector3& center,
		float radius,
		std::vector<uint32_t>& outTriangleIndices
	) const
	{
		Vector3 rVec(radius, radius, radius);
		queryAABB(center - rVec, center + rVec, outTriangleIndices);
	}
}
