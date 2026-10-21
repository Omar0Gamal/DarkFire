#include "Editor/Panels/InspectorPanel.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Scripting/ScriptRegistry.h"
#include <imgui.h>
#include <string>

namespace Editor
{
	static void DrawVec3Control(const char* label, Core::Vector3& values, float resetValue = 0.0f, float columnWidth = 100.0f)
	{
		ImGui::PushID(label);

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text("%s", label);
		ImGui::NextColumn();

		float totalWidth = ImGui::CalcItemWidth();
		float itemWidth = (totalWidth - 2.0f * ImGui::GetStyle().ItemSpacing.x) / 3.0f;
		if (itemWidth < 50.0f) itemWidth = 50.0f;

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

		float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		if (ImGui::Button("X", buttonSize)) values.x = resetValue;
		ImGui::PopStyleColor();
		ImGui::SameLine();
		ImGui::SetNextItemWidth(itemWidth);
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		if (ImGui::Button("Y", buttonSize)) values.y = resetValue;
		ImGui::PopStyleColor();
		ImGui::SameLine();
		ImGui::SetNextItemWidth(itemWidth);
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		if (ImGui::Button("Z", buttonSize)) values.z = resetValue;
		ImGui::PopStyleColor();
		ImGui::SameLine();
		ImGui::SetNextItemWidth(itemWidth);
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");

		ImGui::PopStyleVar();
		ImGui::Columns(1);
		ImGui::PopID();
	}

	void InspectorPanel::onImGuiRender(Core::Entity selectedEntity)
	{
		ImGui::Begin("Inspector");

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

		// 2. Transform Component
		if (entity.hasComponent<Core::TransformComponent>())
		{
			if (ImGui::TreeNodeEx((void*)typeid(Core::TransformComponent).hash_code(), treeNodeFlags, "Transform"))
			{
				auto& tc = entity.getComponent<Core::TransformComponent>();
				DrawVec3Control("Position", tc.position);

				Core::Vector3 euler = tc.eulerDegrees;
				DrawVec3Control("Rotation", euler);
				if (euler != tc.eulerDegrees)
				{
					tc.setEulerDegrees(euler);
				}

				DrawVec3Control("Scale", tc.scale, 1.0f);
				ImGui::TreePop();
			}
		}

		// 3. Camera Component
		if (entity.hasComponent<Core::CameraComponent>())
		{
			ImGui::PushID("CameraComponent");
			bool open = ImGui::TreeNodeEx((void*)typeid(Core::CameraComponent).hash_code(), treeNodeFlags, "Camera");
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Remove Component"))
				{
					entity.removeComponent<Core::CameraComponent>();
					ImGui::EndPopup();
					ImGui::PopID();
					return;
				}
				ImGui::EndPopup();
			}

			if (open)
			{
				auto& cam = entity.getComponent<Core::CameraComponent>();
				ImGui::Checkbox("Primary Camera", &cam.isPrimary);
				ImGui::Checkbox("Orthographic", &cam.isOrthographic);
				if (!cam.isOrthographic)
				{
					ImGui::SliderFloat("FOV", &cam.fov, 10.0f, 120.0f);
				}
				else
				{
					ImGui::DragFloat("Ortho Width", &cam.orthoWidth, 0.5f, 1.0f, 100.0f);
					ImGui::DragFloat("Ortho Height", &cam.orthoHeight, 0.5f, 1.0f, 100.0f);
				}
				ImGui::DragFloat("Near Z", &cam.nearZ, 0.01f, 0.001f, 10.0f);
				ImGui::DragFloat("Far Z", &cam.farZ, 1.0f, 1.0f, 10000.0f);
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		// 4. MeshRenderer Component
		if (entity.hasComponent<Core::MeshRendererComponent>())
		{
			ImGui::PushID("MeshRendererComponent");
			bool open = ImGui::TreeNodeEx((void*)typeid(Core::MeshRendererComponent).hash_code(), treeNodeFlags, "Mesh Renderer");
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Remove Component"))
				{
					entity.removeComponent<Core::MeshRendererComponent>();
					ImGui::EndPopup();
					ImGui::PopID();
					return;
				}
				ImGui::EndPopup();
			}

			if (open)
			{
				auto& mr = entity.getComponent<Core::MeshRendererComponent>();

				char meshBuf[256];
				memset(meshBuf, 0, sizeof(meshBuf));
				strncpy_s(meshBuf, mr.meshPath.c_str(), sizeof(meshBuf) - 1);
				if (ImGui::InputText("Mesh Path", meshBuf, sizeof(meshBuf)))
				{
					mr.meshPath = meshBuf;
				}

				char matBuf[256];
				memset(matBuf, 0, sizeof(matBuf));
				strncpy_s(matBuf, mr.materialPath.c_str(), sizeof(matBuf) - 1);
				if (ImGui::InputText("Material Path", matBuf, sizeof(matBuf)))
				{
					mr.materialPath = matBuf;
				}

				ImGui::Checkbox("Cast Shadows", &mr.castShadows);
				ImGui::Checkbox("Receive Shadows", &mr.receiveShadows);
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		// 5. Light Component
		if (entity.hasComponent<Core::LightComponent>())
		{
			ImGui::PushID("LightComponent");
			bool open = ImGui::TreeNodeEx((void*)typeid(Core::LightComponent).hash_code(), treeNodeFlags, "Light");
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Remove Component"))
				{
					entity.removeComponent<Core::LightComponent>();
					ImGui::EndPopup();
					ImGui::PopID();
					return;
				}
				ImGui::EndPopup();
			}

			if (open)
			{
				auto& light = entity.getComponent<Core::LightComponent>();

				const char* lightTypes[] = { "Directional", "Point", "Spot" };
				int currentType = static_cast<int>(light.type);
				if (ImGui::Combo("Type", &currentType, lightTypes, 3))
				{
					light.type = static_cast<Core::LightType>(currentType);
				}

				float color[4] = { light.color.R(), light.color.G(), light.color.B(), light.color.A() };
				if (ImGui::ColorEdit3("Color", color))
				{
					light.color = Core::Color(color[0], color[1], color[2], color[3]);
				}

				ImGui::DragFloat("Intensity", &light.intensity, 0.1f, 0.0f, 100.0f);
				if (light.type != Core::LightType::Directional)
				{
					ImGui::DragFloat("Range", &light.range, 0.5f, 0.1f, 500.0f);
				}
				if (light.type == Core::LightType::Spot)
				{
					ImGui::SliderFloat("Inner Angle", &light.spotInnerAngle, 1.0f, 89.0f);
					ImGui::SliderFloat("Outer Angle", &light.spotOuterAngle, light.spotInnerAngle, 90.0f);
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		// 6. Rigidbody Component
		if (entity.hasComponent<Core::RigidbodyComponent>())
		{
			ImGui::PushID("RigidbodyComponent");
			bool open = ImGui::TreeNodeEx((void*)typeid(Core::RigidbodyComponent).hash_code(), treeNodeFlags, "Rigidbody");
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Remove Component"))
				{
					entity.removeComponent<Core::RigidbodyComponent>();
					ImGui::EndPopup();
					ImGui::PopID();
					return;
				}
				ImGui::EndPopup();
			}

			if (open)
			{
				auto& rb = entity.getComponent<Core::RigidbodyComponent>();
				ImGui::DragFloat("Mass", &rb.mass, 0.1f, 0.001f, 10000.0f);
				ImGui::DragFloat("Drag", &rb.drag, 0.01f, 0.0f, 10.0f);
				ImGui::DragFloat("Angular Drag", &rb.angularDrag, 0.01f, 0.0f, 10.0f);
				ImGui::Checkbox("Is Kinematic", &rb.isKinematic);
				ImGui::Checkbox("Use Gravity", &rb.useGravity);
				ImGui::SliderFloat("Restitution", &rb.restitution, 0.0f, 1.0f);
				ImGui::SliderFloat("Friction", &rb.friction, 0.0f, 1.0f);
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		// 7. Collider Component
		if (entity.hasComponent<Core::ColliderComponent>())
		{
			ImGui::PushID("ColliderComponent");
			bool open = ImGui::TreeNodeEx((void*)typeid(Core::ColliderComponent).hash_code(), treeNodeFlags, "Collider");
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Remove Component"))
				{
					entity.removeComponent<Core::ColliderComponent>();
					ImGui::EndPopup();
					ImGui::PopID();
					return;
				}
				ImGui::EndPopup();
			}

			if (open)
			{
				auto& col = entity.getComponent<Core::ColliderComponent>();

				const char* shapes[] = { "Box", "Sphere", "Capsule" };
				int currentShape = static_cast<int>(col.shape);
				if (ImGui::Combo("Shape", &currentShape, shapes, 3))
				{
					col.shape = static_cast<Core::ColliderShape>(currentShape);
				}

				if (col.shape == Core::ColliderShape::Box)
				{
					DrawVec3Control("Size", col.size, 1.0f);
				}
				else if (col.shape == Core::ColliderShape::Sphere)
				{
					ImGui::DragFloat("Radius", &col.radius, 0.05f, 0.01f, 100.0f);
				}
				else if (col.shape == Core::ColliderShape::Capsule)
				{
					ImGui::DragFloat("Radius", &col.radius, 0.05f, 0.01f, 100.0f);
					ImGui::DragFloat("Height", &col.height, 0.05f, 0.01f, 100.0f);
				}

				ImGui::Checkbox("Is Trigger", &col.isTrigger);
				int layer = static_cast<int>(col.layer);
				if (ImGui::SliderInt("Layer", &layer, 0, 15))
				{
					col.layer = static_cast<uint8_t>(layer);
				}
				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		// 8. Scripts Component
		if (entity.hasComponent<Core::ScriptsComponent>())
		{
			ImGui::PushID("ScriptsComponent");
			bool open = ImGui::TreeNodeEx((void*)typeid(Core::ScriptsComponent).hash_code(), treeNodeFlags, "Scripts");
			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Remove Component"))
				{
					entity.removeComponent<Core::ScriptsComponent>();
					ImGui::EndPopup();
					ImGui::PopID();
					return;
				}
				ImGui::EndPopup();
			}

			if (open)
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

				ImGui::TreePop();
			}
			ImGui::PopID();
		}

		// Add Component button
		ImGui::Spacing();
		if (ImGui::Button("Add Component...", ImVec2{ ImGui::GetContentRegionAvail().x, 0.0f }))
		{
			ImGui::OpenPopup("AddComponentPopup");
		}

		if (ImGui::BeginPopup("AddComponentPopup"))
		{
			if (!entity.hasComponent<Core::CameraComponent>() && ImGui::MenuItem("Camera"))
			{
				entity.addComponent<Core::CameraComponent>();
				ImGui::CloseCurrentPopup();
			}
			if (!entity.hasComponent<Core::MeshRendererComponent>() && ImGui::MenuItem("Mesh Renderer"))
			{
				entity.addComponent<Core::MeshRendererComponent>();
				ImGui::CloseCurrentPopup();
			}
			if (!entity.hasComponent<Core::LightComponent>() && ImGui::MenuItem("Light"))
			{
				entity.addComponent<Core::LightComponent>();
				ImGui::CloseCurrentPopup();
			}
			if (!entity.hasComponent<Core::RigidbodyComponent>() && ImGui::MenuItem("Rigidbody"))
			{
				entity.addComponent<Core::RigidbodyComponent>();
				ImGui::CloseCurrentPopup();
			}
			if (!entity.hasComponent<Core::ColliderComponent>() && ImGui::MenuItem("Collider"))
			{
				entity.addComponent<Core::ColliderComponent>();
				ImGui::CloseCurrentPopup();
			}
			if (!entity.hasComponent<Core::ScriptsComponent>() && ImGui::MenuItem("Scripts"))
			{
				entity.addComponent<Core::ScriptsComponent>();
				ImGui::CloseCurrentPopup();
			}
			ImGui::EndPopup();
		}
	}
}
