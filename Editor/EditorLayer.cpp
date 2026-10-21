#include "Editor/EditorLayer.h"
#include "Editor/FileDialogs.h"
#include "Engine/Runtime/Application.h"
#include "Engine/Core/Scene/SceneRendererBridge.h"
#include "Engine/Core/Serialization/SceneSerializer.h"
#include "Engine/Platform/Input.h"
#include "Engine/Platform/KeyCodes.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/RHI/RHIDefines.h"
#include "Editor/Clipboard.h"
#include "Editor/Commands/EntityCommands.h"

#include <imgui.h>
#include <ImGuizmo.h>

namespace Editor
{
	EditorLayer::EditorLayer()
		: Layer("EditorLayer")
	{
	}

	EditorLayer::~EditorLayer()
	{
		onDetach();
	}

	void EditorLayer::onAttach()
	{
		setupImGuiStyle();

		m_hierarchyPanel.setContext(m_activeScene, &m_selectionContext, &m_commandHistory);
		m_inspectorPanel.setHistory(&m_commandHistory);

		newScene();

		// Try loading default Playground scene if it exists
		std::filesystem::path playgroundScene = "Templates/Playground3D/Scenes/Playground.scene";
		if (std::filesystem::exists(playgroundScene))
		{
			openScene(playgroundScene);
		}

		resizeViewport(m_viewportWidth, m_viewportHeight);
		resizeGameViewport(m_gameViewportWidth, m_gameViewportHeight);
		updateWindowTitle();
		Core::Log::Get().info("Editor", "EditorLayer attached and ready.");
	}

	void EditorLayer::onDetach()
	{
		m_viewportTexture.reset();
		m_gameViewportTexture.reset();
	}

	void EditorLayer::setupImGuiStyle()
	{
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec4* colors = style.Colors;

		style.WindowRounding = 6.0f;
		style.ChildRounding = 4.0f;
		style.FrameRounding = 4.0f;
		style.PopupRounding = 4.0f;
		style.ScrollbarRounding = 4.0f;
		style.GrabRounding = 4.0f;
		style.TabRounding = 4.0f;

		style.WindowPadding = ImVec2(8.0f, 8.0f);
		style.FramePadding = ImVec2(5.0f, 3.0f);
		style.ItemSpacing = ImVec2(6.0f, 4.0f);

		// Deep dark sleek theme with subtle accents
		colors[ImGuiCol_Text] = ImVec4(0.92f, 0.92f, 0.95f, 1.0f);
		colors[ImGuiCol_TextDisabled] = ImVec4(0.50f, 0.50f, 0.55f, 1.0f);
		colors[ImGuiCol_WindowBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
		colors[ImGuiCol_ChildBg] = ImVec4(0.10f, 0.10f, 0.11f, 1.0f);
		colors[ImGuiCol_PopupBg] = ImVec4(0.14f, 0.14f, 0.16f, 1.0f);
		colors[ImGuiCol_Border] = ImVec4(0.20f, 0.20f, 0.24f, 1.0f);
		colors[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.0f);
		colors[ImGuiCol_FrameBg] = ImVec4(0.18f, 0.18f, 0.20f, 1.0f);
		colors[ImGuiCol_FrameBgHovered] = ImVec4(0.25f, 0.25f, 0.28f, 1.0f);
		colors[ImGuiCol_FrameBgActive] = ImVec4(0.30f, 0.30f, 0.35f, 1.0f);
		colors[ImGuiCol_TitleBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
		colors[ImGuiCol_TitleBgActive] = ImVec4(0.14f, 0.14f, 0.17f, 1.0f);
		colors[ImGuiCol_TitleBgCollapsed] = ImVec4(0.08f, 0.08f, 0.09f, 1.0f);
		colors[ImGuiCol_MenuBarBg] = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
		colors[ImGuiCol_ScrollbarBg] = ImVec4(0.10f, 0.10f, 0.12f, 1.0f);
		colors[ImGuiCol_ScrollbarGrab] = ImVec4(0.22f, 0.22f, 0.26f, 1.0f);
		colors[ImGuiCol_ScrollbarGrabHovered] = ImVec4(0.30f, 0.30f, 0.35f, 1.0f);
		colors[ImGuiCol_ScrollbarGrabActive] = ImVec4(0.38f, 0.38f, 0.44f, 1.0f);
		colors[ImGuiCol_CheckMark] = ImVec4(0.20f, 0.70f, 0.95f, 1.0f);
		colors[ImGuiCol_SliderGrab] = ImVec4(0.20f, 0.70f, 0.95f, 1.0f);
		colors[ImGuiCol_SliderGrabActive] = ImVec4(0.30f, 0.80f, 1.00f, 1.0f);
		colors[ImGuiCol_Button] = ImVec4(0.20f, 0.20f, 0.24f, 1.0f);
		colors[ImGuiCol_ButtonHovered] = ImVec4(0.28f, 0.28f, 0.33f, 1.0f);
		colors[ImGuiCol_ButtonActive] = ImVec4(0.35f, 0.35f, 0.42f, 1.0f);
		colors[ImGuiCol_Header] = ImVec4(0.22f, 0.22f, 0.26f, 1.0f);
		colors[ImGuiCol_HeaderHovered] = ImVec4(0.30f, 0.30f, 0.36f, 1.0f);
		colors[ImGuiCol_HeaderActive] = ImVec4(0.36f, 0.36f, 0.44f, 1.0f);
		colors[ImGuiCol_Separator] = ImVec4(0.20f, 0.20f, 0.24f, 1.0f);
		colors[ImGuiCol_SeparatorHovered] = ImVec4(0.28f, 0.28f, 0.35f, 1.0f);
		colors[ImGuiCol_SeparatorActive] = ImVec4(0.35f, 0.35f, 0.45f, 1.0f);
		colors[ImGuiCol_Tab] = ImVec4(0.14f, 0.14f, 0.16f, 1.0f);
		colors[ImGuiCol_TabHovered] = ImVec4(0.25f, 0.25f, 0.30f, 1.0f);
		colors[ImGuiCol_TabSelected] = ImVec4(0.20f, 0.20f, 0.24f, 1.0f);
		colors[ImGuiCol_TabDimmed] = ImVec4(0.12f, 0.12f, 0.14f, 1.0f);
		colors[ImGuiCol_TabDimmedSelected] = ImVec4(0.16f, 0.16f, 0.19f, 1.0f);
		colors[ImGuiCol_DockingPreview] = ImVec4(0.20f, 0.70f, 0.95f, 0.4f);
		colors[ImGuiCol_DockingEmptyBg] = ImVec4(0.08f, 0.08f, 0.09f, 1.0f);
	}

	void EditorLayer::resizeViewport(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0) return;

		m_viewportWidth = width;
		m_viewportHeight = height;

		m_editorCamera.setViewportSize(width, height);

		Core::RHI::GpuTextureDesc desc{};
		desc.width = width;
		desc.height = height;
		desc.format = Core::RHI::PixelFormat::R8G8B8A8_UNORM;
		desc.bindFlags = Core::RHI::BufferBindFlags::RenderTarget | Core::RHI::BufferBindFlags::ShaderResource;

		auto texRes = Runtime::Application::Get().getDevice().createTexture(desc, nullptr);
		if (texRes.isOk())
		{
			m_viewportTexture = std::move(texRes.value());
		}
	}

	void EditorLayer::resizeGameViewport(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0) return;

		m_gameViewportWidth = width;
		m_gameViewportHeight = height;

		Core::RHI::GpuTextureDesc desc{};
		desc.width = width;
		desc.height = height;
		desc.format = Core::RHI::PixelFormat::R8G8B8A8_UNORM;
		desc.bindFlags = Core::RHI::BufferBindFlags::RenderTarget | Core::RHI::BufferBindFlags::ShaderResource;

		auto texRes = Runtime::Application::Get().getDevice().createTexture(desc, nullptr);
		if (texRes.isOk())
		{
			m_gameViewportTexture = std::move(texRes.value());
		}
	}

	void EditorLayer::onResize(uint32_t width, uint32_t height)
	{
		(void)width;
		(void)height;
	}

	void EditorLayer::newScene()
	{
		m_activeScene = std::make_shared<Core::Scene>();
		m_currentScenePath.clear();
		m_commandHistory.clear();
		m_selectionContext.clear();
		m_hierarchyPanel.setContext(m_activeScene, &m_selectionContext, &m_commandHistory);
		m_isSceneDirty = false;
		updateWindowTitle();

		// Default starter setup: Camera & Directional Light
		auto cam = m_activeScene->createEntity("Main Camera");
		cam.getComponent<Core::TransformComponent>().position = Core::Vector3(0.0f, 2.0f, -6.0f);
		auto& camComp = cam.addComponent<Core::CameraComponent>();
		camComp.isPrimary = true;

		auto light = m_activeScene->createEntity("Directional Light");
		light.getComponent<Core::TransformComponent>().position = Core::Vector3(0.0f, 10.0f, 0.0f);
		light.getComponent<Core::TransformComponent>().setEulerDegrees(Core::Vector3(45.0f, 0.0f, 0.0f));
		auto& lightComp = light.addComponent<Core::LightComponent>();
		lightComp.type = Core::LightType::Directional;
		lightComp.color = Core::Color(1.0f, 0.95f, 0.8f, 1.0f);
		lightComp.intensity = 1.0f;
	}

	void EditorLayer::openScene()
	{
		std::string filepath = FileDialogs::OpenFile("DarkFire Scene (*.scene)\0*.scene\0All Files (*.*)\0*.*\0");
		if (!filepath.empty())
		{
			openScene(filepath);
		}
	}

	void EditorLayer::openScene(const std::filesystem::path& path)
	{
		auto newSc = std::make_shared<Core::Scene>();
		Core::SceneSerializer serializer(newSc.get());
		if (serializer.deserializeFromFile(path))
		{
			m_activeScene = std::move(newSc);
			m_currentScenePath = path;
			m_commandHistory.clear();
			m_selectionContext.clear();
			m_hierarchyPanel.setContext(m_activeScene, &m_selectionContext, &m_commandHistory);
			m_isSceneDirty = false;
			updateWindowTitle();
			Core::Log::Get().info("Editor", "Opened scene: {}", path.string());
		}
		else
		{
			Core::Log::Get().error("Editor", "Failed to open scene: {}", path.string());
		}
	}

	void EditorLayer::saveScene()
	{
		if (m_currentScenePath.empty())
		{
			saveSceneAs();
		}
		else
		{
			Core::SceneSerializer serializer(m_activeScene.get());
			if (serializer.serializeToFile(m_currentScenePath))
			{
				m_isSceneDirty = false;
				updateWindowTitle();
				Core::Log::Get().info("Editor", "Saved scene to: {}", m_currentScenePath.string());
			}
			else
			{
				Core::Log::Get().error("Editor", "Failed to save scene to: {}", m_currentScenePath.string());
			}
		}
	}

	void EditorLayer::saveSceneAs()
	{
		std::string filepath = FileDialogs::SaveFile("DarkFire Scene (*.scene)\0*.scene\0All Files (*.*)\0*.*\0");
		if (!filepath.empty())
		{
			if (!filepath.ends_with(".scene"))
			{
				filepath += ".scene";
			}
			m_currentScenePath = filepath;
			saveScene();
		}
	}

	void EditorLayer::onUpdate(float deltaTime)
	{
		using namespace Platform;

		// Input shortcuts
		bool ctrlHeld = Input::isKeyPressed(KeyCode::LeftControl) || Input::isKeyPressed(KeyCode::RightControl);
		bool shiftHeld = Input::isKeyPressed(KeyCode::LeftShift) || Input::isKeyPressed(KeyCode::RightShift);
		bool textInputActive = ImGui::GetIO().WantTextInput;

		if (ctrlHeld && !textInputActive)
		{
			if (Input::isKeyJustPressed(KeyCode::N)) newScene();
			if (Input::isKeyJustPressed(KeyCode::O)) openScene();
			if (Input::isKeyJustPressed(KeyCode::S))
			{
				if (shiftHeld) saveSceneAs();
				else saveScene();
			}
			if (Input::isKeyJustPressed(KeyCode::Z))
			{
				if (shiftHeld)
				{
					if (m_commandHistory.redo()) { m_isSceneDirty = true; updateWindowTitle(); }
				}
				else
				{
					if (m_commandHistory.undo()) { m_isSceneDirty = true; updateWindowTitle(); }
				}
			}
			if (Input::isKeyJustPressed(KeyCode::Y))
			{
				if (m_commandHistory.redo()) { m_isSceneDirty = true; updateWindowTitle(); }
			}
			if (Input::isKeyJustPressed(KeyCode::C))
			{
				Clipboard::Copy(m_selectionContext.getEntities(), m_activeScene.get());
			}
			if (Input::isKeyJustPressed(KeyCode::X))
			{
				Clipboard::Cut(m_selectionContext.getEntities(), m_activeScene.get(), m_commandHistory);
				m_selectionContext.clear();
				m_isSceneDirty = true;
				updateWindowTitle();
			}
			if (Input::isKeyJustPressed(KeyCode::V))
			{
				auto pasted = Clipboard::Paste(m_activeScene.get(), m_commandHistory);
				if (!pasted.empty())
				{
					m_selectionContext.clear();
					for (auto e : pasted) m_selectionContext.select(e, true);
					m_isSceneDirty = true;
					updateWindowTitle();
				}
			}
			if (Input::isKeyJustPressed(KeyCode::D))
			{
				auto dup = Clipboard::Duplicate(m_selectionContext.getEntities(), m_activeScene.get(), m_commandHistory);
				if (!dup.empty())
				{
					m_selectionContext.clear();
					for (auto e : dup) m_selectionContext.select(e, true);
					m_isSceneDirty = true;
					updateWindowTitle();
				}
			}
			if (Input::isKeyJustPressed(KeyCode::A))
			{
				m_selectionContext.clear();
				if (m_activeScene)
				{
					auto view = m_activeScene->view<Core::IdentityComponent>();
					for (auto handle : view)
					{
						m_selectionContext.select(Core::Entity(handle, m_activeScene.get()), true);
					}
				}
			}
		}

		// Gizmo mode shortcuts when viewport focused & not in fly mode & not typing
		if (m_viewportFocused && !m_editorCamera.isFlyModeActive() && !ctrlHeld && !textInputActive)
		{
			if (Input::isKeyJustPressed(KeyCode::Q)) m_gizmoType = -1;
			if (Input::isKeyJustPressed(KeyCode::W)) m_gizmoType = 0; // Translate
			if (Input::isKeyJustPressed(KeyCode::E)) m_gizmoType = 1; // Rotate
			if (Input::isKeyJustPressed(KeyCode::R)) m_gizmoType = 2; // Scale

			// Focus shortcut 'F'
			if (Input::isKeyJustPressed(KeyCode::F))
			{
				auto sel = m_selectionContext.getPrimary();
				if (sel.isValid())
				{
					m_editorCamera.focusOn(sel.getTransform().position, 3.0f);
				}
			}

			// Delete shortcut
			if (Input::isKeyJustPressed(KeyCode::Delete))
			{
				auto selected = m_selectionContext.getEntities();
				if (!selected.empty())
				{
					auto trans = std::make_unique<TransactionCommand>("Delete Entities");
					for (auto e : selected)
					{
						if (e.isValid())
						{
							trans->addCommand(std::make_unique<DeleteEntityCommand>(m_activeScene.get(), e));
						}
					}
					m_selectionContext.clear();
					m_commandHistory.execute(std::move(trans));
					m_isSceneDirty = true;
					updateWindowTitle();
				}
			}
		}

		// Autosave tick
		m_autosaveTimer += deltaTime;
		if (m_autosaveTimer >= 300.0f)
		{
			m_autosaveTimer = 0.0f;
			performAutosave();
		}

		m_editorCamera.onUpdate(deltaTime, m_viewportHovered, m_viewportFocused);

		// Render Scene
		if (m_activeScene)
		{
			m_activeScene->updateTransforms();
			Core::ExtractRenderScene(*m_activeScene, m_renderScene);

			// Override camera with Editor Camera
			m_renderScene.camera.position = m_editorCamera.getPosition();
			m_renderScene.camera.forward = m_editorCamera.getForward();
			m_renderScene.camera.nearZ = m_editorCamera.getNearZ();
			m_renderScene.camera.farZ = m_editorCamera.getFarZ();
			m_renderScene.camera.fov = Core::Math::toRadians(m_editorCamera.getFov());
			m_renderScene.camera.aspect = m_editorCamera.getAspectRatio();
			m_renderScene.camera.viewMatrix = m_editorCamera.getViewMatrix();
			m_renderScene.camera.projMatrix = m_editorCamera.getProjectionMatrix();
			m_renderScene.camera.viewProjMatrix = m_editorCamera.getViewProjectionMatrix();
			m_renderScene.camera.invViewMatrix = m_editorCamera.getViewMatrix().Invert();
			m_renderScene.camera.invProjMatrix = m_editorCamera.getProjectionMatrix().Invert();
			m_renderScene.camera.invViewProjMatrix = m_editorCamera.getInverseViewProjectionMatrix();

			Core::Renderer::RenderView view{};
			view.width = m_viewportWidth;
			view.height = m_viewportHeight;
			view.targetTexture = m_viewportTexture;
			view.isEditorView = true;

			Runtime::Application::Get().getRenderer().render(m_renderScene, view);

			// Render Game Viewport (primary camera)
			entt::entity primaryCamEntity = entt::null;
			auto camView = m_activeScene->view<Core::CameraComponent, Core::TransformComponent>();
			for (auto entity : camView)
			{
				const auto& cam = camView.get<Core::CameraComponent>(entity);
				if (cam.isPrimary)
				{
					primaryCamEntity = entity;
					break;
				}
			}

			if (primaryCamEntity != entt::null && m_gameViewportTexture)
			{
				const auto& camComp = camView.get<Core::CameraComponent>(primaryCamEntity);
				const auto& tc = camView.get<Core::TransformComponent>(primaryCamEntity);

				Core::Renderer::RenderScene gameRenderScene;
				Core::ExtractRenderScene(*m_activeScene, gameRenderScene);

				float aspect = static_cast<float>(m_gameViewportWidth) / static_cast<float>(m_gameViewportHeight);
				float fovRad = Core::Math::toRadians(camComp.fov);
				Core::Matrix proj = Core::Math::perspectiveFovLH_ReversedZ(fovRad, aspect, camComp.nearZ, camComp.farZ);
				Core::Vector3 forward = tc.getForward();
				Core::Vector3 up = tc.getUp();
				Core::Matrix viewMat = Core::Math::lookAtLH(tc.position, tc.position + forward, up);

				gameRenderScene.camera.position = tc.position;
				gameRenderScene.camera.forward = forward;
				gameRenderScene.camera.nearZ = camComp.nearZ;
				gameRenderScene.camera.farZ = camComp.farZ;
				gameRenderScene.camera.fov = fovRad;
				gameRenderScene.camera.aspect = aspect;
				gameRenderScene.camera.viewMatrix = viewMat;
				gameRenderScene.camera.projMatrix = proj;
				gameRenderScene.camera.viewProjMatrix = viewMat * proj;
				gameRenderScene.camera.invViewMatrix = viewMat.Invert();
				gameRenderScene.camera.invProjMatrix = proj.Invert();
				gameRenderScene.camera.invViewProjMatrix = (viewMat * proj).Invert();

				Core::Renderer::RenderView gameView{};
				gameView.width = m_gameViewportWidth;
				gameView.height = m_gameViewportHeight;
				gameView.targetTexture = m_gameViewportTexture;
				gameView.isEditorView = false;

				Runtime::Application::Get().getRenderer().render(gameRenderScene, gameView);
			}
		}
	}

	void EditorLayer::onImGuiRender()
	{
		// Main DockSpace
		ImGuiViewport* viewport = ImGui::GetMainViewport();
		ImGui::DockSpaceOverViewport(0, viewport, ImGuiDockNodeFlags_PassthruCentralNode);

		drawMenuBar();
		drawSceneViewport();

		if (m_showGameViewport)
		{
			drawGameViewport();
		}

		if (m_showHierarchy)
		{
			m_hierarchyPanel.onImGuiRender();
		}

		if (m_showInspector)
		{
			m_inspectorPanel.onImGuiRender(m_selectionContext.getPrimary());
		}

		if (m_showConsole)
		{
			m_consolePanel.onImGuiRender();
		}

		if (m_showStats)
		{
			ImGui::Begin("Renderer Stats", &m_showStats);
			const auto& renderer = Runtime::Application::Get().getRenderer();
			ImGui::Text("FPS: %.1f (%.2f ms)", ImGui::GetIO().Framerate, 1000.0f / ImGui::GetIO().Framerate);
			ImGui::Text("Draw Calls: %u", renderer.getDrawCallCount());
			ImGui::Text("Batches: %u", renderer.getBatchCount());
			ImGui::Text("Instances: %u", renderer.getInstanceCount());
			if (m_activeScene)
			{
				ImGui::Text("Entities: %zu", m_activeScene->getEntityCount());
				ImGui::Text("Selected: %zu", m_selectionContext.getCount());
			}
			ImGui::Text("Undo Memory: %.2f / %.2f MB",
				static_cast<float>(m_commandHistory.getMemoryUsage()) / (1024.0f * 1024.0f),
				static_cast<float>(m_commandHistory.getMaxMemory()) / (1024.0f * 1024.0f)
			);
			ImGui::End();
		}
	}

	void EditorLayer::drawMenuBar()
	{
		if (ImGui::BeginMainMenuBar())
		{
			if (ImGui::BeginMenu("File"))
			{
				if (ImGui::MenuItem("New Scene", "Ctrl+N")) newScene();
				if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) openScene();
				ImGui::Separator();
				if (ImGui::MenuItem("Save Scene", "Ctrl+S")) saveScene();
				if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) saveSceneAs();
				ImGui::Separator();
				if (ImGui::MenuItem("Exit", "Alt+F4")) Runtime::Application::Get().close();
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Edit"))
			{
				bool canUndo = m_commandHistory.canUndo();
				std::string undoLabel = canUndo ? ("Undo " + std::string(m_commandHistory.getUndoName())) : "Undo";
				if (ImGui::MenuItem(undoLabel.c_str(), "Ctrl+Z", false, canUndo))
				{
					if (m_commandHistory.undo()) { m_isSceneDirty = true; updateWindowTitle(); }
				}

				bool canRedo = m_commandHistory.canRedo();
				std::string redoLabel = canRedo ? ("Redo " + std::string(m_commandHistory.getRedoName())) : "Redo";
				if (ImGui::MenuItem(redoLabel.c_str(), "Ctrl+Y", false, canRedo))
				{
					if (m_commandHistory.redo()) { m_isSceneDirty = true; updateWindowTitle(); }
				}

				ImGui::Separator();

				bool hasSelection = !m_selectionContext.isEmpty();
				if (ImGui::MenuItem("Cut", "Ctrl+X", false, hasSelection))
				{
					Clipboard::Cut(m_selectionContext.getEntities(), m_activeScene.get(), m_commandHistory);
					m_selectionContext.clear();
					m_isSceneDirty = true;
					updateWindowTitle();
				}

				if (ImGui::MenuItem("Copy", "Ctrl+C", false, hasSelection))
				{
					Clipboard::Copy(m_selectionContext.getEntities(), m_activeScene.get());
				}

				bool hasClipboard = Clipboard::HasClipboardData();
				if (ImGui::MenuItem("Paste", "Ctrl+V", false, hasClipboard))
				{
					auto pasted = Clipboard::Paste(m_activeScene.get(), m_commandHistory);
					if (!pasted.empty())
					{
						m_selectionContext.clear();
						for (auto e : pasted) m_selectionContext.select(e, true);
						m_isSceneDirty = true;
						updateWindowTitle();
					}
				}

				if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, hasSelection))
				{
					auto dup = Clipboard::Duplicate(m_selectionContext.getEntities(), m_activeScene.get(), m_commandHistory);
					if (!dup.empty())
					{
						m_selectionContext.clear();
						for (auto e : dup) m_selectionContext.select(e, true);
						m_isSceneDirty = true;
						updateWindowTitle();
					}
				}

				if (ImGui::MenuItem("Delete", "Delete", false, hasSelection))
				{
					auto selected = m_selectionContext.getEntities();
					if (!selected.empty())
					{
						auto trans = std::make_unique<TransactionCommand>("Delete Entities");
						for (auto e : selected)
						{
							if (e.isValid())
							{
								trans->addCommand(std::make_unique<DeleteEntityCommand>(m_activeScene.get(), e));
							}
						}
						m_selectionContext.clear();
						m_commandHistory.execute(std::move(trans));
						m_isSceneDirty = true;
						updateWindowTitle();
					}
				}

				if (ImGui::MenuItem("Select All", "Ctrl+A"))
				{
					m_selectionContext.clear();
					if (m_activeScene)
					{
						auto view = m_activeScene->view<Core::IdentityComponent>();
						for (auto handle : view)
						{
							m_selectionContext.select(Core::Entity(handle, m_activeScene.get()), true);
						}
					}
				}

				ImGui::Separator();
				if (ImGui::MenuItem("Translate", "W", m_gizmoType == 0)) m_gizmoType = 0;
				if (ImGui::MenuItem("Rotate", "E", m_gizmoType == 1)) m_gizmoType = 1;
				if (ImGui::MenuItem("Scale", "R", m_gizmoType == 2)) m_gizmoType = 2;
				if (ImGui::MenuItem("None", "Q", m_gizmoType == -1)) m_gizmoType = -1;
				ImGui::Separator();
				if (ImGui::MenuItem("World Coordinates", nullptr, m_gizmoMode == 1)) m_gizmoMode = 1;
				if (ImGui::MenuItem("Local Coordinates", nullptr, m_gizmoMode == 0)) m_gizmoMode = 0;
				ImGui::Separator();
				ImGui::MenuItem("Snapping", nullptr, &m_snapEnabled);
				ImGui::EndMenu();
			}

			if (ImGui::BeginMenu("Window"))
			{
				ImGui::MenuItem("Scene Viewport", nullptr, nullptr, false);
				ImGui::MenuItem("Game Viewport", nullptr, &m_showGameViewport);
				ImGui::Separator();
				ImGui::MenuItem("Hierarchy", nullptr, &m_showHierarchy);
				ImGui::MenuItem("Inspector", nullptr, &m_showInspector);
				ImGui::MenuItem("Console", nullptr, &m_showConsole);
				ImGui::MenuItem("Stats", nullptr, &m_showStats);
				ImGui::EndMenu();
			}

			ImGui::EndMainMenuBar();
		}
	}

	void EditorLayer::drawSceneViewport()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Scene Viewport");

		auto viewportMinRegion = ImGui::GetWindowContentRegionMin();
		auto viewportMaxRegion = ImGui::GetWindowContentRegionMax();
		auto viewportOffset = ImGui::GetWindowPos();
		m_viewportBounds[0] = { viewportMinRegion.x + viewportOffset.x, viewportMinRegion.y + viewportOffset.y };
		m_viewportBounds[1] = { viewportMaxRegion.x + viewportOffset.x, viewportMaxRegion.y + viewportOffset.y };

		m_viewportFocused = ImGui::IsWindowFocused();
		m_viewportHovered = ImGui::IsWindowHovered();

		ImVec2 viewportPanelSize = ImGui::GetContentRegionAvail();
		uint32_t newW = static_cast<uint32_t>(viewportPanelSize.x);
		uint32_t newH = static_cast<uint32_t>(viewportPanelSize.y);

		if (newW > 0 && newH > 0 && (newW != m_viewportWidth || newH != m_viewportHeight))
		{
			resizeViewport(newW, newH);
		}

		if (m_viewportTexture && m_viewportTexture->getShaderResourceView())
		{
			ImGui::Image(
				(ImTextureID)m_viewportTexture->getShaderResourceView(),
				ImVec2{ static_cast<float>(m_viewportWidth), static_cast<float>(m_viewportHeight) }
			);
		}

		drawGizmo();

		// Handle entity picking on click
		if (ImGui::IsMouseClicked(0) && m_viewportHovered && !m_editorCamera.isFlyModeActive())
		{
			if (!ImGuizmo::IsOver() && !ImGuizmo::IsUsing())
			{
				onViewportClick();
			}
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void EditorLayer::drawGameViewport()
	{
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2{ 0, 0 });
		ImGui::Begin("Game Viewport", &m_showGameViewport);

		// Aspect Preset selector
		const char* aspectPresets[] = { "Free Aspect", "16:9", "16:10", "4:3" };
		ImGui::SetNextItemWidth(120.0f);
		ImGui::Combo("##AspectPreset", &m_aspectPresetIndex, aspectPresets, IM_ARRAYSIZE(aspectPresets));
		ImGui::SameLine();
		ImGui::TextDisabled("(Displays Primary Camera)");

		ImVec2 avail = ImGui::GetContentRegionAvail();
		if (avail.x > 0 && avail.y > 0)
		{
			float targetAspect = avail.x / avail.y;
			if (m_aspectPresetIndex == 1) targetAspect = 16.0f / 9.0f;
			else if (m_aspectPresetIndex == 2) targetAspect = 16.0f / 10.0f;
			else if (m_aspectPresetIndex == 3) targetAspect = 4.0f / 3.0f;

			float dispW = avail.x;
			float dispH = dispW / targetAspect;
			if (dispH > avail.y)
			{
				dispH = avail.y;
				dispW = dispH * targetAspect;
			}

			uint32_t newW = static_cast<uint32_t>(dispW);
			uint32_t newH = static_cast<uint32_t>(dispH);
			if (newW > 0 && newH > 0 && (newW != m_gameViewportWidth || newH != m_gameViewportHeight))
			{
				resizeGameViewport(newW, newH);
			}

			if (m_gameViewportTexture && m_gameViewportTexture->getShaderResourceView())
			{
				float offsetX = (avail.x - dispW) * 0.5f;
				float offsetY = (avail.y - dispH) * 0.5f;
				if (offsetX > 0.0f || offsetY > 0.0f)
				{
					ImGui::SetCursorPos(ImVec2(ImGui::GetCursorPosX() + offsetX, ImGui::GetCursorPosY() + offsetY));
				}

				ImGui::Image(
					(ImTextureID)m_gameViewportTexture->getShaderResourceView(),
					ImVec2{ dispW, dispH }
				);
			}
			else
			{
				ImGui::TextDisabled("No primary camera active in scene.");
			}
		}

		ImGui::End();
		ImGui::PopStyleVar();
	}

	void EditorLayer::drawGizmo()
	{
		auto selectedEntity = m_selectionContext.getPrimary();
		if (!selectedEntity.isValid() || m_gizmoType == -1 || m_editorCamera.isFlyModeActive())
		{
			m_gizmoWasUsing = false;
			return;
		}

		if (Platform::Input::isMouseButtonPressed(Platform::MouseCode::Right))
		{
			m_gizmoWasUsing = false;
			return;
		}

		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();

		ImGuizmo::SetRect(
			m_viewportBounds[0].x,
			m_viewportBounds[0].y,
			m_viewportBounds[1].x - m_viewportBounds[0].x,
			m_viewportBounds[1].y - m_viewportBounds[0].y
		);

		const Core::Matrix& cameraView = m_editorCamera.getViewMatrix();
		const Core::Matrix& cameraProj = m_editorCamera.getProjectionMatrix();

		auto& tc = selectedEntity.getTransform();
		auto parent = selectedEntity.getParent();
		Core::Matrix parentWorldInv{};
		Core::Matrix transform = tc.getLocalMatrix();
		if (parent.isValid())
		{
			Core::Matrix parentWorld = parent.getWorldMatrix();
			parentWorldInv = parentWorld.Invert();
			transform = transform * parentWorld;
		}

		ImGuizmo::OPERATION op = ImGuizmo::TRANSLATE;
		if (m_gizmoType == 1) op = ImGuizmo::ROTATE;
		else if (m_gizmoType == 2) op = ImGuizmo::SCALE;

		ImGuizmo::MODE mode = (m_gizmoMode == 0) ? ImGuizmo::LOCAL : ImGuizmo::WORLD;

		float snapValue = m_snapTranslation;
		if (op == ImGuizmo::ROTATE) snapValue = m_snapRotation;
		else if (op == ImGuizmo::SCALE) snapValue = m_snapScale;
		float snapValues[3] = { snapValue, snapValue, snapValue };

		ImGuizmo::Manipulate(
			(const float*)&cameraView,
			(const float*)&cameraProj,
			op,
			mode,
			(float*)&transform,
			nullptr,
			m_snapEnabled ? snapValues : nullptr
		);

		bool isUsing = ImGuizmo::IsUsing();
		if (!m_gizmoWasUsing && isUsing)
		{
			m_gizmoInitialTransform = tc;
		}

		if (isUsing)
		{
			if (parent.isValid())
			{
				transform = transform * parentWorldInv;
			}
			Core::Vector3 translation, scale;
			Core::Quaternion rotation;
			Core::Math::decomposeTRS(transform, translation, rotation, scale);
			tc.position = translation;
			tc.rotation = rotation;
			tc.scale = scale;
			Core::Vector3 rads = rotation.ToEuler();
			tc.eulerDegrees = Core::Vector3(Core::Math::toDegrees(rads.x), Core::Math::toDegrees(rads.y), Core::Math::toDegrees(rads.z));
			m_isSceneDirty = true;
		}

		if (m_gizmoWasUsing && !isUsing)
		{
			m_commandHistory.execute<ModifyTransformCommand>(m_activeScene.get(), selectedEntity.getUUID(), m_gizmoInitialTransform, tc, false);
			m_isSceneDirty = true;
			updateWindowTitle();
		}

		m_gizmoWasUsing = isUsing;
	}

	static bool RayIntersectsAABB(const Core::Vector3& origin, const Core::Vector3& dir, const Core::Vector3& boxMin, const Core::Vector3& boxMax, float& outDist)
	{
		float tmin = (boxMin.x - origin.x) / (std::abs(dir.x) > 1e-6f ? dir.x : 1e-6f);
		float tmax = (boxMax.x - origin.x) / (std::abs(dir.x) > 1e-6f ? dir.x : 1e-6f);
		if (tmin > tmax) std::swap(tmin, tmax);

		float tymin = (boxMin.y - origin.y) / (std::abs(dir.y) > 1e-6f ? dir.y : 1e-6f);
		float tymax = (boxMax.y - origin.y) / (std::abs(dir.y) > 1e-6f ? dir.y : 1e-6f);
		if (tymin > tymax) std::swap(tymin, tymax);

		if ((tmin > tymax) || (tymin > tmax)) return false;
		if (tymin > tmin) tmin = tymin;
		if (tymax < tmax) tmax = tymax;

		float tzmin = (boxMin.z - origin.z) / (std::abs(dir.z) > 1e-6f ? dir.z : 1e-6f);
		float tzmax = (boxMax.z - origin.z) / (std::abs(dir.z) > 1e-6f ? dir.z : 1e-6f);
		if (tzmin > tzmax) std::swap(tzmin, tzmax);

		if ((tmin > tzmax) || (tzmin > tmax)) return false;
		if (tzmin > tmin) tmin = tzmin;
		if (tzmax < tmax) tmax = tzmax;

		if (tmax < 0.0f) return false;
		outDist = tmin > 0.0f ? tmin : tmax;
		return true;
	}

	void EditorLayer::onViewportClick()
	{
		ImVec2 mousePos = ImGui::GetMousePos();
		float mouseX = mousePos.x - m_viewportBounds[0].x;
		float mouseY = mousePos.y - m_viewportBounds[0].y;
		float viewportWidth = m_viewportBounds[1].x - m_viewportBounds[0].x;
		float viewportHeight = m_viewportBounds[1].y - m_viewportBounds[0].y;

		if (mouseX < 0 || mouseY < 0 || mouseX >= viewportWidth || mouseY >= viewportHeight)
		{
			return;
		}

		bool ctrlHeld = Platform::Input::isKeyPressed(Platform::KeyCode::LeftControl) ||
		                Platform::Input::isKeyPressed(Platform::KeyCode::RightControl);

		// 1. Raycast fallback through Physics
		auto ray = Core::Renderer::SceneRenderer::unprojectRayReversedZ(
			mouseX, mouseY, viewportWidth, viewportHeight, m_editorCamera.getInverseViewProjectionMatrix()
		);

		Physics::RaycastHit hit;
		if (Runtime::Application::Get().getPhysicsWorld().raycast(*m_activeScene, ray.position, ray.direction, 1000.0f, hit))
		{
			m_selectionContext.select(hit.entity, ctrlHeld);
			return;
		}

		// 2. Raycast fallback through MeshRenderers AABBs
		float closestDist = 1000.0f;
		entt::entity closestEntity = entt::null;
		auto meshView = m_activeScene->view<Core::MeshRendererComponent, Core::TransformComponent>();
		for (auto entity : meshView)
		{
			const auto& tc = meshView.get<Core::TransformComponent>(entity);
			Core::Vector3 halfExtents = tc.scale * 0.5f;
			Core::Vector3 boxMin = tc.position - halfExtents;
			Core::Vector3 boxMax = tc.position + halfExtents;
			float dist = 0.0f;
			if (RayIntersectsAABB(ray.position, ray.direction, boxMin, boxMax, dist))
			{
				if (dist < closestDist)
				{
					closestDist = dist;
					closestEntity = entity;
				}
			}
		}

		if (closestEntity != entt::null)
		{
			m_selectionContext.select(Core::Entity(closestEntity, m_activeScene.get()), ctrlHeld);
			return;
		}

		// 3. Read Entity ID from GPU target
		uint32_t pickedId = Runtime::Application::Get().getRenderer().readEntityId(
			static_cast<uint32_t>(mouseX), static_cast<uint32_t>(mouseY)
		);

		if (pickedId != 0 && pickedId != 0xFFFFFFFF)
		{
			entt::entity entHandle = static_cast<entt::entity>(pickedId);
			if (m_activeScene->getRegistry().valid(entHandle))
			{
				m_selectionContext.select(Core::Entity(entHandle, m_activeScene.get()), ctrlHeld);
				return;
			}
		}

		// Clicked on empty background: clear selection if not holding ctrl
		if (!ctrlHeld)
		{
			m_selectionContext.clear();
		}
	}

	void EditorLayer::performAutosave()
	{
		if (!m_activeScene || !m_isSceneDirty)
		{
			return;
		}

		std::filesystem::path autosavePath;
		if (!m_currentScenePath.empty())
		{
			autosavePath = m_currentScenePath;
			autosavePath.replace_extension(".autosave.scene");
		}
		else
		{
			autosavePath = "Untitled.autosave.scene";
		}

		Core::SceneSerializer serializer(m_activeScene.get());
		if (serializer.serializeToFile(autosavePath))
		{
			Core::Log::Get().info("Editor", "Autosaved scene to: {}", autosavePath.string());
		}
	}

	void EditorLayer::updateWindowTitle()
	{
		std::string sceneName = m_currentScenePath.empty() ? "Untitled" : m_currentScenePath.filename().string();
		std::string title = "DarkFire Editor - [" + sceneName + (m_isSceneDirty ? "*]" : "]");
		Runtime::Application::Get().getWindow().setTitle(title);
	}
}
