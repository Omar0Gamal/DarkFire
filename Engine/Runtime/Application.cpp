#include "Engine/Runtime/Application.h"
#include "Engine/RHI/RHIFactory.h"
#include "Engine/Physics/PhysicsWorld.h"
#include "Engine/Core/Log/Log.h"

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

	void Application::run()
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

		while (m_running && !m_window->shouldClose())
		{
			m_window->pollEvents();

			double currentTime = timer.getElapsedSeconds();
			float frameTime = static_cast<float>(currentTime - lastTime);
			lastTime = currentTime;

			if (frameTime > 0.25f)
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

			// ImGui Rendering (if any)
			for (auto& layer : m_layerStack)
			{
				layer->onImGuiRender();
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
}
