#pragma once
#include "Core/Game/IComponent.h"

namespace Core
{
	enum ColliderType
	{
		SphereC,
		PlaneC,
		CubeC,
		CapsuleC,
		MeshC
	};

	class ColliderComponent : public IComponent
	{
	public:
		ColliderComponent() = default;
	public:
		ColliderType type = ColliderType::SphereC;
	};

	class SphereColliderComponent : public ColliderComponent
	{
	public:
		SphereColliderComponent(float radius)
			: radius(radius) {}
	public:
		float radius = 1.0f;
	};
}