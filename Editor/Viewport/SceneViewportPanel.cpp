#include "Editor/Viewport/SceneViewportPanel.h"
#include "Editor/Viewport/ViewportOverlays.h"
#include "Editor/Commands/EntityCommands.h"
#include "Editor/UI/DockLayout.h"
#include "Engine/Runtime/Application.h"
#include <imgui.h>
#include <ImGuizmo.h>

namespace Editor
{
	void SceneViewportPanel::onImGuiRender(
		std::shared_ptr<Core::Scene>& activeScene,
		std::shared_ptr<Core::RHI::IGpuTexture> viewportTexture,
		std::shared_ptr<Core::RHI::IGpuTexture> gameViewportTexture,
		EditorCamera& camera,
		SelectionContext& selectionContext,
		CommandHistory& commandHistory,
		bool* p_open)
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin(UI::PanelNames::Viewport3D, p_open);

		// Viewport Sub-header / In-viewport toolbar overlay
		ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
		ImVec2 viewportSize = ImGui::GetContentRegionAvail();

		m_viewportBounds[0] = { cursorScreenPos.x, cursorScreenPos.y };
		m_viewportBounds[1] = { cursorScreenPos.x + viewportSize.x, cursorScreenPos.y + viewportSize.y };

		m_viewportFocused = ImGui::IsWindowFocused();
		m_viewportHovered = ImGui::IsWindowHovered();

		uint32_t newW = static_cast<uint32_t>(viewportSize.x);
		uint32_t newH = static_cast<uint32_t>(viewportSize.y);

		if (newW > 0 && newH > 0 && (newW != m_viewportWidth || newH != m_viewportHeight))
		{
			m_pendingViewportWidth = newW;
			m_pendingViewportHeight = newH;
		}

		if (viewportTexture && viewportTexture->getShaderResourceView())
		{
			ImGui::Image(
				(ImTextureID)viewportTexture->getShaderResourceView(),
				viewportSize
			);
		}

		const auto& viewProj = camera.getViewProjectionMatrix();

		// 1. Draw 3D Ground Grid
		if (m_showGrid)
		{
			ViewportOverlays::DrawGrid3D(
				viewProj,
				camera.getPosition(),
				m_viewportBounds[0].x, m_viewportBounds[0].y,
				static_cast<float>(m_viewportWidth), static_cast<float>(m_viewportHeight)
			);
		}

		// 2. Draw Viewport Icons (Camera, Lights)
		ViewportOverlays::DrawViewportIcons(
			activeScene.get(),
			selectionContext,
			viewProj,
			m_viewportBounds[0].x, m_viewportBounds[0].y,
			static_cast<float>(m_viewportWidth), static_cast<float>(m_viewportHeight),
			m_viewportHovered
		);

		// 3. Draw Selection Wireframe
		ViewportOverlays::DrawSelectionWireframes(
			selectionContext,
			viewProj,
			m_viewportBounds[0].x, m_viewportBounds[0].y,
			static_cast<float>(m_viewportWidth), static_cast<float>(m_viewportHeight)
		);

		// 4. Draw ImGuizmo
		drawGizmo(activeScene, camera, selectionContext, commandHistory);

		// 5. In-Viewport Toolbar Overlay (top-left)
		ImGui::SetCursorPos(ImVec2(12.0f, 28.0f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.14f, 0.17f, 0.75f));
		ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 3.0f);

		if (ImGui::Button(m_gizmoType == 0 ? "[W] Move*" : "[W] Move")) m_gizmoType = 0;
		ImGui::SameLine();
		if (ImGui::Button(m_gizmoType == 1 ? "[E] Rotate*" : "[E] Rotate")) m_gizmoType = 1;
		ImGui::SameLine();
		if (ImGui::Button(m_gizmoType == 2 ? "[R] Scale*" : "[R] Scale")) m_gizmoType = 2;
		ImGui::SameLine();
		if (ImGui::Button(m_gizmoType == -1 ? "[Q] Select*" : "[Q] Select")) m_gizmoType = -1;

		ImGui::SameLine();
		if (ImGui::Button(m_gizmoMode == 1 ? "World" : "Local")) m_gizmoMode = 1 - m_gizmoMode;

		ImGui::SameLine();
		if (ImGui::Button(m_snapEnabled ? "Snap On" : "Snap Off")) m_snapEnabled = !m_snapEnabled;

		ImGui::SameLine();
		if (ImGui::Button(m_showWireframeMode ? "Wireframe" : "Shaded")) m_showWireframeMode = !m_showWireframeMode;

		ImGui::SameLine();
		if (ImGui::Button(m_enableFXAA ? "FXAA" : "No AA"))
		{
			m_enableFXAA = !m_enableFXAA;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Toggle Viewport FXAA (Fast Approximate Anti-Aliasing)");
		}

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.35f, 0.85f));
		if (ImGui::Button("+ Sandbox"))
		{
			if (m_onNewSandboxScene) m_onNewSandboxScene();
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Populate scene with sun, camera, ground plane, and physics shapes");
		}
		ImGui::PopStyleColor();

		ImGui::PopStyleVar();
		ImGui::PopStyleColor();

		// 6. Orientation Gizmo (top-right)
		ViewportOverlays::DrawOrientationGizmo(
			camera.getViewMatrix(),
			m_viewportBounds[0],
			Core::Vector2(viewportSize.x, viewportSize.y)
		);

		// 7. Camera PiP Preview (bottom-right) - only when a Camera entity is selected
		if (m_showPiP && selectionContext.hasSelection())
		{
			Core::Entity selectedEntity = selectionContext.getPrimary();
			if (selectedEntity.isValid() && selectedEntity.hasComponent<Core::CameraComponent>())
			{
				ViewportOverlays::DrawCameraPiP(
					gameViewportTexture,
					m_viewportBounds[0],
					Core::Vector2(viewportSize.x, viewportSize.y)
				);
			}
		}

		// Asset Drag & Drop into 3D Viewport
		if (ImGui::BeginDragDropTarget())
		{
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("DARKFIRE_ASSET_PATH"))
			{
				const char* pathStr = static_cast<const char*>(payload->Data);
				std::filesystem::path assetPath = pathStr;
				if (assetPath.extension() == ".scene")
				{
					if (m_onOpenScene) m_onOpenScene(assetPath);
				}
				else if (assetPath.extension() == ".obj")
				{
					if (activeScene)
					{
						auto ent = activeScene->createEntity(assetPath.stem().string());
						auto& mesh = ent.addComponent<Core::MeshRendererComponent>();
						mesh.meshPath = assetPath.generic_string();
						mesh.materialPath = "/engine/Content/Default.mat";
						commandHistory.execute<CreateEntityCommand>(activeScene.get(), ent);
						selectionContext.select(ent, false);
						if (m_onSceneModified) m_onSceneModified();
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void SceneViewportPanel::drawGizmo(
		std::shared_ptr<Core::Scene>& activeScene,
		EditorCamera& camera,
		SelectionContext& selectionContext,
		CommandHistory& commandHistory)
	{
		if (m_gizmoType == -1 || !selectionContext.hasSelection()) return;

		Core::Entity selectedEntity = selectionContext.getPrimary();
		if (!selectedEntity.isValid() || !selectedEntity.hasComponent<Core::TransformComponent>()) return;

		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(m_viewportBounds[0].x, m_viewportBounds[0].y, static_cast<float>(m_viewportWidth), static_cast<float>(m_viewportHeight));

		auto viewMat = camera.getViewMatrix();
		auto projMat = camera.getProjectionMatrix();

		auto& tc = selectedEntity.getComponent<Core::TransformComponent>();
		Core::Matrix worldMat = tc.getLocalMatrix();

		ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
		if (m_gizmoType == 1) op = ImGuizmo::ROTATE;
		else if (m_gizmoType == 2) op = ImGuizmo::SCALE;

		ImGuizmo::MODE mode = (m_gizmoMode == 1) ? ImGuizmo::WORLD : ImGuizmo::LOCAL;

		float snapValues[3] = { m_snapTranslation, m_snapTranslation, m_snapTranslation };
		if (m_gizmoType == 1) snapValues[0] = snapValues[1] = snapValues[2] = m_snapRotation;
		else if (m_gizmoType == 2) snapValues[0] = snapValues[1] = snapValues[2] = m_snapScale;

		ImGuizmo::Manipulate(
			reinterpret_cast<const float*>(&viewMat),
			reinterpret_cast<const float*>(&projMat),
			op,
			mode,
			reinterpret_cast<float*>(&worldMat),
			nullptr,
			m_snapEnabled ? snapValues : nullptr
		);

		if (ImGuizmo::IsUsing())
		{
			if (!m_gizmoWasUsing)
			{
				m_gizmoInitialTransform = tc;
				m_gizmoWasUsing = true;
			}

			Core::Vector3 trans, scale;
			Core::Quaternion rot;
			worldMat.Decompose(scale, rot, trans);
			tc.position = trans;
			tc.rotation = rot;
			tc.scale = scale;
			if (m_onSceneModified) m_onSceneModified();
		}
		else if (m_gizmoWasUsing)
		{
			commandHistory.execute<ModifyTransformCommand>(
				activeScene.get(),
				selectedEntity.getUUID(),
				m_gizmoInitialTransform,
				tc,
				false
			);
			m_gizmoWasUsing = false;
		}
	}
}
