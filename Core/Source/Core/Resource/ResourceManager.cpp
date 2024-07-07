#include "ResourceManager.h"
#include "Texture.h"
#include "Mesh.h"
#include "Material.h"

#include <filesystem>
namespace Core
{
	ResourceManager::ResourceManager(GamePtr game) :m_game(game) {}

	ResourcePtr ResourceManager::createResourceFromFileConcrete(const wchar_t* path)
	{
		auto it = m_mapResources.find(path);

		std::filesystem::path resPath = path;
		auto ext = resPath.extension();

		if (it != m_mapResources.end())
		{
			if (!ext.compare(L".hlsl") || !ext.compare(L".fx"))
			{
				return std::make_shared<Material>(std::static_pointer_cast<Material>(it->second), this);
			}
			return it->second;
		}


		if (!std::filesystem::exists(resPath)) return ResourcePtr();


		if (!ext.compare(L".jpg") || !ext.compare(L".png") || !ext.compare(L".bmp") || !ext.compare(L".tga"))
		{
			//texture resource
			auto ptr = std::make_shared<Texture>(resPath.c_str(), this);
			if (ptr)
			{
				m_mapResources.emplace(path, ptr);
				return ptr;
			}
		}
		else if (!ext.compare(L".obj"))
		{
			//mesh resource
			auto ptr = std::make_shared<Mesh>(resPath.c_str(), this);
			if (ptr)
			{
				m_mapResources.emplace(path, ptr);
				return ptr;
			}
		}
		else if (!ext.compare(L".hlsl") || !ext.compare(L".fx"))
		{
			//material resource
			auto ptr = std::make_shared<Material>(resPath.c_str(), this);
			if (ptr)
			{
				m_mapResources.emplace(path, ptr);
				return ptr;
			}
		}

		return ResourcePtr();
	}

	GamePtr ResourceManager::getGame()
	{
		return m_game;
	}
}
