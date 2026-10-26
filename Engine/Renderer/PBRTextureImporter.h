#pragma once

#include "Engine/Core/Defines.h"
#include "Material.h"
#include <filesystem>
#include <vector>
#include <string>
#include <memory>

namespace Core::Renderer
{
	enum class PBRMapType : uint8_t
	{
		Unknown = 0,
		BaseColor,
		Normal,
		NormalGL,
		ORM,
		Roughness,
		Metallic,
		AmbientOcclusion,
		Emissive,
		Height
	};

	struct PBRTextureSet
	{
		std::string materialName;
		std::filesystem::path baseColorPath;
		std::filesystem::path normalPath;
		std::filesystem::path ormPath;
		std::filesystem::path roughnessPath;
		std::filesystem::path metallicPath;
		std::filesystem::path aoPath;
		std::filesystem::path emissivePath;
		bool isNormalGL = false;
	};

	class DF_API PBRTextureImporter
	{
	public:
		static PBRMapType deduceMapType(const std::filesystem::path& texturePath);

		static PBRTextureSet scanDirectory(const std::filesystem::path& dirPath, const std::string& preferredName = "");

		static bool packORM(
			const std::filesystem::path& aoPath,
			const std::filesystem::path& roughnessPath,
			const std::filesystem::path& metallicPath,
			const std::filesystem::path& outputPath
		);

		static std::shared_ptr<Material> importFromDirectory(
			const std::filesystem::path& dirPath,
			const std::filesystem::path& outputMatPath = "",
			bool autoPackORM = true
		);
	};
}
