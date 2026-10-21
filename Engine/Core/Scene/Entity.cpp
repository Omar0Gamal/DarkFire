#include "Entity.h"
#include "Scene.h"

namespace Core
{
	bool Entity::isValid() const noexcept
	{
		return m_handle != entt::null && m_scene != nullptr && m_scene->getRegistry().valid(m_handle);
	}

	UUID Entity::getUUID() const
	{
		DF_ASSERT(isValid(), "Entity is invalid!");
		return getComponent<IdentityComponent>().id;
	}

	const std::string& Entity::getName() const
	{
		DF_ASSERT(isValid(), "Entity is invalid!");
		return getComponent<IdentityComponent>().name;
	}

	TransformComponent& Entity::getTransform()
	{
		return getComponent<TransformComponent>();
	}

	const TransformComponent& Entity::getTransform() const
	{
		return getComponent<TransformComponent>();
	}

	Matrix Entity::getWorldMatrix() const
	{
		DF_ASSERT(isValid(), "Entity is invalid!");
		if (hasComponent<WorldTransform>())
		{
			return getComponent<WorldTransform>().worldMatrix;
		}
		return getTransform().getLocalMatrix();
	}

	Entity Entity::getParent() const
	{
		DF_ASSERT(isValid(), "Entity is invalid!");
		auto& h = getComponent<HierarchyComponent>();
		if (h.parent == entt::null)
		{
			return Entity{};
		}
		return Entity{ h.parent, m_scene };
	}

	std::vector<Entity> Entity::getChildren() const
	{
		DF_ASSERT(isValid(), "Entity is invalid!");
		auto& h = getComponent<HierarchyComponent>();
		std::vector<Entity> result;
		result.reserve(h.children.size());
		for (auto childHandle : h.children)
		{
			result.emplace_back(childHandle, m_scene);
		}
		return result;
	}

	void Entity::setParent(Entity newParent, bool keepWorldTransform)
	{
		DF_ASSERT(isValid(), "Entity is invalid!");
		m_scene->reparent(*this, newParent, keepWorldTransform);
	}

	void Entity::destroy()
	{
		if (isValid())
		{
			m_scene->destroyEntity(*this);
			m_handle = entt::null;
			m_scene = nullptr;
		}
	}
}
