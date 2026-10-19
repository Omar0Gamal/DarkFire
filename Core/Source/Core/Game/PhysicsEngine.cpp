#include "PhysicsEngine.h"
#include "Game.h"
#include "World.h"
#include "Component/TransformComponent.h"
#include "Component/ColliderComponent.h"
#include "Component/RigidbodyComponent.h"
#include "Core/Graphics/DebugRenderer.h"
#include "Core/Log/Log.h"

#include <cmath>
#include <algorithm>

namespace Core
{
	PhysicsEngine::PhysicsEngine(Game* game)
		: m_Game(game)
	{
	}

	PhysicsEngine::~PhysicsEngine()
	{
	}

	bool PhysicsEngine::intersectRaySphere(const Ray& ray, const Vector3d& center, float radius, RaycastHit& outHit)
	{
		Vector3d m = ray.origin - center;
		float b = Vector3d::dot(m, ray.direction);
		float c = Vector3d::dot(m, m) - (radius * radius);

		if (c > 0.0f && b > 0.0f)
			return false;

		float discr = (b * b) - c;
		if (discr < 0.0f)
			return false;

		float t = -b - std::sqrt(discr);
		if (t < 0.0f)
			t = 0.0f;

		outHit.hasHit = true;
		outHit.distance = t;
		outHit.point = ray.getPoint(t);
		outHit.normal = Vector3d::normalize(outHit.point - center);
		return true;
	}

	bool PhysicsEngine::intersectRayAABB(const Ray& ray, const Vector3d& min, const Vector3d& max, RaycastHit& outHit)
	{
		float tmin = -1e9f;
		float tmax = 1e9f;
		Vector3d hitNormal(0, 1, 0);

		// X slab
		if (std::abs(ray.direction.x) > 1e-6f)
		{
			float t1 = (min.x - ray.origin.x) / ray.direction.x;
			float t2 = (max.x - ray.origin.x) / ray.direction.x;
			Vector3d n1(-1, 0, 0);
			Vector3d n2(1, 0, 0);
			if (t1 > t2) { std::swap(t1, t2); std::swap(n1, n2); }
			if (t1 > tmin) { tmin = t1; hitNormal = n1; }
			tmax = (std::min)(tmax, t2);
			if (tmin > tmax) return false;
		}
		else if (ray.origin.x < min.x || ray.origin.x > max.x) return false;

		// Y slab
		if (std::abs(ray.direction.y) > 1e-6f)
		{
			float t1 = (min.y - ray.origin.y) / ray.direction.y;
			float t2 = (max.y - ray.origin.y) / ray.direction.y;
			Vector3d n1(0, -1, 0);
			Vector3d n2(0, 1, 0);
			if (t1 > t2) { std::swap(t1, t2); std::swap(n1, n2); }
			if (t1 > tmin) { tmin = t1; hitNormal = n1; }
			tmax = (std::min)(tmax, t2);
			if (tmin > tmax) return false;
		}
		else if (ray.origin.y < min.y || ray.origin.y > max.y) return false;

		// Z slab
		if (std::abs(ray.direction.z) > 1e-6f)
		{
			float t1 = (min.z - ray.origin.z) / ray.direction.z;
			float t2 = (max.z - ray.origin.z) / ray.direction.z;
			Vector3d n1(0, 0, -1);
			Vector3d n2(0, 0, 1);
			if (t1 > t2) { std::swap(t1, t2); std::swap(n1, n2); }
			if (t1 > tmin) { tmin = t1; hitNormal = n1; }
			tmax = (std::min)(tmax, t2);
			if (tmin > tmax) return false;
		}
		else if (ray.origin.z < min.z || ray.origin.z > max.z) return false;

		if (tmax < 0.0f) return false;

		float t = tmin > 0.0f ? tmin : tmax;
		outHit.hasHit = true;
		outHit.distance = t;
		outHit.point = ray.getPoint(t);
		outHit.normal = hitNormal;
		return true;
	}

	bool PhysicsEngine::intersectRayPlane(const Ray& ray, const Vector3d& planePoint, const Vector3d& planeNormal, RaycastHit& outHit)
	{
		float denom = Vector3d::dot(planeNormal, ray.direction);
		if (std::abs(denom) > 1e-6f)
		{
			float t = Vector3d::dot(planePoint - ray.origin, planeNormal) / denom;
			if (t >= 0.0f)
			{
				outHit.hasHit = true;
				outHit.distance = t;
				outHit.point = ray.getPoint(t);
				outHit.normal = denom < 0.0f ? planeNormal : -planeNormal;
				return true;
			}
		}
		return false;
	}

	bool PhysicsEngine::raycast(const Ray& ray, RaycastHit& outHit, float maxDistance)
	{
		if (!m_Game || !m_Game->getActiveWorld())
			return false;

		bool hitFound = false;
		float closestDist = maxDistance;
		RaycastHit closestHit = {};

		// 1. Raycast against ground plane (y = 0)
		RaycastHit groundHit;
		if (intersectRayPlane(ray, Vector3d(0, 0, 0), Vector3d(0, 1, 0), groundHit))
		{
			if (groundHit.distance < closestDist)
			{
				closestDist = groundHit.distance;
				closestHit = groundHit;
				hitFound = true;
			}
		}

		// 2. Raycast against Sphere Colliders
		m_Game->getActiveWorld()->getECS()->ForEach<TransformComponent, SphereColliderComponent>(
			[&](TransformComponent& transform, SphereColliderComponent& sphere)
			{
				Vector3d center = transform.m_Position + sphere.centerOffset;
				float radius = sphere.radius * (std::max)({ transform.m_Scale.x, transform.m_Scale.y, transform.m_Scale.z });

				RaycastHit hit;
				if (intersectRaySphere(ray, center, radius, hit))
				{
					if (hit.distance < closestDist)
					{
						closestDist = hit.distance;
						closestHit = hit;
						hitFound = true;
					}
				}
			});

		// 3. Raycast against Box Colliders
		m_Game->getActiveWorld()->getECS()->ForEach<TransformComponent, BoxColliderComponent>(
			[&](TransformComponent& transform, BoxColliderComponent& box)
			{
				Vector3d center = transform.m_Position + box.centerOffset;
				Vector3d scaledExtents = Vector3d(box.halfExtents.x * transform.m_Scale.x,
				                                  box.halfExtents.y * transform.m_Scale.y,
				                                  box.halfExtents.z * transform.m_Scale.z);

				Vector3d min = center - scaledExtents;
				Vector3d max = center + scaledExtents;

				RaycastHit hit;
				if (intersectRayAABB(ray, min, max, hit))
				{
					if (hit.distance < closestDist)
					{
						closestDist = hit.distance;
						closestHit = hit;
						hitFound = true;
					}
				}
			});

		if (hitFound)
		{
			outHit = closestHit;
			return true;
		}

		return false;
	}

	bool PhysicsEngine::raycastDown(const Vector3d& origin, RaycastHit& outHit, float maxDistance)
	{
		Ray ray;
		ray.origin = origin;
		ray.direction = Vector3d(0, -1, 0);
		return raycast(ray, outHit, maxDistance);
	}

	void PhysicsEngine::update(float deltaTime)
	{
		if (!m_Game || !m_Game->getActiveWorld())
			return;

		auto ecs = m_Game->getActiveWorld()->getECS();
		m_spatialGrid.clear();

		// 1. Semi-Implicit Euler Rigidbody Integration
		ecs->ForEach<TransformComponent, RigidbodyComponent>(
			[&](TransformComponent& transform, RigidbodyComponent& rb)
			{
				if (rb.isKinematic)
					return;

				// Gravity
				if (rb.useGravity)
				{
					rb.velocity.y -= m_gravity * deltaTime;
				}

				// Linear Drag
				float dragFactor = (std::max)(0.0f, 1.0f - (rb.linearDrag * deltaTime));
				rb.velocity = rb.velocity * dragFactor;

				// Positional Integration
				transform.m_Position += rb.velocity * deltaTime;

				// Prevent falling below ground plane
				if (transform.m_Position.y < 0.0f)
				{
					transform.m_Position.y = 0.0f;
					if (rb.velocity.y < 0.0f)
					{
						rb.velocity.y = -rb.velocity.y * rb.bounciness;
						if (std::abs(rb.velocity.y) < 0.5f)
							rb.velocity.y = 0.0f;
					}
				}

				rb.force = Vector3d(0, 0, 0);
				transform.proccessMatrix();
			});

		// 2. Resolve Collisions
		resolveCollisions(deltaTime);

		// 3. Debug Visualization (F1 toggle)
		if (DebugRenderer::get().isEnabled())
		{
			// Draw ground grid
			for (int x = -100; x <= 100; x += 20)
			{
				DebugRenderer::get().drawLine(Vector3d(static_cast<float>(x), 0, -100), Vector3d(static_cast<float>(x), 0, 100), Vector4d(0.2f, 0.2f, 0.3f, 1));
			}
			for (int z = -100; z <= 100; z += 20)
			{
				DebugRenderer::get().drawLine(Vector3d(-100, 0, static_cast<float>(z)), Vector3d(100, 0, static_cast<float>(z)), Vector4d(0.2f, 0.2f, 0.3f, 1));
			}

			// Draw spheres
			ecs->ForEach<TransformComponent, SphereColliderComponent>(
				[](TransformComponent& transform, SphereColliderComponent& sphere)
				{
					Vector3d center = transform.m_Position + sphere.centerOffset;
					float radius = sphere.radius * (std::max)({ transform.m_Scale.x, transform.m_Scale.y, transform.m_Scale.z });
					Vector4d color = sphere.isTrigger ? Vector4d(1, 0.5f, 0, 1) : Vector4d(0, 1, 0, 1);
					DebugRenderer::get().drawWireSphere(center, radius, color);
				});

			// Draw boxes
			ecs->ForEach<TransformComponent, BoxColliderComponent>(
				[](TransformComponent& transform, BoxColliderComponent& box)
				{
					Vector3d center = transform.m_Position + box.centerOffset;
					Vector3d extents = Vector3d(box.halfExtents.x * transform.m_Scale.x,
					                            box.halfExtents.y * transform.m_Scale.y,
					                            box.halfExtents.z * transform.m_Scale.z);
					Vector4d color = box.isTrigger ? Vector4d(1, 0.5f, 0, 1) : Vector4d(0, 1, 1, 1);
					DebugRenderer::get().drawWireBox(center, extents, color);
				});
		}
	}

	void PhysicsEngine::resolveCollisions(float deltaTime)
	{
		if (!m_Game || !m_Game->getActiveWorld())
			return;

		auto ecs = m_Game->getActiveWorld()->getECS();

		// Bumper impulse resolution between dynamic bodies
		struct BodyRef
		{
			TransformComponent* transform;
			SphereColliderComponent* collider;
			RigidbodyComponent* rigidbody;
		};

		std::vector<BodyRef> bodies;
		ecs->ForEach<TransformComponent, SphereColliderComponent, RigidbodyComponent>(
			[&](TransformComponent& transform, SphereColliderComponent& sphere, RigidbodyComponent& rb)
			{
				bodies.push_back({ &transform, &sphere, &rb });
			});

		for (size_t i = 0; i < bodies.size(); ++i)
		{
			for (size_t j = i + 1; j < bodies.size(); ++j)
			{
				auto& a = bodies[i];
				auto& b = bodies[j];

				Vector3d posA = a.transform->m_Position + a.collider->centerOffset;
				Vector3d posB = b.transform->m_Position + b.collider->centerOffset;

				float radA = a.collider->radius * (std::max)({ a.transform->m_Scale.x, a.transform->m_Scale.y, a.transform->m_Scale.z });
				float radB = b.collider->radius * (std::max)({ b.transform->m_Scale.x, b.transform->m_Scale.y, b.transform->m_Scale.z });

				float dist = Vector3d::distance(posA, posB);
				float minDist = radA + radB;

				if (dist < minDist && dist > 0.0001f)
				{
					float overlap = minDist - dist;
					Vector3d normal = (posA - posB) / dist;

					// Positional separation
					if (!a.rigidbody->isKinematic && !b.rigidbody->isKinematic)
					{
						a.transform->m_Position += normal * (overlap * 0.5f);
						b.transform->m_Position -= normal * (overlap * 0.5f);
					}
					else if (!a.rigidbody->isKinematic)
					{
						a.transform->m_Position += normal * overlap;
					}
					else if (!b.rigidbody->isKinematic)
					{
						b.transform->m_Position -= normal * overlap;
					}

					// Elastic bounce impulse
					Vector3d relVel = a.rigidbody->velocity - b.rigidbody->velocity;
					float velAlongNormal = Vector3d::dot(relVel, normal);

					if (velAlongNormal < 0.0f)
					{
						float restitution = (std::max)(a.rigidbody->bounciness, b.rigidbody->bounciness);
						float impulseMag = -(1.0f + restitution) * velAlongNormal / (1.0f / a.rigidbody->mass + 1.0f / b.rigidbody->mass);
						Vector3d impulse = normal * impulseMag;

						a.rigidbody->addImpulse(impulse);
						b.rigidbody->addImpulse(-impulse);
					}
				}
			}
		}
	}
}