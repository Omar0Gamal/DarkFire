#include "CookCommon.h"

#include <iostream>
#include <fstream>
#include <vector>
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
#include <d3dcompiler.h>

namespace dfcook
{
	static bool CompileShader(
		const std::filesystem::path& hlslPath,
		const std::string& entryPoint,
		const std::string& profile,
		const std::filesystem::path& outCsoPath)
	{
		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_OPTIMIZATION_LEVEL3 | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;

		ID3DBlob* codeBlob = nullptr;
		ID3DBlob* errorBlob = nullptr;

		std::wstring wpath = hlslPath.wstring();

		HRESULT hr = D3DCompileFromFile(
			wpath.c_str(),
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
			std::cerr << "[dfcook] Failed compiling " << hlslPath.filename().string() << " (" << profile << "):" << std::endl;
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

	bool CookShaders(const std::filesystem::path& shadersInDir, const std::filesystem::path& shadersOutDir, CookCache* cache)
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
			else if (stem.ends_with("HS") || stem.ends_with("Hull"))
			{
				profile = "hs_5_0";
			}
			else if (stem.ends_with("DS") || stem.ends_with("Domain"))
			{
				profile = "ds_5_0";
			}
			else if (stem.ends_with("CS") || stem.ends_with("Compute"))
			{
				profile = "cs_5_0";
			}
			else
			{
				continue; // Skip helper hlsl files
			}

			std::string vkey = "/Shaders/" + stem + ".cso";
			std::filesystem::path outCso = shadersOutDir / (stem + ".cso");

			if (cache && std::filesystem::exists(outCso) && cache->isUpToDate(entry.path(), vkey))
			{
				continue; // Up to date in cache
			}

			if (!CompileShader(entry.path(), "main", profile, outCso))
			{
				allSucceeded = false;
			}
			else if (cache)
			{
				cache->update(entry.path(), vkey);
			}
		}

		return allSucceeded;
	}
}
