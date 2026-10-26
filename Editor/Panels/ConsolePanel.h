#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Log/Log.h"
#include <imgui.h>
#include <string>
#include <vector>

namespace Editor
{
	class ConsolePanel
	{
	public:
		ConsolePanel();

		void onImGuiRender(bool* p_open = nullptr);

	private:
		static int TextEditCallback(struct ImGuiInputTextCallbackData* data);

		bool m_showTrace = true;
		bool m_showInfo = true;
		bool m_showWarn = true;
		bool m_showError = true;
		bool m_showFatal = true;
		bool m_autoScroll = true;
		int m_selectedIndex = -1;

		char m_filterBuffer[128]{};
		char m_inputBuffer[256]{};
		std::vector<std::string> m_history;
		int m_historyIndex = -1;
		bool m_reclaimFocus = false;
	};
}
