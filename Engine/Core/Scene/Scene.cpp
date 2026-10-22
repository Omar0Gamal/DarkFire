#include "Scene.h"
#include "Engine/Core/Reflection/Reflection.h"
#include <algorithm>

namespace Core
{
	Scene::Scene()
	{
	}

	Scene::~Scene()
	{
		m_entityMap.clear();
		m_registry.clear();
	}

	Entity Scene::createEntity(const std::string& name)
	{
		return createEntityWithUUID(UUID(), name);
	}

	Entity Scene::createEntityWithUUID(UUID uuid, const std::string& name)
	{
		DF_ASSERT(uuid.isValid(), "Attempted to create entity with invalid UUID!");
		DF_ASSERT(m_entityMap.find(uuid) == m_entityMap.end(), "Entity with UUID already exists in scene!");

		entt::entity handle = m_registry.create();
		Entity entity(handle, this);

		entity.addComponent<IdentityComponent>(uuid, name);
		entity.addComponent<TransformComponent>();
		entity.addComponent<HierarchyComponent>();
		entity.addComponent<WorldTransform>();

		m_entityMap[uuid] = handle;
		return entity;
	}

	void Scene::destroyEntity(Entity entity)
	{
		if (!entity.isValid()) return;

		entt::entity handle = entity.getHandle();
		auto& h = m_registry.get<HierarchyComponent>(handle);

		// Recursively destroy all children
		auto childrenCopy = h.children;
		for (auto child : childrenCopy)
		{
			destroyEntity(Entity(child, this));
		}

		// Detach from parent
		if (h.parent != entt::null && m_registry.valid(h.parent))
		{
			auto& parentH = m_registry.get<HierarchyComponent>(h.parent);
			auto it = std::find(parentH.children.begin(), parentH.children.end(), handle);
			if (it != parentH.children.end())
			{
				parentH.children.erase(it);
			}
		}

		UUID id = entity.getUUID();
		m_entityMap.erase(id);
		m_registry.destroy(handle);
	}

	Entity Scene::findEntityByUUID(UUID uuid) const
	{
		auto it = m_entityMap.find(uuid);
		if (it != m_entityMap.end() && m_registry.valid(it->second))
		{
			return Entity(it->second, const_cast<Scene*>(this));
		}
		return Entity{};
	}

	Entity Scene::findEntityByName(std::string_view name) const
	{
		auto view = m_registry.view<IdentityComponent>();
		for (auto entity : view)
		{
			const auto& idComp = view.get<IdentityComponent>(entity);
			if (idComp.name == name)
			{
				return Entity(entity, const_cast<Scene*>(this));
			}
		}
		return Entity{};
	}

	bool Scene::containsEntity(UUID uuid) const
	{
		return m_entityMap.find(uuid) != m_entityMap.end();
	}

	void Scene::reparent(Entity entity, Entity newParent, bool keepWorldTransform)
	{
		DF_ASSERT(entity.isValid(), "Entity must be valid to reparent!");
		entt::entity handle = entity.getHandle();
		entt::entity newParentHandle = newParent ? newParent.getHandle() : entt::null;

		if (handle == newParentHandle) return; // Cannot parent to self

		auto& h = m_registry.get<HierarchyComponent>(handle);
		if (h.parent == newParentHandle) return; // Already parented to this

		// Prevent parenting to a descendant (cycle prevention)
		if (newParentHandle != entt::null)
		{
			entt::entity ancestor = newParentHandle;
			while (ancestor != entt::null && m_registry.valid(ancestor))
			{
				if (ancestor == handle)
				{
					DF_ASSERT(false, "Cannot reparent an entity to one of its own descendants!");
					return;
				}
				ancestor = m_registry.get<HierarchyComponent>(ancestor).parent;
			}
		}

		if (keepWorldTransform)
		{
			updateTransforms();
			Matrix worldMat = m_registry.get<WorldTransform>(handle).worldMatrix;

			if (newParentHandle != entt::null)
			{
				Matrix parentWorld = m_registry.get<WorldTransform>(newParentHandle).worldMatrix;
				Matrix invParent = Math::affineInverse(parentWorld);
				Matrix localMat = worldMat * invParent;

				auto& tc = m_registry.get<TransformComponent>(handle);
				Math::decomposeTRS(localMat, tc.position, tc.rotation, tc.scale);
				Vector3 rads = tc.rotation.ToEuler();
				tc.eulerDegrees = Vector3(Math::toDegrees(rads.x), Math::toDegrees(rads.y), Math::toDegrees(rads.z));
			}
			else
			{
				auto& tc = m_registry.get<TransformComponent>(handle);
				Math::decomposeTRS(worldMat, tc.position, tc.rotation, tc.scale);
				Vector3 rads = tc.rotation.ToEuler();
				tc.eulerDegrees = Vector3(Math::toDegrees(rads.x), Math::toDegrees(rads.y), Math::toDegrees(rads.z));
			}
		}

		// Remove from old parent
		if (h.parent != entt::null && m_registry.valid(h.parent))
		{
			auto& oldParentH = m_registry.get<HierarchyComponent>(h.parent);
			auto it = std::find(oldParentH.children.begin(), oldParentH.children.end(), handle);
			if (it != oldParentH.children.end())
			{
				oldParentH.children.erase(it);
			}
		}

		// Attach to new parent
		h.parent = newParentHandle;
		if (newParentHandle != entt::null)
		{
			auto& newParentH = m_registry.get<HierarchyComponent>(newParentHandle);
			newParentH.children.push_back(handle);
		}

		m_registry.get<WorldTransform>(handle).isDirty = true;
	}

	void Scene::updateTransforms()
	{
		Matrix identity = DirectX::XMMatrixIdentity();
		auto view = m_registry.view<HierarchyComponent, TransformComponent, WorldTransform>();

		for (auto entity : view)
		{
			const auto& h = view.get<HierarchyComponent>(entity);
			if (h.parent == entt::null)
			{
				updateTransformRecursive(entity, identity, false);
			}
		}
	}

	void Scene::updateTransformRecursive(entt::entity e, const Matrix& parentWorld, bool parentDirty)
	{
		(void)parentDirty;
		auto& tc = m_registry.get<TransformComponent>(e);
		auto& wt = m_registry.get<WorldTransform>(e);

		Matrix local = tc.getLocalMatrix();
		wt.worldMatrix = local * parentWorld;
		wt.isDirty = false;

		const auto& h = m_registry.get<HierarchyComponent>(e);
		for (auto child : h.children)
		{
			if (m_registry.valid(child))
			{
				updateTransformRecursive(child, wt.worldMatrix, false);
			}
		}
	}

	std::unique_ptr<Scene> Scene::copy(const Scene& src)
	{
		auto dst = std::make_unique<Scene>();
		auto& registry = ComponentRegistry::Get();
		registry.registerBuiltinTypes();

		// 1. Create all entities with identical UUIDs
		for (const auto& [uuid, srcHandle] : src.m_entityMap)
		{
			const auto& idComp = src.m_registry.get<IdentityComponent>(srcHandle);
			dst->createEntityWithUUID(uuid, idComp.name);
		}

		// 2. Copy all authored components for each entity
		const auto& allComponents = registry.getAll();
		for (const auto& [uuid, srcHandle] : src.m_entityMap)
		{
			entt::entity dstHandle = dst->m_entityMap.at(uuid);

			for (const auto& desc : allComponents)
			{
				if (desc.isAuthored && desc.hasComponent(src, srcHandle))
				{
					desc.copyComponent(src, srcHandle, *dst, dstHandle);
				}
			}

			// Remap HierarchyComponent handles
			const auto& srcH = src.m_registry.get<HierarchyComponent>(srcHandle);
			auto& dstH = dst->m_registry.get<HierarchyComponent>(dstHandle);

			if (srcH.parent != entt::null && src.m_registry.valid(srcH.parent))
			{
				UUID parentUUID = src.m_registry.get<IdentityComponent>(srcH.parent).id;
				dstH.parent = dst->m_entityMap.at(parentUUID);
			}
			else
			{
				dstH.parent = entt::null;
			}

			dstH.children.clear();
			dstH.children.reserve(srcH.children.size());
			for (auto srcChild : srcH.children)
			{
				if (src.m_registry.valid(srcChild))
				{
					UUID childUUID = src.m_registry.get<IdentityComponent>(srcChild).id;
					dstH.children.push_back(dst->m_entityMap.at(childUUID));
				}
			}
		}

		// 3. Rebuild world transforms fresh in dst
		dst->updateTransforms();
		return dst;
	}
}
