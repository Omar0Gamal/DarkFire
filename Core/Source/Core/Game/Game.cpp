#include "Game.h"
#include "IEntity.h"
#include "World.h"
#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"
#include "Component/LightComponent.h"
#include "Component/MeshComponent.h"
#include "Component/TextComponent.h"
#include "Component/ImageComponent.h"
#include "Core/Resource/ResourceManager.h"
#include "Core/Math/Mat4x4.h"
#include "Core/Resource/Material.h"
#include "Core/Resource/Texture.h"
#include "Core/Resource/Mesh.h"
#include "Core/Resource/Font.h"
#include "Core/Graphics/GraphicsEngine.h"
#include "Core/Graphics/Font2D.h"
#include "Core/Graphics/RenderPipeline.h"
#include "Core/Graphics/MeshRenderPass.h"
#include "Core/Graphics/UIRenderPass.h"
#include "Core/Graphics/PostProcessPipeline.h"
#include "Core/Graphics/PostProcessRenderPass.h"
#include "Core/Graphics/DebugRenderPass.h"
#include "Core/Graphics/ParticleRenderPass.h"
#include "Core/Graphics/ParticleSystem.h"
#include "Core/Input/InputManager.h"
#include "Core/Input/InputActionMap.h"
#include "Core/UI/UICanvas.h"
#include "Core/Game/TrackEditor.h"
#include "Core/Game/WorldManager.h"
#include "Core/Game/PhysicsEngine.h"
#include "Display.h"

namespace Core
{
	Game::Game(const wchar_t* title, int width, int height)
	{
		m_inputManager = std::make_unique<InputManager>();
		m_graphicsEngine = std::make_unique<GraphicsEngine>();
		m_resourceManager = std::make_unique<ResourceManager>(this);
		m_display = std::make_unique<Display>(title, width, height, this);
		m_physicsEngine = std::make_unique<PhysicsEngine>(this);
		m_worldManager = std::make_unique<WorldManager>(this);
		m_renderPipeline = std::make_unique<RenderPipeline>();
		m_postProcessPipeline = std::make_shared<PostProcessPipeline>(m_graphicsEngine.get(), width, height);

		m_inputActionMap = std::make_unique<InputActionMap>(m_inputManager.get());

		m_canvas = std::make_unique<UICanvas>();
		m_canvas->setScreenSize(width, height);

		Box rc;
		rc.height = height;
		rc.width = width;
		m_inputManager->setScreenArea(rc);

		m_world = m_worldManager->createWorld("MainWorld");
		m_trackEditor = std::make_unique<TrackEditor>(this);
		m_particleSystem = std::make_unique<ParticleSystem>(this);

		// Initialize default render passes
		initDefaultRenderPasses();
	}

	void Game::initDefaultRenderPasses()
	{
		// Default pipeline: Geometry (100) -> Debug Wireframe (250) -> Particles (280) -> PostProcess (300) -> UI (500)
		m_renderPipeline->addPass<MeshRenderPass>();
		m_renderPipeline->addPass<DebugRenderPass>();
		m_renderPipeline->addPass<ParticleRenderPass>();
		m_renderPipeline->addPass<PostProcessRenderPass>(m_postProcessPipeline, m_display->getSwapChain());
		m_renderPipeline->addPass<UIRenderPass>(m_canvas.get());
	}

	Game::~Game()
	{
	}

	void Game::run()
	{
		onCreate();
		updateCamerasViewportSize();

		while (m_isRunning)
		{
			MSG msg;
			// WINDOW PROCESSING
			while (PeekMessage(&msg, HWND(), 0, 0, PM_REMOVE))
			{
				if (msg.message == WM_QUIT)
				{
					m_isRunning = false;
					continue;
				}
				else
				{
					TranslateMessage(&msg);
					DispatchMessage(&msg);
				}
			}

			onUpdateInternal();
		}

		onQuit();
	}

	void Game::quit() {}

	InputManager* Game::getInputManager()
	{
		return m_inputManager.get();
	}

	GraphicsEngine* Game::getGraphicsEngine()
	{
		return m_graphicsEngine.get();
	}

	Display* Game::getDisplay()
	{
		return m_display.get();
	}

	ResourceManager* Game::getResourceManager()
	{
		return m_resourceManager.get();
	}

	WorldManager* Game::getWorldManager()
	{
		return m_worldManager.get();
	}

	PhysicsEngine* Game::getPhysicsEngine()
	{
		return m_physicsEngine.get();
	}

	MaterialPtr Game::createMaterial(const wchar_t* path)
	{
		return m_resourceManager->createResourceFromFile<Material>(path);
	}

	TexturePtr Game::createTexture(const wchar_t* path)
	{
		return m_resourceManager->createResourceFromFile<Texture>(path);
	}

	MeshPtr Game::createMesh(const wchar_t* path)
	{
		return m_resourceManager->createResourceFromFile<Mesh>(path);
	}

	PostProcessPipeline* Game::getPostProcessPipeline()
	{
		return m_postProcessPipeline.get();
	}

	FontPtr Game::createFont(const wchar_t* path)
	{
		return m_resourceManager->createResourceFromFile<Font>(path);
	}

	void Game::onDisplaySize(const Box& size)
	{
		if (m_postProcessPipeline)
		{
			m_postProcessPipeline->resize(size.width, size.height);
		}
		if (m_canvas)
		{
			m_canvas->setScreenSize(size.width, size.height);
		}
		updateCamerasViewportSize();
		m_inputManager->setScreenArea(size);
		onUpdateInternal();
	}

	void Game::onUpdateInternal()
	{
		m_inputManager->update();

		auto now = std::chrono::system_clock::now();
		std::chrono::duration<double> elapsedSeconds = now - m_oldTime;
		if (!m_oldTime.time_since_epoch().count())
			elapsedSeconds = std::chrono::duration<double>();
		m_oldTime = now;

		m_avgDt += static_cast<float>(elapsedSeconds.count());
		m_avgCount++;

		m_deltaTime = m_avgDt / m_avgCount;
		m_totalTime += m_deltaTime;

		if (m_avgCount == 2000)
		{
			m_avgCount = 0;
			m_avgDt = static_cast<float>(elapsedSeconds.count());
		}

		m_physicsEngine->update(m_deltaTime);

		if (m_trackEditor)
		{
			m_trackEditor->update(m_deltaTime);
		}

		if (m_particleSystem && m_world && m_world->getECS())
		{
			m_particleSystem->update(m_deltaTime, m_world->getECS().get());
		}

		if (m_canvas)
		{
			m_canvas->update(m_deltaTime, m_inputManager.get());
		}

		onUpdate(m_deltaTime);

		for (auto& [key, entity] : m_world->getEntities())
		{
			entity->onUpdate(m_deltaTime);
		}

		onGraphicsUpdate(m_deltaTime);
	}

	RenderContext Game::buildRenderContext()
	{
		RenderContext ctx;
		ctx.deltaTime = m_deltaTime;
		ctx.totalTime = m_totalTime;
		ctx.viewportWidth = m_display->getClientSize().width;
		ctx.viewportHeight = m_display->getClientSize().height;

		// Get camera data
		auto mainCamera = m_world->getEntity<Entity>(m_world->getMainCameraID());
		if (mainCamera)
		{
			CameraComponent& camComp = mainCamera->getComponent<CameraComponent>();
			TransformComponent& transComp = mainCamera->getComponent<TransformComponent>();
			transComp.proccessMatrix();
			camComp.proccessMatrix(transComp.m_worldMatrix);

			ctx.projection = camComp.m_ProjectionMatrix;
			ctx.view = camComp.m_ViewMatrix;
			ctx.cameraPosition = transComp.m_worldMatrix.getTranslation();
		}

		// Gather lights
		m_world->getECS()->ForEach<TransformComponent, LightComponent>(
			[&](TransformComponent& transform, LightComponent& light)
			{
				transform.proccessMatrix();
				LightData ld;
				ld.lightPosition = transform.m_worldMatrix.getTranslation();
				ld.lightColor = light.m_Color;
				ld.lightDirection = transform.m_worldMatrix.getForwardDirection();
				ctx.lights.push_back(ld);
			});

		return ctx;
	}

	void Game::onGraphicsUpdate(float deltaTime)
	{
		if (m_postProcessPipeline && m_postProcessPipeline->isEnabled())
		{
			m_postProcessPipeline->beginScene(Vector4d(0.02f, 0.02f, 0.04f, 1.0f));
		}
		else
		{
			m_graphicsEngine->clearState();
			m_graphicsEngine->clearColor(
				m_display->getSwapChain(), Vector4d(0, 0, 0, 1));
			m_graphicsEngine->clearDepthStencil(m_display->getSwapChain());
			m_graphicsEngine->setViewportSize(
				m_display->getClientSize().width,
				m_display->getClientSize().height);
		}

		// Build render context and execute pipeline
		RenderContext ctx = buildRenderContext();

		if (m_trackEditor)
		{
			m_trackEditor->render(m_graphicsEngine.get(), ctx);
		}

		m_renderPipeline->execute(
			m_graphicsEngine.get(),
			m_world->getECS().get(),
			ctx);

		m_display->getSwapChain()->present(false);
	}

	void Game::updateCamerasViewportSize()
	{
		auto mainCamera = m_world->getEntity<Entity>(m_world->getMainCameraID());
		if (mainCamera)
		{
			// Use reference instead of copy!
			CameraComponent& camComp = mainCamera->getComponent<CameraComponent>();
			auto displaySize = m_display->getClientSize();
			camComp.m_Screen.width = displaySize.width;
			camComp.m_Screen.height = displaySize.height;
			camComp.m_ProjectionMatrix.setPerspectiveFovLH(
				1.3f,
				static_cast<float>(displaySize.width) / static_cast<float>(displaySize.height),
				0.1f,
				camComp.m_FarClip
			);
		}
	}

	UICanvas* Game::getUICanvas()
	{
		return m_canvas.get();
	}

	TrackEditor* Game::getTrackEditor()
	{
		return m_trackEditor.get();
	}

	InputActionMap* Game::getInputActionMap()
	{
		return m_inputActionMap.get();
	}

	ParticleSystem* Game::getParticleSystem()
	{
		return m_particleSystem.get();
	}
}
