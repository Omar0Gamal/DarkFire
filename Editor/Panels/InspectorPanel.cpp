#include "Editor/Panels/InspectorPanel.h"
#include "Editor/Commands/EntityCommands.h"
#include "Editor/UI/DockLayout.h"
#include "Editor/Inspector/FieldDrawers.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Reflection/Reflection.h"
#include "Engine/Core/Scripting/ScriptRegistry.h"
#include <imgui.h>
#include <string>
#include <vector>

namespace Editor
{
	void InspectorPanel::onImGuiRender(Core::Entity selectedEntity, bool* p_open)
	{
		ImGui::Begin(UI::PanelNames::Inspector, p_open);

		// Top Tab Bar
		if (ImGui::BeginTabBar("InspectorTabBar"))
		{
			if (ImGui::BeginTabItem("Inspector"))
			{
				m_selectedTab = 0;
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("Environment"))
			{
				m_selectedTab = 1;
				ImGui::EndTabItem();
			}
			if (ImGui::BeginTabItem("History"))
			{
				m_selectedTab = 2;
				ImGui::EndTabItem();
			}
			ImGui::EndTabBar();
		}
		ImGui::Separator();

		if (m_selectedTab == 1)
		{
			ImGui::TextColored(ImVec4{ 0.35f, 0.7f, 1.0f, 1.0f }, "World Environment & Post-Processing");
			ImGui::Spacing();

			if (!m_scene)
			{
				ImGui::TextDisabled("No active scene.");
				ImGui::End();
				return;
			}

			// Find EnvironmentComponent entity in scene
			Core::Entity envEntity{};
			auto envView = m_scene->view<Core::EnvironmentComponent>();
			for (auto handle : envView)
			{
				envEntity = Core::Entity{ handle, m_scene.get() };
				break;
			}

			// Find PostProcessingComponent entity in scene
			Core::Entity ppEntity{};
			auto ppView = m_scene->view<Core::PostProcessingComponent>();
			for (auto handle : ppView)
			{
				ppEntity = Core::Entity{ handle, m_scene.get() };
				break;
			}

			if (!envEntity.isValid() && !ppEntity.isValid())
			{
				ImGui::TextDisabled("No Environment or Post-Processing components found in scene.");
				ImGui::Spacing();
				if (ImGui::Button("+ Create World Environment Entity", ImVec2(-1, 30.0f)))
				{
					auto entity = m_scene->createEntity("World Environment");
					entity.addComponent<Core::EnvironmentComponent>();
					entity.addComponent<Core::PostProcessingComponent>();
				}
				ImGui::End();
				return;
			}

			if (envEntity.isValid())
			{
				auto& env = envEntity.getComponent<Core::EnvironmentComponent>();
				if (ImGui::CollapsingHeader("Sky & Atmosphere", ImGuiTreeNodeFlags_DefaultOpen))
				{
					const char* skyTypes[] = { "Procedural Gradient", "Skybox Texture", "Custom Color" };
					int curSky = static_cast<int>(env.skyType);
					if (ImGui::Combo("Sky Mode", &curSky, skyTypes, 3)) env.skyType = static_cast<Core::SkyType>(curSky);

					float skyCol[4] = { env.skyColor.R(), env.skyColor.G(), env.skyColor.B(), env.skyColor.A() };
					if (ImGui::ColorEdit3("Sky Zenith Color", skyCol)) env.skyColor = Core::Color(skyCol[0], skyCol[1], skyCol[2], skyCol[3]);

					float horCol[4] = { env.horizonColor.R(), env.horizonColor.G(), env.horizonColor.B(), env.horizonColor.A() };
					if (ImGui::ColorEdit3("Horizon Color", horCol)) env.horizonColor = Core::Color(horCol[0], horCol[1], horCol[2], horCol[3]);

					float gndCol[4] = { env.groundColor.R(), env.groundColor.G(), env.groundColor.B(), env.groundColor.A() };
					if (ImGui::ColorEdit3("Ground / Nadir Color", gndCol)) env.groundColor = Core::Color(gndCol[0], gndCol[1], gndCol[2], gndCol[3]);

					float ambCol[4] = { env.ambientColor.R(), env.ambientColor.G(), env.ambientColor.B(), env.ambientColor.A() };
					if (ImGui::ColorEdit3("Ambient Color", ambCol)) env.ambientColor = Core::Color(ambCol[0], ambCol[1], ambCol[2], ambCol[3]);

					ImGui::SliderFloat("Ambient Energy", &env.ambientEnergy, 0.0f, 5.0f, "%.2f");

					char texBuf[256];
					memset(texBuf, 0, sizeof(texBuf));
					strncpy_s(texBuf, env.skyboxTexturePath.c_str(), sizeof(texBuf) - 1);
					if (ImGui::InputText("Skybox Texture", texBuf, sizeof(texBuf))) env.skyboxTexturePath = texBuf;
				}
			}

			if (ppEntity.isValid())
			{
				auto& pp = ppEntity.getComponent<Core::PostProcessingComponent>();
				if (ImGui::CollapsingHeader("Tonemapping & Color (SDR)", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Checkbox("Enable Tonemapping", &pp.enableTonemapping);
					const char* tonemappers[] = { "ACES Filmic (Unreal / Godot 4)", "Reinhard", "Linear" };
					int curTm = static_cast<int>(pp.tonemapper);
					if (ImGui::Combo("Tonemapper", &curTm, tonemappers, 3)) pp.tonemapper = static_cast<Core::TonemapperType>(curTm);

					ImGui::SliderFloat("Exposure", &pp.exposure, 0.1f, 5.0f, "%.2f");
					ImGui::SliderFloat("Gamma (sRGB)", &pp.gamma, 1.0f, 3.0f, "%.2f");
				}

				if (ImGui::CollapsingHeader("Anti-Aliasing (FXAA 3.11)", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Checkbox("Enable FXAA", &pp.enableFXAA);
					ImGui::SliderFloat("Subpixel Quality", &pp.fxaaSubpix, 0.0f, 1.0f, "%.2f");
					ImGui::SliderFloat("Edge Threshold", &pp.fxaaEdgeThreshold, 0.063f, 0.333f, "%.3f");
					ImGui::SliderFloat("Edge Threshold Min", &pp.fxaaEdgeThresholdMin, 0.0312f, 0.0833f, "%.4f");
				}

				if (ImGui::CollapsingHeader("Ambient Occlusion (SSAO)", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Checkbox("Enable SSAO", &pp.enableSSAO);
					ImGui::SliderFloat("Radius", &pp.ssaoRadius, 0.1f, 2.0f, "%.2f");
					ImGui::SliderFloat("Bias", &pp.ssaoBias, 0.001f, 0.1f, "%.3f");
					ImGui::SliderFloat("Intensity", &pp.ssaoIntensity, 0.0f, 4.0f, "%.2f");
				}

				if (ImGui::CollapsingHeader("Bloom (Physically-based)", ImGuiTreeNodeFlags_DefaultOpen))
				{
					ImGui::Checkbox("Enable Bloom", &pp.enableBloom);
					ImGui::SliderFloat("Threshold", &pp.bloomThreshold, 0.1f, 5.0f, "%.2f");
					ImGui::SliderFloat("Intensity (UE subtle)", &pp.bloomIntensity, 0.0f, 0.5f, "%.3f");
				}
			}

			ImGui::End();
			return;
		}

		if (m_selectedTab == 2)
		{
			ImGui::TextColored(ImVec4{ 0.35f, 0.7f, 1.0f, 1.0f }, "Command History");
			ImGui::Spacing();
			if (!m_history)
			{
				ImGui::TextDisabled("No history attached.");
			}
			else
			{
				ImGui::Text("Undo Count: %zu | Redo Count: %zu", m_history->getUndoCount(), m_history->getRedoCount());
				ImGui::Text("Memory: %.2f KB", m_history->getMemoryUsage() / 1024.0f);
				ImGui::Separator();

				if (ImGui::Button("Undo", ImVec2{ 80.0f, 0.0f }) && m_history->canUndo())
				{
					m_history->undo();
				}
				ImGui::SameLine();
				if (ImGui::Button("Redo", ImVec2{ 80.0f, 0.0f }) && m_history->canRedo())
				{
					m_history->redo();
				}
				ImGui::SameLine();
				if (ImGui::Button("Clear"))
				{
					m_history->clear();
				}

				ImGui::Spacing();
				ImGui::TextDisabled("Recent Commands:");
				for (const auto& cmd : m_history->getUndoStack())
				{
					ImGui::BulletText("%s", std::string(cmd->getName()).c_str());
				}
			}
			ImGui::End();
			return;
		}

		if (!selectedEntity.isValid())
		{
			ImGui::TextDisabled("Select an entity to view its components.");
			ImGui::End();
			return;
		}

		drawComponents(selectedEntity);

		ImGui::End();
	}

	void InspectorPanel::drawComponents(Core::Entity entity)
	{
		// 1. Identity Component
		auto& id = entity.getComponent<Core::IdentityComponent>();

		char buffer[256];
		memset(buffer, 0, sizeof(buffer));
		strncpy_s(buffer, id.name.c_str(), sizeof(buffer) - 1);
		if (ImGui::InputText("##Name", buffer, sizeof(buffer)))
		{
			id.name = buffer;
		}

		ImGui::SameLine();
		ImGui::TextDisabled("ID: %llu", id.id.value());

		char tagBuffer[64];
		memset(tagBuffer, 0, sizeof(tagBuffer));
		strncpy_s(tagBuffer, id.tag.c_str(), sizeof(tagBuffer) - 1);
		if (ImGui::InputText("Tag", tagBuffer, sizeof(tagBuffer)))
		{
			id.tag = tagBuffer;
		}

		ImGui::Separator();

		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed |
			ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding;

		// 2. Transform Component (Dedicated XYZ handles & undo/redo tracking)
		if (entity.hasComponent<Core::TransformComponent>())
		{
			if (ImGui::TreeNodeEx((void*)typeid(Core::TransformComponent).hash_code(), treeNodeFlags, "Transform"))
			{
				auto& tc = entity.getComponent<Core::TransformComponent>();
				bool activated = false;
				bool deactivated = false;

				DrawVec3Control("Position", tc.position, 0.0f, 100.0f, &activated, &deactivated);

				Core::Vector3 euler = tc.eulerDegrees;
				if (DrawVec3Control("Rotation", euler, 0.0f, 100.0f, &activated, &deactivated))
				{
					tc.setEulerDegrees(euler);
				}

				DrawVec3Control("Scale", tc.scale, 1.0f, 100.0f, &activated, &deactivated, true, m_uniformScale);
				ImGui::Checkbox("Uniform Scale (🔗)", &m_uniformScale);

				if (activated && !m_isDraggingTransform)
				{
					m_dragInitialTransform = tc;
					m_isDraggingTransform = true;
				}

				if (deactivated)
				{
					if (m_history)
					{
						m_history->execute(std::make_unique<ModifyTransformCommand>(
							entity.getScene(),
							entity.getUUID(),
							m_dragInitialTransform,
							tc,
							false
						));
					}
					m_isDraggingTransform = false;
				}

				ImGui::TreePop();
			}
		}

		// 3. Dynamic Reflection-Driven Components
		std::string compToRemove;
		const auto& registeredComps = Core::ComponentRegistry::Get().getAll();

		for (const auto& desc : registeredComps)
		{
			if (!desc.isAuthored) continue;
			if (desc.name == "IdentityComponent" || desc.name == "TransformComponent") continue;
			if (!desc.hasComponent(*entity.getScene(), entity.getHandle())) continue;

			std::string displayName = FormatDisplayName(desc.name);
			ImGui::PushID(desc.name.c_str());

			bool open = ImGui::TreeNodeEx((void*)(uintptr_t)desc.typeId, treeNodeFlags, "%s", displayName.c_str());
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Remove Component"))
				{
					compToRemove = desc.name;
				}
				ImGui::EndPopup();
			}

			if (open)
			{
				if (desc.name == "ScriptsComponent")
				{
					auto& sc = entity.getComponent<Core::ScriptsComponent>();

					// Missing scripts warning banner
					if (!sc.missingScripts.empty())
					{
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 1.0f, 0.8f, 0.2f, 1.0f });
						for (size_t i = 0; i < sc.missingScripts.size(); ++i)
						{
							ImGui::Text("! Missing Script: %s", sc.missingScripts[i].scriptName.c_str());
							if (ImGui::IsItemHovered())
							{
								ImGui::SetTooltip("Serialized Data: %s", sc.missingScripts[i].rawJson.c_str());
							}
						}
						ImGui::PopStyleColor();
						ImGui::Separator();
					}

					// Attached scripts
					int scriptToDelete = -1;
					for (size_t i = 0; i < sc.scripts.size(); ++i)
					{
						ImGui::PushID(static_cast<int>(i));
						auto& scr = sc.scripts[i];

						ImGui::Checkbox("##Enabled", &scr.isEnabled);
						ImGui::SameLine();
						ImGui::Text("%s", scr.scriptName.c_str());
						ImGui::SameLine();

						if (ImGui::SmallButton("X"))
						{
							scriptToDelete = static_cast<int>(i);
						}
						ImGui::PopID();
					}

					if (scriptToDelete >= 0)
					{
						sc.scripts.erase(sc.scripts.begin() + scriptToDelete);
					}

					// Add script popup
					if (ImGui::Button("Add Script..."))
					{
						ImGui::OpenPopup("AddScriptPopup");
					}

					if (ImGui::BeginPopup("AddScriptPopup"))
					{
						const auto& registeredNames = Core::ScriptRegistry::Get().getRegisteredNames();
						if (registeredNames.empty())
						{
							ImGui::TextDisabled("No scripts registered.");
						}
						else
						{
							for (const auto& name : registeredNames)
							{
								if (ImGui::MenuItem(name.c_str()))
								{
									sc.addScript(name);
								}
							}
						}
						ImGui::EndPopup();
					}
				}
				else
				{
					void* compData = desc.getComponentRaw ? desc.getComponentRaw(*entity.getScene(), entity.getHandle()) : nullptr;
					if (compData)
					{
						for (const auto& field : desc.fields)
						{
							DrawField(field, compData);
						}

						// Component-specific editor ergonomic helpers
						if (desc.name == "MeshRendererComponent")
						{
							auto* mr = static_cast<Core::MeshRendererComponent*>(compData);
							ImGui::TextDisabled("Primitives:");
							ImGui::SameLine();
							if (ImGui::SmallButton("Cube")) mr->meshPath = "builtin:cube";
							ImGui::SameLine();
							if (ImGui::SmallButton("Sphere")) mr->meshPath = "builtin:sphere";
							ImGui::SameLine();
							if (ImGui::SmallButton("Plane")) mr->meshPath = "builtin:plane";
						}
					}
				}

				ImGui::TreePop();
			}

			ImGui::PopID();
		}

		if (!compToRemove.empty())
		{
			if (const auto* toRemove = Core::ComponentRegistry::Get().findByName(compToRemove))
			{
				toRemove->removeComponent(*entity.getScene(), entity.getHandle());
			}
		}

		// 4. Dynamic "Add Component" Menu (Auto-populated from reflection registry)
		ImGui::Spacing();
		if (ImGui::Button("Add Component...", ImVec2{ ImGui::GetContentRegionAvail().x, 0.0f }))
		{
			ImGui::OpenPopup("AddComponentPopup");
		}

		if (ImGui::BeginPopup("AddComponentPopup"))
		{
			for (const auto& desc : registeredComps)
			{
				if (!desc.isAuthored) continue;
				if (desc.name == "IdentityComponent" || desc.name == "TransformComponent") continue;
				if (!desc.hasComponent(*entity.getScene(), entity.getHandle()))
				{
					std::string itemLabel = FormatDisplayName(desc.name);
					if (ImGui::MenuItem(itemLabel.c_str()))
					{
						desc.addDefaultComponent(*entity.getScene(), entity.getHandle());
						ImGui::CloseCurrentPopup();
					}
				}
			}
			ImGui::EndPopup();
		}
	}
}
