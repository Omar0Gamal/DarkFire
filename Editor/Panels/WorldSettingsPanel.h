#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Scene/Scene.h"
#include <memory>

namespace Editor
{
	class WorldSettingsPanel
	{
	public:
		WorldSettingsPanel() = default;

		void setScene(std::shared_ptr<Core::Scene> scene)
		{
			m_scene = std::move(scene);
		}

		void onImGuiRender(bool* p_open = nullptr);

	private:
		std::shared_ptr<Core::Scene> m_scene;
	};
}
