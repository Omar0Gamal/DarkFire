#pragma once
#include <map>
#include <vector>
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

		void addSearchPath(const std::wstring& path);
		std::wstring resolvePath(const std::wstring& path);
		void unloadUnused();
	private:
		ResourcePtr createResourceFromFileConcrete(const wchar_t* path);
	private:
		std::map<std::wstring, ResourcePtr> m_mapResources;
		std::vector<std::wstring> m_searchPaths;
		MaterialPtr m_defaultMaterial = nullptr;
		Game* m_game = nullptr;
	};
}

