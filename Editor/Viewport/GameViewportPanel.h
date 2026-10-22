#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/RHI/IRHI.h"
#include <memory>
#include <cstdint>

namespace Editor
{
	class GameViewportPanel
	{
	public:
		GameViewportPanel() = default;

		void onImGuiRender(
			std::shared_ptr<Core::RHI::IGpuTexture> gameTexture,
			bool* p_open = nullptr
		);

		uint32_t getWidth() const noexcept { return m_width; }
		uint32_t getHeight() const noexcept { return m_height; }
		uint32_t getPendingWidth() const noexcept { return m_pendingWidth; }
		uint32_t getPendingHeight() const noexcept { return m_pendingHeight; }
		void clearPendingResize() noexcept { m_pendingWidth = 0; m_pendingHeight = 0; }
		void setSize(uint32_t width, uint32_t height) { m_width = width; m_height = height; }

		int getAspectPreset() const noexcept { return m_aspectPresetIndex; }
		void setAspectPreset(int preset) noexcept { m_aspectPresetIndex = preset; }

	private:
		uint32_t m_width = 1280;
		uint32_t m_height = 720;
		uint32_t m_pendingWidth = 0;
		uint32_t m_pendingHeight = 0;
		int m_aspectPresetIndex = 1; // 0: Free, 1: 16:9, 2: 16:10, 3: 4:3
	};
}
