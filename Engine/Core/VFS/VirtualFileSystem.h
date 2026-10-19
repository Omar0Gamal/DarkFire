#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Result.h"
#include <string>
#include <string_view>
#include <filesystem>
#include <vector>
#include <map>
#include <shared_mutex>

namespace Core
{
	class DF_API VirtualFileSystem
	{
	public:
		VirtualFileSystem();
		~VirtualFileSystem() = default;

		void mount(std::string_view virtualPrefix, const std::filesystem::path& physicalPath);
		void unmount(std::string_view virtualPrefix);

		Result<std::filesystem::path> resolve(std::string_view virtualPath) const;
		bool exists(std::string_view virtualPath) const;

		Result<std::vector<uint8_t>> readBinaryFile(std::string_view virtualPath) const;
		Result<std::string> readTextFile(std::string_view virtualPath) const;

	private:
		static std::string NormalizeVirtualPath(std::string_view path);

		mutable std::shared_mutex m_mutex;
		std::map<std::string, std::filesystem::path> m_mounts;
	};
}
