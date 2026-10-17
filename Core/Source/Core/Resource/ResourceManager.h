#pragma once
#include <map>
#include <string>
#include "Core/Graphics/Prerequisites.h"
#include "Resource.h"

namespace Core
{
	class ResourceManager
	{
	public:
		ResourceManager(Game* game);

		template <typename T>
		std::shared_ptr<T> createResourceFromFile(const wchar_t* path)
		{
			return std::dynamic_pointer_cast<T>(createResourceFromFileConcrete(path));
		}
		Game* getGame();
		MaterialPtr getDefaultMaterial();
	private:
		ResourcePtr createResourceFromFileConcrete(const wchar_t* path);
	private:
		std::map<std::wstring, ResourcePtr> m_mapResources;
		MaterialPtr m_defaultMaterial = nullptr;
		Game* m_game = nullptr;
	};
}

