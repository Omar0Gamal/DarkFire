#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <filesystem>
#include <algorithm>
#include <chrono>
#include <cstdint>
#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3dcompiler.h>
#include <DirectXTex.h>

namespace dfcook
{
#pragma pack(push, 1)
	struct PakHeader
	{
		char magic[4];          // "DFPK"
		uint32_t version;       // 1
		uint32_t entryCount;    // Number of entries
		uint64_t tableOffset;   // Offset to entry table
	};

	struct PakDiskEntry
	{
		uint64_t pathHash;
		uint64_t dataOffset;
		uint64_t dataSize;
		uint32_t pathLength;
	};
#pragma pack(pop)

	static constexpr uint64_t FNV1A_OFFSET = 14695981039346656037ULL;
	static constexpr uint64_t FNV1A_PRIME = 1099511628211ULL;

	inline uint64_t HashPath(std::string_view path) noexcept
	{
		uint64_t hash = FNV1A_OFFSET;
		for (char c : path)
		{
			char lower = (c >= 'A' && c <= 'Z') ? static_cast<char>(c + 32) : (c == '\\' ? '/' : c);
			hash ^= static_cast<uint64_t>(static_cast<uint8_t>(lower));
			hash *= FNV1A_PRIME;
		}
		return hash;
	}

	inline std::string NormalizeVirtualPath(std::string_view path)
	{
		std::string result(path);
		std::replace(result.begin(), result.end(), '\\', '/');
		if (!result.empty() && result.front() != '/')
		{
			result.insert(result.begin(), '/');
		}
		return result;
	}

	struct CookedAsset
	{
		std::string virtualPath;
		uint64_t size = 0;
		uint64_t hash = 0;
	};

	bool CompileShader(
		const std::filesystem::path& hlslPath,
		const std::string& entryPoint,
		const std::string& profile,
		const std::filesystem::path& outCsoPath)
	{
		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3;

		ID3DBlob* codeBlob = nullptr;
		ID3DBlob* errorBlob = nullptr;

		HRESULT hr = D3DCompileFromFile(
			hlslPath.c_str(),
			nullptr,
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entryPoint.c_str(),
			profile.c_str(),
			flags,
			0,
			&codeBlob,
			&errorBlob);

		if (FAILED(hr))
		{
			std::cerr << "[dfcook] Shader compilation error in " << hlslPath.filename().string() << ":" << std::endl;
			if (errorBlob)
			{
				std::cerr << static_cast<const char*>(errorBlob->GetBufferPointer()) << std::endl;
				errorBlob->Release();
			}
			if (codeBlob) codeBlob->Release();
			return false;
		}

		if (errorBlob) errorBlob->Release();

		std::filesystem::create_directories(outCsoPath.parent_path());
		std::ofstream outFile(outCsoPath, std::ios::binary | std::ios::trunc);
		if (!outFile.is_open())
		{
			std::cerr << "[dfcook] Failed to write CSO file: " << outCsoPath.string() << std::endl;
			if (codeBlob) codeBlob->Release();
			return false;
		}

		outFile.write(static_cast<const char*>(codeBlob->GetBufferPointer()), codeBlob->GetBufferSize());
		outFile.close();

		std::cout << "[dfcook] Compiled " << hlslPath.filename().string() << " (" << profile << ") -> " << outCsoPath.filename().string() << " (" << codeBlob->GetBufferSize() << " bytes)" << std::endl;
		codeBlob->Release();
		return true;
	}

	bool CookShaders(const std::filesystem::path& shadersInDir, const std::filesystem::path& shadersOutDir)
	{
		std::cout << "[dfcook] Precompiling shaders from: " << shadersInDir.string() << std::endl;

		if (!std::filesystem::exists(shadersInDir))
		{
			std::cerr << "[dfcook] Shaders directory not found: " << shadersInDir.string() << std::endl;
			return false;
		}

		bool allSucceeded = true;

		for (const auto& entry : std::filesystem::directory_iterator(shadersInDir))
		{
			if (!entry.is_regular_file()) continue;

			std::string ext = entry.path().extension().string();
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
			if (ext != ".hlsl") continue;

			std::string stem = entry.path().stem().string();
			std::string profile = "vs_5_0";
			if (stem.ends_with("PS") || stem.ends_with("Pixel"))
			{
				profile = "ps_5_0";
			}
			else if (stem.ends_with("VS") || stem.ends_with("Vertex"))
			{
				profile = "vs_5_0";
			}
			else
			{
				continue; // Skip helper hlsl files
			}

			std::filesystem::path outCso = shadersOutDir / (stem + ".cso");
			if (!CompileShader(entry.path(), "main", profile, outCso))
			{
				allSucceeded = false;
			}
		}

		return allSucceeded;
	}

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

				DirectX::ScratchImage compressed;
				hr = DirectX::Compress(
					imageToCompress.GetImages(),
					imageToCompress.GetImageCount(),
					imageToCompress.GetMetadata(),
					DXGI_FORMAT_BC7_UNORM,
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

	bool WritePakArchive(
		const std::filesystem::path& pakPath,
		const std::vector<std::pair<std::string, std::vector<uint8_t>>>& files,
		std::vector<CookedAsset>& manifestAssets)
	{
		std::filesystem::create_directories(pakPath.parent_path());
		std::ofstream outFile(pakPath, std::ios::binary | std::ios::trunc);
		if (!outFile.is_open())
		{
			std::cerr << "[dfcook] Error: Failed to create pak file: " << pakPath.string() << std::endl;
			return false;
		}

		PakHeader header{};
		header.magic[0] = 'D';
		header.magic[1] = 'F';
		header.magic[2] = 'P';
		header.magic[3] = 'K';
		header.version = 1;
		header.entryCount = static_cast<uint32_t>(files.size());
		header.tableOffset = 0;

		outFile.write(reinterpret_cast<const char*>(&header), sizeof(header));

		struct EntryInfo
		{
			uint64_t hash;
			uint64_t offset;
			uint64_t size;
			std::string path;
		};

		std::vector<EntryInfo> entries;
		entries.reserve(files.size());

		for (const auto& [vpath, data] : files)
		{
			std::string norm = NormalizeVirtualPath(vpath);
			uint64_t hash = HashPath(norm);
			uint64_t offset = static_cast<uint64_t>(outFile.tellp());
			uint64_t size = static_cast<uint64_t>(data.size());

			if (!data.empty())
			{
				outFile.write(reinterpret_cast<const char*>(data.data()), size);
			}

			entries.push_back({ hash, offset, size, norm });
			manifestAssets.push_back({ norm, size, hash });
		}

		header.tableOffset = static_cast<uint64_t>(outFile.tellp());
		for (const auto& entry : entries)
		{
			PakDiskEntry diskEntry{};
			diskEntry.pathHash = entry.hash;
			diskEntry.dataOffset = entry.offset;
			diskEntry.dataSize = entry.size;
			diskEntry.pathLength = static_cast<uint32_t>(entry.path.size());

			outFile.write(reinterpret_cast<const char*>(&diskEntry), sizeof(diskEntry));
			outFile.write(entry.path.data(), diskEntry.pathLength);
		}

		outFile.seekp(0, std::ios::beg);
		outFile.write(reinterpret_cast<const char*>(&header), sizeof(header));
		outFile.close();

		std::cout << "[dfcook] Created Pak: " << pakPath.filename().string() << " (" << files.size() << " files, " << header.tableOffset << " payload bytes)" << std::endl;
		return true;
	}

	std::vector<uint8_t> ReadDiskFile(const std::filesystem::path& path)
	{
		std::ifstream file(path, std::ios::binary | std::ios::ate);
		if (!file.is_open()) return {};
		auto sz = file.tellg();
		file.seekg(0, std::ios::beg);
		std::vector<uint8_t> buf(static_cast<size_t>(sz));
		file.read(reinterpret_cast<char*>(buf.data()), sz);
		return buf;
	}

	bool CreateEnginePak(
		const std::filesystem::path& cookedShadersDir,
		const std::filesystem::path& engineContentDir,
		const std::filesystem::path& outPakPath,
		std::vector<CookedAsset>& manifestAssets)
	{
		std::vector<std::pair<std::string, std::vector<uint8_t>>> files;

		// 1. Pack Shaders
		if (std::filesystem::exists(cookedShadersDir))
		{
			for (const auto& entry : std::filesystem::directory_iterator(cookedShadersDir))
			{
				if (entry.is_regular_file() && entry.path().extension() == ".cso")
				{
					std::string vpath = "/Shaders/" + entry.path().filename().string();
					files.push_back({ vpath, ReadDiskFile(entry.path()) });
				}
			}
		}

		// 2. Pack Engine Content
		if (std::filesystem::exists(engineContentDir))
		{
			for (const auto& entry : std::filesystem::recursive_directory_iterator(engineContentDir))
			{
				if (entry.is_regular_file())
				{
					auto rel = std::filesystem::relative(entry.path(), engineContentDir).string();
					std::replace(rel.begin(), rel.end(), '\\', '/');
					std::string vpath = "/Content/" + rel;
					files.push_back({ vpath, ReadDiskFile(entry.path()) });
				}
			}
		}

		return WritePakArchive(outPakPath, files, manifestAssets);
	}

	bool CreateGamePak(
		const std::filesystem::path& projectFilePath,
		const std::filesystem::path& outPakPath,
		std::vector<CookedAsset>& manifestAssets)
	{
		std::vector<std::pair<std::string, std::vector<uint8_t>>> files;
		std::filesystem::path projectDir = projectFilePath.parent_path();

		// 1. Pack Project File
		if (std::filesystem::exists(projectFilePath))
		{
			files.push_back({ "/Project.dfproj", ReadDiskFile(projectFilePath) });
		}

		// 2. Pack Scenes
		std::filesystem::path scenesDir = projectDir / "Scenes";
		if (std::filesystem::exists(scenesDir))
		{
			for (const auto& entry : std::filesystem::recursive_directory_iterator(scenesDir))
			{
				if (entry.is_regular_file() && entry.path().extension() == ".scene")
				{
					auto rel = std::filesystem::relative(entry.path(), projectDir).string();
					std::replace(rel.begin(), rel.end(), '\\', '/');
					files.push_back({ "/" + rel, ReadDiskFile(entry.path()) });
				}
			}
		}

		// 3. Pack Assets
		std::filesystem::path assetsDir = projectDir / "Assets";
		if (std::filesystem::exists(assetsDir))
		{
			for (const auto& entry : std::filesystem::recursive_directory_iterator(assetsDir))
			{
				if (entry.is_regular_file())
				{
					auto rel = std::filesystem::relative(entry.path(), projectDir).string();
					std::replace(rel.begin(), rel.end(), '\\', '/');
					files.push_back({ "/" + rel, ReadDiskFile(entry.path()) });
				}
			}
		}

		return WritePakArchive(outPakPath, files, manifestAssets);
	}

	void WriteManifest(const std::filesystem::path& manifestPath, const std::vector<CookedAsset>& assets)
	{
		std::ofstream f(manifestPath, std::ios::trunc);
		if (!f.is_open()) return;

		f << "{\n";
		f << "  \"engine_version\": \"2.0.0\",\n";
		f << "  \"asset_count\": " << assets.size() << ",\n";
		f << "  \"assets\": [\n";

		for (size_t i = 0; i < assets.size(); ++i)
		{
			const auto& a = assets[i];
			f << "    {\n";
			f << "      \"virtual_path\": \"" << a.virtualPath << "\",\n";
			f << "      \"size\": " << a.size << ",\n";
			f << "      \"hash\": " << a.hash << "\n";
			f << "    }" << (i + 1 < assets.size() ? "," : "") << "\n";
		}

		f << "  ]\n";
		f << "}\n";
		f.close();
		std::cout << "[dfcook] Wrote cook manifest to: " << manifestPath.filename().string() << std::endl;
	}
}

int main(int argc, char** argv)
{
	std::cout << "========================================" << std::endl;
	std::cout << "  DarkFire Engine Asset Cooker (dfcook) " << std::endl;
	std::cout << "========================================" << std::endl;

	std::filesystem::path projectPath = "Templates/Playground3D/Project.dfproj";
	std::filesystem::path outputDir = "Cooked";
	bool cookShaders = false;
	bool cookTextures = false;
	bool cookPaks = false;

	for (int i = 1; i < argc; ++i)
	{
		std::string arg = argv[i];
		if (arg == "--project" && i + 1 < argc)
		{
			projectPath = argv[++i];
		}
		else if (arg == "--output" && i + 1 < argc)
		{
			outputDir = argv[++i];
		}
		else if (arg == "--shaders")
		{
			cookShaders = true;
		}
		else if (arg == "--textures")
		{
			cookTextures = true;
		}
		else if (arg == "--pak" || arg == "--paks")
		{
			cookPaks = true;
		}
		else if (arg == "--all")
		{
			cookShaders = true;
			cookTextures = true;
			cookPaks = true;
		}
		else if (!arg.starts_with("--") && i == 1)
		{
			projectPath = arg;
		}
		else if (!arg.starts_with("--") && i == 2)
		{
			outputDir = arg;
		}
	}

	if (!cookShaders && !cookTextures && !cookPaks)
	{
		cookShaders = true;
		cookTextures = true;
		cookPaks = true;
	}

	std::cout << "[dfcook] Target Project: " << projectPath.string() << std::endl;
	std::cout << "[dfcook] Output Dir:     " << outputDir.string() << std::endl;

	std::filesystem::path engineShadersDir = "Engine/Shaders";
	std::filesystem::path engineContentDir = "Engine/Content";
	std::filesystem::path cookedShadersDir = outputDir / "Shaders";
	std::filesystem::path cookedContentDir = outputDir / "Content";

	std::vector<dfcook::CookedAsset> manifestAssets;

	if (cookShaders)
	{
		if (!dfcook::CookShaders(engineShadersDir, cookedShadersDir))
		{
			std::cerr << "[dfcook] Warning: Some shaders failed compilation!" << std::endl;
		}
	}

	if (cookTextures)
	{
		dfcook::CookTextures(engineContentDir, cookedContentDir);
		if (std::filesystem::exists(projectPath))
		{
			std::filesystem::path projectAssetsDir = projectPath.parent_path() / "Assets";
			dfcook::CookTextures(projectAssetsDir, outputDir / "Assets");
		}
	}

	if (cookPaks)
	{
		std::filesystem::path enginePakPath = outputDir / "Engine.pak";
		dfcook::CreateEnginePak(cookedShadersDir, engineContentDir, enginePakPath, manifestAssets);

		std::filesystem::path gamePakPath = outputDir / "Game.pak";
		dfcook::CreateGamePak(projectPath, gamePakPath, manifestAssets);

		std::filesystem::path manifestPath = outputDir / "cook_manifest.json";
		dfcook::WriteManifest(manifestPath, manifestAssets);
	}

	std::cout << "[dfcook] Cooking process finished successfully." << std::endl;
	return 0;
}
