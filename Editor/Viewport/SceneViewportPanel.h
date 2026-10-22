#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/RHI/IRHI.h"
#include "Editor/EditorCamera.h"
#include "Editor/SelectionContext.h"
#include "Editor/Commands/CommandHistory.h"
#include <memory>
#include <filesystem>
#include <functional>

namespace Editor
{
	class SceneViewportPanel
	{
	public:
		SceneViewportPanel() = default;

		void onImGuiRender(
			std::shared_ptr<Core::Scene>& activeScene,
			std::shared_ptr<Core::RHI::IGpuTexture> viewportTexture,
			std::shared_ptr<Core::RHI::IGpuTexture> gameViewportTexture,
			EditorCamera& camera,
			SelectionContext& selectionContext,
			CommandHistory& commandHistory,
			bool* p_open = nullptr
		);

		uint32_t getWidth() const noexcept { return m_viewportWidth; }
		uint32_t getHeight() const noexcept { return m_viewportHeight; }
		uint32_t getPendingWidth() const noexcept { return m_pendingViewportWidth; }
		uint32_t getPendingHeight() const noexcept { return m_pendingViewportHeight; }
		void clearPendingResize() noexcept { m_pendingViewportWidth = 0; m_pendingViewportHeight = 0; }
		void setSize(uint32_t w, uint32_t h) { m_viewportWidth = w; m_viewportHeight = h; }

		bool isHovered() const noexcept { return m_viewportHovered; }
		bool isFocused() const noexcept { return m_viewportFocused; }

		int getGizmoType() const noexcept { return m_gizmoType; }
		void setGizmoType(int type) noexcept { m_gizmoType = type; }

		bool getShowWireframe() const noexcept { return m_showWireframeMode; }
		void setShowWireframe(bool wireframe) noexcept { m_showWireframeMode = wireframe; }

		void setOnOpenScene(std::function<void(const std::filesystem::path&)> cb) { m_onOpenScene = std::move(cb); }
		void setOnNewSandboxScene(std::function<void()> cb) { m_onNewSandboxScene = std::move(cb); }
		void setOnSceneModified(std::function<void()> cb) { m_onSceneModified = std::move(cb); }

	private:
		void drawGizmo(
			std::shared_ptr<Core::Scene>& activeScene,
			EditorCamera& camera,
			SelectionContext& selectionContext,
			CommandHistory& commandHistory
		);

		uint32_t m_viewportWidth = 1280;
		uint32_t m_viewportHeight = 720;
		uint32_t m_pendingViewportWidth = 0;
		uint32_t m_pendingViewportHeight = 0;
		bool m_viewportHovered = false;
		bool m_viewportFocused = false;
		Core::Vector2 m_viewportBounds[2]{};

		// Gizmo
		int m_gizmoType = 0; // 0: Translate, 1: Rotate, 2: Scale, -1: None
		int m_gizmoMode = 1; // 0: Local, 1: World
		bool m_snapEnabled = false;
		float m_snapTranslation = 1.0f;
		float m_snapRotation = 15.0f;
		float m_snapScale = 0.5f;

		Core::TransformComponent m_gizmoInitialTransform{};
		bool m_gizmoWasUsing = false;

		// Overlays
		bool m_showWireframeMode = false;
		bool m_showGrid = true;
		bool m_showPiP = true;

		std::function<void(const std::filesystem::path&)> m_onOpenScene;
		std::function<void()> m_onNewSandboxScene;
		std::function<void()> m_onSceneModified;
	};
}
