#include "PBRTextureImporter.h"
#include "Engine/Core/Log/Log.h"
#include <DirectXTex.h>
#include <algorithm>
#include <fstream>

namespace Core::Renderer
{
	static std::string ToLower(std::string str)
	{
		std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
		return str;
	}

	static bool ContainsAny(const std::string& str, const std::vector<std::string>& patterns)
	{
		for (const auto& p : patterns)
		{
			if (str.find(p) != std::string::npos) return true;
		}
		return false;
	}

	PBRMapType PBRTextureImporter::deduceMapType(const std::filesystem::path& texturePath)
	{
		std::string stem = ToLower(texturePath.stem().string());

		// 1. Packed ORM / ARM
		if (ContainsAny(stem, { "_orm", "-orm", "_arm", "-arm", "occlusionroughnessmetallic", "metallicroughness" }))
		{
			return PBRMapType::ORM;
		}

		// 2. Normal Map (check GL vs DX)
		if (ContainsAny(stem, { "_normal_gl", "-normal_gl", "_norm_gl", "-norm_gl", "_normalgl", "normal_opengl", "_n_gl" }))
		{
			return PBRMapType::NormalGL;
		}
		if (ContainsAny(stem, { "normal", "norm", "nrm", "_nor" }))
		{
			return PBRMapType::Normal;
		}

		// 3. Base Color / Albedo
		if (ContainsAny(stem, { "basecolor", "base_color", "albedo", "diffuse", "diff", "_color", "_col", "_bc", "_d" }))
		{
			return PBRMapType::BaseColor;
		}

		// 4. Roughness
		if (ContainsAny(stem, { "roughness", "rough", "_rgh", "_r" }))
		{
			return PBRMapType::Roughness;
		}

		// 5. Metallic
		if (ContainsAny(stem, { "metallic", "metalness", "metal", "_met", "_m" }))
		{
			return PBRMapType::Metallic;
		}

		// 6. Ambient Occlusion
		if (ContainsAny(stem, { "ambientocclusion", "ambient_occlusion", "_ao", "-ao", "occlusion", "_occ" }))
		{
			return PBRMapType::AmbientOcclusion;
		}

		// 7. Emissive
		if (ContainsAny(stem, { "emissive", "emission", "emit", "_e" }))
		{
			return PBRMapType::Emissive;
		}

		// 8. Height / Displacement
		if (ContainsAny(stem, { "height", "displacement", "disp" }))
		{
			return PBRMapType::Height;
		}

		return PBRMapType::Unknown;
	}

	PBRTextureSet PBRTextureImporter::scanDirectory(const std::filesystem::path& dirPath, const std::string& preferredName)
	{
		PBRTextureSet set{};
		set.materialName = preferredName.empty() ? dirPath.stem().string() : preferredName;

		if (!std::filesystem::exists(dirPath))
		{
			return set;
		}

		const std::vector<std::string> validExts = { ".png", ".jpg", ".jpeg", ".tga", ".dds", ".bmp", ".tif", ".tiff" };

		for (const auto& entry : std::filesystem::directory_iterator(dirPath))
		{
			if (!entry.is_regular_file()) continue;

			std::string ext = ToLower(entry.path().extension().string());
			if (std::find(validExts.begin(), validExts.end(), ext) == validExts.end()) continue;

			PBRMapType type = deduceMapType(entry.path());
			switch (type)
			{
			case PBRMapType::BaseColor:
				if (set.baseColorPath.empty()) set.baseColorPath = entry.path();
				break;
			case PBRMapType::Normal:
				if (set.normalPath.empty()) set.normalPath = entry.path();
				break;
			case PBRMapType::NormalGL:
				if (set.normalPath.empty())
				{
					set.normalPath = entry.path();
					set.isNormalGL = true;
				}
				break;
			case PBRMapType::ORM:
				if (set.ormPath.empty()) set.ormPath = entry.path();
				break;
			case PBRMapType::Roughness:
				if (set.roughnessPath.empty()) set.roughnessPath = entry.path();
				break;
			case PBRMapType::Metallic:
				if (set.metallicPath.empty()) set.metallicPath = entry.path();
				break;
			case PBRMapType::AmbientOcclusion:
				if (set.aoPath.empty()) set.aoPath = entry.path();
				break;
			case PBRMapType::Emissive:
				if (set.emissivePath.empty()) set.emissivePath = entry.path();
				break;
			default:
				break;
			}
		}

		return set;
	}

	bool PBRTextureImporter::packORM(
		const std::filesystem::path& aoPath,
		const std::filesystem::path& roughnessPath,
		const std::filesystem::path& metallicPath,
		const std::filesystem::path& outputPath)
	{
		DirectX::ScratchImage imgAO, imgRough, imgMetal;
		bool hasAO = !aoPath.empty() && std::filesystem::exists(aoPath);
		bool hasRough = !roughnessPath.empty() && std::filesystem::exists(roughnessPath);
		bool hasMetal = !metallicPath.empty() && std::filesystem::exists(metallicPath);

		if (!hasAO && !hasRough && !hasMetal)
		{
			return false;
		}

		size_t width = 1024, height = 1024;

		if (hasRough && SUCCEEDED(DirectX::LoadFromWICFile(roughnessPath.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, imgRough)))
		{
			width = imgRough.GetMetadata().width;
			height = imgRough.GetMetadata().height;
		}
		else if (hasAO && SUCCEEDED(DirectX::LoadFromWICFile(aoPath.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, imgAO)))
		{
			width = imgAO.GetMetadata().width;
			height = imgAO.GetMetadata().height;
		}
		else if (hasMetal && SUCCEEDED(DirectX::LoadFromWICFile(metallicPath.c_str(), DirectX::WIC_FLAGS_NONE, nullptr, imgMetal)))
		{
			width = imgMetal.GetMetadata().width;
			height = imgMetal.GetMetadata().height;
		}

		// Ensure all images are converted/resized to RGBA8 32bpp
		DirectX::ScratchImage roughConv, aoConv, metalConv;
		if (hasRough)
		{
			if (imgRough.GetMetadata().format != DXGI_FORMAT_R8G8B8A8_UNORM)
				DirectX::Convert(*imgRough.GetImage(0, 0, 0), DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, roughConv);
			else
				roughConv = std::move(imgRough);
		}

		if (hasAO)
		{
			if (imgAO.GetMetadata().format != DXGI_FORMAT_R8G8B8A8_UNORM)
				DirectX::Convert(*imgAO.GetImage(0, 0, 0), DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, aoConv);
			else
				aoConv = std::move(imgAO);
		}

		if (hasMetal)
		{
			if (imgMetal.GetMetadata().format != DXGI_FORMAT_R8G8B8A8_UNORM)
				DirectX::Convert(*imgMetal.GetImage(0, 0, 0), DXGI_FORMAT_R8G8B8A8_UNORM, DirectX::TEX_FILTER_DEFAULT, DirectX::TEX_THRESHOLD_DEFAULT, metalConv);
			else
				metalConv = std::move(imgMetal);
		}

		DirectX::ScratchImage ormImage;
		if (FAILED(ormImage.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1)))
		{
			return false;
		}

		const uint8_t* pAO = hasAO && aoConv.GetPixels() ? aoConv.GetPixels() : nullptr;
		const uint8_t* pRough = hasRough && roughConv.GetPixels() ? roughConv.GetPixels() : nullptr;
		const uint8_t* pMetal = hasMetal && metalConv.GetPixels() ? metalConv.GetPixels() : nullptr;
		uint8_t* pDst = ormImage.GetPixels();

		for (size_t y = 0; y < height; ++y)
		{
			for (size_t x = 0; x < width; ++x)
			{
				size_t idx = (y * width + x) * 4;

				// R = Ambient Occlusion (default 255)
				uint8_t aoVal = pAO ? pAO[idx] : 255;
				// G = Roughness (default 128)
				uint8_t roughVal = pRough ? pRough[idx] : 128;
				// B = Metallic (default 0)
				uint8_t metalVal = pMetal ? pMetal[idx] : 0;

				pDst[idx + 0] = aoVal;
				pDst[idx + 1] = roughVal;
				pDst[idx + 2] = metalVal;
				pDst[idx + 3] = 255;
			}
		}

		std::error_code ec;
		std::filesystem::create_directories(outputPath.parent_path(), ec);

		HRESULT hr = DirectX::SaveToWICFile(
			*ormImage.GetImage(0, 0, 0),
			DirectX::WIC_FLAGS_NONE,
			DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),
			outputPath.c_str()
		);

		return SUCCEEDED(hr);
	}

	std::shared_ptr<Material> PBRTextureImporter::importFromDirectory(
		const std::filesystem::path& dirPath,
		const std::filesystem::path& outputMatPath,
		bool autoPackORM)
	{
		PBRTextureSet set = scanDirectory(dirPath);
		auto mat = std::make_shared<Material>(set.materialName);

		if (!set.baseColorPath.empty())
		{
			mat->setAlbedoPath(set.baseColorPath.generic_string());
		}

		if (!set.normalPath.empty())
		{
			mat->setNormalPath(set.normalPath.generic_string());
			if (set.isNormalGL)
			{
				mat->setFlipNormalY(true);
			}
		}

		if (!set.ormPath.empty())
		{
			mat->setMetallicRoughnessPath(set.ormPath.generic_string());
			mat->setORMPacked(true);
		}
		else if (autoPackORM && (!set.roughnessPath.empty() || !set.metallicPath.empty() || !set.aoPath.empty()))
		{
			std::filesystem::path packedPath = dirPath / (set.materialName + "_ORM.png");
			if (packORM(set.aoPath, set.roughnessPath, set.metallicPath, packedPath))
			{
				mat->setMetallicRoughnessPath(packedPath.generic_string());
				mat->setORMPacked(true);
			}
		}
		else
		{
			if (!set.roughnessPath.empty()) mat->setMetallicRoughnessPath(set.roughnessPath.generic_string());
			if (!set.aoPath.empty()) mat->setOcclusionPath(set.aoPath.generic_string());
		}

		if (!set.emissivePath.empty())
		{
			mat->setEmissivePath(set.emissivePath.generic_string());
			mat->setEmissiveIntensity(1.0f);
		}

		std::filesystem::path targetMatFile = outputMatPath;
		if (targetMatFile.empty())
		{
			targetMatFile = dirPath / (set.materialName + ".mat");
		}

		mat->saveToFile(targetMatFile);
		return mat;
	}
}
