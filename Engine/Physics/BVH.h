#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Scene/Components.h"
#include <vector>
#include <memory>
#include <cstdint>

namespace Physics
{
	using Core::Vector3;

	struct DF_API BVHNode
	{
		Vector3 aabbMin = Vector3(1e30f, 1e30f, 1e30f);
		Vector3 aabbMax = Vector3(-1e30f, -1e30f, -1e30f);
		int leftChild = -1;
		int rightChild = -1;
		uint32_t firstTriangle = 0;
		uint32_t triangleCount = 0;

		bool isLeaf() const noexcept { return leftChild == -1 && rightChild == -1; }
	};

	class DF_API BVH
	{
	public:
		BVH() = default;
		~BVH() = default;

		void build(const std::vector<Vector3>& vertices, const std::vector<uint32_t>& indices);
		void buildFromTriangles(const std::vector<Core::CollisionTriangle>& triangles);

		bool raycast(
			const Vector3& origin,
			const Vector3& dir,
			float maxDist,
			float& outT,
			Vector3& outNormal,
			uint32_t& outTriangleIndex
		) const;

		void queryAABB(
			const Vector3& boxMin,
			const Vector3& boxMax,
			std::vector<uint32_t>& outTriangleIndices
		) const;

		void querySphere(
			const Vector3& center,
			float radius,
			std::vector<uint32_t>& outTriangleIndices
		) const;

		const std::vector<BVHNode>& getNodes() const noexcept { return m_nodes; }
		const std::vector<Core::CollisionTriangle>& getTriangles() const noexcept { return m_triangles; }
		const Vector3& getBoundsMin() const noexcept { return m_boundsMin; }
		const Vector3& getBoundsMax() const noexcept { return m_boundsMax; }

	private:
		void buildNode(int nodeIndex, uint32_t start, uint32_t count, int depth);

		std::vector<BVHNode> m_nodes;
		std::vector<Core::CollisionTriangle> m_triangles;
		std::vector<uint32_t> m_triangleIndices;
		Vector3 m_boundsMin = Vector3(0.0f, 0.0f, 0.0f);
		Vector3 m_boundsMax = Vector3(0.0f, 0.0f, 0.0f);
	};
}
