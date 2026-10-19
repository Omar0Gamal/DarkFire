#pragma once
#include "Core/Game/IComponent.h"
#include "Core/Math/Vector3d.h"
#include "Core/Graphics/Prerequisites.h"
#include <functional>

namespace Core
{
	enum class ColliderType
	{
		Sphere,
		Box,
		Capsule,
		Plane
	};

	class ColliderComponent : public IComponent
	{
	public:
		ColliderComponent(ColliderType type = ColliderType::Sphere, bool isTrigger = false, Vector3d offset = Vector3d(0, 0, 0))
			: colliderType(type), isTrigger(isTrigger), centerOffset(offset) {}
		virtual ~ColliderComponent() = default;

	public:
		ColliderType colliderType = ColliderType::Sphere;
		bool isTrigger = false;
		Vector3d centerOffset = Vector3d(0, 0, 0);

		// Optional callbacks for gameplay events
		std::function<void(EntityID self, EntityID other)> onTriggerEnter = nullptr;
		std::function<void(EntityID self, EntityID other)> onCollisionEnter = nullptr;
	};

	class SphereColliderComponent : public ColliderComponent
	{
	public:
		SphereColliderComponent(float radius = 1.0f, Vector3d offset = Vector3d(0, 0, 0), bool isTrigger = false)
			: ColliderComponent(ColliderType::Sphere, isTrigger, offset), radius(radius) {}

	public:
		float radius = 1.0f;
	};

	class BoxColliderComponent : public ColliderComponent
	{
	public:
		BoxColliderComponent(Vector3d halfExtents = Vector3d(1, 1, 1), Vector3d offset = Vector3d(0, 0, 0), bool isTrigger = false)
			: ColliderComponent(ColliderType::Box, isTrigger, offset), halfExtents(halfExtents) {}

	public:
		Vector3d halfExtents = Vector3d(1, 1, 1); // Distance from center to faces
	};

	class PlaneColliderComponent : public ColliderComponent
	{
	public:
		PlaneColliderComponent(Vector3d normal = Vector3d(0, 1, 0), float distance = 0.0f, bool isTrigger = false)
			: ColliderComponent(ColliderType::Plane, isTrigger), normal(Vector3d::normalize(normal)), distance(distance) {}

	public:
		Vector3d normal = Vector3d(0, 1, 0);
		float distance = 0.0f;
	};
}