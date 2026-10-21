#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Scene/Entity.h"

namespace Editor
{
	class InspectorPanel
	{
	public:
		InspectorPanel() = default;

		void onImGuiRender(Core::Entity selectedEntity);

	private:
		void drawComponents(Core::Entity entity);
	};
}
