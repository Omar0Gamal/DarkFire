#pragma once

#include "Engine/Core/Defines.h"
#include <vector>
#include <cstdint>

namespace Editor
{
	class ProfilerPanel
	{
	public:
		ProfilerPanel();
		~ProfilerPanel() = default;

		void onImGuiRender(float deltaTime, bool* p_open = nullptr);

	private:
		void drawPerformanceGraphs();
		void drawSubsystemBreakdown();
		void drawGpuCaptureControls();

		static constexpr size_t k_historySize = 120;
		std::vector<float> m_frameTimeHistory;
		std::vector<float> m_fpsHistory;
		size_t m_historyOffset = 0;

		float m_maxFrameTime = 33.3f;
		float m_minFrameTime = 0.0f;
		float m_avgFrameTime = 16.6f;
		float m_currentFps = 60.0f;

		bool m_renderDocAvailable = false;
		bool m_pixAvailable = false;
		bool m_captureNextFrame = false;
		void* m_renderDocApi = nullptr;

		void initRenderDoc();
		void triggerCapture();
	};
}
