#pragma once
#include "Core/Graphics/Prerequisites.h"
#include "Core/Math/Vector3d.h"
#include <typeinfo>
#include <vector>
#include <string>
#include "World.h"
#include "Display.h"
#include "IComponent.h"
#include "Component/TransformComponent.h"
#include "Component/CameraComponent.h"
#include "Component/LightComponent.h"
#include "Component/MeshComponent.h"
#include "Component/ColliderComponent.h"

namespace Core
{
	class IEntity
	{
	public:
		IEntity() = default;
		virtual ~IEntity() = default;
	public:
		IEntity(const IEntity& other) = delete;
		IEntity& operator=(const IEntity& other) = delete;
		IEntity(IEntity&& other) = delete;
		IEntity& operator=(IEntity&& other) = delete;
	public:
		virtual void onCreate() = 0;
		virtual void onDestroy() = 0;
		virtual void onUpdate(float delta) = 0;
	public:
        template<typename T>
        T& getComponent()
        {
            static_assert(std::is_base_of<IComponent, T>::value, "T must inherit from IComponent");
            return m_World->getECS()->Get<T>(m_ID);
        }
		template<typename T>
		bool hasComponent()
		{
			static_assert(std::is_base_of<IComponent, T>::value, "T must inherit from List");
			return m_World->getECS()->Has<T>(m_ID);
		}
		template<typename T>
		void addComponent(T data)
		{
			static_assert(std::is_base_of<IComponent, T>::value, "T must inherit from List");
			m_World->getECS()->Add<T>(m_ID, std::move(data));
		}
		template<typename T>
		void removeComponent()
		{
			static_assert(std::is_base_of<IComponent, T>::value, "T must inherit from List");
			m_World->getECS()->Remove<T>(m_ID);
		}
	public:
		EntityID getID() const { return m_ID; }
		const char* getName() const { return m_Name; }
		void remove() 
		{ 
			onDestroy();
			m_World->removeEntity(m_ID);
			m_World->getECS()->DeleteEntity(m_ID);
		}
	protected:
		EntityID m_ID;
		const char* m_Name;
		WorldPtr m_World;
	private:
		friend class World;
		void setEntityID(EntityID id) { m_ID = id; }
		void setEntityName(const char* name) { m_Name = name; }
		void setWorld(WorldPtr world) { m_World = world; }
	};

	// Default entity classes

	class Entity : public IEntity
	{
	public:
		Entity() = default;
		virtual ~Entity() = default;
	public:
		virtual void onCreate() override 
		{
			// Add default components
			addComponent<TransformComponent>(TransformComponent({0,0,0},{0,0,0},{1,1,1}));
		}
		virtual void onDestroy() override {}
		virtual void onUpdate(float delta) override {}
	};

	class CameraEntity : public Entity
	{
	public:
		CameraEntity(float farPlane): m_farPlane(farPlane){}
		virtual ~CameraEntity() = default;
	public:
		virtual void onCreate() override
		{
			// Add default components
			Entity::onCreate();
			addComponent<CameraComponent>(CameraComponent(m_farPlane,m_World->getGame()->getDisplay()->getClientSize()));
		}
		virtual void onDestroy() override {}
		virtual void onUpdate(float delta) override {}
	private:
		float m_farPlane = 100.0f;
	};

	class LightEntity : public Entity
	{
	public:
		LightEntity(Vector3d color) : m_color(color) {}
		virtual ~LightEntity() = default;
	public:
		virtual void onCreate() override
		{
			// Add default components
			Entity::onCreate();
			addComponent<LightComponent>(LightComponent(m_color,1));
		}
		virtual void onDestroy() override {}
		virtual void onUpdate(float delta) override {}
	private:
		Vector3d m_color = { 1,1,1 };
	};

	class ModelEntity : public Entity
	{
	public:
		ModelEntity() = default;
		ModelEntity(MeshPtr mesh, std::vector<MaterialPtr> materials) 
			: m_Mesh(mesh), m_Materials(materials) {}
		virtual ~ModelEntity() = default;
	public:
		virtual void onCreate() override
		{
			// Add default components
			Entity::onCreate();
			addComponent<MeshComponent>(MeshComponent(m_Mesh, m_Materials));
			addComponent<SphereColliderComponent>(SphereColliderComponent(1.0));
		}
		virtual void onDestroy() override {}
		virtual void onUpdate(float delta) override {}

		void setMesh(MeshPtr mesh) { m_Mesh = mesh; }
		void setMaterials(std::vector<MaterialPtr> materials) { m_Materials = materials; }
	private:
		MeshPtr m_Mesh;
		std::vector<MaterialPtr> m_Materials;
	};
}
