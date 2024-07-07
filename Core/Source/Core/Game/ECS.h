#pragma once
#include "Core/Log/log.h"
#include "Core/Util/String.h"
#include "Core/Game/SparseSet.h"
#include "Game.h"

namespace Core
{
	constexpr size_t MAX_ENTITIES = 1000000;
	constexpr size_t MAX_COMPONENTS = 64;
	typedef uint64_t EntityID;
	static constexpr EntityID NULL_ENTITY = (std::numeric_limits<EntityID>::max)();

	class ECS
	{
	public:
		ECS(Game* game) : m_game(game){}

		EntityID CreateEntity(std::string_view name) {
            EntityID id = tombstone;

            if (m_availableEntities.size() == 0) {
                if (m_maxEntityID >= MAX_ENTITIES)
                    darklog.error(L"Exceeded max number of entities");
                id = m_maxEntityID++;
            }
            else {
                id = m_availableEntities.back();
                m_availableEntities.pop_back();
            }

            if (id == NULL_ENTITY)
                darklog.error(L"Entity ID is NULL_ENTITY");

            m_entityMasks.Set(id, {});

            if (!name.empty())
                m_entityNames[id] = name.data();

			return id;
        }

		std::string GetEntityName(EntityID id) {
			if (id == NULL_ENTITY) return "NULL_ENTITY";
			if (id == tombstone) return "TOMBSTONE";

			if (m_entityMasks.Get(id) == nullptr) {
				darklog.warn(L"Entity " + std::to_wstring(id) + L" has no component mask");
				return "Entity";
			}

			auto it = m_entityNames.find(id);
			if (it == m_entityNames.end())
				return "Entity";

			return it->second;
		}


		void DeleteEntity(EntityID& id) {
			if (id == NULL_ENTITY) return;
			if (id == tombstone) return;

			if (m_entityMasks.Get(id) == nullptr) {
				darklog.warn(L"Entity " + std::to_wstring(id) + L" has no component mask");
				return;
			}

			std::string name = GetEntityName(id);
			ComponentMask& mask = GetEntityMask(id);

			// Remove from group
			RemoveFromGroup(mask, id);

			// Destroy component associations
			for (int i = 0; i < MAX_COMPONENTS; i++)
				if (mask[i] == 1) {
					m_componentPools[i]->Delete(id);
				}

			m_entityMasks.Delete(id);
			m_entityNames.erase(id);
			m_availableEntities.push_back(id);

			id = NULL_ENTITY;
		}

		/*
		*  Register a component and create a pool for it
		*/
		template <typename T>
		void RegisterComponent() {
			TypeName name = typeid(T).name();
			if (m_componentBitPosition.find(name) != m_componentBitPosition.end())
			{
				darklog.warn(L"Component with name '" + ToWide(std::string(name)) + L"' already registered");
				return;
			}
				

			if (m_componentPools.size() >= MAX_COMPONENTS)
			{
				darklog.error(L"Exceeded max number of components");
				return;
			}
				

			m_componentBitPosition.emplace(name, m_componentPools.size());
			m_componentPools.push_back(std::make_unique<SparseSet<T>>());
		}

		template <typename T>
		T& Add(EntityID id, T&& component = {}) {
	
			if (m_entityMasks.Get(id) == nullptr) {
				darklog.warn(L"Entity " + std::to_wstring(id) + L" has no component mask");
			}

			// Do this first so component pool gets registered before Has<T>()
			SparseSet<T>& pool = GetComponentPool<T>(true);

			if(pool.Get(id))
			{
				darklog.warn(L"Entity " + std::to_wstring(id) + L" already has component " + ToWide(std::string(typeid(T).name())));
			}

			ComponentMask& mask = GetEntityMask(id);

			// Remove from old group
			RemoveFromGroup(mask, id);

			SetComponentBit<T>(mask, 1);

			// Add ID to new group
			AssignToGroup(mask, id);

			return *pool.Set(id, std::move(component));
		}


		template <typename T>
		T& Get(EntityID id) {
			SparseSet<T>& pool = GetComponentPool<T>();
			T* component = pool.Get(id);

			if (!component)
				darklog.warn(L"Entity " + std::to_wstring(id) + L" has no component " + ToWide(std::string(typeid(T).name())));

			return *component;
		}

		template <typename T>
		void Remove(EntityID id) {
			if (id == NULL_ENTITY) return;
			if (id == tombstone) return;

			SparseSet<T>& pool = GetComponentPool<T>();
			if (!pool.Get(id))
			{
				darklog.warn(L"Entity " + std::to_wstring(id) + L" does not have component " + ToWide(std::string(typeid(T).name())));
				return;
			}

			ComponentMask& mask = GetEntityMask(id);

			// Remove from old group
			RemoveFromGroup(mask, id);

			SetComponentBit<T>(mask, 0);

			// Shift to new group with new mask
			AssignToGroup(mask, id);

			pool.Delete(id);
		}

		template <typename... Ts>
		bool HasAll(EntityID id) {
			return (Has<Ts>(id) && ...);
		}

		template <typename T>
		bool Has(EntityID id) {
			SparseSet<T>& pool = GetComponentPool<T>();
			return pool.Get(id) ? true : false;
		}

		
		template <typename ...Components>
		std::vector<EntityID> ViewIDs() {
			std::vector<EntityID> result;
			ComponentMask targetMask = GetMask<Components...>();

			for (auto& [mask, ids] : m_groupings) {
				if ((mask & targetMask) == targetMask)
					result.insert(result.end(), ids.Data().begin(), ids.Data().end());
			}

			return result;
		}
		template <typename ...Components>
		std::vector<std::tuple<EntityID, Components&...>> View() {
			std::vector<std::tuple<EntityID, Components&...>> result;
			ComponentMask targetMask = GetMask<Components...>();

			for (auto& [mask, ids] : m_groupings) {
				if ((mask & targetMask) == targetMask) {
					for (EntityID id : ids.Data()) {
						result.emplace_back(id, Get<Components>(id)...);
					}
				}
			}

			return result;
		}


		template <typename ...Components, typename Func>
		void ForEach(Func&& func) {
			std::vector<std::tuple<EntityID, Components&...>> result;
			ComponentMask targetMask = GetMask<Components...>();

			for (auto& [mask, ids] : m_groupings) {
				if ((mask & targetMask) == targetMask) {
					for (EntityID id : ids.Data()) {

						if constexpr (std::is_invocable_v<Func, EntityID, Components&...>) {
							func(id, Get<Components>(id)...);
						}

						// This branch is for [](Component& c1, Component& c2);
						else if constexpr (std::is_invocable_v<Func, Components&...>) {
							func(Get<Components>(id)...);
						}

						else {
							darklog.error(L"Invalid lambda signature");
						}

					}
				}
			}
		}
		Game* getGame() { return m_game.get(); }
	private:
		using ComponentMask = std::bitset<MAX_COMPONENTS>;
		using TypeName = const char*;

		template <typename T>
		size_t GetComponentBitPosition() {
			TypeName name = typeid(T).name();
			auto it = m_componentBitPosition.find(name);
			if (it == m_componentBitPosition.end())
				return tombstone;

			return it->second;
		}
		template <typename T>
		SparseSet<T>& GetComponentPool(bool registerIfNotFound = false) {
			size_t bitPos = GetComponentBitPosition<T>();

			if (bitPos == tombstone) {
				if (registerIfNotFound) {
					RegisterComponent<T>();
					bitPos = GetComponentBitPosition<T>();
				}
				else {
					darklog.warn(L"Attempting to operate on unregistered component" + ToWide(std::string(typeid(T).name())));
				}
			}

			if (!(bitPos < m_componentPools.size() && bitPos >= 0)) {
				darklog.warn(L"(Internal): Attempting to index into m_componentPools with out of range bit position");
			}

			// Downcast the generic pointer to the specific sparse set
			ISparseSet* genericPtr = m_componentPools[bitPos].get();
			SparseSet<T>* pool = dynamic_cast<SparseSet<T>*>(genericPtr);
			if (!pool) {
				darklog.warn(L"Dynamic cast failed for component pool '" + ToWide(std::string(typeid(T).name())) + L"'");
			}

			return *pool;
		}

		template <typename Component>
		void SetComponentBit(ComponentMask& mask, bool val) {
			size_t bitPos = GetComponentBitPosition<Component>();
			if (bitPos == tombstone)
				darklog.warn(L"Attempting to operate on unregistered component '" + ToWide(std::string(typeid(Component).name())) + L"'");

			mask[bitPos] = val;
		}

		template <typename Component>
		ComponentMask::reference GetComponentBit(ComponentMask& mask) {
			size_t bitPos = GetComponentBitPosition<Component>();
			if (bitPos == tombstone)
				darklog.warn(L"Attempting to operate on unregistered component '" + ToWide(std::string(typeid(Component).name())) + L"'");

			return mask[bitPos];
		}

		ComponentMask& GetEntityMask(EntityID id) {
			ComponentMask* mask = m_entityMasks.Get(id);
			if (!mask)
                darklog.warn(std::wstring(L"Entity ") + std::to_wstring(id) + L" has no component mask");
			return *mask;
		}

		template <typename... Components>
		ComponentMask GetMask() {
			ComponentMask mask;
			(SetComponentBit<Components>(mask, 1), ...); // fold expression
			return mask;
		}

		// Removes an entity from it's group with its current mask
		void RemoveFromGroup(ComponentMask& mask, EntityID id) {
			if (mask.none()) return;

			SparseSet<EntityID>& group = GetGroupedEntities(mask);
			group.Delete(id);

			// Delete grouping if it's empty
			if (group.IsEmpty())
				m_groupings.erase(mask);
		}

		void AssignToGroup(ComponentMask& mask, EntityID id) {
			// If mask is empty, no group
			if (mask.none()) return;

			// Create group if it doesn't exist
			m_groupings.emplace(std::piecewise_construct,
				std::forward_as_tuple(mask),
				std::forward_as_tuple()); // Empty sparse set
			m_groupings[mask].Set(id, id);
		}

		SparseSet<EntityID>& GetGroupedEntities(ComponentMask& mask) {
			auto found = m_groupings.find(mask);
			if (found == m_groupings.end())
                darklog.warn(L"Cannot find group for entities with mask " + ToWide(std::to_string(mask.to_ullong())));
			return found->second;
		}
		
		private:
			std::vector<EntityID> m_availableEntities;
			SparseSet<ComponentMask> m_entityMasks;
			std::unordered_map<ComponentMask, SparseSet<EntityID>> m_groupings;
			std::unordered_map<EntityID, std::string> m_entityNames;
			std::vector<std::unique_ptr<ISparseSet>> m_componentPools;
			std::unordered_map<TypeName, size_t> m_componentBitPosition;
			std::unique_ptr<Game> m_game;
			EntityID m_maxEntityID = 0;
			static constexpr size_t tombstone = (std::numeric_limits<size_t>::max)();
	};
}