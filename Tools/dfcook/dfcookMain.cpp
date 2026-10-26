#include "CookCommon.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <string>
#include <string_view>
#include <filesystem>
#include <algorithm>
#include <cstdint>
#include <cstring>

namespace dfcook
{
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

	bool CreateEnginePak(
		const std::filesystem::path& cookedShadersDir,
		const std::filesystem::path& engineContentDir,
		const std::filesystem::path& bakedContentDir,
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

		// 3. Pack Baked Content (BRDF LUT, Environment IBL, etc.)
		if (std::filesystem::exists(bakedContentDir))
		{
			for (const auto& entry : std::filesystem::directory_iterator(bakedContentDir))
			{
				if (entry.is_regular_file())
				{
					std::string ext = entry.path().extension().string();
					std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
					if (ext == ".dds")
					{
						std::string vpath = "/Content/Baked/" + entry.path().filename().string();
						auto it = std::find_if(files.begin(), files.end(), [&](const auto& item) {
							return item.first == vpath;
						});
						if (it != files.end())
						{
							it->second = ReadDiskFile(entry.path());
						}
						else
						{
							files.push_back({ vpath, ReadDiskFile(entry.path()) });
						}
					}
				}
			}
		}

		return WritePakArchive(outPakPath, files, manifestAssets);
	}

	inline bool CreateEnginePak(
		const std::filesystem::path& cookedShadersDir,
		const std::filesystem::path& engineContentDir,
		const std::filesystem::path& outPakPath,
		std::vector<CookedAsset>& manifestAssets)
	{
		return CreateEnginePak(cookedShadersDir, engineContentDir, outPakPath.parent_path() / "Content" / "Baked", outPakPath, manifestAssets);
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
	bool cookLighting = false;
	bool useCache = false;
	dfcook::BakeQuality bakeQuality = dfcook::BakeQuality::Production;

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
		else if (arg == "--ibl" || arg == "--lighting")
		{
			cookLighting = true;
		}
		else if ((arg == "--bake-quality" || arg == "-bq") && i + 1 < argc)
		{
			std::string qStr = argv[++i];
			std::transform(qStr.begin(), qStr.end(), qStr.begin(), ::tolower);
			if (qStr == "fast" || qStr == "0") bakeQuality = dfcook::BakeQuality::Fast;
			else if (qStr == "balanced" || qStr == "1") bakeQuality = dfcook::BakeQuality::Balanced;
			else bakeQuality = dfcook::BakeQuality::Production;
		}
		else if (arg == "--cache")
		{
			useCache = true;
		}
		else if (arg == "--all")
		{
			cookShaders = true;
			cookTextures = true;
			cookPaks = true;
			cookLighting = true;
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

	if (!cookShaders && !cookTextures && !cookPaks && !cookLighting)
	{
		cookShaders = true;
		cookTextures = true;
		cookPaks = true;
		cookLighting = true;
	}

	// Run CookLightingIBL automatically whenever --shaders, --pak, or --all is passed
	if (cookShaders || cookPaks)
	{
		cookLighting = true;
	}

	std::cout << "[dfcook] Target Project: " << projectPath.string() << std::endl;
	std::cout << "[dfcook] Output Dir:     " << outputDir.string() << std::endl;

	std::filesystem::path engineShadersDir = "Engine/Shaders";
	std::filesystem::path engineContentDir = "Engine/Content";
	std::filesystem::path cookedShadersDir = outputDir / "Shaders";
	std::filesystem::path cookedContentDir = outputDir / "Content";
	std::filesystem::path cookedBakedDir = outputDir / "Content" / "Baked";

	std::vector<dfcook::CookedAsset> manifestAssets;

	if (cookShaders)
	{
		if (!dfcook::CookShaders(engineShadersDir, cookedShadersDir))
		{
			std::cerr << "[dfcook] Warning: Some shaders failed compilation!" << std::endl;
		}
	}

	if (cookLighting)
	{
		if (!dfcook::CookLightingIBL(cookedBakedDir, bakeQuality))
		{
			std::cerr << "[dfcook] Warning: Offline Lighting & IBL baking failed!" << std::endl;
		}

		if (std::filesystem::exists(projectPath))
		{
			std::filesystem::path projectAssetsDir = projectPath.parent_path() / "Assets" / "Textures";
			if (std::filesystem::exists(projectAssetsDir))
			{
				for (const auto& entry : std::filesystem::directory_iterator(projectAssetsDir))
				{
					if (entry.is_regular_file() && entry.path().extension() == ".hdr")
					{
						std::filesystem::path outDds = entry.path();
						outDds.replace_extension(".dds");
						if (!std::filesystem::exists(outDds))
						{
							dfcook::ConvolveProbeHDR(entry.path(), outDds, bakeQuality);
						}
					}
				}
			}
		}
	}

	if (cookTextures)
	{
		dfcook::CookTextures(engineContentDir, cookedContentDir);
		if (!cookPaks && std::filesystem::exists(projectPath))
		{
			std::filesystem::path projectAssetsDir = projectPath.parent_path() / "Assets";
			dfcook::CookTextures(projectAssetsDir, outputDir / "Assets");
		}
	}

	if (cookPaks)
	{
		std::filesystem::path enginePakPath = outputDir / "Engine.pak";
		dfcook::CreateEnginePak(cookedShadersDir, engineContentDir, cookedBakedDir, enginePakPath, manifestAssets);

		std::filesystem::path gamePakPath = outputDir / "Game.pak";
		dfcook::CreateGamePak(projectPath, gamePakPath, manifestAssets);

		std::filesystem::path manifestPath = outputDir / "cook_manifest.json";
		dfcook::WriteManifest(manifestPath, manifestAssets);
	}

	std::cout << "[dfcook] Cooking process finished successfully." << std::endl;
	return 0;
}
