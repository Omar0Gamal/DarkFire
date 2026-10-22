#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Scene/Entity.h"
#include "Editor/Commands/CommandHistory.h"

namespace Editor
{
	class InspectorPanel
	{
	public:
		InspectorPanel() = default;

		void setHistory(CommandHistory* history) noexcept { m_history = history; }
		void onImGuiRender(Core::Entity selectedEntity, bool* p_open = nullptr);

	private:
		void drawComponents(Core::Entity entity);

		CommandHistory* m_history = nullptr;
		Core::TransformComponent m_dragInitialTransform{};
		bool m_isDraggingTransform = false;
		bool m_uniformScale = false;
		int m_selectedTab = 0; // 0 = Inspector, 1 = Environment, 2 = History
	};
}
