#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Result.h"
#include <string>
#include <string_view>
#include <vector>
#include <optional>
#include <filesystem>

namespace Core
{
	struct DF_API ResolvedAsset
	{
		std::filesystem::path physicalPath;
		std::string virtualPath;
		bool isVFS = false;
	};

	class DF_API AssetPathResolver
	{
	public:
		// Resolve a virtual or relative path to its canonical source
		static std::optional<ResolvedAsset> resolve(std::string_view virtualOrRelativePath);

		// Read text file contents using unified VFS -> Physical fallback order
		static Result<std::string, std::string> readText(std::string_view path);

		// Read binary file contents using unified VFS -> Physical fallback order
		static Result<std::vector<uint8_t>, std::string> readBinary(std::string_view path);
	};
}
