#pragma once
#include "Resource.h"
#include "Core/Graphics/Prerequisites.h"

namespace Core
{
	class Font : public Resource
	{
	public:
		Font(const wchar_t* full_path, ResourceManager* manager);
	public:
		const Font2DPtr& getFont();
	private:
		Font2DPtr m_font;
	};

	class FontUtility
	{
	public:
		bool createFont(const wchar_t* font_name, float size, const wchar_t* outputPath);
	};
}
