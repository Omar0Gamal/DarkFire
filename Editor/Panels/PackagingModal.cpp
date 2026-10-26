#include "Editor/Panels/PackagingModal.h"
#include "Engine/Core/Log/Log.h"
#include <imgui.h>
#include <windows.h>
#include <shellapi.h>
#include <fstream>
#include <thread>
#include <nlohmann/json.hpp>

namespace Editor
{
	PackagingModal::PackagingModal() = default;

	void PackagingModal::open(const std::filesystem::path& projectPath)
	{
		m_projectPath = projectPath;
		m_projectName = projectPath.parent_path().filename().string();
		if (m_projectName.empty()) m_projectName = "Game";

		// Try loading name from .dfproj
		if (std::filesystem::exists(projectPath))
		{
			std::ifstream f(projectPath);
			if (f.is_open())
			{
				try
				{
					nlohmann::json j;
					f >> j;
					if (j.contains("Project") && j["Project"].contains("Name"))
					{
						m_projectName = j["Project"]["Name"].get<std::string>();
					}
				}
				catch (...) {}
			}
		}

		m_isOpen = true;
		m_isPackaging = false;
		m_packagingFinished = false;
		m_packagingSuccess = false;
		m_progress = 0.0f;
		m_statusMessage = "Ready to package project '" + m_projectName + "'.";
	}

	void PackagingModal::executePackaging()
	{
		m_isPackaging = true;
		m_packagingFinished = false;
		m_packagingSuccess = false;
		m_progress = 0.05f;
		m_statusMessage = "Starting packaging pipeline...";

		std::string configName = (m_configIndex == 0) ? "Release" : ((m_configIndex == 1) ? "Debug" : "Dist");
		std::filesystem::path stagingDir = std::filesystem::path(m_outputDir) / m_projectName;
		m_finalOutputDir = stagingDir.string();

		std::thread([this, stagingDir, configName]() {
			try
			{
				std::error_code ec;
				std::filesystem::create_directories(stagingDir, ec);

				// Step 1: Run dfcook to create Engine.pak and Game.pak
				m_progress = 0.25f;
				m_statusMessage = "Cooking assets and building PAK archives...";

				std::string dfcookPath = "bin/" + configName + "-windows-x86_64/Tools/dfcook/dfcook.exe";
				if (!std::filesystem::exists(dfcookPath))
				{
					dfcookPath = "bin/Debug-windows-x86_64/Tools/dfcook/dfcook.exe";
				}

				if (std::filesystem::exists(dfcookPath))
				{
					std::string cmd = "\"" + dfcookPath + "\" \"" + m_projectPath.string() + "\" \"" + stagingDir.string() + "\" --all";
					if (m_useCookCache) cmd += " --cache";

					STARTUPINFOA si{};
					si.cb = sizeof(si);
					si.dwFlags |= STARTF_USESHOWWINDOW;
					si.wShowWindow = SW_HIDE;
					PROCESS_INFORMATION pi{};

					if (CreateProcessA(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NO_WINDOW, nullptr, nullptr, &si, &pi))
					{
						WaitForSingleObject(pi.hProcess, 60000);
						CloseHandle(pi.hProcess);
						CloseHandle(pi.hThread);
					}
				}

				// Step 2: Copy Player runtime and rename to <GameName>.exe
				m_progress = 0.65f;
				m_statusMessage = "Staging standalone runtime executable...";

				std::string playerExePath = "bin/" + configName + "-windows-x86_64/Player/Player.exe";
				if (!std::filesystem::exists(playerExePath))
				{
					playerExePath = "bin/Debug-windows-x86_64/Player/Player.exe";
				}

				if (std::filesystem::exists(playerExePath))
				{
					std::filesystem::path targetExe = stagingDir / (m_projectName + ".exe");
					std::filesystem::copy_file(playerExePath, targetExe, std::filesystem::copy_options::overwrite_existing, ec);
				}

				// Step 3: Copy required DLLs
				m_progress = 0.85f;
				m_statusMessage = "Copying runtime DLL dependencies...";

				std::vector<std::string> searchDirs = {
					"bin/" + configName + "-windows-x86_64/DarkFire",
					"bin/Debug-windows-x86_64/DarkFire",
					"bin/" + configName + "-windows-x86_64/Templates/" + m_projectName,
					"bin/Debug-windows-x86_64/Templates/" + m_projectName,
					"bin/" + configName + "-windows-x86_64/Player",
					"bin/Debug-windows-x86_64/Player"
				};

				for (const auto& sDir : searchDirs)
				{
					if (std::filesystem::exists(sDir))
					{
						for (const auto& entry : std::filesystem::directory_iterator(sDir))
						{
							if (entry.is_regular_file() && entry.path().extension() == ".dll")
							{
								std::filesystem::copy_file(entry.path(), stagingDir / entry.path().filename(), std::filesystem::copy_options::overwrite_existing, ec);
							}
						}
					}
				}

				m_progress = 1.0f;
				m_statusMessage = "Packaging finished successfully!";
				m_packagingSuccess = true;
			}
			catch (const std::exception& e)
			{
				m_statusMessage = std::string("Packaging error: ") + e.what();
				m_packagingSuccess = false;
			}

			m_isPackaging = false;
			m_packagingFinished = true;
		}).detach();
	}

	void PackagingModal::draw()
	{
		if (!m_isOpen) return;

		ImGui::OpenPopup("Package Game##PackagingModalWindow");
		ImGui::SetNextWindowSize(ImVec2(550.0f, 320.0f), ImGuiCond_Appearing);

		if (ImGui::BeginPopupModal("Package Game##PackagingModalWindow", &m_isOpen, ImGuiWindowFlags_NoResize))
		{
			ImGui::Text("Target Game: %s", m_projectName.c_str());
			ImGui::Text("Project File: %s", m_projectPath.string().c_str());
			ImGui::Separator();

			const char* configs[] = { "Release", "Debug", "Dist (Shipping)" };
			ImGui::Combo("Build Configuration", &m_configIndex, configs, IM_ARRAYSIZE(configs));

			char outBuf[256]{};
			strncpy_s(outBuf, m_outputDir.c_str(), sizeof(outBuf) - 1);
			if (ImGui::InputText("Staging Directory", outBuf, sizeof(outBuf)))
			{
				m_outputDir = outBuf;
			}

			ImGui::Checkbox("Use Incremental Cook Cache (.cookcache)", &m_useCookCache);
			ImGui::Separator();

			// Progress bar & status
			ImGui::ProgressBar(m_progress, ImVec2(-1, 24.0f));
			ImGui::TextWrapped("%s", m_statusMessage.c_str());

			ImGui::Separator();

			if (!m_isPackaging && !m_packagingFinished)
			{
				if (ImGui::Button("Package Game", ImVec2(130.0f, 28.0f)))
				{
					executePackaging();
				}
				ImGui::SameLine();
				if (ImGui::Button("Cancel", ImVec2(80.0f, 28.0f)))
				{
					m_isOpen = false;
					ImGui::CloseCurrentPopup();
				}
			}
			else if (m_packagingFinished)
			{
				if (m_packagingSuccess)
				{
					if (ImGui::Button("Open Package Folder", ImVec2(160.0f, 28.0f)))
					{
						ShellExecuteA(nullptr, "open", m_finalOutputDir.c_str(), nullptr, nullptr, SW_SHOW);
					}
					ImGui::SameLine();
				}
				if (ImGui::Button("Close", ImVec2(80.0f, 28.0f)))
				{
					m_isOpen = false;
					ImGui::CloseCurrentPopup();
				}
			}
			else
			{
				ImGui::BeginDisabled();
				ImGui::Button("Packaging in progress...", ImVec2(180.0f, 28.0f));
				ImGui::EndDisabled();
			}

			ImGui::EndPopup();
		}
	}
}
