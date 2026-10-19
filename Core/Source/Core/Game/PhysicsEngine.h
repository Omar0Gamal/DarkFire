#pragma once
#include "Core/Graphics/Prerequisites.h"
#include "Core/Math/Vector3d.h"
#include "Component/RigidbodyComponent.h"
#include "Component/ColliderComponent.h"
#include "Core/Game/ECS.h"
#include <vector>
#include <unordered_map>

namespace Core
{
	struct Ray
	{
		Vector3d origin;
		Vector3d direction; // Normalized

		Vector3d getPoint(float t) const
		{
			return origin + (direction * t);
		}
	};

	struct RaycastHit
	{
		bool hasHit = false;
		Vector3d point = { 0, 0, 0 };
		Vector3d normal = { 0, 1, 0 };
		float distance = 1e9f;
		EntityID entityID = NULL_ENTITY;
	};

	class SpatialGrid
	{
	public:
		SpatialGrid(float cellSize = 10.0f) : m_cellSize(cellSize) {}

		void clear()
		{
			m_grid.clear();
		}

		void insert(EntityID id, const Vector3d& pos)
		{
			int cx = static_cast<int>(std::floor(pos.x / m_cellSize));
			int cy = static_cast<int>(std::floor(pos.y / m_cellSize));
			int cz = static_cast<int>(std::floor(pos.z / m_cellSize));
			int64_t key = (static_cast<int64_t>(cx) & 0x1FFFFF) |
			              ((static_cast<int64_t>(cy) & 0x1FFFFF) << 21) |
			              ((static_cast<int64_t>(cz) & 0x1FFFFF) << 42);
			m_grid[key].push_back(id);
		}

		std::vector<EntityID> getNearby(const Vector3d& pos)
		{
			std::vector<EntityID> result;
			int cx = static_cast<int>(std::floor(pos.x / m_cellSize));
			int cy = static_cast<int>(std::floor(pos.y / m_cellSize));
			int cz = static_cast<int>(std::floor(pos.z / m_cellSize));

			for (int dx = -1; dx <= 1; ++dx)
			{
				for (int dy = -1; dy <= 1; ++dy)
				{
					for (int dz = -1; dz <= 1; ++dz)
					{
						int64_t key = (static_cast<int64_t>(cx + dx) & 0x1FFFFF) |
						              ((static_cast<int64_t>(cy + dy) & 0x1FFFFF) << 21) |
						              ((static_cast<int64_t>(cz + dz) & 0x1FFFFF) << 42);
						auto it = m_grid.find(key);
						if (it != m_grid.end())
						{
							result.insert(result.end(), it->second.begin(), it->second.end());
						}
					}
				}
			}
			return result;
		}

	private:
		float m_cellSize = 10.0f;
		std::unordered_map<int64_t, std::vector<EntityID>> m_grid;
	};

	class PhysicsEngine
	{
	public:
		PhysicsEngine(Game* game);
		~PhysicsEngine();

		void update(float deltaTime);

		// 3D Raycasting against all colliders
		bool raycast(const Ray& ray, RaycastHit& outHit, float maxDistance = 1000.0f);

		// Raycast specifically down to detect ground surface and normal
		bool raycastDown(const Vector3d& origin, RaycastHit& outHit, float maxDistance = 50.0f);

		void resolveCollisions(float deltaTime);

		Game* getGame() { return m_Game; }

		// Global physics controls
		void setGravity(float g) { m_gravity = g; }
		float getGravity() const { return m_gravity; }

	public:
		static bool intersectRaySphere(const Ray& ray, const Vector3d& center, float radius, RaycastHit& outHit);
		static bool intersectRayAABB(const Ray& ray, const Vector3d& min, const Vector3d& max, RaycastHit& outHit);
		static bool intersectRayPlane(const Ray& ray, const Vector3d& planePoint, const Vector3d& planeNormal, RaycastHit& outHit);

	private:
		Game* m_Game = nullptr;
		SpatialGrid m_spatialGrid;
		float m_gravity = 9.8f;
	};
}
