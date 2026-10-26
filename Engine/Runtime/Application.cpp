#include "Engine/Runtime/Application.h"
#include "Engine/RHI/RHIFactory.h"
#include "Engine/Physics/PhysicsWorld.h"
#include "Engine/Core/Log/Log.h"

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>
#include <ImGuizmo.h>

namespace Runtime
{
	Application* Application::s_instance = nullptr;

	Application::Application(const ProjectConfig& config)
		: m_config(config)
	{
		s_instance = this;
	}

	Application::~Application()
	{
		onShutdown();
		if (m_imguiInitialized)
		{
			shutdownImGui();
		}
		m_layerStack.clear();
		s_instance = nullptr;
	}

	bool Application::init()
	{
		Core::Log::Get().info("Runtime", "Initializing Application: '{}' (v{})", m_config.name, m_config.version);

		// 1. Create Window
		Platform::WindowDesc winDesc;
		winDesc.title = m_config.windowTitle;
		winDesc.width = m_config.windowWidth;
		winDesc.height = m_config.windowHeight;
		winDesc.vsync = m_config.vsync;
		winDesc.fullscreen = m_config.fullscreen;

		m_window = std::make_unique<Platform::Window>();
		if (!m_window->init(winDesc))
		{
			Core::Log::Get().fatal("Runtime", "Failed to initialize Application window!");
			return false;
		}

		m_window->setResizeCallback([this](uint32_t w, uint32_t h) {
			onWindowResize(w, h);
		});

		m_window->setCloseCallback([this]() {
			close();
		});

		// 2. Initialize RHI Device
		bool debugRHI = false;
#if defined(DF_DEBUG)
		debugRHI = true;
#endif

		m_device = Core::RHI::CreateRHIDevice(Core::RHI::BackendType::D3D11, debugRHI);
		if (!m_device)
		{
			Core::Log::Get().warn("Runtime", "D3D11 RHI Device failed to create! Falling back to Null RHI Device.");
			m_device = Core::RHI::CreateRHIDevice(Core::RHI::BackendType::Null, false);
		}

		// 3. Create SwapChain
		if (m_device)
		{
			auto scRes = m_device->createSwapChain(m_window->getNativeHandle(), m_config.windowWidth, m_config.windowHeight);
			if (scRes.isOk())
			{
				m_swapChain = std::move(scRes.value());
			}
			else
			{
				Core::Log::Get().warn("Runtime", "Failed to create swapchain: {}", scRes.error());
			}
		}

		// 4. Initialize SceneRenderer
		if (m_device)
		{
			m_sceneRenderer = std::make_unique<Core::Renderer::SceneRenderer>(*m_device);
			m_sceneRenderer->resize(m_config.windowWidth, m_config.windowHeight);
		}

		// 5. Initialize Physics
		m_physicsWorld = std::make_unique<Physics::PhysicsWorld>();
		m_physicsWorld->init(m_config.gravity, m_config.fixedTimeStep);
		m_physicsWorld->setCollisionMatrix(m_config.collisionMatrix);

		initImGui();

		onInit();
		Core::Log::Get().info("Runtime", "Application initialized successfully.");
		return true;
	}

	void Application::pushLayer(std::shared_ptr<Layer> layer)
	{
		m_layerStack.pushLayer(std::move(layer));
	}

	void Application::pushOverlay(std::shared_ptr<Layer> overlay)
	{
		m_layerStack.pushOverlay(std::move(overlay));
	}

	void Application::onWindowResize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0)
		{
			return;
		}

		m_config.windowWidth = width;
		m_config.windowHeight = height;

		if (m_swapChain)
		{
			m_swapChain->resize(width, height);
		}

		if (m_sceneRenderer)
		{
			m_sceneRenderer->resize(width, height);
		}

		for (auto& layer : m_layerStack)
		{
			layer->onResize(width, height);
		}
	}

	void Application::close()
	{
		m_running = false;
	}

	void Application::run(int maxFrames)
	{
		m_running = true;
		Core::Timer timer;
		float accumulator = 0.0f;
		float fixedDeltaTime = m_config.fixedTimeStep;
		if (fixedDeltaTime <= 0.0f)
		{
			fixedDeltaTime = 1.0f / 60.0f;
		}

		double lastTime = timer.getElapsedSeconds();
		int frameCount = 0;

		while (m_running && !m_window->shouldClose())
		{
			if (maxFrames > 0 && frameCount >= maxFrames)
			{
				break;
			}
			++frameCount;

			m_window->pollEvents();

			double currentTime = timer.getElapsedSeconds();
			float frameTime = static_cast<float>(currentTime - lastTime);
			lastTime = currentTime;

			if (maxFrames > 0)
			{
				frameTime = fixedDeltaTime;
			}
			else if (frameTime > 0.25f)
			{
				frameTime = 0.25f; // Prevent spiral of death
			}

			accumulator += frameTime;

			// 60 Hz Deterministic Fixed Update
			while (accumulator >= fixedDeltaTime)
			{
				for (auto& layer : m_layerStack)
				{
					layer->onFixedUpdate(fixedDeltaTime);
				}
				accumulator -= fixedDeltaTime;
			}

			float alpha = accumulator / fixedDeltaTime;
			(void)alpha;

			// Variable Frame Update
			for (auto& layer : m_layerStack)
			{
				layer->onUpdate(frameTime);
			}

			// ImGui Rendering (Editor DockSpace or In-Game HUD Overlay)
			if (m_imguiInitialized)
			{
				beginImGuiFrame();
				for (auto& layer : m_layerStack)
				{
					layer->onImGuiRender();
				}
				endImGuiFrame();
			}

			// Present to display
			if (m_swapChain)
			{
				m_swapChain->present(m_config.vsync);
			}

			Platform::Input::newFrame();
		}

		Core::Log::Get().info("Runtime", "Application loop terminated.");
	}

	void Application::initImGui()
	{
		if (!m_window || !m_device || !m_device->getNativeDevice() || !m_device->getNativeContext())
		{
			return;
		}

		IMGUI_CHECKVERSION();
		ImGui::CreateContext();
		ImGuiIO& io = ImGui::GetIO();
		io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
		if (m_config.enableImGui)
		{
			io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;
			io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;
		}

		const float dpiScale = m_window ? m_window->getDpiScale() : 1.0f;
		const float validDpi = (dpiScale > 0.5f) ? dpiScale : 1.0f;

		// Crisp native fonts with 2x subpixel oversampling
		ImFontConfig fontCfg;
		fontCfg.OversampleH = 2;
		fontCfg.OversampleV = 1;
		fontCfg.PixelSnapH = true;

		const float baseFontSize = std::round(15.0f * validDpi);
		const float codeFontSize = std::round(14.0f * validDpi);

		bool fontLoaded = false;
		if (std::filesystem::exists("C:\\Windows\\Fonts\\segoeui.ttf"))
		{
			io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segoeui.ttf", baseFontSize, &fontCfg);
			fontLoaded = true;
		}

		if (std::filesystem::exists("C:\\Windows\\Fonts\\consola.ttf"))
		{
			io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\consola.ttf", codeFontSize, &fontCfg);
		}

		if (fontLoaded)
		{
			// Merge native Windows vector icon glyphs (Segoe Fluent / MDL2)
			static const ImWchar iconRanges[] = { 0xE700, 0xF8FF, 0 };
			ImFontConfig iconCfg;
			iconCfg.MergeMode = true;
			iconCfg.PixelSnapH = true;
			iconCfg.OversampleH = 2;
			iconCfg.OversampleV = 1;
			if (std::filesystem::exists("C:\\Windows\\Fonts\\SegoeIcons.ttf"))
			{
				io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\SegoeIcons.ttf", baseFontSize, &iconCfg, iconRanges);
			}
			else if (std::filesystem::exists("C:\\Windows\\Fonts\\segmdl2.ttf"))
			{
				io.Fonts->AddFontFromFileTTF("C:\\Windows\\Fonts\\segmdl2.ttf", baseFontSize, &iconCfg, iconRanges);
			}
		}

		if (!fontLoaded)
		{
			io.Fonts->AddFontDefault();
		}

		ImGuiStyle& style = ImGui::GetStyle();
		style.ScaleAllSizes(validDpi);

		ImGui_ImplWin32_Init(m_window->getNativeHandle());

		void* nativeDev = m_device->getNativeDevice();
		void* nativeCtx = m_device->getNativeContext();
		if (nativeDev && nativeCtx)
		{
			ImGui_ImplDX11_Init(
				reinterpret_cast<ID3D11Device*>(nativeDev),
				reinterpret_cast<ID3D11DeviceContext*>(nativeCtx)
			);
			m_imguiInitialized = true;
		}

		ImGuiMemAllocFunc allocFunc = nullptr;
		ImGuiMemFreeFunc freeFunc = nullptr;
		void* userData = nullptr;
		ImGui::GetAllocatorFunctions(&allocFunc, &freeFunc, &userData);

		auto& edCtx = Core::EditorContext::Get();
		edCtx.imGuiContext = ImGui::GetCurrentContext();
		edCtx.allocFunc = allocFunc;
		edCtx.freeFunc = freeFunc;
		edCtx.userData = userData;

		Core::Log::Get().info("Runtime", "ImGui & ImGuizmo initialized for Application.");
	}

	void Application::shutdownImGui()
	{
		if (m_imguiInitialized)
		{
			ImGui_ImplDX11_Shutdown();
			ImGui_ImplWin32_Shutdown();
			ImGui::DestroyContext();
			m_imguiInitialized = false;
			Core::Log::Get().info("Runtime", "ImGui shut down.");
		}
	}

	void Application::beginImGuiFrame()
	{
		ImGui_ImplDX11_NewFrame();
		ImGui_ImplWin32_NewFrame();
		ImGui::NewFrame();
		ImGuizmo::BeginFrame();
	}

	void Application::endImGuiFrame()
	{
		if (m_swapChain && m_device)
		{
			auto backBuffer = m_swapChain->getBackBufferTexture();
			if (backBuffer)
			{
				std::shared_ptr<Core::RHI::IGpuTexture> rtvs[] = { backBuffer };
				m_device->getImmediate()->setRenderTargets(rtvs, nullptr);
				if (m_config.enableImGui)
				{
					m_device->getImmediate()->clearRenderTarget(backBuffer, Core::Color(0.10f, 0.10f, 0.12f, 1.0f));
				}
			}
		}

		ImGui::Render();
		ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());

		ImGuiIO& io = ImGui::GetIO();
		if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			ImGui::UpdatePlatformWindows();
			ImGui::RenderPlatformWindowsDefault();
		}
	}
}
