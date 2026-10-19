#pragma once

#include "Engine/Physics/IPhysicsWorld.h"
#include <set>
#include <vector>

namespace Physics
{
	class DF_API PhysicsWorld : public IPhysicsWorld
	{
	public:
		PhysicsWorld();
		~PhysicsWorld() override = default;

		void init(const Vector3& gravity, float fixedTimeStep) override;
		void step(Core::Scene& scene, float fixedTimeStep) override;

		bool raycast(
			Core::Scene& scene,
			const Vector3& origin,
			const Vector3& dir,
			float maxDist,
			RaycastHit& outHit,
			uint16_t layerMask = 0xFFFF
		) override;

		void setCollisionMatrix(const std::array<uint16_t, 16>& matrix) override { m_collisionMatrix = matrix; }
		const std::array<uint16_t, 16>& getCollisionMatrix() const override { return m_collisionMatrix; }

		void setGravity(const Vector3& gravity) override { m_gravity = gravity; }
		Vector3 getGravity() const override { return m_gravity; }

	private:
		struct CollisionContact
		{
			entt::entity entityA = entt::null;
			entt::entity entityB = entt::null;
			Vector3 normal = Vector3(0.0f, 1.0f, 0.0f);
			float penetration = 0.0f;
			bool isTrigger = false;
		};

		bool canCollide(uint8_t layerA, uint8_t layerB) const noexcept;
		void dispatchEvents(Core::Scene& scene, const std::set<std::pair<entt::entity, entt::entity>>& currentCollisions, const std::set<std::pair<entt::entity, entt::entity>>& currentTriggers);

		Vector3 m_gravity = Vector3(0.0f, -9.81f, 0.0f);
		float m_fixedTimeStep = 1.0f / 60.0f;
		std::array<uint16_t, 16> m_collisionMatrix{};

		std::set<std::pair<entt::entity, entt::entity>> m_activeCollisions;
		std::set<std::pair<entt::entity, entt::entity>> m_activeTriggers;
	};
}
