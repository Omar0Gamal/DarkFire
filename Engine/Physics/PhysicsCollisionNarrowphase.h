#pragma once

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Scene/Components.h"

namespace Physics
{
	using Core::Vector3;
	using Core::Quaternion;
	using Core::Matrix;

	struct DF_API OBB
	{
		Vector3 center = Vector3(0.0f, 0.0f, 0.0f);
		Vector3 axes[3] = { Vector3(1.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f), Vector3(0.0f, 0.0f, 1.0f) };
		Vector3 extents = Vector3(0.5f, 0.5f, 0.5f);
	};

	DF_API OBB MakeOBB(const Core::TransformComponent& trans, const Core::ColliderComponent& col);
	DF_API bool TestOBBOBB(const OBB& a, const OBB& b, Vector3& outNormal, float& outPenetration);

	DF_API void ComputeColliderWorldAABB(
		const Core::ColliderComponent& col,
		const Core::TransformComponent& trans,
		Vector3& outMin,
		Vector3& outMax
	);

	DF_API bool TestColliderPair(
		const Core::ColliderComponent& colA, const Core::TransformComponent& transA,
		const Core::ColliderComponent& colB, const Core::TransformComponent& transB,
		Vector3& outNormal, float& outPenetration
	);

	DF_API bool RaycastCollider(
		const Core::ColliderComponent& col,
		const Core::TransformComponent& trans,
		const Vector3& origin,
		const Vector3& normalizedDir,
		float maxDist,
		float& outT,
		Vector3& outNormal
	);
}
