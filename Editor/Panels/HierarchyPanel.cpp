#include "Editor/Panels/HierarchyPanel.h"
#include "Editor/Clipboard.h"
#include "Editor/Commands/EntityCommands.h"
#include "Editor/UI/EditorIcons.h"
#include "Editor/UI/DockLayout.h"
#include "Engine/Core/Scene/Components.h"
#include <imgui.h>
#include <cstring>
#include <algorithm>

namespace Editor
{
	HierarchyPanel::HierarchyPanel(std::shared_ptr<Core::Scene> scene, SelectionContext* selection, CommandHistory* history)
		: m_scene(std::move(scene))
		, m_selection(selection)
		, m_history(history)
	{
	}

	void HierarchyPanel::setContext(std::shared_ptr<Core::Scene> scene, SelectionContext* selection, CommandHistory* history)
	{
		m_scene = std::move(scene);
		m_selection = selection;
		m_history = history;
	}

	void HierarchyPanel::setScene(std::shared_ptr<Core::Scene> scene)
	{
		m_scene = std::move(scene);
		if (m_selection)
		{
			m_selection->clear();
		}
	}

	void HierarchyPanel::onImGuiRender(bool* p_open)
	{
		ImGui::Begin(UI::PanelNames::Scene, p_open);

		if (!m_scene)
		{
			ImGui::TextDisabled("No active scene loaded.");
			ImGui::End();
			return;
		}

		if (m_selection)
		{
			m_selection->removeInvalidEntities();
		}

		// Godot 4 Top Action Bar
		if (ImGui::Button("+ Add Node"))
		{
			m_showAddNodeModal = true;
			m_addNodeFilter[0] = '\0';
		}
		ImGui::SameLine();
		if (ImGui::Button("Instantiate"))
		{
			// Pick scene
		}

		// Search Filter
		ImGui::SetNextItemWidth(-1);
		ImGui::InputTextWithHint("##Search", "Filter nodes...", m_searchFilter, sizeof(m_searchFilter));
		ImGui::Separator();

		drawAddNodeModal();

		// Context menu on empty space
		if (ImGui::BeginPopupContextWindow("HierarchyContextMenu", ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems))
		{
			drawPanelContextMenu();
			ImGui::EndPopup();
		}

		// Iterate entities
		auto view = m_scene->view<Core::IdentityComponent, Core::HierarchyComponent>();
		for (auto handle : view)
		{
			const auto& hier = view.get<Core::HierarchyComponent>(handle);
			// Draw root entities first
			if (hier.parent == entt::null || !m_scene->getRegistry().valid(hier.parent))
			{
				drawEntityNode(Core::Entity(handle, m_scene.get()));
			}
		}

		// Click on empty space clears selection
		if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered())
		{
			if (m_selection && !ImGui::GetIO().KeyCtrl)
			{
				m_selection->clear();
			}
		}

		ImGui::End();
	}

	void HierarchyPanel::drawEntityNode(Core::Entity entity)
	{
		if (!entity.isValid())
		{
			return;
		}

		const auto& name = entity.getName();

		// Filter
		if (m_searchFilter[0] != '\0')
		{
			std::string lowerName = name;
			std::string lowerFilter = m_searchFilter;
			std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
			std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);
			if (lowerName.find(lowerFilter) == std::string::npos)
			{
				return;
			}
		}

		auto children = entity.getChildren();
		bool hasChildren = !children.empty();

		bool isSelected = m_selection && m_selection->isSelected(entity);
		ImGuiTreeNodeFlags flags = (isSelected ? ImGuiTreeNodeFlags_Selected : 0);
		flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (!hasChildren)
		{
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}

		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)entity.getUUID().value(), flags, "%s", name.c_str());

		if (ImGui::IsItemClicked())
		{
			bool ctrlHeld = ImGui::GetIO().KeyCtrl;
			if (m_selection)
			{
				m_selection->select(entity, ctrlHeld);
			}
		}

		// Context Menu
		if (ImGui::BeginPopupContextItem())
		{
			drawContextMenu(entity);
			ImGui::EndPopup();
		}

		// Drag & Drop Source
		if (ImGui::BeginDragDropSource())
		{
			entt::entity handle = entity.getHandle();
			ImGui::SetDragDropPayload("DARKFIRE_ENTITY", &handle, sizeof(entt::entity));
			ImGui::Text("Reparent %s", name.c_str());
			ImGui::EndDragDropSource();
		}

		// Drag & Drop Target
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DARKFIRE_ENTITY"))
			{
				entt::entity droppedHandle = *reinterpret_cast<const entt::entity*>(payload->Data);
				Core::Entity droppedEntity(droppedHandle, m_scene.get());
				if (droppedEntity.isValid() && droppedEntity != entity)
				{
					if (m_history)
					{
						Core::UUID oldParent = droppedEntity.getParent().isValid() ? droppedEntity.getParent().getUUID() : Core::UUID(0);
						m_history->execute(std::make_unique<ReparentCommand>(m_scene.get(), droppedEntity.getUUID(), oldParent, entity.getUUID()));
					}
					else
					{
						droppedEntity.setParent(entity);
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		// Draw children
		if (opened && hasChildren)
		{
			for (auto child : children)
			{
				drawEntityNode(child);
			}
			ImGui::TreePop();
		}
	}

	void HierarchyPanel::drawContextMenu(Core::Entity entity)
	{
		if (ImGui::MenuItem("Create Child Entity"))
		{
			auto child = m_scene->createEntity("Child");
			child.setParent(entity);
			if (m_history)
			{
				m_history->execute(std::make_unique<CreateEntityCommand>(m_scene.get(), child));
			}
			if (m_selection)
			{
				m_selection->select(child, false);
			}
		}

		if (ImGui::MenuItem("Duplicate", "Ctrl+D"))
		{
			if (m_history && m_selection)
			{
				auto duplicated = Clipboard::Duplicate({ entity }, m_scene.get(), *m_history);
				if (!duplicated.empty())
				{
					m_selection->select(duplicated[0], false);
				}
			}
		}

		if (ImGui::MenuItem("Delete Entity", "Delete"))
		{
			if (m_selection)
			{
				m_selection->deselect(entity);
			}
			if (m_history)
			{
				m_history->execute(std::make_unique<DeleteEntityCommand>(m_scene.get(), entity));
			}
			else
			{
				entity.destroy();
			}
		}
	}

	void HierarchyPanel::drawPanelContextMenu()
	{
		auto registerNewEntity = [&](Core::Entity ent) {
			if (m_history)
			{
				m_history->execute(std::make_unique<CreateEntityCommand>(m_scene.get(), ent));
			}
			if (m_selection)
			{
				m_selection->select(ent, false);
			}
		};

		if (ImGui::MenuItem("Create Empty Entity"))
		{
			registerNewEntity(m_scene->createEntity("Empty Entity"));
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Camera"))
		{
			auto ent = m_scene->createEntity("Main Camera");
			ent.getComponent<Core::TransformComponent>().position = Core::Vector3(0.0f, 2.0f, -5.0f);
			auto& cam = ent.addComponent<Core::CameraComponent>();
			cam.isPrimary = true;
			registerNewEntity(ent);
		}

		if (ImGui::BeginMenu("Light"))
		{
			if (ImGui::MenuItem("Directional Light"))
			{
				auto ent = m_scene->createEntity("Directional Light");
				ent.getComponent<Core::TransformComponent>().position = Core::Vector3(0.0f, 10.0f, 0.0f);
				ent.getComponent<Core::TransformComponent>().setEulerDegrees(Core::Vector3(45.0f, 0.0f, 0.0f));
				auto& l = ent.addComponent<Core::LightComponent>();
				l.type = Core::LightType::Directional;
				l.color = Core::Color(1.0f, 0.95f, 0.8f, 1.0f);
				l.intensity = 1.0f;
				registerNewEntity(ent);
			}
			if (ImGui::MenuItem("Point Light"))
			{
				auto ent = m_scene->createEntity("Point Light");
				ent.getComponent<Core::TransformComponent>().position = Core::Vector3(0.0f, 3.0f, 0.0f);
				auto& l = ent.addComponent<Core::LightComponent>();
				l.type = Core::LightType::Point;
				l.color = Core::Color(1.0f, 1.0f, 1.0f, 1.0f);
				l.intensity = 2.0f;
				l.range = 10.0f;
				registerNewEntity(ent);
			}
			ImGui::EndMenu();
		}

		if (ImGui::BeginMenu("3D Object"))
		{
			if (ImGui::MenuItem("Cube"))
			{
				auto ent = m_scene->createEntity("Cube");
				ent.getComponent<Core::TransformComponent>().position = Core::Vector3(0.0f, 0.5f, 0.0f);
				auto& mesh = ent.addComponent<Core::MeshRendererComponent>();
				mesh.meshPath = "builtin:cube";
				mesh.materialPath = "/engine/Content/Default.mat";
				auto& col = ent.addComponent<Core::ColliderComponent>();
				col.shape = Core::ColliderShape::Box;
				col.size = Core::Vector3(1.0f, 1.0f, 1.0f);
				registerNewEntity(ent);
			}

			if (ImGui::MenuItem("Sphere"))
			{
				auto ent = m_scene->createEntity("Sphere");
				ent.getComponent<Core::TransformComponent>().position = Core::Vector3(0.0f, 0.5f, 0.0f);
				auto& mesh = ent.addComponent<Core::MeshRendererComponent>();
				mesh.meshPath = "builtin:sphere";
				mesh.materialPath = "/engine/Content/Default.mat";
				auto& col = ent.addComponent<Core::ColliderComponent>();
				col.shape = Core::ColliderShape::Sphere;
				col.radius = 0.5f;
				registerNewEntity(ent);
			}

			if (ImGui::MenuItem("Plane"))
			{
				auto ent = m_scene->createEntity("Plane");
				ent.getComponent<Core::TransformComponent>().position = Core::Vector3(0.0f, 0.0f, 0.0f);
				ent.getComponent<Core::TransformComponent>().scale = Core::Vector3(10.0f, 1.0f, 10.0f);
				auto& mesh = ent.addComponent<Core::MeshRendererComponent>();
				mesh.meshPath = "builtin:plane";
				mesh.materialPath = "/engine/Content/Default.mat";
				auto& col = ent.addComponent<Core::ColliderComponent>();
				col.shape = Core::ColliderShape::Box;
				col.size = Core::Vector3(10.0f, 0.1f, 10.0f);
				registerNewEntity(ent);
			}
			ImGui::EndMenu();
		}
	}

	void HierarchyPanel::drawAddNodeModal()
	{
		if (m_showAddNodeModal)
		{
			ImGui::OpenPopup("Create New Node");
		}

		if (ImGui::BeginPopupModal("Create New Node", &m_showAddNodeModal, ImGuiWindowFlags_AlwaysAutoResize))
		{
			ImGui::Text("Select a node type to create:");
			ImGui::SetNextItemWidth(350.0f);
			ImGui::InputTextWithHint("##FilterNodes", "Search node types...", m_addNodeFilter, sizeof(m_addNodeFilter));
			ImGui::Separator();

			struct NodeOption
			{
				const char* name;
				const char* category;
				const char* desc;
			};

			static const NodeOption kOptions[] = {
				{ "Node3D", "3D", "Empty 3D spatial node with transform" },
				{ "MeshInstance3D (Cube)", "3D", "Cube mesh renderer with material" },
				{ "MeshInstance3D (Sphere)", "3D", "Sphere mesh renderer with material" },
				{ "MeshInstance3D (Plane)", "3D", "Ground plane mesh renderer" },
				{ "Camera3D", "3D", "Perspective 3D camera" },
				{ "DirectionalLight3D", "Lighting", "Sun/Directional light" },
				{ "PointLight3D", "Lighting", "Omni-directional point light" },
				{ "SpotLight3D", "Lighting", "Cone spot light" },
				{ "RigidBody3D", "Physics", "Physics rigid body with gravity & restitution" },
				{ "BoxCollider3D", "Physics", "Box collision shape" },
				{ "SphereCollider3D", "Physics", "Sphere collision shape" },
				{ "ScriptableNode3D", "Scripting", "Node with game script component" }
			};

			ImGui::BeginChild("NodeListChild", ImVec2(350.0f, 220.0f), true);
			std::string filterStr = m_addNodeFilter;
			std::transform(filterStr.begin(), filterStr.end(), filterStr.begin(), ::tolower);

			bool createTriggered = false;
			for (int i = 0; i < static_cast<int>(std::size(kOptions)); ++i)
			{
				std::string optName = kOptions[i].name;
				std::string optLower = optName;
				std::transform(optLower.begin(), optLower.end(), optLower.begin(), ::tolower);

				if (!filterStr.empty() && optLower.find(filterStr) == std::string::npos)
				{
					continue;
				}

				bool isSel = (m_selectedNodeType == i);
				if (ImGui::Selectable(kOptions[i].name, isSel))
				{
					m_selectedNodeType = i;
				}
				if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(0))
				{
					m_selectedNodeType = i;
					createTriggered = true;
				}
			}
			ImGui::EndChild();

			ImGui::TextDisabled("%s", kOptions[m_selectedNodeType].desc);
			ImGui::Separator();

			if (ImGui::Button("Create", ImVec2(120, 0)) || createTriggered)
			{
				auto parentEnt = getSelectedEntity();
				Core::Entity newEnt;

				switch (m_selectedNodeType)
				{
				case 0: // Node3D
					newEnt = m_scene->createEntity("Node3D");
					break;
				case 1: // Cube
					newEnt = m_scene->createEntity("Cube");
					{
						auto& m = newEnt.addComponent<Core::MeshRendererComponent>();
						m.meshPath = "cube.obj";
						auto& c = newEnt.addComponent<Core::ColliderComponent>();
						c.shape = Core::ColliderShape::Box;
					}
					break;
				case 2: // Sphere
					newEnt = m_scene->createEntity("Sphere");
					{
						auto& m = newEnt.addComponent<Core::MeshRendererComponent>();
						m.meshPath = "sphere.obj";
						auto& c = newEnt.addComponent<Core::ColliderComponent>();
						c.shape = Core::ColliderShape::Sphere;
					}
					break;
				case 3: // Plane
					newEnt = m_scene->createEntity("Plane");
					{
						auto& m = newEnt.addComponent<Core::MeshRendererComponent>();
						m.meshPath = "plane.obj";
						newEnt.getComponent<Core::TransformComponent>().scale = Core::Vector3(10.0f, 1.0f, 10.0f);
					}
					break;
				case 4: // Camera3D
					newEnt = m_scene->createEntity("Camera3D");
					newEnt.addComponent<Core::CameraComponent>();
					break;
				case 5: // DirectionalLight3D
					newEnt = m_scene->createEntity("DirectionalLight3D");
					{
						auto& l = newEnt.addComponent<Core::LightComponent>();
						l.type = Core::LightType::Directional;
					}
					break;
				case 6: // PointLight3D
					newEnt = m_scene->createEntity("PointLight3D");
					{
						auto& l = newEnt.addComponent<Core::LightComponent>();
						l.type = Core::LightType::Point;
					}
					break;
				case 7: // SpotLight3D
					newEnt = m_scene->createEntity("SpotLight3D");
					{
						auto& l = newEnt.addComponent<Core::LightComponent>();
						l.type = Core::LightType::Spot;
					}
					break;
				case 8: // RigidBody3D
					newEnt = m_scene->createEntity("RigidBody3D");
					newEnt.addComponent<Core::RigidbodyComponent>();
					break;
				case 9: // BoxCollider3D
					newEnt = m_scene->createEntity("BoxCollider3D");
					{
						auto& c = newEnt.addComponent<Core::ColliderComponent>();
						c.shape = Core::ColliderShape::Box;
					}
					break;
				case 10: // SphereCollider3D
					newEnt = m_scene->createEntity("SphereCollider3D");
					{
						auto& c = newEnt.addComponent<Core::ColliderComponent>();
						c.shape = Core::ColliderShape::Sphere;
					}
					break;
				case 11: // ScriptableNode3D
					newEnt = m_scene->createEntity("ScriptNode");
					newEnt.addComponent<Core::ScriptsComponent>();
					break;
				default:
					newEnt = m_scene->createEntity("Node3D");
					break;
				}

				if (parentEnt.isValid())
				{
					newEnt.setParent(parentEnt);
				}

				if (m_history)
				{
					m_history->execute(std::make_unique<CreateEntityCommand>(m_scene.get(), newEnt));
				}

				if (m_selection)
				{
					m_selection->select(newEnt, false);
				}

				m_showAddNodeModal = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::SameLine();
			if (ImGui::Button("Cancel", ImVec2(120, 0)))
			{
				m_showAddNodeModal = false;
				ImGui::CloseCurrentPopup();
			}

			ImGui::EndPopup();
		}
	}
}
