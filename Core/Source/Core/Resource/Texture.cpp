#include "Texture.h"
#include "ResourceManager.h"
#include "Core/Game/Game.h"
#include "Core/Graphics/GraphicsEngine.h"
#include "Core/Graphics/Texture2D.h"
#include "Core/Log/Log.h"

namespace Core
{
	Texture::Texture(const wchar_t* full_path, ResourceManager* manager) : Resource(full_path, manager)
	{
		m_texture = manager->getGame()->getGraphicsEngine()->CreateTexture2D(full_path);
		if (!m_texture)
		{
			darklog.warn(L"CXTexture - Static Texture : Creation failed");
		}
	}

	Texture::Texture(const TextureDesc& desc, ResourceManager* manager) : Resource(L"", manager)
	{
		Texture2DDesc desc2d = {};
		desc2d.size = desc.size;
		desc2d.type = desc.type;

		m_texture = manager->getGame()->getGraphicsEngine()->CreateTexture2D(desc2d);
		if (!m_texture) darklog.warn(L"CXTexture - Static Texture : Creation failed");
	}

	Texture2DPtr Texture::getTexture2D()
	{
		return m_texture;
	}
}
