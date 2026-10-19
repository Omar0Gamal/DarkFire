#include "Engine/Physics/PhysicsWorld.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Scripting/ScriptableEntity.h"
#include <algorithm>
#include <cmath>

namespace Physics
{
	PhysicsWorld::PhysicsWorld()
	{
		m_collisionMatrix.fill(0xFFFF);
	}

	void PhysicsWorld::init(const Vector3& gravity, float fixedTimeStep)
	{
		m_gravity = gravity;
		m_fixedTimeStep = fixedTimeStep;
		m_activeCollisions.clear();
		m_activeTriggers.clear();
	}

	bool PhysicsWorld::canCollide(uint8_t layerA, uint8_t layerB) const noexcept
	{
		if (layerA >= 16 || layerB >= 16)
		{
			return true;
		}
		return (m_collisionMatrix[layerA] & (1 << layerB)) != 0;
	}

	void PhysicsWorld::step(Core::Scene& scene, float fixedTimeStep)
	{
		auto& reg = scene.getRegistry();

		// 1. Integrate dynamic rigidbodies
		auto rbView = reg.view<Core::RigidbodyComponent, Core::TransformComponent>();
		for (auto entity : rbView)
		{
			auto& rb = rbView.get<Core::RigidbodyComponent>(entity);
			auto& transform = rbView.get<Core::TransformComponent>(entity);
			auto& state = reg.get_or_emplace<Core::RigidbodyRuntimeState>(entity);

			if (!rb.isKinematic)
			{
				if (rb.useGravity)
				{
					state.linearVelocity += m_gravity * fixedTimeStep;
				}

				state.linearVelocity *= std::max(0.0f, 1.0f - rb.drag * fixedTimeStep);
				state.angularVelocity *= std::max(0.0f, 1.0f - rb.angularDrag * fixedTimeStep);

				transform.position += state.linearVelocity * fixedTimeStep;
			}
		}

		// 2. Collision detection & resolution
		std::vector<entt::entity> colliderEntities;
		auto colView = reg.view<Core::ColliderComponent, Core::TransformComponent>();
		for (auto entity : colView)
		{
			colliderEntities.push_back(entity);
		}

		std::set<std::pair<entt::entity, entt::entity>> currentCollisions;
		std::set<std::pair<entt::entity, entt::entity>> currentTriggers;

		size_t numColliders = colliderEntities.size();
		for (size_t i = 0; i < numColliders; ++i)
		{
			auto entA = colliderEntities[i];
			const auto& colA = reg.get<Core::ColliderComponent>(entA);
			auto& transA = reg.get<Core::TransformComponent>(entA);

			for (size_t j = i + 1; j < numColliders; ++j)
			{
				auto entB = colliderEntities[j];
				const auto& colB = reg.get<Core::ColliderComponent>(entB);
				auto& transB = reg.get<Core::TransformComponent>(entB);

				if (!canCollide(colA.layer, colB.layer))
				{
					continue;
				}

				Vector3 normal(0.0f, 1.0f, 0.0f);
				float penetration = 0.0f;
				bool isColliding = false;

				// Collision shapes
				if (colA.shape == Core::ColliderShape::Sphere && colB.shape == Core::ColliderShape::Sphere)
				{
					Vector3 delta = transB.position - transA.position;
					float distSq = delta.LengthSquared();
					float radiusSum = colA.radius + colB.radius;
					if (distSq < radiusSum * radiusSum)
					{
						float dist = std::sqrt(distSq);
						normal = (dist > 0.0001f) ? (delta / dist) : Vector3(0.0f, 1.0f, 0.0f);
						penetration = radiusSum - dist;
						isColliding = true;
					}
				}
				else if (colA.shape == Core::ColliderShape::Box && colB.shape == Core::ColliderShape::Box)
				{
					Vector3 delta = transB.position - transA.position;
					Vector3 halfA = colA.size * 0.5f;
					Vector3 halfB = colB.size * 0.5f;

					float overlapX = (halfA.x + halfB.x) - std::abs(delta.x);
					float overlapY = (halfA.y + halfB.y) - std::abs(delta.y);
					float overlapZ = (halfA.z + halfB.z) - std::abs(delta.z);

					if (overlapX > 0.0f && overlapY > 0.0f && overlapZ > 0.0f)
					{
						isColliding = true;
						if (overlapX <= overlapY && overlapX <= overlapZ)
						{
							penetration = overlapX;
							normal = Vector3(delta.x >= 0.0f ? 1.0f : -1.0f, 0.0f, 0.0f);
						}
						else if (overlapY <= overlapX && overlapY <= overlapZ)
						{
							penetration = overlapY;
							normal = Vector3(0.0f, delta.y >= 0.0f ? 1.0f : -1.0f, 0.0f);
						}
						else
						{
							penetration = overlapZ;
							normal = Vector3(0.0f, 0.0f, delta.z >= 0.0f ? 1.0f : -1.0f);
						}
					}
				}
				else if (colA.shape == Core::ColliderShape::Box && colB.shape == Core::ColliderShape::Sphere)
				{
					Vector3 halfA = colA.size * 0.5f;
					Vector3 relPos = transB.position - transA.position;
					Vector3 clamped = Vector3(
						std::clamp(relPos.x, -halfA.x, halfA.x),
						std::clamp(relPos.y, -halfA.y, halfA.y),
						std::clamp(relPos.z, -halfA.z, halfA.z)
					);
					Vector3 delta = relPos - clamped;
					float distSq = delta.LengthSquared();
					if (distSq < colB.radius * colB.radius)
					{
						float dist = std::sqrt(distSq);
						normal = (dist > 0.0001f) ? (delta / dist) : Vector3(0.0f, 1.0f, 0.0f);
						penetration = colB.radius - dist;
						isColliding = true;
					}
				}
				else if (colA.shape == Core::ColliderShape::Sphere && colB.shape == Core::ColliderShape::Box)
				{
					Vector3 halfB = colB.size * 0.5f;
					Vector3 relPos = transA.position - transB.position;
					Vector3 clamped = Vector3(
						std::clamp(relPos.x, -halfB.x, halfB.x),
						std::clamp(relPos.y, -halfB.y, halfB.y),
						std::clamp(relPos.z, -halfB.z, halfB.z)
					);
					Vector3 delta = relPos - clamped;
					float distSq = delta.LengthSquared();
					if (distSq < colA.radius * colA.radius)
					{
						float dist = std::sqrt(distSq);
						normal = (dist > 0.0001f) ? (-delta / dist) : Vector3(0.0f, -1.0f, 0.0f);
						penetration = colA.radius - dist;
						isColliding = true;
					}
				}

				if (isColliding)
				{
					auto pairKey = (entA < entB) ? std::make_pair(entA, entB) : std::make_pair(entB, entA);

					if (colA.isTrigger || colB.isTrigger)
					{
						currentTriggers.insert(pairKey);
					}
					else
					{
						currentCollisions.insert(pairKey);

						// Physical response
						auto* rbA = reg.try_get<Core::RigidbodyComponent>(entA);
						auto* rbB = reg.try_get<Core::RigidbodyComponent>(entB);

						float invMassA = (rbA && !rbA->isKinematic && rbA->mass > 0.0f) ? (1.0f / rbA->mass) : 0.0f;
						float invMassB = (rbB && !rbB->isKinematic && rbB->mass > 0.0f) ? (1.0f / rbB->mass) : 0.0f;
						float totalInvMass = invMassA + invMassB;

						if (totalInvMass > 0.0f)
						{
							// Positional correction
							transA.position -= normal * (penetration * (invMassA / totalInvMass));
							transB.position += normal * (penetration * (invMassB / totalInvMass));

							// Velocity impulse
							auto* stateA = reg.try_get<Core::RigidbodyRuntimeState>(entA);
							auto* stateB = reg.try_get<Core::RigidbodyRuntimeState>(entB);

							Vector3 velA = stateA ? stateA->linearVelocity : Vector3(0.0f, 0.0f, 0.0f);
							Vector3 velB = stateB ? stateB->linearVelocity : Vector3(0.0f, 0.0f, 0.0f);
							Vector3 relVel = velB - velA;
							float normVel = relVel.Dot(normal);

							if (normVel < 0.0f)
							{
								float restA = rbA ? rbA->restitution : 0.0f;
								float restB = rbB ? rbB->restitution : 0.0f;
								float restitution = std::max(restA, restB);

								float impulseMag = -(1.0f + restitution) * normVel / totalInvMass;
								Vector3 impulse = normal * impulseMag;

								if (invMassA > 0.0f && stateA)
								{
									stateA->linearVelocity -= impulse * invMassA;
								}
								if (invMassB > 0.0f && stateB)
								{
									stateB->linearVelocity += impulse * invMassB;
								}
							}
						}
					}
				}
			}
		}

		// 3. Dispatch trigger and collision events
		dispatchEvents(scene, currentCollisions, currentTriggers);

		m_activeCollisions = std::move(currentCollisions);
		m_activeTriggers = std::move(currentTriggers);
	}

	void PhysicsWorld::dispatchEvents(
		Core::Scene& scene,
		const std::set<std::pair<entt::entity, entt::entity>>& currentCollisions,
		const std::set<std::pair<entt::entity, entt::entity>>& currentTriggers
	)
	{
		auto dispatchScriptPair = [&](entt::entity e1, entt::entity e2, bool isCollision, bool isEnter) {
			Core::Entity entity1(e1, &scene);
			Core::Entity entity2(e2, &scene);
			if (!entity1.isValid() || !entity2.isValid())
			{
				return;
			}

			auto invokeScripts = [&](Core::Entity self, Core::Entity other) {
				auto* sc = self.tryGetComponent<Core::ScriptsComponent>();
				if (sc)
				{
					for (auto& s : sc->scripts)
					{
						if (s.isEnabled && s.instance)
						{
							if (isCollision)
							{
								if (isEnter) s.instance->onCollisionEnter(other);
								else s.instance->onCollisionExit(other);
							}
							else
							{
								if (isEnter) s.instance->onTriggerEnter(other);
								else s.instance->onTriggerExit(other);
							}
						}
					}
				}
			};

			invokeScripts(entity1, entity2);
			invokeScripts(entity2, entity1);
		};

		// 1. Collision Enter
		for (const auto& pair : currentCollisions)
		{
			if (m_activeCollisions.find(pair) == m_activeCollisions.end())
			{
				dispatchScriptPair(pair.first, pair.second, true, true);
			}
		}

		// 2. Collision Exit
		for (const auto& pair : m_activeCollisions)
		{
			if (currentCollisions.find(pair) == currentCollisions.end())
			{
				dispatchScriptPair(pair.first, pair.second, true, false);
			}
		}

		// 3. Trigger Enter
		for (const auto& pair : currentTriggers)
		{
			if (m_activeTriggers.find(pair) == m_activeTriggers.end())
			{
				dispatchScriptPair(pair.first, pair.second, false, true);
			}
		}

		// 4. Trigger Exit
		for (const auto& pair : m_activeTriggers)
		{
			if (currentTriggers.find(pair) == currentTriggers.end())
			{
				dispatchScriptPair(pair.first, pair.second, false, false);
			}
		}
	}

	bool PhysicsWorld::raycast(
		Core::Scene& scene,
		const Vector3& origin,
		const Vector3& dir,
		float maxDist,
		RaycastHit& outHit,
		uint16_t layerMask
	)
	{
		outHit.hasHit = false;
		outHit.distance = maxDist;

		Vector3 normalizedDir = dir;
		normalizedDir.Normalize();

		auto& reg = scene.getRegistry();
		auto view = reg.view<Core::ColliderComponent, Core::TransformComponent>();

		for (auto entity : view)
		{
			const auto& col = view.get<Core::ColliderComponent>(entity);
			const auto& trans = view.get<Core::TransformComponent>(entity);

			if ((layerMask & (1 << col.layer)) == 0)
			{
				continue;
			}

			if (col.shape == Core::ColliderShape::Sphere)
			{
				Vector3 m = origin - trans.position;
				float b = m.Dot(normalizedDir);
				float c = m.Dot(m) - col.radius * col.radius;

				if (c > 0.0f && b > 0.0f)
				{
					continue;
				}

				float discr = b * b - c;
				if (discr < 0.0f)
				{
					continue;
				}

				float t = -b - std::sqrt(discr);
				if (t < 0.0f)
				{
					t = 0.0f;
				}

				if (t < outHit.distance)
				{
					outHit.hasHit = true;
					outHit.distance = t;
					outHit.point = origin + normalizedDir * t;
					outHit.normal = (outHit.point - trans.position);
					outHit.normal.Normalize();
					outHit.entity = Core::Entity(entity, &scene);
				}
			}
			else if (col.shape == Core::ColliderShape::Box)
			{
				Vector3 minPt = trans.position - col.size * 0.5f;
				Vector3 maxPt = trans.position + col.size * 0.5f;

				float tmin = 0.0f;
				float tmax = outHit.distance;
				Vector3 hitNorm(0.0f, 1.0f, 0.0f);

				bool intersects = true;
				for (int i = 0; i < 3; ++i)
				{
					float origComp = (i == 0) ? origin.x : (i == 1 ? origin.y : origin.z);
					float dirComp = (i == 0) ? normalizedDir.x : (i == 1 ? normalizedDir.y : normalizedDir.z);
					float minComp = (i == 0) ? minPt.x : (i == 1 ? minPt.y : minPt.z);
					float maxComp = (i == 0) ? maxPt.x : (i == 1 ? maxPt.y : maxPt.z);

					if (std::abs(dirComp) < 0.00001f)
					{
						if (origComp < minComp || origComp > maxComp)
						{
							intersects = false;
							break;
						}
					}
					else
					{
						float ood = 1.0f / dirComp;
						float t1 = (minComp - origComp) * ood;
						float t2 = (maxComp - origComp) * ood;
						Vector3 n1 = (i == 0) ? Vector3(-1, 0, 0) : (i == 1 ? Vector3(0, -1, 0) : Vector3(0, 0, -1));

						if (t1 > t2)
						{
							std::swap(t1, t2);
							n1 = -n1;
						}

						if (t1 > tmin)
						{
							tmin = t1;
							hitNorm = n1;
						}
						tmax = std::min(tmax, t2);

						if (tmin > tmax)
						{
							intersects = false;
							break;
						}
					}
				}

				if (intersects && tmin < outHit.distance)
				{
					outHit.hasHit = true;
					outHit.distance = tmin;
					outHit.point = origin + normalizedDir * tmin;
					outHit.normal = hitNorm;
					outHit.entity = Core::Entity(entity, &scene);
				}
			}
		}

		return outHit.hasHit;
	}
}
