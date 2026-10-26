#include "Editor/Panels/ProjectSettingsPanel.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Console/CVar.h"
#include "Engine/Audio/AudioSystem.h"
#include "Engine/Runtime/Application.h"
#include <imgui.h>
#include <nlohmann/json.hpp>
#include <fstream>

namespace Editor
{
	ProjectSettingsPanel::ProjectSettingsPanel()
	{
		m_collisionMatrix.fill(0xFFFF);
	}

	void ProjectSettingsPanel::open(const std::filesystem::path& projectPath)
	{
		m_projectPath = projectPath;
		m_isOpen = true;
		loadFromFile(projectPath);
	}

	bool ProjectSettingsPanel::loadFromFile(const std::filesystem::path& path)
	{
		if (!std::filesystem::exists(path)) return false;

		std::ifstream file(path);
		if (!file.is_open()) return false;

		try
		{
			nlohmann::json j;
			file >> j;

			if (j.contains("Project"))
			{
				const auto& p = j["Project"];
				if (p.contains("Name")) m_projectName = p["Name"].get<std::string>();
				if (p.contains("Version")) m_version = p["Version"].get<std::string>();
				if (p.contains("EngineVersion")) m_engineVersion = p["EngineVersion"].get<std::string>();
				if (p.contains("AssetDirectory")) m_assetDir = p["AssetDirectory"].get<std::string>();
				if (p.contains("StartScene")) m_startScene = p["StartScene"].get<std::string>();
				if (p.contains("Module")) m_modulePath = p["Module"].get<std::string>();
			}

			if (j.contains("Window"))
			{
				const auto& w = j["Window"];
				if (w.contains("Title")) m_windowTitle = w["Title"].get<std::string>();
				if (w.contains("Width")) m_windowWidth = w["Width"].get<int>();
				if (w.contains("Height")) m_windowHeight = w["Height"].get<int>();
				if (w.contains("VSync")) m_vsync = w["VSync"].get<bool>();
				if (w.contains("Fullscreen")) m_fullscreen = w["Fullscreen"].get<bool>();
			}

			if (j.contains("Physics"))
			{
				const auto& phys = j["Physics"];
				if (phys.contains("Gravity") && phys["Gravity"].is_array() && phys["Gravity"].size() >= 3)
				{
					m_gravity[0] = phys["Gravity"][0].get<float>();
					m_gravity[1] = phys["Gravity"][1].get<float>();
					m_gravity[2] = phys["Gravity"][2].get<float>();
				}
				if (phys.contains("FixedTimeStep")) m_fixedTimeStep = phys["FixedTimeStep"].get<float>();
				if (phys.contains("CollisionMatrix") && phys["CollisionMatrix"].is_array())
				{
					for (size_t i = 0; i < std::min(m_collisionMatrix.size(), phys["CollisionMatrix"].size()); ++i)
					{
						m_collisionMatrix[i] = phys["CollisionMatrix"][i].get<uint16_t>();
					}
				}
			}

			if (j.contains("Networking"))
			{
				const auto& net = j["Networking"];
				if (net.contains("TickRate")) m_tickRate = net["TickRate"].get<int>();
				if (net.contains("BroadcastRate")) m_broadcastRate = net["BroadcastRate"].get<int>();
				if (net.contains("SimulatedLatency")) m_simulatedLatency = net["SimulatedLatency"].get<float>();
				if (net.contains("SimulatedLoss")) m_simulatedLoss = net["SimulatedLoss"].get<float>();
			}

			if (j.contains("Audio"))
			{
				const auto& aud = j["Audio"];
				if (aud.contains("MasterVolume")) m_masterVolume = aud["MasterVolume"].get<float>();
				if (aud.contains("SFXVolume")) m_sfxVolume = aud["SFXVolume"].get<float>();
				if (aud.contains("MusicVolume")) m_musicVolume = aud["MusicVolume"].get<float>();
				if (aud.contains("DopplerFactor")) m_dopplerFactor = aud["DopplerFactor"].get<float>();
			}

			return true;
		}
		catch (const std::exception& e)
		{
			Core::Log::Get().error("ProjectSettings", "Failed to parse {}: {}", path.string(), e.what());
			return false;
		}
	}

	bool ProjectSettingsPanel::saveToFile(const std::filesystem::path& path)
	{
		nlohmann::json j;

		j["Project"] = {
			{ "Name", m_projectName },
			{ "Version", m_version },
			{ "EngineVersion", m_engineVersion },
			{ "AssetDirectory", m_assetDir },
			{ "StartScene", m_startScene },
			{ "Module", m_modulePath }
		};

		j["Window"] = {
			{ "Title", m_windowTitle.empty() ? ("DarkFire - " + m_projectName) : m_windowTitle },
			{ "Width", m_windowWidth },
			{ "Height", m_windowHeight },
			{ "VSync", m_vsync },
			{ "Fullscreen", m_fullscreen }
		};

		j["Physics"] = {
			{ "Gravity", { m_gravity[0], m_gravity[1], m_gravity[2] } },
			{ "FixedTimeStep", m_fixedTimeStep },
			{ "CollisionMatrix", m_collisionMatrix }
		};

		j["Networking"] = {
			{ "TickRate", m_tickRate },
			{ "BroadcastRate", m_broadcastRate },
			{ "SimulatedLatency", m_simulatedLatency },
			{ "SimulatedLoss", m_simulatedLoss }
		};

		j["Audio"] = {
			{ "MasterVolume", m_masterVolume },
			{ "SFXVolume", m_sfxVolume },
			{ "MusicVolume", m_musicVolume },
			{ "DopplerFactor", m_dopplerFactor }
		};

		std::ofstream outFile(path);
		if (!outFile.is_open())
		{
			Core::Log::Get().error("ProjectSettings", "Failed to write project settings: {}", path.string());
			return false;
		}

		outFile << j.dump(2) << std::endl;
		Core::Log::Get().info("ProjectSettings", "Saved project settings to {}", path.string());

		// Apply immediately to active engine systems
		Runtime::Application::Get().getPhysicsWorld().setGravity(Core::Vector3(m_gravity[0], m_gravity[1], m_gravity[2]));
		Runtime::Application::Get().getPhysicsWorld().setCollisionMatrix(m_collisionMatrix);
		Audio::AudioSystem::Get().setBusVolume(Core::AudioBus::Master, m_masterVolume);
		Audio::AudioSystem::Get().setBusVolume(Core::AudioBus::SFX, m_sfxVolume);
		Audio::AudioSystem::Get().setBusVolume(Core::AudioBus::Music, m_musicVolume);

		if (auto* cvarLatency = Core::CVarSystem::Get().findCVar("net.sim.latency"))
		{
			cvarLatency->set(m_simulatedLatency);
		}
		if (auto* cvarLoss = Core::CVarSystem::Get().findCVar("net.sim.packetloss"))
		{
			cvarLoss->set(m_simulatedLoss);
		}

		return true;
	}

	void ProjectSettingsPanel::draw(bool* p_open)
	{
		if (!m_isOpen) return;

		ImGui::SetNextWindowSize(ImVec2(650.0f, 480.0f), ImGuiCond_FirstUseEver);
		if (ImGui::Begin("Project Settings", &m_isOpen, ImGuiWindowFlags_NoCollapse))
		{
			if (ImGui::BeginTabBar("##ProjectSettingsTabs"))
			{
				// 1. General Tab
				if (ImGui::BeginTabItem("General"))
				{
					char nameBuf[128]{};
					strncpy_s(nameBuf, m_projectName.c_str(), sizeof(nameBuf) - 1);
					if (ImGui::InputText("Project Name", nameBuf, sizeof(nameBuf)))
					{
						m_projectName = nameBuf;
					}

					char verBuf[32]{};
					strncpy_s(verBuf, m_version.c_str(), sizeof(verBuf) - 1);
					if (ImGui::InputText("Project Version", verBuf, sizeof(verBuf)))
					{
						m_version = verBuf;
					}

					char sceneBuf[256]{};
					strncpy_s(sceneBuf, m_startScene.c_str(), sizeof(sceneBuf) - 1);
					if (ImGui::InputText("Start Scene", sceneBuf, sizeof(sceneBuf)))
					{
						m_startScene = sceneBuf;
					}

					char assetBuf[128]{};
					strncpy_s(assetBuf, m_assetDir.c_str(), sizeof(assetBuf) - 1);
					if (ImGui::InputText("Asset Directory", assetBuf, sizeof(assetBuf)))
					{
						m_assetDir = assetBuf;
					}

					char modBuf[256]{};
					strncpy_s(modBuf, m_modulePath.c_str(), sizeof(modBuf) - 1);
					if (ImGui::InputText("Game Module DLL", modBuf, sizeof(modBuf)))
					{
						m_modulePath = modBuf;
					}

					ImGui::EndTabItem();
				}

				// 2. Rendering Tab
				if (ImGui::BeginTabItem("Rendering"))
				{
					ImGui::InputInt("Default Width", &m_windowWidth);
					ImGui::InputInt("Default Height", &m_windowHeight);
					ImGui::Checkbox("VSync Enabled", &m_vsync);
					ImGui::Checkbox("Fullscreen on Launch", &m_fullscreen);

					ImGui::Separator();
					ImGui::TextDisabled("Direct3D 11 FL 11_1 Target: Reversed-Z 32-bit Floating Point Depth");

					ImGui::EndTabItem();
				}

				// 3. Physics Tab
				if (ImGui::BeginTabItem("Physics"))
				{
					ImGui::DragFloat3("Gravity (m/s^2)", m_gravity, 0.1f, -50.0f, 50.0f);
					ImGui::DragFloat("Fixed Timestep (s)", &m_fixedTimeStep, 0.001f, 0.001f, 0.1f, "%.4f (60 Hz = 0.0167s)");

					ImGui::Separator();
					ImGui::Text("Collision Matrix (16x16 Layers):");
					if (ImGui::BeginTable("##CollisionMatrixTable", 17, ImGuiTableFlags_Borders | ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingFixedFit))
					{
						ImGui::TableNextRow();
						ImGui::TableSetColumnIndex(0);
						ImGui::Text("L");
						for (int col = 0; col < 16; ++col)
						{
							ImGui::TableSetColumnIndex(col + 1);
							ImGui::Text("%d", col);
						}

						for (int row = 0; row < 16; ++row)
						{
							ImGui::TableNextRow();
							ImGui::TableSetColumnIndex(0);
							ImGui::Text("%d", row);

							for (int col = 0; col < 16; ++col)
							{
								ImGui::TableSetColumnIndex(col + 1);
								bool enabled = (m_collisionMatrix[row] & (1 << col)) != 0;
								ImGui::PushID(row * 16 + col);
								if (ImGui::Checkbox("##col", &enabled))
								{
									if (enabled)
									{
										m_collisionMatrix[row] |= (1 << col);
										m_collisionMatrix[col] |= (1 << row);
									}
									else
									{
										m_collisionMatrix[row] &= ~(1 << col);
										m_collisionMatrix[col] &= ~(1 << row);
									}
								}
								ImGui::PopID();
							}
						}
						ImGui::EndTable();
					}

					ImGui::EndTabItem();
				}

				// 4. Networking Tab
				if (ImGui::BeginTabItem("Networking"))
				{
					ImGui::SliderInt("Server Simulation Rate (Hz)", &m_tickRate, 10, 120);
					ImGui::SliderInt("Client Send Rate (Hz)", &m_broadcastRate, 10, 60);

					ImGui::Separator();
					ImGui::Text("Network Simulation (Testing):");
					ImGui::SliderFloat("Simulated Latency (ms)", &m_simulatedLatency, 0.0f, 500.0f, "%.0f ms");
					ImGui::SliderFloat("Simulated Packet Loss (%)", &m_simulatedLoss, 0.0f, 50.0f, "%.1f %%");

					ImGui::EndTabItem();
				}

				// 5. Audio Tab
				if (ImGui::BeginTabItem("Audio"))
				{
					ImGui::SliderFloat("Master Bus Volume", &m_masterVolume, 0.0f, 1.0f);
					ImGui::SliderFloat("SFX Bus Volume", &m_sfxVolume, 0.0f, 1.0f);
					ImGui::SliderFloat("Music Bus Volume", &m_musicVolume, 0.0f, 1.0f);
					ImGui::SliderFloat("Global Doppler Scale", &m_dopplerFactor, 0.0f, 3.0f);

					ImGui::EndTabItem();
				}

				ImGui::EndTabBar();
			}

			ImGui::Separator();
			if (ImGui::Button("Save & Apply Settings", ImVec2(180.0f, 28.0f)))
			{
				if (!m_projectPath.empty())
				{
					saveToFile(m_projectPath);
				}
			}
			ImGui::SameLine();
			if (ImGui::Button("Close", ImVec2(80.0f, 28.0f)))
			{
				m_isOpen = false;
			}
		}
		ImGui::End();

		if (p_open) *p_open = m_isOpen;
	}
}
