#pragma once

#include "Resource.h"
#include "Core/Graphics/Prerequisites.h"
#include "Core/Math/Box.h"

namespace Core
{
	struct TextureDesc
	{
		Box size = {};
		TextureType type = TextureType::Normal;
	};

	class Texture : public Resource
	{
	public:
		Texture(const wchar_t* full_path, ResourceManager* manager);
		Texture(const TextureDesc& desc, ResourceManager* manager);
		Texture2DPtr getTexture2D();
	private:
		Texture2DPtr m_texture;
		friend class  GraphicsEngine;
	};
}

