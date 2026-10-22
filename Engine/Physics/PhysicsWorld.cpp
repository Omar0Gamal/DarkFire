#include "Engine/Physics/PhysicsWorld.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Scripting/ScriptableEntity.h"
#include "Engine/Core/Math/Math.h"
#include <algorithm>
#include <cmath>
#include <limits>

namespace Physics
{
	using Core::Matrix;
	using Core::Quaternion;

	namespace
	{
		struct OBB
		{
			Vector3 center;
			Vector3 axes[3];   // Local X, Y, Z unit vectors in world space
			Vector3 extents;   // Half-sizes along axes
		};

		inline OBB MakeOBB(const Core::TransformComponent& trans, const Core::ColliderComponent& col)
		{
			OBB obb{};
			obb.center = trans.position;
			obb.extents = col.size * 0.5f;
			obb.axes[0] = trans.getRight();
			obb.axes[1] = trans.getUp();
			obb.axes[2] = trans.getForward();
			return obb;
		}

		inline bool TestOBBOBB(const OBB& a, const OBB& b, Vector3& outNormal, float& outPenetration)
		{
			Vector3 D = b.center - a.center;

			float minOverlap = (std::numeric_limits<float>::max)();
			Vector3 bestAxis(0.0f, 1.0f, 0.0f);

			// Helper to test a single candidate axis L
			auto testAxis = [&](Vector3 L, float bias = 1.0f) -> bool
			{
				float lenSq = L.LengthSquared();
				if (lenSq < 0.0001f) return true; // Degenerate cross product (parallel edges)

				L /= std::sqrt(lenSq);

				float rA = a.extents.x * std::abs(a.axes[0].Dot(L))
				         + a.extents.y * std::abs(a.axes[1].Dot(L))
				         + a.extents.z * std::abs(a.axes[2].Dot(L));

				float rB = b.extents.x * std::abs(b.axes[0].Dot(L))
				         + b.extents.y * std::abs(b.axes[1].Dot(L))
				         + b.extents.z * std::abs(b.axes[2].Dot(L));

				float s = std::abs(D.Dot(L));
				float overlap = (rA + rB) - s;

				if (overlap <= 0.0f)
				{
					return false; // Separating axis found!
				}

				if (overlap * bias < minOverlap)
				{
					minOverlap = overlap * bias;
					bestAxis = L;
				}
				return true;
			};

			// 1. Box A face normals (3) - priority
			if (!testAxis(a.axes[0], 1.0f)) return false;
			if (!testAxis(a.axes[1], 1.0f)) return false;
			if (!testAxis(a.axes[2], 1.0f)) return false;

			// 2. Box B face normals (3) - priority
			if (!testAxis(b.axes[0], 1.0f)) return false;
			if (!testAxis(b.axes[1], 1.0f)) return false;
			if (!testAxis(b.axes[2], 1.0f)) return false;

			// 3. Edge cross products (3 x 3 = 9) - slight bias to prevent jittering on flat faces
			for (int i = 0; i < 3; ++i)
			{
				for (int j = 0; j < 3; ++j)
				{
					Vector3 axis = a.axes[i].Cross(b.axes[j]);
					if (!testAxis(axis, 1.05f)) return false;
				}
			}

			// Ensure normal points from A to B
			if (D.Dot(bestAxis) < 0.0f)
			{
				bestAxis = -bestAxis;
			}
			else if (std::abs(D.Dot(bestAxis)) < 0.00001f)
			{
				Vector3 diff = b.center - a.center;
				if (diff.LengthSquared() > 0.0001f)
				{
					diff.Normalize();
					if (diff.Dot(bestAxis) < 0.0f)
					{
						bestAxis = -bestAxis;
					}
				}
			}

			outNormal = bestAxis;
			outPenetration = minOverlap;
			return true;
		}
	}

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

		// 2. Collision detection & resolution (Multi-iteration constraint solver)
		std::vector<entt::entity> colliderEntities;
		auto colView = reg.view<Core::ColliderComponent, Core::TransformComponent>();
		for (auto entity : colView)
		{
			colliderEntities.push_back(entity);
		}

		std::set<std::pair<entt::entity, entt::entity>> currentCollisions;
		std::set<std::pair<entt::entity, entt::entity>> currentTriggers;

		constexpr int kSolverIterations = 4;
		size_t numColliders = colliderEntities.size();

		for (int iter = 0; iter < kSolverIterations; ++iter)
		{
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
					// Full 3D Oriented Bounding Box (OBB) collision via Separating Axis Theorem (SAT)
					OBB obbA = MakeOBB(transA, colA);
					OBB obbB = MakeOBB(transB, colB);
					if (TestOBBOBB(obbA, obbB, normal, penetration))
					{
						isColliding = true;
					}
				}
				else if (colA.shape == Core::ColliderShape::Box && colB.shape == Core::ColliderShape::Sphere)
				{
					// Rotated Box A vs Sphere B: evaluate in Box A's local space
					Vector3 halfA = colA.size * 0.5f;
					Quaternion invRotA;
					transA.rotation.Inverse(invRotA);
					Vector3 localSpherePos = Vector3::Transform(transB.position - transA.position, invRotA);

					Vector3 localClamped = Vector3(
						std::clamp(localSpherePos.x, -halfA.x, halfA.x),
						std::clamp(localSpherePos.y, -halfA.y, halfA.y),
						std::clamp(localSpherePos.z, -halfA.z, halfA.z)
					);
					Vector3 localDelta = localSpherePos - localClamped;
					float distSq = localDelta.LengthSquared();

					if (distSq > 0.000001f)
					{
						if (distSq < colB.radius * colB.radius)
						{
							float dist = std::sqrt(distSq);
							Vector3 localNorm = localDelta / dist;
							normal = Vector3::Transform(localNorm, transA.rotation);
							penetration = colB.radius - dist;
							isColliding = true;
						}
					}
					else
					{
						// Sphere center is inside Box A: resolve along closest exit face in local space
						float distPosX = halfA.x - localSpherePos.x;
						float distNegX = localSpherePos.x - (-halfA.x);
						float distPosY = halfA.y - localSpherePos.y;
						float distNegY = localSpherePos.y - (-halfA.y);
						float distPosZ = halfA.z - localSpherePos.z;
						float distNegZ = localSpherePos.z - (-halfA.z);

						float minDist = distPosY;
						Vector3 localNorm = Vector3(0.0f, 1.0f, 0.0f);

						if (distPosX < minDist) { minDist = distPosX; localNorm = Vector3(1.0f, 0.0f, 0.0f); }
						if (distNegX < minDist) { minDist = distNegX; localNorm = Vector3(-1.0f, 0.0f, 0.0f); }
						if (distPosY < minDist) { minDist = distPosY; localNorm = Vector3(0.0f, 1.0f, 0.0f); }
						if (distNegY < minDist) { minDist = distNegY; localNorm = Vector3(0.0f, -1.0f, 0.0f); }
						if (distPosZ < minDist) { minDist = distPosZ; localNorm = Vector3(0.0f, 0.0f, 1.0f); }
						if (distNegZ < minDist) { minDist = distNegZ; localNorm = Vector3(0.0f, 0.0f, -1.0f); }

						normal = Vector3::Transform(localNorm, transA.rotation);
						penetration = colB.radius + minDist;
						isColliding = true;
					}
				}
				else if (colA.shape == Core::ColliderShape::Sphere && colB.shape == Core::ColliderShape::Box)
				{
					// Sphere A vs Rotated Box B: evaluate in Box B's local space
					Vector3 halfB = colB.size * 0.5f;
					Quaternion invRotB;
					transB.rotation.Inverse(invRotB);
					Vector3 localSpherePos = Vector3::Transform(transA.position - transB.position, invRotB);

					Vector3 localClamped = Vector3(
						std::clamp(localSpherePos.x, -halfB.x, halfB.x),
						std::clamp(localSpherePos.y, -halfB.y, halfB.y),
						std::clamp(localSpherePos.z, -halfB.z, halfB.z)
					);
					Vector3 localDelta = localSpherePos - localClamped;
					float distSq = localDelta.LengthSquared();

					if (distSq > 0.000001f)
					{
						if (distSq < colA.radius * colA.radius)
						{
							float dist = std::sqrt(distSq);
							Vector3 localNorm = -localDelta / dist;
							normal = Vector3::Transform(localNorm, transB.rotation);
							penetration = colA.radius - dist;
							isColliding = true;
						}
					}
					else
					{
						// Sphere center is inside Box B: resolve along closest exit face in local space
						float distPosX = halfB.x - localSpherePos.x;
						float distNegX = localSpherePos.x - (-halfB.x);
						float distPosY = halfB.y - localSpherePos.y;
						float distNegY = localSpherePos.y - (-halfB.y);
						float distPosZ = halfB.z - localSpherePos.z;
						float distNegZ = localSpherePos.z - (-halfB.z);

						float minDist = distPosY;
						Vector3 exitNormal = Vector3(0.0f, 1.0f, 0.0f);

						if (distPosX < minDist) { minDist = distPosX; exitNormal = Vector3(1.0f, 0.0f, 0.0f); }
						if (distNegX < minDist) { minDist = distNegX; exitNormal = Vector3(-1.0f, 0.0f, 0.0f); }
						if (distPosY < minDist) { minDist = distPosY; exitNormal = Vector3(0.0f, 1.0f, 0.0f); }
						if (distNegY < minDist) { minDist = distNegY; exitNormal = Vector3(0.0f, -1.0f, 0.0f); }
						if (distPosZ < minDist) { minDist = distPosZ; exitNormal = Vector3(0.0f, 0.0f, 1.0f); }
						if (distNegZ < minDist) { minDist = distNegZ; exitNormal = Vector3(0.0f, 0.0f, -1.0f); }

						normal = -Vector3::Transform(exitNormal, transB.rotation);
						penetration = colA.radius + minDist;
						isColliding = true;
					}
				}

				if (isColliding)
				{
					auto pairKey = (entA < entB) ? std::make_pair(entA, entB) : std::make_pair(entB, entA);

					if (colA.isTrigger || colB.isTrigger)
					{
						if (iter == 0)
						{
							currentTriggers.insert(pairKey);
						}
					}
					else
					{
						if (iter == 0)
						{
							currentCollisions.insert(pairKey);
						}

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

								// Friction impulse (Coulomb friction model)
								Vector3 tangentVel = relVel - normal * normVel;
								float tangentSpeed = tangentVel.Length();
								if (tangentSpeed > 0.0001f)
								{
									Vector3 tangent = tangentVel / tangentSpeed;
									float fA = rbA ? rbA->friction : 0.5f;
									float fB = rbB ? rbB->friction : 0.5f;
									float friction = std::sqrt(fA * fB);
									float maxFrictionImpulse = friction * impulseMag;
									float dynamicFrictionImpulse = std::min(maxFrictionImpulse, tangentSpeed / totalInvMass);
									Vector3 fImpulse = tangent * dynamicFrictionImpulse;

									if (invMassA > 0.0f && stateA)
									{
										stateA->linearVelocity += fImpulse * invMassA;
									}
									if (invMassB > 0.0f && stateB)
									{
										stateB->linearVelocity -= fImpulse * invMassB;
									}
								}
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
