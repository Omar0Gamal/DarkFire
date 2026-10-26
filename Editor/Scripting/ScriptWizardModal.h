#pragma once

#include "Engine/Core/Defines.h"
#include <string>
#include <filesystem>

namespace Editor
{
	class ScriptWizardModal
	{
	public:
		void open()
		{
			m_isOpen = true;
			m_scriptName[0] = '\0';
			m_statusMessage.clear();
		}

		void onImGuiRender(const std::filesystem::path& projectDir);

	private:
		bool m_isOpen = false;
		char m_scriptName[128]{};
		int m_selectedTemplate = 0;
		std::string m_statusMessage;
	};
}
