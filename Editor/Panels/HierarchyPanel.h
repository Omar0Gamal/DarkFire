#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Entity.h"
#include <memory>
#include <string>

namespace Editor
{
	class HierarchyPanel
	{
	public:
		HierarchyPanel() = default;
		explicit HierarchyPanel(std::shared_ptr<Core::Scene> scene);

		void setScene(std::shared_ptr<Core::Scene> scene);
		void onImGuiRender();

		Core::Entity getSelectedEntity() const noexcept { return m_selectedEntity; }
		void setSelectedEntity(Core::Entity entity) noexcept { m_selectedEntity = entity; }

	private:
		void drawEntityNode(Core::Entity entity);
		void drawContextMenu(Core::Entity entity);
		void drawPanelContextMenu();

		std::shared_ptr<Core::Scene> m_scene;
		Core::Entity m_selectedEntity{};
		char m_searchFilter[128]{};
	};
}
