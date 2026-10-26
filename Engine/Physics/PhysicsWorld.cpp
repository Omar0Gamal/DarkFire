#include "Engine/Physics/PhysicsWorld.h"
#include "Engine/Physics/PhysicsCollisionNarrowphase.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Scripting/ScriptableEntity.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Renderer/TerrainGenerator.h"
#include <algorithm>
#include <cmath>
#include <limits>
#include <unordered_map>

#ifdef min
#undef min
#endif
#ifdef max
#undef max
#endif

namespace Physics
{
	using Core::Matrix;
	using Core::Quaternion;

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

		// Sync previous transforms for all explicitly interpolated entities
		auto interpView = reg.view<Core::TransformComponent>();
		for (auto entity : interpView)
		{
			auto& tc = interpView.get<Core::TransformComponent>(entity);
			if (tc.interpolate)
			{
				tc.syncPreviousTransform();
			}
		}

		// 1. Integrate dynamic rigidbodies
		auto rbView = reg.view<Core::RigidbodyComponent, Core::TransformComponent>();
		for (auto entity : rbView)
		{
			auto& rb = rbView.get<Core::RigidbodyComponent>(entity);
			auto& transform = rbView.get<Core::TransformComponent>(entity);
			auto& state = reg.get_or_emplace<Core::RigidbodyRuntimeState>(entity);

			transform.syncPreviousTransform();

			if (!rb.isKinematic)
			{
				if (rb.useGravity)
				{
					state.linearVelocity += m_gravity * fixedTimeStep;
				}

				// Water Archimedes Buoyancy & Fluid Drag
				float colliderRadius = 0.5f;
				auto* col = reg.try_get<Core::ColliderComponent>(entity);
				if (col)
				{
					if (col->shape == Core::ColliderShape::Sphere) colliderRadius = col->radius;
					else if (col->shape == Core::ColliderShape::Box) colliderRadius = col->size.y * 0.5f;
					else if (col->shape == Core::ColliderShape::Capsule) colliderRadius = col->height * 0.5f;
				}

				auto waterView = reg.view<Core::WaterComponent>();
				for (auto waterEntity : waterView)
				{
					const auto& water = waterView.get<Core::WaterComponent>(waterEntity);
					float surfaceY = water.waterLevel + std::sin(transform.position.x * water.waveFrequency) * water.waveAmplitude * 0.5f;

					if (transform.position.y < surfaceY + colliderRadius)
					{
						float submerged = std::clamp((surfaceY + colliderRadius - transform.position.y) / (2.0f * colliderRadius), 0.0f, 1.0f);
						state.linearVelocity.y += (9.81f * water.buoyancyDensity * submerged) * fixedTimeStep;
						state.linearVelocity *= std::max(0.0f, 1.0f - water.fluidDrag * submerged * fixedTimeStep);
					}
				}

				state.linearVelocity *= std::max(0.0f, 1.0f - rb.drag * fixedTimeStep);
				state.angularVelocity *= std::max(0.0f, 1.0f - rb.angularDrag * fixedTimeStep);

				transform.position += state.linearVelocity * fixedTimeStep;
			}
		}

		// 2. Collision detection & resolution (Multi-iteration constraint solver with 3D Spatial-Hash Grid Broadphase)
		std::vector<entt::entity> colliderEntities;
		auto colView = reg.view<Core::ColliderComponent, Core::TransformComponent>();
		for (auto entity : colView)
		{
			colliderEntities.push_back(entity);
		}

		std::set<std::pair<entt::entity, entt::entity>> currentCollisions;
		std::set<std::pair<entt::entity, entt::entity>> currentTriggers;

		size_t numColliders = colliderEntities.size();
		if (numColliders > 0)
		{
			// 3D Spatial-Hash Grid Broadphase
			constexpr float cellSize = 4.0f;
			constexpr float invCellSize = 1.0f / cellSize;
			constexpr int kMaxSpan = 8;

			std::unordered_map<uint64_t, std::vector<size_t>> grid;
			for (size_t i = 0; i < numColliders; ++i)
			{
				auto ent = colliderEntities[i];
				const auto& col = reg.get<Core::ColliderComponent>(ent);
				const auto& trans = reg.get<Core::TransformComponent>(ent);

				Vector3 minPt, maxPt;
				ComputeColliderWorldAABB(col, trans, minPt, maxPt);

				int minX = static_cast<int>(std::floor(minPt.x * invCellSize));
				int minY = static_cast<int>(std::floor(minPt.y * invCellSize));
				int minZ = static_cast<int>(std::floor(minPt.z * invCellSize));
				int maxX = static_cast<int>(std::floor(maxPt.x * invCellSize));
				int maxY = static_cast<int>(std::floor(maxPt.y * invCellSize));
				int maxZ = static_cast<int>(std::floor(maxPt.z * invCellSize));

				// Clamp cell span per object to at most 8 cells per axis so giant floor boxes don't flood the hash table
				maxX = std::min(maxX, minX + kMaxSpan - 1);
				maxY = std::min(maxY, minY + kMaxSpan - 1);
				maxZ = std::min(maxZ, minZ + kMaxSpan - 1);

				for (int z = minZ; z <= maxZ; ++z)
				{
					for (int y = minY; y <= maxY; ++y)
					{
						for (int x = minX; x <= maxX; ++x)
						{
							constexpr uint64_t p1 = 73856093ULL;
							constexpr uint64_t p2 = 19349663ULL;
							constexpr uint64_t p3 = 83492791ULL;
							uint64_t cellHash = (static_cast<uint64_t>(x) * p1) ^ (static_cast<uint64_t>(y) * p2) ^ (static_cast<uint64_t>(z) * p3);
							grid[cellHash].push_back(i);
						}
					}
				}
			}

			// Collect unique candidate index pairs (i < j)
			std::vector<std::pair<size_t, size_t>> candidatePairs;
			for (const auto& [cellHash, indices] : grid)
			{
				size_t count = indices.size();
				for (size_t a = 0; a < count; ++a)
				{
					for (size_t b = a + 1; b < count; ++b)
					{
						size_t idxA = indices[a];
						size_t idxB = indices[b];
						if (idxA > idxB) std::swap(idxA, idxB);
						else if (idxA == idxB) continue;
						candidatePairs.emplace_back(idxA, idxB);
					}
				}
			}
			std::sort(candidatePairs.begin(), candidatePairs.end());
			candidatePairs.erase(std::unique(candidatePairs.begin(), candidatePairs.end()), candidatePairs.end());

			// Filter by collision layer matrix
			candidatePairs.erase(
				std::remove_if(candidatePairs.begin(), candidatePairs.end(), [&](const std::pair<size_t, size_t>& pair) {
					const auto& colA = reg.get<Core::ColliderComponent>(colliderEntities[pair.first]);
					const auto& colB = reg.get<Core::ColliderComponent>(colliderEntities[pair.second]);
					return !canCollide(colA.layer, colB.layer);
				}),
				candidatePairs.end()
			);

			constexpr int kSolverIterations = 4;
			for (int iter = 0; iter < kSolverIterations; ++iter)
			{
				for (const auto& [i, j] : candidatePairs)
				{
					auto entA = colliderEntities[i];
					auto entB = colliderEntities[j];
					const auto& colA = reg.get<Core::ColliderComponent>(entA);
					auto& transA = reg.get<Core::TransformComponent>(entA);
					const auto& colB = reg.get<Core::ColliderComponent>(entB);
					auto& transB = reg.get<Core::TransformComponent>(entB);

					Vector3 normal(0.0f, 1.0f, 0.0f);
					float penetration = 0.0f;

					if (TestColliderPair(colA, transA, colB, transB, normal, penetration))
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

				// Heightfield Terrain Collision
				auto terrainView = reg.view<Core::TerrainComponent, Core::TransformComponent>();
				for (auto rbEntity : colliderEntities)
				{
					auto* rb = reg.try_get<Core::RigidbodyComponent>(rbEntity);
					if (!rb || rb->isKinematic) continue;

					const auto& col = reg.get<Core::ColliderComponent>(rbEntity);
					auto& trans = reg.get<Core::TransformComponent>(rbEntity);
					auto& state = reg.get_or_emplace<Core::RigidbodyRuntimeState>(rbEntity);

					float bottomOffset = col.radius;
					if (col.shape == Core::ColliderShape::Box) bottomOffset = col.size.y * 0.5f;
					else if (col.shape == Core::ColliderShape::Capsule) bottomOffset = col.height * 0.5f;

					float bottomY = trans.position.y - bottomOffset;

					for (auto terrainEntity : terrainView)
					{
						const auto& terrainComp = terrainView.get<Core::TerrainComponent>(terrainEntity);
						const auto& terrainTrans = terrainView.get<Core::TransformComponent>(terrainEntity);

						float localX = trans.position.x - terrainTrans.position.x;
						float localZ = trans.position.z - terrainTrans.position.z;

						float terrainHeight = Core::Renderer::TerrainGenerator::sampleHeight(terrainComp, localX, localZ) + terrainTrans.position.y;
						Vector3 terrainNormal = Core::Renderer::TerrainGenerator::sampleNormal(terrainComp, localX, localZ);

						if (bottomY < terrainHeight)
						{
							float penetration = terrainHeight - bottomY;
							trans.position += terrainNormal * penetration;

							auto pairKey = (rbEntity < terrainEntity) ? std::make_pair(rbEntity, terrainEntity) : std::make_pair(terrainEntity, rbEntity);
							if (iter == 0)
							{
								currentCollisions.insert(pairKey);
							}

							float normVel = state.linearVelocity.Dot(terrainNormal);
							if (iter == 0 && normVel < 0.0f)
							{
								float impulseMag = -(1.0f + rb->restitution) * normVel;
								state.linearVelocity += terrainNormal * impulseMag;

								if (rb->restitution == 0.0f && state.linearVelocity.y > 0.0f)
								{
									state.linearVelocity.y = 0.0f;
								}

								Vector3 tangentVel = state.linearVelocity - terrainNormal * state.linearVelocity.Dot(terrainNormal);
								float tangentSpeed = tangentVel.Length();
								if (tangentSpeed > 0.0001f)
								{
									Vector3 tangent = tangentVel / tangentSpeed;
									float frictionImpulse = std::min(rb->friction * impulseMag, tangentSpeed);
									state.linearVelocity -= tangent * frictionImpulse;
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

			float hitT = 0.0f;
			Vector3 hitNormal;
			if (RaycastCollider(col, trans, origin, normalizedDir, outHit.distance, hitT, hitNormal))
			{
				if (hitT < outHit.distance)
				{
					outHit.hasHit = true;
					outHit.distance = hitT;
					outHit.point = origin + normalizedDir * hitT;
					outHit.normal = hitNormal;
					outHit.entity = Core::Entity(entity, &scene);
				}
			}
		}

		// Raycast against TerrainComponent heightfields via ray-marching
		auto terrainView = reg.view<Core::TerrainComponent, Core::TransformComponent>();
		for (auto entity : terrainView)
		{
			const auto& terrain = terrainView.get<Core::TerrainComponent>(entity);
			const auto& trans = terrainView.get<Core::TransformComponent>(entity);

			float halfSize = terrain.worldSize * 0.5f;
			Vector3 minBox = trans.position + Vector3(-halfSize, terrain.seaFloorDepth - 10.0f, -halfSize);
			Vector3 maxBox = trans.position + Vector3(halfSize, terrain.maxHeight + 10.0f, halfSize);

			float tBoxMin = 0.0f;
			float tBoxMax = std::min(maxDist, outHit.distance);
			bool hitsBox = true;
			for (int i = 0; i < 3; ++i)
			{
				float origComp = (i == 0) ? origin.x : (i == 1 ? origin.y : origin.z);
				float dirComp = (i == 0) ? normalizedDir.x : (i == 1 ? normalizedDir.y : normalizedDir.z);
				float minComp = (i == 0) ? minBox.x : (i == 1 ? minBox.y : minBox.z);
				float maxComp = (i == 0) ? maxBox.x : (i == 1 ? maxBox.y : maxBox.z);

				if (std::abs(dirComp) < 0.00001f)
				{
					if (origComp < minComp || origComp > maxComp) { hitsBox = false; break; }
				}
				else
				{
					float ood = 1.0f / dirComp;
					float t1 = (minComp - origComp) * ood;
					float t2 = (maxComp - origComp) * ood;
					if (t1 > t2) std::swap(t1, t2);
					tBoxMin = std::max(tBoxMin, t1);
					tBoxMax = std::min(tBoxMax, t2);
					if (tBoxMin > tBoxMax) { hitsBox = false; break; }
				}
			}

			if (!hitsBox) continue;

			float stepSize = std::clamp(terrain.worldSize / static_cast<float>(terrain.resolution), 0.25f, 2.0f);
			float prevT = tBoxMin;

			for (float t = tBoxMin + stepSize; t <= tBoxMax; t += stepSize)
			{
				Vector3 pos = origin + normalizedDir * t;
				float h = Core::Renderer::TerrainGenerator::sampleHeight(terrain, pos.x - trans.position.x, pos.z - trans.position.z) + trans.position.y;
				float diff = pos.y - h;

				if (diff <= 0.0f)
				{
					float tLo = prevT;
					float tHi = t;
					for (int iter = 0; iter < 6; ++iter)
					{
						float tMid = (tLo + tHi) * 0.5f;
						Vector3 midPos = origin + normalizedDir * tMid;
						float midH = Core::Renderer::TerrainGenerator::sampleHeight(terrain, midPos.x - trans.position.x, midPos.z - trans.position.z) + trans.position.y;
						if (midPos.y <= midH)
						{
							tHi = tMid;
						}
						else
						{
							tLo = tMid;
						}
					}

					float hitT = (tLo + tHi) * 0.5f;
					if (hitT < outHit.distance)
					{
						outHit.hasHit = true;
						outHit.distance = hitT;
						outHit.point = origin + normalizedDir * hitT;
						outHit.normal = Core::Renderer::TerrainGenerator::sampleNormal(terrain, outHit.point.x - trans.position.x, outHit.point.z - trans.position.z);
						outHit.entity = Core::Entity(entity, &scene);
					}
					break;
				}

				prevT = t;
			}
		}

		return outHit.hasHit;
	}

	bool PhysicsWorld::sphereCast(
		Core::Scene& scene,
		const Vector3& origin,
		float radius,
		const Vector3& dir,
		float maxDist,
		SweepHit& outHit,
		uint16_t layerMask
	)
	{
		outHit.hasHit = false;
		outHit.distance = maxDist;

		Vector3 normalizedDir = dir;
		if (normalizedDir.LengthSquared() < 1e-6f) return false;
		normalizedDir.Normalize();

		auto& reg = scene.getRegistry();
		auto view = reg.view<Core::ColliderComponent, Core::TransformComponent>();

		for (auto entity : view)
		{
			const auto& col = view.get<Core::ColliderComponent>(entity);
			const auto& trans = view.get<Core::TransformComponent>(entity);

			if ((layerMask & (1 << col.layer)) == 0) continue;

			float hitT = maxDist + 1.0f;
			Vector3 hitNormal;
			Vector3 hitPoint;
			bool entityHit = false;

			if (col.shape == Core::ColliderShape::Sphere)
			{
				float rTotal = radius + col.radius;
				Vector3 m = origin - trans.position;
				float b = m.Dot(normalizedDir);
				float c = m.Dot(m) - rTotal * rTotal;

				if (c <= 0.0f)
				{
					hitT = 0.0f;
					hitNormal = -normalizedDir;
					hitPoint = origin;
					entityHit = true;
				}
				else if (b <= 0.0f)
				{
					float discr = b * b - c;
					if (discr >= 0.0f)
					{
						float t = -b - std::sqrt(discr);
						if (t >= 0.0f && t <= outHit.distance)
						{
							hitT = t;
							Vector3 sphereCenterAtT = origin + normalizedDir * t;
							hitNormal = sphereCenterAtT - trans.position;
							if (hitNormal.LengthSquared() > 1e-6f) hitNormal.Normalize();
							hitPoint = sphereCenterAtT - hitNormal * radius;
							entityHit = true;
						}
					}
				}
			}
			else
			{
				Core::ColliderComponent sweepCol;
				sweepCol.shape = Core::ColliderShape::Sphere;
				sweepCol.radius = radius;

				Core::TransformComponent sweepTrans;
				sweepTrans.rotation = Core::Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
				sweepTrans.scale = Vector3(1.0f, 1.0f, 1.0f);

				float currentT = 0.0f;
				float stepSize = std::max(0.05f, radius * 0.25f);

				while (currentT <= outHit.distance)
				{
					sweepTrans.position = origin + normalizedDir * currentT;
					Vector3 norm;
					float pen = 0.0f;

					if (TestColliderPair(sweepCol, sweepTrans, col, trans, norm, pen))
					{
						float tLo = std::max(0.0f, currentT - stepSize);
						float tHi = currentT;
						for (int iter = 0; iter < 8; ++iter)
						{
							float tMid = (tLo + tHi) * 0.5f;
							sweepTrans.position = origin + normalizedDir * tMid;
							Vector3 midNorm; float midPen = 0.0f;
							if (TestColliderPair(sweepCol, sweepTrans, col, trans, midNorm, midPen))
							{
								tHi = tMid;
								norm = midNorm;
							}
							else
							{
								tLo = tMid;
							}
						}
						hitT = tHi;
						if (norm.Dot(normalizedDir) > 0.0f) norm = -norm;
						hitNormal = norm;
						hitPoint = (origin + normalizedDir * hitT) - norm * radius;
						entityHit = true;
						break;
					}

					currentT += stepSize;
				}
			}

			if (entityHit && hitT < outHit.distance)
			{
				outHit.hasHit = true;
				outHit.distance = hitT;
				outHit.normal = hitNormal;
				outHit.point = hitPoint;
				outHit.entity = Core::Entity(entity, &scene);
			}
		}

		return outHit.hasHit;
	}

	bool PhysicsWorld::boxCast(
		Core::Scene& scene,
		const Vector3& origin,
		const Vector3& halfExtents,
		const Quaternion& orientation,
		const Vector3& dir,
		float maxDist,
		SweepHit& outHit,
		uint16_t layerMask
	)
	{
		outHit.hasHit = false;
		outHit.distance = maxDist;

		Vector3 normalizedDir = dir;
		if (normalizedDir.LengthSquared() < 1e-6f) return false;
		normalizedDir.Normalize();

		auto& reg = scene.getRegistry();
		auto view = reg.view<Core::ColliderComponent, Core::TransformComponent>();

		Core::ColliderComponent sweepCol;
		sweepCol.shape = Core::ColliderShape::Box;
		sweepCol.size = halfExtents * 2.0f;

		Core::TransformComponent sweepTrans;
		sweepTrans.rotation = orientation;
		sweepTrans.scale = Vector3(1.0f, 1.0f, 1.0f);

		for (auto entity : view)
		{
			const auto& col = view.get<Core::ColliderComponent>(entity);
			const auto& trans = view.get<Core::TransformComponent>(entity);

			if ((layerMask & (1 << col.layer)) == 0) continue;

			float currentT = 0.0f;
			float minExtent = std::min(halfExtents.x, std::min(halfExtents.y, halfExtents.z));
			float stepSize = std::max(0.05f, minExtent * 0.25f);
			bool entityHit = false;
			float hitT = maxDist + 1.0f;
			Vector3 hitNormal;
			Vector3 hitPoint;

			while (currentT <= outHit.distance)
			{
				sweepTrans.position = origin + normalizedDir * currentT;
				Vector3 norm;
				float pen = 0.0f;

				if (TestColliderPair(sweepCol, sweepTrans, col, trans, norm, pen))
				{
					float tLo = std::max(0.0f, currentT - stepSize);
					float tHi = currentT;
					for (int iter = 0; iter < 8; ++iter)
					{
						float tMid = (tLo + tHi) * 0.5f;
						sweepTrans.position = origin + normalizedDir * tMid;
						Vector3 midNorm; float midPen = 0.0f;
						if (TestColliderPair(sweepCol, sweepTrans, col, trans, midNorm, midPen))
						{
							tHi = tMid;
							norm = midNorm;
						}
						else
						{
							tLo = tMid;
						}
					}
					hitT = tHi;
					if (norm.Dot(normalizedDir) > 0.0f) norm = -norm;
					hitNormal = norm;
					hitPoint = (origin + normalizedDir * hitT) - norm * minExtent;
					entityHit = true;
					break;
				}

				currentT += stepSize;
			}

			if (entityHit && hitT < outHit.distance)
			{
				outHit.hasHit = true;
				outHit.distance = hitT;
				outHit.normal = hitNormal;
				outHit.point = hitPoint;
				outHit.entity = Core::Entity(entity, &scene);
			}
		}

		return outHit.hasHit;
	}
}
