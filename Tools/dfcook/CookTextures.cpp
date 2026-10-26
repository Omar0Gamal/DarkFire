#include "CookCommon.h"

#include <iostream>
#include <string>
#include <filesystem>
#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <DirectXTex.h>

namespace dfcook
{
	bool CookTextures(const std::filesystem::path& inDir, const std::filesystem::path& outDir)
	{
		std::cout << "[dfcook] Cooking textures from: " << inDir.string() << " to: " << outDir.string() << std::endl;
		if (!std::filesystem::exists(inDir)) return true;

		HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		(void)hrCo;

		for (const auto& entry : std::filesystem::recursive_directory_iterator(inDir))
		{
			if (!entry.is_regular_file()) continue;

			std::string ext = entry.path().extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

			if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp")
			{
				auto relPath = std::filesystem::relative(entry.path(), inDir);
				auto outDdsPath = outDir / relPath;
				outDdsPath.replace_extension(".dds");
				std::filesystem::create_directories(outDdsPath.parent_path());

				DirectX::TexMetadata metadata;
				DirectX::ScratchImage image;

				HRESULT hr = DirectX::LoadFromWICFile(entry.path().c_str(), DirectX::WIC_FLAGS_NONE, &metadata, image);
				if (FAILED(hr))
				{
					std::cerr << "[dfcook] Failed to load image: " << entry.path().string() << std::endl;
					continue;
				}

				DirectX::ScratchImage mipChain;
				hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_DEFAULT, 0, mipChain);
				const DirectX::ScratchImage& imageToCompress = SUCCEEDED(hr) ? mipChain : image;

				std::string stem = entry.path().stem().string();
				std::transform(stem.begin(), stem.end(), stem.begin(), ::tolower);

				DXGI_FORMAT compressFormat = DXGI_FORMAT_BC7_UNORM;
				if (stem.find("normal") != std::string::npos || stem.find("norm") != std::string::npos || stem.find("_n") != std::string::npos)
				{
					compressFormat = DXGI_FORMAT_BC5_UNORM;
				}
				else if (stem.find("basecolor") != std::string::npos || stem.find("albedo") != std::string::npos || stem.find("diffuse") != std::string::npos || stem.find("_col") != std::string::npos)
				{
					compressFormat = DXGI_FORMAT_BC7_UNORM_SRGB;
				}
				else if (stem.find("roughness") != std::string::npos || stem.find("metallic") != std::string::npos || stem.find("ao") != std::string::npos || stem.find("occlusion") != std::string::npos)
				{
					compressFormat = DXGI_FORMAT_BC4_UNORM;
				}
				else
				{
					compressFormat = DXGI_FORMAT_BC7_UNORM;
				}

				DirectX::ScratchImage compressed;
				hr = DirectX::Compress(
					imageToCompress.GetImages(),
					imageToCompress.GetImageCount(),
					imageToCompress.GetMetadata(),
					compressFormat,
					DirectX::TEX_COMPRESS_PARALLEL,
					DirectX::TEX_THRESHOLD_DEFAULT,
					compressed);

				if (SUCCEEDED(hr))
				{
					DirectX::SaveToDDSFile(compressed.GetImages(), compressed.GetImageCount(), compressed.GetMetadata(), DirectX::DDS_FLAGS_NONE, outDdsPath.c_str());
					std::cout << "[dfcook] Compressed texture " << relPath.string() << " -> " << outDdsPath.filename().string() << std::endl;
				}
				else
				{
					// Fallback to uncompressed DDS
					DirectX::SaveToDDSFile(imageToCompress.GetImages(), imageToCompress.GetImageCount(), imageToCompress.GetMetadata(), DirectX::DDS_FLAGS_NONE, outDdsPath.c_str());
					std::cout << "[dfcook] Saved uncompressed texture " << relPath.string() << " -> " << outDdsPath.filename().string() << std::endl;
				}
			}
		}

		CoUninitialize();
		return true;
	}
}
