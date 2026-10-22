#include "Editor/Viewport/GameViewportPanel.h"
#include "Editor/UI/DockLayout.h"
#include <imgui.h>

namespace Editor
{
	void GameViewportPanel::onImGuiRender(
		std::shared_ptr<Core::RHI::IGpuTexture> gameTexture,
		bool* p_open)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin(UI::PanelNames::GameViewport, p_open);

		ImVec2 avail = ImGui::GetContentRegionAvail();
		uint32_t newW = static_cast<uint32_t>(avail.x);
		uint32_t newH = static_cast<uint32_t>(avail.y);

		if (newW > 0 && newH > 0 && (newW != m_width || newH != m_height))
		{
			m_pendingWidth = newW;
			m_pendingHeight = newH;
		}

		if (gameTexture && gameTexture->getShaderResourceView())
		{
			ImGui::Image(
				(ImTextureID)gameTexture->getShaderResourceView(),
				avail
			);
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}
}
