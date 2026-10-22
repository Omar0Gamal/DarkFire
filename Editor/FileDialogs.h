#pragma once

#include <string>
#include <filesystem>

namespace Editor
{
	class FileDialogs
	{
	public:
		static std::string OpenFile(const char* filter);
		static std::string SaveFile(const char* filter);
		static std::string SelectFolder();

		static std::filesystem::path GetDocumentsPath();
		static std::filesystem::path GetDefaultProjectsDirectory();
	};
}
