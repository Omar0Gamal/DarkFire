#include "Editor/Scripting/ScriptWizardModal.h"
#include "Engine/Core/Log/Log.h"
#include <imgui.h>
#include <fstream>
#include <sstream>

namespace Editor
{
	void ScriptWizardModal::onImGuiRender(const std::filesystem::path& projectDir)
	{
		if (m_isOpen)
		{
			ImGui::OpenPopup("New C++ Script");
		}

		if (ImGui::BeginPopupModal("New C++ Script", &m_isOpen, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Create a new C++ script class inheriting from Core::ScriptableEntity");
			ImGui::Spacing();

			ImGui::Text("Script Class Name:");
			ImGui::SetNextItemWidth(350.0f);
			if (ImGui::InputTextWithHint("##ScriptName", "e.g. EnemyController, CameraFollow", m_scriptName, sizeof(m_scriptName)))
			{
				m_statusMessage.clear();
			}

			ImGui::Spacing();
			ImGui::Text("Script Template:");
			const char* templates[] = {
				"Gameplay Entity (Movement, Physics & Input)",
				"Rotator / Environmental Animator",
				"Trigger Zone / Volume",
				"Custom Logic Minimal"
			};
			ImGui::SetNextItemWidth(350.0f);
			ImGui::Combo("##Template", &m_selectedTemplate, templates, 4);

			ImGui::Spacing();
			if (!m_statusMessage.empty())
			{
				ImGui::TextColored(ImVec4(0.3f, 0.85f, 0.45f, 1.0f), "%s", m_statusMessage.c_str());
				ImGui::Spacing();
			}

			ImGui::Separator();

			if (ImGui::Button("Create Script", ImVec2(120, 28)))
			{
				std::string className = m_scriptName;
				// Trim spaces
				className.erase(0, className.find_first_not_of(" \t\n\r"));
				className.erase(className.find_last_not_of(" \t\n\r") + 1);

				if (className.empty())
				{
					m_statusMessage = "Error: Class name cannot be empty.";
				}
				else
				{
					std::filesystem::path sourceDir = projectDir / "Source";
					if (!std::filesystem::exists(sourceDir))
					{
						sourceDir = projectDir;
					}

					std::filesystem::path filePath = sourceDir / (className + ".h");
					std::ofstream out(filePath);
					if (out.is_open())
					{
						out << "#pragma once\n\n";
						out << "#include \"Engine/Core/Scripting/ScriptableEntity.h\"\n";
						out << "#include \"Engine/Core/Log/Log.h\"\n\n";
						out << "namespace Playground\n{\n";
						out << "\tclass " << className << " : public Core::ScriptableEntity\n\t{\n\tpublic:\n";

						if (m_selectedTemplate == 0) // Gameplay Entity
						{
							out << "\t\tvoid onCreate() override\n\t\t{\n";
							out << "\t\t\texposeFloat(\"Speed\", m_speed, 1.0f, 50.0f, 0.2f);\n";
							out << "\t\t}\n\n";
							out << "\t\tvoid onStart() override\n\t\t{\n";
							out << "\t\t\tCore::Log::Get().info(\"Game\", \"" << className << " started on entity: {}\", getEntity().getName());\n";
							out << "\t\t}\n\n";
							out << "\t\tvoid onUpdate(float deltaTime) override\n\t\t{\n";
							out << "\t\t\tCore::Vector2 moveInput = getMovementInput2D();\n";
							out << "\t\t\tCore::Vector3 moveDir(moveInput.x, 0.0f, moveInput.y);\n";
							out << "\t\t\tif (hasRigidbody())\n\t\t\t{\n";
							out << "\t\t\t\tsetHorizontalVelocity(moveDir * m_speed);\n";
							out << "\t\t\t}\n\t\t\telse\n\t\t\t{\n";
							out << "\t\t\t\ttranslate(moveDir * (m_speed * deltaTime));\n";
							out << "\t\t\t}\n\t\t}\n\n";
							out << "\tprivate:\n\t\tfloat m_speed = 5.0f;\n";
						}
						else if (m_selectedTemplate == 1) // Rotator
						{
							out << "\t\tvoid onCreate() override\n\t\t{\n";
							out << "\t\t\texposeFloat(\"Rotation Speed\", m_rotationSpeed, -360.0f, 360.0f, 1.0f);\n";
							out << "\t\t}\n\n";
							out << "\t\tvoid onUpdate(float deltaTime) override\n\t\t{\n";
							out << "\t\t\trotateY(m_rotationSpeed * deltaTime);\n";
							out << "\t\t}\n\n";
							out << "\tprivate:\n\t\tfloat m_rotationSpeed = 45.0f;\n";
						}
						else if (m_selectedTemplate == 2) // Trigger Zone
						{
							out << "\t\tvoid onTriggerEnter(Core::Entity other) override\n\t\t{\n";
							out << "\t\t\tCore::Log::Get().info(\"Game\", \"" << className << ": Entity entered trigger!\");\n";
							out << "\t\t}\n\n";
							out << "\t\tvoid onTriggerExit(Core::Entity other) override\n\t\t{\n";
							out << "\t\t\tCore::Log::Get().info(\"Game\", \"" << className << ": Entity exited trigger!\");\n";
							out << "\t\t}\n";
						}
						else // Minimal
						{
							out << "\t\tvoid onStart() override {}\n\n";
							out << "\t\tvoid onUpdate(float deltaTime) override { (void)deltaTime; }\n";
						}

						out << "\t};\n}\n";
						out.close();

						m_statusMessage = "Created script: " + filePath.string() + "\nPress F7 to build and hot-reload!";
						Core::Log::Get().info("Editor", "Created C++ script file at: {}", filePath.string());
					}
					else
					{
						m_statusMessage = "Failed to create file: " + filePath.string();
					}
				}
			}

			ImGui::SameLine();
			if (ImGui::Button("Close", ImVec2(90, 28)))
			{
				m_isOpen = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}
}
