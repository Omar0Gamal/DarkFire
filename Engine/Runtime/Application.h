#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Timer.h"
#include "Engine/Platform/Window.h"
#include "Engine/Platform/Input.h"
#include "Engine/Runtime/Config.h"
#include "Engine/Runtime/LayerStack.h"
#include "Engine/RHI/IRHI.h"
#include "Engine/Renderer/SceneRenderer.h"
#include "Engine/Physics/IPhysicsWorld.h"
#include "Engine/Runtime/EngineContext.h"
#include <memory>

namespace Runtime
{
	class DF_API Application
	{
	public:
		explicit Application(const ProjectConfig& config = ProjectConfig{});
		virtual ~Application();

		Application(const Application&) = delete;
		Application& operator=(const Application&) = delete;

		static Application& Get() noexcept { return *s_instance; }

		bool init();
		void run();
		void close();

		void pushLayer(std::shared_ptr<Layer> layer);
		void pushOverlay(std::shared_ptr<Layer> overlay);

		Platform::Window& getWindow() noexcept { return *m_window; }
		const Platform::Window& getWindow() const noexcept { return *m_window; }

		Core::RHI::IRHIDevice& getDevice() noexcept { return *m_device; }
		Core::RHI::ISwapChain* getSwapChain() noexcept { return m_swapChain.get(); }
		std::shared_ptr<Core::RHI::ISwapChain> getSwapChainShared() const noexcept { return m_swapChain; }
		Core::Renderer::SceneRenderer& getRenderer() noexcept { return *m_sceneRenderer; }
		Physics::IPhysicsWorld& getPhysicsWorld() noexcept { return *m_physicsWorld; }
		const ProjectConfig& getConfig() const noexcept { return m_config; }

	protected:
		virtual void onInit() {}
		virtual void onShutdown() {}

	private:
		void onWindowResize(uint32_t width, uint32_t height);

		static Application* s_instance;

		ProjectConfig m_config;
		bool m_running = false;

		std::unique_ptr<Platform::Window> m_window;
		std::shared_ptr<Core::RHI::IRHIDevice> m_device;
		std::shared_ptr<Core::RHI::ISwapChain> m_swapChain;
		std::unique_ptr<Core::Renderer::SceneRenderer> m_sceneRenderer;
		std::unique_ptr<Physics::IPhysicsWorld> m_physicsWorld;
		LayerStack m_layerStack;

#if defined(DF_EDITOR)
		bool m_imguiInitialized = false;
		void initImGui();
		void shutdownImGui();
		void beginImGuiFrame();
		void endImGuiFrame();
#endif
	};
}

