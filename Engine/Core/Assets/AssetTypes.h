#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Assets/AssetGuid.h"
#include <string>
#include <string_view>
#include <filesystem>

namespace Core
{
	enum class AssetType : uint32_t
	{
		Unknown = 0,
		Texture,
		Mesh,
		Material,
		Shader,
		Scene,
		Prefab,
		Font,
		Audio,
		Script
	};

	inline std::string_view AssetTypeToString(AssetType type) noexcept
	{
		switch (type)
		{
		case AssetType::Texture:  return "Texture";
		case AssetType::Mesh:     return "Mesh";
		case AssetType::Material: return "Material";
		case AssetType::Shader:   return "Shader";
		case AssetType::Scene:    return "Scene";
		case AssetType::Prefab:   return "Prefab";
		case AssetType::Font:     return "Font";
		case AssetType::Audio:    return "Audio";
		case AssetType::Script:   return "Script";
		default:                  return "Unknown";
		}
	}

	inline AssetType AssetTypeFromString(std::string_view str) noexcept
	{
		if (str == "Texture")  return AssetType::Texture;
		if (str == "Mesh")     return AssetType::Mesh;
		if (str == "Material") return AssetType::Material;
		if (str == "Shader")   return AssetType::Shader;
		if (str == "Scene")    return AssetType::Scene;
		if (str == "Prefab")   return AssetType::Prefab;
		if (str == "Font")     return AssetType::Font;
		if (str == "Audio")    return AssetType::Audio;
		if (str == "Script")   return AssetType::Script;
		return AssetType::Unknown;
	}

	inline AssetType DetectAssetTypeFromExtension(const std::filesystem::path& path) noexcept
	{
		std::string ext = path.extension().string();
		for (auto& c : ext) c = static_cast<char>(::tolower(c));

		if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".dds" || ext == ".tga" || ext == ".bmp")
		{
			return AssetType::Texture;
		}
		if (ext == ".obj" || ext == ".gltf" || ext == ".glb" || ext == ".fbx")
		{
			return AssetType::Mesh;
		}
		if (ext == ".mat")
		{
			return AssetType::Material;
		}
		if (ext == ".hlsl" || ext == ".hlsli")
		{
			return AssetType::Shader;
		}
		if (ext == ".scene")
		{
			return AssetType::Scene;
		}
		if (ext == ".prefab")
		{
			return AssetType::Prefab;
		}
		if (ext == ".ttf" || ext == ".spritefont")
		{
			return AssetType::Font;
		}
		if (ext == ".wav" || ext == ".mp3" || ext == ".ogg")
		{
			return AssetType::Audio;
		}
		if (ext == ".lua" || ext == ".cpp" || ext == ".h")
		{
			return AssetType::Script;
		}
		return AssetType::Unknown;
	}

	struct AssetMetadata
	{
		AssetGuid guid = AssetGuid::Empty;
		AssetType type = AssetType::Unknown;
		std::filesystem::path filePath;
		bool isLoaded = false;
	};
}
