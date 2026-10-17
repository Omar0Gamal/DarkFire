#include "Font.h"
#include "Core/Graphics/Font2D.h"
#include "ResourceManager.h"
#include "Core/Game/Game.h"
#include "Core/Graphics/GraphicsEngine.h"
#include "Core/Log/Log.h"
#include <sstream>
#include <regex>
#include <filesystem>

namespace Core
{
	Font::Font(const wchar_t* full_path, ResourceManager* manager)
		: Resource(full_path, manager)
	{
		m_font = m_resManager->getGame()->getGraphicsEngine()->CreateFont2D(full_path);
	}
	const Font2DPtr& Font::getFont()
	{
		return m_font;
	}
	bool FontUtility::createFont(const wchar_t* font_name, float size, const wchar_t* outputPath)
	{
		std::wstringstream ss;
		std::wstring s = std::regex_replace(font_name, std::wregex(L"\\s+"), L"");

		ss << outputPath << s << ".font";

		if (!std::filesystem::exists(ss.str()))
		{
			std::wstringstream command;
			command << "Libs\\DirectXTK\\bin\\MakeSpriteFont ";
			command << "\"" << font_name << "\" ";
			command << ss.str() << " ";
			command << "/FontSize:" << size;

			if (_wsystem(command.str().c_str()) != 0)
			{
				darklog.error(L"Failed to create font: " + std::wstring(font_name));
			}
			return true;
		}
		return true;
	}
}
