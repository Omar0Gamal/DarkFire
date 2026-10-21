#pragma once

#include "Engine/Core/Scene/Entity.h"
#include <vector>
#include <algorithm>

namespace Editor
{
	class SelectionContext
	{
	public:
		SelectionContext() = default;

		void select(Core::Entity entity, bool additive = false)
		{
			if (!entity.isValid())
			{
				if (!additive)
				{
					clear();
				}
				return;
			}

			if (!additive)
			{
				m_selectedEntities.clear();
				m_selectedEntities.push_back(entity);
				m_primaryEntity = entity;
				return;
			}

			// Additive mode: toggle
			auto it = std::find(m_selectedEntities.begin(), m_selectedEntities.end(), entity);
			if (it != m_selectedEntities.end())
			{
				m_selectedEntities.erase(it);
				if (m_primaryEntity == entity)
				{
					m_primaryEntity = m_selectedEntities.empty() ? Core::Entity{} : m_selectedEntities.back();
				}
			}
			else
			{
				m_selectedEntities.push_back(entity);
				m_primaryEntity = entity;
			}
		}

		void deselect(Core::Entity entity)
		{
			auto it = std::find(m_selectedEntities.begin(), m_selectedEntities.end(), entity);
			if (it != m_selectedEntities.end())
			{
				m_selectedEntities.erase(it);
				if (m_primaryEntity == entity)
				{
					m_primaryEntity = m_selectedEntities.empty() ? Core::Entity{} : m_selectedEntities.back();
				}
			}
		}

		void clear()
		{
			m_selectedEntities.clear();
			m_primaryEntity = {};
		}

		bool isSelected(Core::Entity entity) const
		{
			if (!entity.isValid()) return false;
			return std::find(m_selectedEntities.begin(), m_selectedEntities.end(), entity) != m_selectedEntities.end();
		}

		Core::Entity getPrimary() const noexcept { return m_primaryEntity; }
		const std::vector<Core::Entity>& getEntities() const noexcept { return m_selectedEntities; }
		size_t getCount() const noexcept { return m_selectedEntities.size(); }
		bool isEmpty() const noexcept { return m_selectedEntities.empty(); }

		void removeInvalidEntities()
		{
			m_selectedEntities.erase(
				std::remove_if(m_selectedEntities.begin(), m_selectedEntities.end(), [](const Core::Entity& e) {
					return !e.isValid();
				}),
				m_selectedEntities.end()
			);

			if (!m_primaryEntity.isValid())
			{
				m_primaryEntity = m_selectedEntities.empty() ? Core::Entity{} : m_selectedEntities.back();
			}
		}

	private:
		std::vector<Core::Entity> m_selectedEntities;
		Core::Entity m_primaryEntity{};
	};
}
