#pragma once
#include "Core/Graphics/Prerequisites.h"
#include <string>
#include <vector>
#include <unordered_map>
#include "ECS.h"
namespace Core
{
	class World : public std::enable_shared_from_this<World>
	{
	public:
		World(const char* name,GamePtr game);
		~World();
	public:
		const char* getName() const;
	public:
        template<typename T, typename... Args>
		std::shared_ptr<T> createEntity(const char* name, Args&&... args)
        {
            static_assert(std::is_base_of<IEntity, T>::value, "T must inherit from IEntity");
            EntityID id = m_ECS->CreateEntity(std::string_view(name));
			auto entity = std::make_shared<T>(std::forward<Args>(args)...);
			entity->setEntityID(id);
			entity->setEntityName(name);
			entity->setWorld(shared_from_this());
			entity->onCreate();
            m_Entities.emplace(id, entity);
			m_EntityNames.emplace(id, name);
            return entity;
        }

		template<typename T>
		std::shared_ptr<T> getEntity(EntityID id)
		{
			static_assert(std::is_base_of<IEntity, T>::value, "T must inherit from IEntity");
			if (m_Entities.contains(id))
			{
				return std::static_pointer_cast<T>(m_Entities[id]);
			}
			return nullptr;
		}

		template<typename T>
		std::shared_ptr<T> getEntity(const char* name)
        {
            static_assert(std::is_base_of<IEntity, T>::value, "T must inherit from IEntity");
            for (auto entity : m_Entities)
            {
                if (entity.second->getName() == name)
                {
                    return std::static_pointer_cast<T>(entity.second);
                }
            }
            return nullptr;
        }
		template<typename T>
		std::vector<std::shared_ptr<T>> getAllEntities()
		{
			std::vector<std::shared_ptr<T>> entities;
			for (auto entity : m_Entities)
			{
				auto e = std::dynamic_pointer_cast<T>(entity.second);
				if (e)
				{
					entities.push_back(e);
				}
			}
			return entities;
		}
        template<typename T>
        void destroyEntity(EntityID id)
        {
            static_assert(std::is_base_of<IEntity, T>::value, "T must inherit from IEntity");
            auto it = m_Entities.find(id);
            if (it != m_Entities.end())
            {
				m_ECS->DeleteEntity(id);
                auto entity = it->second;
                m_Entities.erase(it);
            }
        }

		void removeEntity(EntityID id)
		{
			m_Entities.erase(id);
		}
	public:
		void setMainCamera(EntityID id);
		EntityID getMainCameraID() const;
	public:
		GamePtr getGame() { return m_Game; }
        ECSPtr& getECS() { return m_ECS; }
	public:
		std::string getEntityName(EntityID id) { return m_EntityNames[id]; }
		std::unordered_map<EntityID, std::shared_ptr<IEntity>> getEntities() { return m_Entities; }
	public:
		float getGravity() const { return m_gravity; }
		void setGravity(float gravity) { m_gravity = gravity; }
	private:
		ECSPtr m_ECS;
		GamePtr m_Game;
		std::string m_Name;
		EntityID mainCameraID;
		std::unordered_map<EntityID, std::shared_ptr<IEntity>> m_Entities;
		std::unordered_map<EntityID, std::string> m_EntityNames;
		float m_gravity = 9.8f;
	};
}
