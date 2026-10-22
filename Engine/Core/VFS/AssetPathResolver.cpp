#include "Engine/Core/VFS/AssetPathResolver.h"
#include "Engine/Runtime/EngineContext.h"
#include "Engine/Core/Log/Log.h"
#include <fstream>
#include <algorithm>

namespace Core
{
	std::optional<ResolvedAsset> AssetPathResolver::resolve(std::string_view virtualOrRelativePath)
	{
		if (virtualOrRelativePath.empty())
		{
			return std::nullopt;
		}

		std::string pathStr(virtualOrRelativePath);
		std::replace(pathStr.begin(), pathStr.end(), '\\', '/');

		std::string vpath = pathStr;
		if (vpath.front() != '/')
		{
			vpath = "/" + vpath;
		}

		// 1. Check Virtual File System
		auto& vfs = EngineContext::Get().vfs;
		if (vfs.exists(vpath))
		{
			ResolvedAsset res{};
			res.virtualPath = vpath;
			res.isVFS = true;
			return res;
		}

		// 2. Check Physical Filesystem with standardized engine/project fallbacks
		std::string rawPath = pathStr;
		if (!rawPath.empty() && rawPath.front() == '/')
		{
			rawPath = rawPath.substr(1);
		}

		std::string stripped = rawPath;
		if (stripped.starts_with("engine/Content/")) stripped = stripped.substr(15);
		else if (stripped.starts_with("Engine/Content/")) stripped = stripped.substr(15);
		else if (stripped.starts_with("engine/")) stripped = stripped.substr(7);
		else if (stripped.starts_with("Engine/")) stripped = stripped.substr(7);

		std::vector<std::string> candidates = {
			rawPath,
			"Engine/Content/" + stripped,
			"Engine/Content/" + std::filesystem::path(rawPath).filename().string(),
			"Assets/" + rawPath,
			"Templates/Playground3D/Assets/" + rawPath
		};

		for (const auto& candidate : candidates)
		{
			if (std::filesystem::exists(candidate) && !std::filesystem::is_directory(candidate))
			{
				ResolvedAsset res{};
				res.physicalPath = std::filesystem::path(candidate);
				res.virtualPath = vpath;
				res.isVFS = false;
				return res;
			}
		}

		return std::nullopt;
	}

	Result<std::string, std::string> AssetPathResolver::readText(std::string_view path)
	{
		auto resolved = resolve(path);
		if (!resolved.has_value())
		{
			return Result<std::string, std::string>::error("Asset not found: " + std::string(path));
		}

		if (resolved->isVFS)
		{
			auto& vfs = EngineContext::Get().vfs;
			return vfs.readTextFile(resolved->virtualPath);
		}

		std::ifstream file(resolved->physicalPath);
		if (!file.is_open())
		{
			return Result<std::string, std::string>::error("Failed to open file: " + resolved->physicalPath.string());
		}

		std::string content((std::istreambuf_iterator<char>(file)), std::istreambuf_iterator<char>());
		return Result<std::string, std::string>::ok(content);
	}

	Result<std::vector<uint8_t>, std::string> AssetPathResolver::readBinary(std::string_view path)
	{
		auto resolved = resolve(path);
		if (!resolved.has_value())
		{
			return Result<std::vector<uint8_t>, std::string>::error("Asset not found: " + std::string(path));
		}

		if (resolved->isVFS)
		{
			auto& vfs = EngineContext::Get().vfs;
			return vfs.readBinaryFile(resolved->virtualPath);
		}

		std::ifstream file(resolved->physicalPath, std::ios::binary | std::ios::ate);
		if (!file.is_open())
		{
			return Result<std::vector<uint8_t>, std::string>::error("Failed to open binary file: " + resolved->physicalPath.string());
		}

		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::vector<uint8_t> buffer(static_cast<size_t>(size));
		if (size > 0)
		{
			file.read(reinterpret_cast<char*>(buffer.data()), size);
		}

		return Result<std::vector<uint8_t>, std::string>::ok(buffer);
	}
}
