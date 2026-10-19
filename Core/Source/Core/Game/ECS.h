#pragma once
#include "Core/Log/log.h"
#include "Core/Util/String.h"
#include "Core/Game/SparseSet.h"
#include "Core/Game/ComponentTypeID.h"
#include "Core/Graphics/Prerequisites.h"
#include <stdexcept>
#include <typeindex>

namespace Core
{
	constexpr size_t MAX_ENTITIES = 1000000;
	constexpr size_t MAX_COMPONENTS = 64;

	class ECS
	{
	public:
		ECS(Game* game) : m_game(game){}
		
		/// Check if we're currently iterating (unsafe to delete entities)
		bool isIterating() const { return m_iterationDepth > 0; }
		
		/// Mark an entity for destruction (safe to call during iteration)
		void markForDestroy(EntityID id) {
			if (m_iterationDepth > 0) {
				m_pendingDestroy.push_back(id);
			} else {
				DeleteEntity(id);
			}
		}
		
		/// Process all pending entity destructions
		void flushPendingDestroy() {
			for (auto id : m_pendingDestroy) {
				DeleteEntity(id);
			}
			m_pendingDestroy.clear();
		}
		
		/// Get count of entities
		size_t getEntityCount() const { return m_maxEntityID - m_availableEntities.size(); }
		
		/// Get count of a specific component type
		template<typename T>
		size_t getComponentCount() {
			auto* pool = tryGetComponentPool<T>();
			return pool ? pool->Data().size() : 0;
		}

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
			for (size_t i = 0; i < m_componentPools.size(); i++)
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
			std::type_index typeIdx(typeid(T));
			if (m_componentBitPosition.find(typeIdx) != m_componentBitPosition.end())
			{
				darklog.warn(L"Component with name '" + ToWide(std::string(typeid(T).name())) + L"' already registered");
				return;
			}
				

			if (m_componentPools.size() >= MAX_COMPONENTS)
			{
				darklog.error(L"Exceeded max number of components");
				return;
			}
				

			m_componentBitPosition.emplace(typeIdx, m_componentPools.size());
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
			SparseSet<T>* pool = tryGetComponentPool<T>();
			if (!pool) {
				darklog.error(L"Attempting to get unregistered component '" + ToWide(std::string(typeid(T).name())) + L"'");
				throw std::runtime_error("Unregistered component type");
			}

			T* component = pool->Get(id);
			if (!component) {
				darklog.error(L"Entity " + std::to_wstring(id) + L" has no component " + ToWide(std::string(typeid(T).name())));
				throw std::runtime_error("Entity does not have component");
			}

			return *component;
		}

		template <typename T>
		void Remove(EntityID id) {
			if (id == NULL_ENTITY) return;
			if (id == tombstone) return;

			SparseSet<T>* pool = tryGetComponentPool<T>();
			if (!pool || !pool->Get(id))
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

			pool->Delete(id);
		}

		template <typename... Ts>
		bool HasAll(EntityID id) {
			return (Has<Ts>(id) && ...);
		}

		template <typename T>
		bool Has(EntityID id) {
			SparseSet<T>* pool = tryGetComponentPool<T>();
			if (!pool) return false;
			return pool->Get(id) != nullptr;
		}

		
		template <typename ...Components>
		std::vector<EntityID> ViewIDs() {
			std::vector<EntityID> result;
			
			// Check if all components are registered
			if (!(tryGetComponentPool<Components>() && ...)) {
				return result; // Return empty if any component is unregistered
			}
			
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
			
			// Check if all components are registered
			if (!(tryGetComponentPool<Components>() && ...)) {
				return result; // Return empty if any component is unregistered
			}
			
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
		// Check if all components are registered
		if (!(tryGetComponentPool<Components>() && ...)) {
		return; // Skip if any component is unregistered
		}
			
		ComponentMask targetMask = GetMask<Components...>();
			
			// Track iteration depth for safe destruction
			m_iterationDepth++;
			
			// Cache the pools for faster access during iteration
			auto pools = std::make_tuple(tryGetComponentPool<Components>()...);

			for (auto& [mask, ids] : m_groupings) {
				if ((mask & targetMask) == targetMask) {
					// Copy IDs to allow modification during iteration
					auto idsCopy = ids.Data();
					for (EntityID id : idsCopy) {
						// Skip if entity was marked for destruction
						if (std::find(m_pendingDestroy.begin(), m_pendingDestroy.end(), id) != m_pendingDestroy.end())
							continue;

						if constexpr (std::is_invocable_v<Func, EntityID, Components&...>) {
							func(id, *std::get<SparseSet<Components>*>(pools)->Get(id)...);
						}
						else if constexpr (std::is_invocable_v<Func, Components&...>) {
							func(*std::get<SparseSet<Components>*>(pools)->Get(id)...);
						}
						else {
							static_assert(std::is_invocable_v<Func, Components&...>, 
								"Lambda must accept (EntityID, Components&...) or (Components&...)");
						}
					}
				}
			}
			
			m_iterationDepth--;
			
			// Flush pending destroys when we exit the outermost iteration
			if (m_iterationDepth == 0) {
				flushPendingDestroy();
			}
		}
		Game* getGame() { return m_game; }
		
	private:
		using ComponentMask = std::bitset<MAX_COMPONENTS>;
		using TypeName = std::type_index;
		
		// Iteration safety
		int m_iterationDepth = 0;
		std::vector<EntityID> m_pendingDestroy;

		template <typename T>
		size_t GetComponentBitPosition() {
			std::type_index typeIdx(typeid(T));
			auto it = m_componentBitPosition.find(typeIdx);
			if (it == m_componentBitPosition.end())
				return tombstone;

			return it->second;
		}
		
		/// Try to get a component pool without registering (returns nullptr if not found)
		template <typename T>
		SparseSet<T>* tryGetComponentPool() {
			size_t bitPos = GetComponentBitPosition<T>();
			if (bitPos == tombstone || bitPos >= m_componentPools.size())
				return nullptr;
			return dynamic_cast<SparseSet<T>*>(m_componentPools[bitPos].get());
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
					darklog.error(L"Attempting to operate on unregistered component '" + ToWide(std::string(typeid(T).name())) + L"'");
					throw std::runtime_error("Unregistered component type");
				}
			}

			if (bitPos >= m_componentPools.size()) {
				darklog.error(L"(Internal): Attempting to index into m_componentPools with out of range bit position");
				throw std::runtime_error("Component pool index out of range");
			}

			// Downcast the generic pointer to the specific sparse set
			ISparseSet* genericPtr = m_componentPools[bitPos].get();
			SparseSet<T>* pool = dynamic_cast<SparseSet<T>*>(genericPtr);
			if (!pool) {
				darklog.error(L"Dynamic cast failed for component pool '" + ToWide(std::string(typeid(T).name())) + L"'");
				throw std::runtime_error("Component pool type mismatch");
			}

			return *pool;
		}

		template <typename Component>
		void SetComponentBit(ComponentMask& mask, bool val) {
			size_t bitPos = GetComponentBitPosition<Component>();
			if (bitPos == tombstone || bitPos >= MAX_COMPONENTS) {
				darklog.warn(L"Attempting to operate on unregistered component '" + ToWide(std::string(typeid(Component).name())) + L"'");
				return;
			}

			mask[bitPos] = val;
		}

		template <typename Component>
		ComponentMask::reference GetComponentBit(ComponentMask& mask) {
			size_t bitPos = GetComponentBitPosition<Component>();
			if (bitPos == tombstone || bitPos >= MAX_COMPONENTS) {
				darklog.warn(L"Attempting to operate on unregistered component '" + ToWide(std::string(typeid(Component).name())) + L"'");
				return mask[0]; // Return reference to first bit as fallback
			}

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
			Game* m_game = nullptr;
			EntityID m_maxEntityID = 0;
			static constexpr size_t tombstone = (std::numeric_limits<size_t>::max)();
	};
}