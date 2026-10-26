#pragma once

#include "Engine/Core/Defines.h"
#include <string>
#include <filesystem>
#include <vector>

namespace Editor
{
	class PackagingModal
	{
	public:
		PackagingModal();

		void open(const std::filesystem::path& projectPath);
		void draw();
		bool isOpen() const noexcept { return m_isOpen; }

	private:
		void executePackaging();

		bool m_isOpen = false;
		std::filesystem::path m_projectPath;
		std::string m_projectName;
		std::string m_outputDir = "Package/Windows";
		int m_configIndex = 0; // 0: Release, 1: Debug, 2: Dist
		bool m_useCookCache = true;

		bool m_isPackaging = false;
		float m_progress = 0.0f;
		std::string m_statusMessage = "Ready to package.";
		bool m_packagingFinished = false;
		bool m_packagingSuccess = false;
		std::string m_finalOutputDir;
	};
}
