#include "Editor/Panels/HierarchyPanel.h"
#include "Engine/Core/Scene/Components.h"
#include <imgui.h>
#include <cstring>
#include <algorithm>

namespace Editor
{
	HierarchyPanel::HierarchyPanel(std::shared_ptr<Core::Scene> scene)
		: m_scene(std::move(scene))
	{
	}

	void HierarchyPanel::setScene(std::shared_ptr<Core::Scene> scene)
	{
		m_scene = std::move(scene);
		m_selectedEntity = {};
	}

	void HierarchyPanel::onImGuiRender()
	{
		ImGui::Begin("Hierarchy");

		if (!m_scene)
		{
			ImGui::TextDisabled("No active scene loaded.");
			ImGui::End();
			return;
		}

		// Search Filter
		ImGui::InputTextWithHint("##Search", "Search entities...", m_searchFilter, sizeof(m_searchFilter));
		ImGui::Separator();

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
		if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
		{
			m_selectedEntity = {};
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

		ImGuiTreeNodeFlags flags = (m_selectedEntity == entity ? ImGuiTreeNodeFlags_Selected : 0);
		flags |= ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		if (!hasChildren)
		{
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}

		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)entity.getUUID().value(), flags, "%s", name.c_str());

		if (ImGui::IsItemClicked())
		{
			m_selectedEntity = entity;
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
					droppedEntity.setParent(entity);
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
			m_selectedEntity = child;
		}

		if (ImGui::MenuItem("Delete Entity"))
		{
			if (m_selectedEntity == entity)
			{
				m_selectedEntity = {};
			}
			entity.destroy();
		}
	}

	void HierarchyPanel::drawPanelContextMenu()
	{
		if (ImGui::MenuItem("Create Empty Entity"))
		{
			m_selectedEntity = m_scene->createEntity("Empty Entity");
		}

		ImGui::Separator();

		if (ImGui::MenuItem("Camera"))
		{
			auto ent = m_scene->createEntity("Main Camera");
			ent.getComponent<Core::TransformComponent>().position = Core::Vector3(0.0f, 2.0f, -5.0f);
			auto& cam = ent.addComponent<Core::CameraComponent>();
			cam.isPrimary = true;
			m_selectedEntity = ent;
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
				m_selectedEntity = ent;
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
				m_selectedEntity = ent;
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
				mesh.meshPath = "/engine/Content/cube.obj";
				mesh.materialPath = "/engine/Content/Default.mat";
				auto& col = ent.addComponent<Core::ColliderComponent>();
				col.shape = Core::ColliderShape::Box;
				col.size = Core::Vector3(1.0f, 1.0f, 1.0f);
				m_selectedEntity = ent;
			}

			if (ImGui::MenuItem("Sphere"))
			{
				auto ent = m_scene->createEntity("Sphere");
				ent.getComponent<Core::TransformComponent>().position = Core::Vector3(0.0f, 0.5f, 0.0f);
				auto& mesh = ent.addComponent<Core::MeshRendererComponent>();
				mesh.meshPath = "/engine/Content/sphere.obj";
				mesh.materialPath = "/engine/Content/Default.mat";
				auto& col = ent.addComponent<Core::ColliderComponent>();
				col.shape = Core::ColliderShape::Sphere;
				col.radius = 0.5f;
				m_selectedEntity = ent;
			}

			if (ImGui::MenuItem("Plane"))
			{
				auto ent = m_scene->createEntity("Plane");
				ent.getComponent<Core::TransformComponent>().position = Core::Vector3(0.0f, 0.0f, 0.0f);
				ent.getComponent<Core::TransformComponent>().scale = Core::Vector3(10.0f, 1.0f, 10.0f);
				auto& mesh = ent.addComponent<Core::MeshRendererComponent>();
				mesh.meshPath = "/engine/Content/cube.obj";
				mesh.materialPath = "/engine/Content/Default.mat";
				auto& col = ent.addComponent<Core::ColliderComponent>();
				col.shape = Core::ColliderShape::Box;
				col.size = Core::Vector3(10.0f, 0.1f, 10.0f);
				m_selectedEntity = ent;
			}
			ImGui::EndMenu();
		}
	}
}
