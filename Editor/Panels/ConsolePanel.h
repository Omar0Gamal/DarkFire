#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Log/Log.h"
#include <string>

namespace Editor
{
	class ConsolePanel
	{
	public:
		ConsolePanel();

		void onImGuiRender(bool* p_open = nullptr);

	private:
		bool m_showTrace = true;
		bool m_showInfo = true;
		bool m_showWarn = true;
		bool m_showError = true;
		bool m_showFatal = true;
		bool m_autoScroll = true;
		int m_selectedIndex = -1;

		char m_filterBuffer[128]{};
	};
}
