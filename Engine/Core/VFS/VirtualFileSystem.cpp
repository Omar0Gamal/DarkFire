#include "Engine/Core/VFS/VirtualFileSystem.h"
#include <fstream>
#include <algorithm>

namespace Core
{
	VirtualFileSystem::VirtualFileSystem() = default;

	std::string VirtualFileSystem::NormalizeVirtualPath(std::string_view path)
	{
		std::string result(path);
		std::replace(result.begin(), result.end(), '\\', '/');
		if (!result.empty() && result.front() != '/')
		{
			result.insert(result.begin(), '/');
		}
		return result;
	}

	void VirtualFileSystem::mount(std::string_view virtualPrefix, const std::filesystem::path& physicalPath)
	{
		std::unique_lock lock(m_mutex);
		std::string prefix = NormalizeVirtualPath(virtualPrefix);
		if (!prefix.empty() && prefix.back() != '/')
		{
			prefix += '/';
		}
		m_mounts[prefix] = std::filesystem::absolute(physicalPath);
	}

	void VirtualFileSystem::unmount(std::string_view virtualPrefix)
	{
		std::unique_lock lock(m_mutex);
		std::string prefix = NormalizeVirtualPath(virtualPrefix);
		if (!prefix.empty() && prefix.back() != '/')
		{
			prefix += '/';
		}
		m_mounts.erase(prefix);
	}

	Result<std::filesystem::path> VirtualFileSystem::resolve(std::string_view virtualPath) const
	{
		std::shared_lock lock(m_mutex);
		std::string norm = NormalizeVirtualPath(virtualPath);

		// Find best matching mount point (longest prefix match)
		const std::string* bestPrefix = nullptr;
		const std::filesystem::path* bestTarget = nullptr;

		for (const auto& [prefix, target] : m_mounts)
		{
			if (norm.rfind(prefix, 0) == 0) // starts_with
			{
				if (!bestPrefix || prefix.length() > bestPrefix->length())
				{
					bestPrefix = &prefix;
					bestTarget = &target;
				}
			}
		}

		if (!bestPrefix)
		{
			return Result<std::filesystem::path>::Err("No mount point found for virtual path: " + norm);
		}

		std::string relativePath = norm.substr(bestPrefix->length());
		std::filesystem::path resolved = *bestTarget / relativePath;
		return Result<std::filesystem::path>::Ok(resolved);
	}

	bool VirtualFileSystem::exists(std::string_view virtualPath) const
	{
		auto resolved = resolve(virtualPath);
		if (resolved.isErr()) return false;
		return std::filesystem::exists(resolved.value());
	}

	Result<std::vector<uint8_t>> VirtualFileSystem::readBinaryFile(std::string_view virtualPath) const
	{
		auto resolved = resolve(virtualPath);
		if (resolved.isErr())
		{
			return Result<std::vector<uint8_t>>::Err(resolved.error());
		}

		std::ifstream file(resolved.value(), std::ios::binary | std::ios::ate);
		if (!file.is_open())
		{
			return Result<std::vector<uint8_t>>::Err("Failed to open file: " + resolved.value().string());
		}

		std::streamsize size = file.tellg();
		file.seekg(0, std::ios::beg);

		std::vector<uint8_t> buffer(static_cast<size_t>(size));
		if (!file.read(reinterpret_cast<char*>(buffer.data()), size))
		{
			return Result<std::vector<uint8_t>>::Err("Failed to read file: " + resolved.value().string());
		}

		return Result<std::vector<uint8_t>>::Ok(std::move(buffer));
	}

	Result<std::string> VirtualFileSystem::readTextFile(std::string_view virtualPath) const
	{
		auto binaryRes = readBinaryFile(virtualPath);
		if (binaryRes.isErr())
		{
			return Result<std::string>::Err(binaryRes.error());
		}
		const auto& bytes = binaryRes.value();
		return Result<std::string>::Ok(std::string(bytes.begin(), bytes.end()));
	}
}
