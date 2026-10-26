#include "Editor/EditorLayer.h"
#include "Editor/UI/EditorIcons.h"
#include "Editor/UI/DockLayout.h"
#include "Editor/Clipboard.h"
#include "Editor/Commands/EntityCommands.h"
#include "Engine/Runtime/Application.h"
#include "Engine/Core/Scripting/ScriptRegistry.h"
#include "Engine/Core/Log/Log.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <filesystem>

namespace Editor
{
	void EditorLayer::onImGuiRender()
	{
		if (m_inProjectManager)
		{
			m_projectManagerPanel.draw();
			return;
		}

		// 1. Fixed Top Bar
		drawTopBar();

		// 2. Fixed Bottom Status Bar
		drawBottomBar();

		// 3. Central DockSpace Host via DockLayout
		UI::DockLayout::Get().beginHost(m_activeWorkspace);

		// 4. Render Docks based on Active Workspace
		if (m_activeWorkspace == Workspace::Viewport3D)
		{
			if (m_showSceneViewport)
			{
				m_sceneViewportPanel.onImGuiRender(
					m_activeScene,
					m_viewportTexture,
					m_gameViewportTexture,
					m_editorCamera,
					m_selectionContext,
					m_commandHistory,
					&m_showSceneViewport
				);
			}
			if (m_showGameViewport)
			{
				m_gameViewportPanel.onImGuiRender(m_gameViewportTexture, &m_showGameViewport);
			}
			if (m_showHierarchy) m_hierarchyPanel.onImGuiRender(&m_showHierarchy);
			if (m_showInspector) m_inspectorPanel.onImGuiRender(m_selectionContext.getPrimary(), &m_showInspector);
			if (m_showWorldSettings) m_worldSettingsPanel.onImGuiRender(&m_showWorldSettings);
			if (m_showContentBrowser) m_contentBrowserPanel.onImGuiRender(&m_showContentBrowser);
			if (m_showConsole) m_consolePanel.onImGuiRender(&m_showConsole);
			if (m_showProfiler) m_profilerPanel.onImGuiRender(ImGui::GetIO().DeltaTime, &m_showProfiler);

			if (m_focusFileSystemOnOpen)
			{
				ImGui::SetWindowFocus(UI::PanelNames::FileSystem);
				m_focusFileSystemOnOpen = false;
			}
		}
		else if (m_activeWorkspace == Workspace::Game)
		{
			if (m_showGameViewport) m_gameViewportPanel.onImGuiRender(m_gameViewportTexture, &m_showGameViewport);
			if (m_showSceneViewport)
			{
				m_sceneViewportPanel.onImGuiRender(
					m_activeScene,
					m_viewportTexture,
					m_gameViewportTexture,
					m_editorCamera,
					m_selectionContext,
					m_commandHistory,
					&m_showSceneViewport
				);
			}
			if (m_showHierarchy) m_hierarchyPanel.onImGuiRender(&m_showHierarchy);
			if (m_showInspector) m_inspectorPanel.onImGuiRender(m_selectionContext.getPrimary(), &m_showInspector);
			if (m_showConsole) m_consolePanel.onImGuiRender(&m_showConsole);
		}
		else if (m_activeWorkspace == Workspace::Script)
		{
			ImGui::Begin(UI::PanelNames::ScriptEditor);
			ImGui::TextColored(ImVec4(1.0f, 0.420f, 0.208f, 1.0f), "DarkFire Script Workspace");
			ImGui::Separator();
			ImGui::Text("Active Game Module: %s", m_gameModule ? m_gameModule->getInfo().name : "None loaded");
			ImGui::Text("Registered C++ Scripts: %zu", Core::ScriptRegistry::Get().getRegisteredNames().size());
			for (const auto& sName : Core::ScriptRegistry::Get().getRegisteredNames())
			{
				ImGui::BulletText("%s", sName.c_str());
			}
			ImGui::End();

			if (m_showHierarchy) m_hierarchyPanel.onImGuiRender(&m_showHierarchy);
			if (m_showContentBrowser) m_contentBrowserPanel.onImGuiRender(&m_showContentBrowser);
			if (m_showInspector) m_inspectorPanel.onImGuiRender(m_selectionContext.getPrimary(), &m_showInspector);
			if (m_showConsole) m_consolePanel.onImGuiRender(&m_showConsole);
		}
		else if (m_activeWorkspace == Workspace::Profiler)
		{
			if (m_showProfiler) m_profilerPanel.onImGuiRender(ImGui::GetIO().DeltaTime, &m_showProfiler);
			if (m_showConsole) m_consolePanel.onImGuiRender(&m_showConsole);
		}

		UI::DockLayout::Get().endHost();

		std::filesystem::path projectDir = m_currentProjectPath.empty() ? std::filesystem::current_path() : m_currentProjectPath.parent_path();
		m_scriptWizardModal.onImGuiRender(projectDir);
		m_projectSettingsPanel.draw();
		m_packagingModal.draw();
	}

	void EditorLayer::drawTopBar()
	{
		const float topBarHeight = 36.0f;
		if (ImGui::BeginViewportSideBar("##TopHeaderBar", ImGui::GetMainViewport(), ImGuiDir_Up, topBarHeight,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar))
		{
			if (ImGui::BeginMenuBar())
			{
				// 1. Menus
				if (ImGui::BeginMenu("Scene"))
				{
					if (ImGui::MenuItem("New Scene", "Ctrl+N")) newScene();
					if (ImGui::MenuItem("New Sandbox Scene", "Ctrl+Shift+N")) newSandboxScene();
					if (ImGui::MenuItem("Open Scene...", "Ctrl+O")) openScene();
					if (ImGui::MenuItem("Save Scene", "Ctrl+S")) saveScene();
					if (ImGui::MenuItem("Save Scene As...", "Ctrl+Shift+S")) saveSceneAs();
					ImGui::Separator();
					if (ImGui::MenuItem("Project Manager")) closeProject();
					ImGui::Separator();
					if (ImGui::MenuItem("Exit", "Alt+F4")) Runtime::Application::Get().close();
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Edit"))
				{
					if (ImGui::MenuItem("Undo", "Ctrl+Z", false, m_commandHistory.canUndo())) m_commandHistory.undo();
					if (ImGui::MenuItem("Redo", "Ctrl+Y", false, m_commandHistory.canRedo())) m_commandHistory.redo();
					ImGui::Separator();
					if (ImGui::MenuItem("Duplicate", "Ctrl+D", false, m_selectionContext.hasSelection()))
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
					if (ImGui::MenuItem("Delete", "Delete", false, m_selectionContext.hasSelection()))
					{
						for (auto e : m_selectionContext.getEntities())
						{
							if (e.isValid())
							{
								m_commandHistory.execute<DeleteEntityCommand>(m_activeScene.get(), e);
							}
						}
						m_selectionContext.clear();
						m_isSceneDirty = true;
						updateWindowTitle();
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("View"))
				{
					ImGui::MenuItem("Scene 3D", nullptr, &m_showSceneViewport);
					ImGui::MenuItem("Game Viewport", nullptr, &m_showGameViewport);
					ImGui::MenuItem("Scene Hierarchy", nullptr, &m_showHierarchy);
					ImGui::MenuItem("Inspector", nullptr, &m_showInspector);
					ImGui::MenuItem("World Settings", nullptr, &m_showWorldSettings);
					ImGui::MenuItem("Content Browser", nullptr, &m_showContentBrowser);
					ImGui::MenuItem("Output Log", nullptr, &m_showConsole);
					ImGui::MenuItem("Profiler", nullptr, &m_showProfiler);
					ImGui::Separator();
					if (ImGui::MenuItem("Reset Godot 4 Layout"))
					{
						UI::DockLayout::Get().requestReset(m_activeWorkspace);
					}
					ImGui::EndMenu();
				}

				if (ImGui::BeginMenu("Project"))
				{
					if (ImGui::MenuItem("New C++ Script..."))
					{
						m_scriptWizardModal.open();
					}
					if (ImGui::MenuItem("Build C++ Project & Hot-Reload", "F7"))
					{
						buildAndReloadGameModule();
					}
					if (ImGui::MenuItem("Project Settings...")) { m_projectSettingsPanel.open(m_currentProjectPath); }
					if (false)
					{
					}
					ImGui::Separator();
					if (ImGui::MenuItem("Export / Package Game..."))
					{
						m_packagingModal.open(m_currentProjectPath);
					}
					ImGui::EndMenu();
				}

				// Center Workspace Switcher Tabs
				ImGui::SameLine(ImGui::GetWindowWidth() * 0.40f);
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(2, 0));
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.14f, 0.17f, 0.6f));

				auto drawWorkspaceButton = [this](const char* label, Workspace ws) {
					bool active = (m_activeWorkspace == ws);
					if (active)
					{
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.23f, 0.28f, 1.0f));
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.278f, 0.549f, 0.749f, 1.0f));
					}
					if (ImGui::Button(label, ImVec2(0, 24.0f)))
					{
						m_activeWorkspace = ws;
					}
					if (active)
					{
						ImGui::PopStyleColor(2);
					}
					ImGui::SameLine();
				};

				drawWorkspaceButton("3D Scene", Workspace::Viewport3D);
				drawWorkspaceButton("Game", Workspace::Game);
				drawWorkspaceButton("Profiler", Workspace::Profiler);

				ImGui::PopStyleColor();
				ImGui::PopStyleVar();

				// Right Playtest Controls
				ImGui::SameLine(ImGui::GetWindowWidth() - 250.0f);
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));

				bool isPlaying = m_playSession.isPlaying();
				bool isPaused = m_playSession.isPaused();

				// Play Button
				if (!isPlaying && !isPaused)
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.18f, 0.45f, 0.25f, 0.85f));
					if (ImGui::Button("▶ Play (F5)", ImVec2(80.0f, 24.0f))) onPlay();
					ImGui::PopStyleColor();
				}
				else
				{
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 0.85f));
					if (ImGui::Button("■ Stop (Shift+F5)", ImVec2(85.0f, 24.0f))) onStop();
					ImGui::PopStyleColor();
				}

				ImGui::SameLine();
				if (ImGui::Button(isPaused ? "▶ Resume" : "❚❚ Pause (F6)", ImVec2(75.0f, 24.0f)))
				{
					onPause();
				}

				ImGui::SameLine();
				if (ImGui::Button("⏭ Step", ImVec2(55.0f, 24.0f)))
				{
					onStep();
				}

				ImGui::SameLine();
				if (m_playSession.hasRunningInstances())
				{
					if (ImGui::Button("■ Stop PIE", ImVec2(80.0f, 24.0f)))
					{
						m_playSession.stopMultiInstances();
					}
				}
				else if (ImGui::Button("PIE ▾", ImVec2(55.0f, 24.0f)))
				{
					ImGui::OpenPopup("##PIEMenuPopup");
				}
				if (ImGui::BeginPopup("##PIEMenuPopup"))
				{
					if (ImGui::MenuItem("Single Player (Standalone)"))
					{
						onPlayStandalone();
					}
					if (ImGui::MenuItem("1 Server + 1 Client"))
					{
						m_playSession.launchMultiInstance(m_currentProjectPath, PIEMode::ServerAnd1Client);
					}
					if (ImGui::MenuItem("1 Server + 2 Clients"))
					{
						m_playSession.launchMultiInstance(m_currentProjectPath, PIEMode::ServerAnd2Clients);
					}
					ImGui::EndPopup();
				}
				if (false)
				{
					onPlayStandalone();
				}

				ImGui::PopStyleVar();
				ImGui::EndMenuBar();
			}
		}
		ImGui::End();
	}

	void EditorLayer::drawBottomBar()
	{
		const float bottomBarHeight = 26.0f;
		if (ImGui::BeginViewportSideBar("##BottomStatusBar", ImGui::GetMainViewport(), ImGuiDir_Down, bottomBarHeight,
			ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar))
		{
			if (ImGui::BeginMenuBar())
			{
				ImGui::TextColored(ImVec4(0.278f, 0.549f, 0.749f, 1.0f), "DarkFire 2.0");
				ImGui::SameLine();
				ImGui::TextDisabled("|");
				ImGui::SameLine();

				if (m_playSession.isPlaying())
				{
					ImGui::TextColored(ImVec4(0.35f, 0.85f, 0.45f, 1.0f), "[PLAYING]");
				}
				else if (m_playSession.isPaused())
				{
					ImGui::TextColored(ImVec4(0.95f, 0.75f, 0.2f, 1.0f), "[PAUSED]");
				}
				else if (m_playSession.getEditorMode() == EditorMode::Simulate)
				{
					ImGui::TextColored(ImVec4(0.4f, 0.7f, 1.0f, 1.0f), "[SIMULATING]");
				}
				else
				{
					ImGui::TextDisabled("[EDIT MODE]");
				}

				ImGui::SameLine();
				ImGui::TextDisabled("| Project: %s", m_projectConfig.name.empty() ? "None" : m_projectConfig.name.c_str());

				ImGui::SameLine();
				ImGui::TextDisabled("| Scene: %s%s",
					m_currentScenePath.empty() ? "Untitled" : m_currentScenePath.filename().string().c_str(),
					m_isSceneDirty ? " *" : "");

				// Right side stats
				float rightOffset = 220.0f;
				ImGui::SameLine(ImGui::GetWindowWidth() - rightOffset);
				float fps = ImGui::GetIO().Framerate;
				ImGui::TextDisabled("%.1f FPS (%.2f ms)", fps, 1000.0f / (fps > 0 ? fps : 60.0f));

				ImGui::EndMenuBar();
			}
		}
		ImGui::End();
	}
}
