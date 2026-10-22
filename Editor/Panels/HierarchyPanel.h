#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Entity.h"
#include "Editor/SelectionContext.h"
#include "Editor/Commands/CommandHistory.h"
#include <memory>
#include <string>

namespace Editor
{
	class HierarchyPanel
	{
	public:
		HierarchyPanel() = default;
		HierarchyPanel(std::shared_ptr<Core::Scene> scene, SelectionContext* selection, CommandHistory* history);

		void setContext(std::shared_ptr<Core::Scene> scene, SelectionContext* selection, CommandHistory* history);
		void setScene(std::shared_ptr<Core::Scene> scene);

		void onImGuiRender(bool* p_open = nullptr);

		Core::Entity getSelectedEntity() const noexcept
		{
			return m_selection ? m_selection->getPrimary() : Core::Entity{};
		}

		void setSelectedEntity(Core::Entity entity) noexcept
		{
			if (m_selection)
			{
				m_selection->select(entity, false);
			}
		}

	private:
		void drawEntityNode(Core::Entity entity);
		void drawContextMenu(Core::Entity entity);
		void drawPanelContextMenu();
		void drawAddNodeModal();

		std::shared_ptr<Core::Scene> m_scene;
		SelectionContext* m_selection = nullptr;
		CommandHistory* m_history = nullptr;
		char m_searchFilter[128]{};

		bool m_showAddNodeModal = false;
		char m_addNodeFilter[64]{};
		int m_selectedNodeType = 0;
	};
}
