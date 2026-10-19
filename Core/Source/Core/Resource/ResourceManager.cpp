#include "ResourceManager.h"
#include "Texture.h"
#include "Mesh.h"
#include "Material.h"
#include "Font.h"
#include "Core/Log/Log.h"

#include <filesystem>
#include <cwctype>
#include <algorithm>

#include "VirtualFileSystem.h"

namespace Core
{
	ResourceManager::ResourceManager(Game* game) : m_game(game)
	{
		// Default search paths
		m_searchPaths.push_back(L"");
		m_searchPaths.push_back(L"Assets/");
		m_searchPaths.push_back(L"SandBox/Assets/");
		m_searchPaths.push_back(L"Games/KartGame/Assets/");
	}

	void ResourceManager::addSearchPath(const std::wstring& path)
	{
		VirtualFileSystem::get().mountDirectory(path);

		std::wstring normalized = path;
		std::replace(normalized.begin(), normalized.end(), L'\\', L'/');
		if (!normalized.empty() && normalized.back() != L'/')
		{
			normalized += L'/';
		}
		if (std::find(m_searchPaths.begin(), m_searchPaths.end(), normalized) == m_searchPaths.end())
		{
			m_searchPaths.push_back(normalized);
		}
	}

	std::wstring ResourceManager::resolvePath(const std::wstring& path)
	{
		std::wstring resolved;
		if (VirtualFileSystem::get().resolvePhysicalPath(path, resolved))
		{
			return resolved;
		}

		std::wstring normalized = path;
		std::replace(normalized.begin(), normalized.end(), L'\\', L'/');

		// Check direct existence
		if (std::filesystem::exists(normalized))
		{
			return normalized;
		}

		// Check search paths
		for (const auto& sp : m_searchPaths)
		{
			std::wstring candidate = sp + normalized;
			if (std::filesystem::exists(candidate))
			{
				return candidate;
			}
		}

		return normalized; // Return normalized path even if missing so fallback can trigger
	}

	void ResourceManager::unloadUnused()
	{
		auto it = m_mapResources.begin();
		while (it != m_mapResources.end())
		{
			// If only m_mapResources holds a reference, it's unused
			if (it->second.use_count() <= 1)
			{
				it = m_mapResources.erase(it);
			}
			else
			{
				++it;
			}
		}
	}

	ResourcePtr ResourceManager::createResourceFromFileConcrete(const wchar_t* path)
	{
		std::wstring resolved = resolvePath(path);
		auto it = m_mapResources.find(resolved);

		std::filesystem::path resPath = resolved;
		auto ext = resPath.extension().wstring();
		for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));

		if (it != m_mapResources.end())
		{
			if (ext == L".hlsl" || ext == L".fx")
			{
				return std::make_shared<Material>(std::static_pointer_cast<Material>(it->second), this);
			}
			return it->second;
		}

		// Textures: DDS, TGA, PNG, JPG, JPEG, BMP
		if (ext == L".dds" || ext == L".tga" || ext == L".png" || ext == L".jpg" || ext == L".jpeg" || ext == L".bmp")
		{
			auto ptr = std::make_shared<Texture>(resolved.c_str(), this);
			if (ptr)
			{
				m_mapResources.emplace(resolved, ptr);
				return ptr;
			}
		}
		else if (ext == L".obj")
		{
			if (std::filesystem::exists(resPath))
			{
				auto ptr = std::make_shared<Mesh>(resolved.c_str(), this);
				if (ptr)
				{
					m_mapResources.emplace(resolved, ptr);
					return ptr;
				}
			}
			else
			{
				darklog.warn(L"ResourceManager: Mesh not found: " + resolved);
			}
		}
		else if (ext == L".hlsl" || ext == L".fx")
		{
			if (std::filesystem::exists(resPath))
			{
				auto ptr = std::make_shared<Material>(resolved.c_str(), this);
				if (ptr)
				{
					m_mapResources.emplace(resolved, ptr);
					return ptr;
				}
			}
			else
			{
				darklog.warn(L"ResourceManager: Shader not found: " + resolved);
			}
		}
		else if (ext == L".font")
		{
			if (std::filesystem::exists(resPath))
			{
				auto ptr = std::make_shared<Font>(resolved.c_str(), this);
				if (ptr)
				{
					m_mapResources.emplace(resolved, ptr);
					return ptr;
				}
			}
		}

		return ResourcePtr();
	}

	Game* ResourceManager::getGame()
	{
		return m_game;
	}

	MaterialPtr ResourceManager::getDefaultMaterial()
	{
		if (!m_defaultMaterial)
		{
			std::wstring candidates[] = {
				L"Assets/Shaders/default_material.hlsl",
				L"Assets/Shaders/base.hlsl",
				L"SandBox/Assets/Shaders/base.hlsl"
			};
			for (const auto& candidate : candidates)
			{
				std::wstring resolved = resolvePath(candidate);
				if (std::filesystem::exists(resolved))
				{
					m_defaultMaterial = createResourceFromFile<Material>(resolved.c_str());
					break;
				}
			}
		}
		return m_defaultMaterial;
	}
}
