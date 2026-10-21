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
		void onImGuiRender(Core::Entity selectedEntity);

	private:
		void drawComponents(Core::Entity entity);

		CommandHistory* m_history = nullptr;
		Core::TransformComponent m_dragInitialTransform{};
		bool m_isDraggingTransform = false;
	};
}
