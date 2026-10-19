#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Scene/Scene.h"

namespace Core
{
	class DF_API ScriptableEntity
	{
	public:
		virtual ~ScriptableEntity() = default;

		void bind(Entity entity, Scene* scene)
		{
			m_entity = entity;
			m_scene = scene;
		}

		Entity getEntity() const noexcept { return m_entity; }
		Scene* getScene() const noexcept { return m_scene; }

		template<typename T>
		T& getComponent() { return m_entity.getComponent<T>(); }

		template<typename T>
		const T& getComponent() const { return m_entity.getComponent<T>(); }

		template<typename T>
		T* tryGetComponent() { return m_entity.tryGetComponent<T>(); }

		template<typename T>
		const T* tryGetComponent() const { return m_entity.tryGetComponent<T>(); }

		template<typename T>
		bool hasComponent() const { return m_entity.hasComponent<T>(); }

		template<typename T, typename... Args>
		T& addComponent(Args&&... args) { return m_entity.addComponent<T>(std::forward<Args>(args)...); }

		template<typename T>
		void removeComponent() { m_entity.removeComponent<T>(); }

		virtual void onCreate() {}
		virtual void onStart() {}
		virtual void onUpdate(float deltaTime) { (void)deltaTime; }
		virtual void onFixedUpdate(float fixedDeltaTime) { (void)fixedDeltaTime; }
		virtual void onLateUpdate(float deltaTime) { (void)deltaTime; }
		virtual void onEnable() {}
		virtual void onDisable() {}
		virtual void onDestroy() {}
		virtual void onCollisionEnter(Entity other) { (void)other; }
		virtual void onCollisionExit(Entity other) { (void)other; }
		virtual void onTriggerEnter(Entity other) { (void)other; }
		virtual void onTriggerExit(Entity other) { (void)other; }
		virtual void onValidate() {}

	protected:
		Entity m_entity{};
		Scene* m_scene = nullptr;
	};
}
