#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Scene/Entity.h"
#include <array>
#include <cstdint>

namespace Core
{
	class Scene;
}

namespace Physics
{
	using Core::Vector3;
	using Core::Quaternion;

	struct DF_API RaycastHit
	{
		bool hasHit = false;
		Core::Entity entity{};
		Vector3 point = Vector3(0.0f, 0.0f, 0.0f);
		Vector3 normal = Vector3(0.0f, 1.0f, 0.0f);
		float distance = 0.0f;
	};

	struct DF_API SweepHit
	{
		bool hasHit = false;
		Core::Entity entity{};
		Vector3 point = Vector3(0.0f, 0.0f, 0.0f);
		Vector3 normal = Vector3(0.0f, 1.0f, 0.0f);
		float distance = 0.0f;
	};

	class DF_API IPhysicsWorld
	{
	public:
		virtual ~IPhysicsWorld() = default;

		virtual void init(const Vector3& gravity, float fixedTimeStep) = 0;
		virtual void step(Core::Scene& scene, float fixedTimeStep) = 0;

		virtual bool raycast(
			Core::Scene& scene,
			const Vector3& origin,
			const Vector3& dir,
			float maxDist,
			RaycastHit& outHit,
			uint16_t layerMask = 0xFFFF
		) = 0;

		virtual bool sphereCast(
			Core::Scene& scene,
			const Vector3& origin,
			float radius,
			const Vector3& dir,
			float maxDist,
			SweepHit& outHit,
			uint16_t layerMask = 0xFFFF
		) = 0;

		virtual bool boxCast(
			Core::Scene& scene,
			const Vector3& origin,
			const Vector3& halfExtents,
			const Quaternion& orientation,
			const Vector3& dir,
			float maxDist,
			SweepHit& outHit,
			uint16_t layerMask = 0xFFFF
		) = 0;

		virtual void setCollisionMatrix(const std::array<uint16_t, 16>& matrix) = 0;
		virtual const std::array<uint16_t, 16>& getCollisionMatrix() const = 0;

		virtual void setGravity(const Vector3& gravity) = 0;
		virtual Vector3 getGravity() const = 0;
	};
}
