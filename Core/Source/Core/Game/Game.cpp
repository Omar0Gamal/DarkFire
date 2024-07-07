#include "Game.h"
#include "IEntity.h"
#include "World.h"
#include "Component/CameraComponent.h"
#include "Component/TransformComponent.h"
#include "Component/LightComponent.h"
#include "Component/MeshComponent.h"
#include "Core/Resource/ResourceManager.h"
#include "Core/Math/Mat4x4.h"
#include "Core/Resource/Material.h"
#include "Core/Resource/Texture.h"
#include "Core/Resource/Mesh.h"
#include "Core/Graphics/GraphicsEngine.h"
#include "Core/Input/InputManager.h"
#include "Core/Game/WorldManager.h"
#include "Core/Game/PhysicsEngine.h"
#include "Display.h"

namespace Core
{
	__declspec(align(16))
		struct LightData
	{
		Vector4d lightColor;
		Vector4d lightDirection;
		Vector4d lightPosition;
	};


	__declspec(align(16))
		struct GraphicsData
	{
		Mat4x4 world;
		Mat4x4 view;
		Mat4x4 proj;
		Vector4d cameraPosition;
		LightData lights[32];
		int numLights = 0;
	};

	Game::Game(const wchar_t* title, int width, int height)
	{
		GamePtr game = std::shared_ptr<Game>(this);
		m_inputManager = std::make_unique<InputManager>();
		m_graphicsEngine = std::make_unique<GraphicsEngine>();
		m_resourceManager = std::make_unique<ResourceManager>(game);
		m_display = std::make_unique<Display>(title,width,height, game);
		m_physicsEngine = std::make_unique<PhysicsEngine>(game);
		m_worldManager = std::make_unique<WorldManager>(game);
		Box rc;
		rc.height = height;
		rc.width = width;
		m_inputManager->setScreenArea(rc);

		m_world = m_worldManager->createWorld("MainWorld");
	}

	Game::~Game() {}
	void Game::run()
	{
		onCreate();

		updateCamerasViewportSize();


		while (m_isRunning)
		{
			MSG msg;
			//WINDOW PROCESSING
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
	void Game::onDisplaySize(const Box& size)
	{
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

		m_avgDt += (float)elapsedSeconds.count();
		m_avgCount++;

		float deltaTime = m_avgDt / m_avgCount;

		unsigned long long max = -1;
		if (m_avgCount == 2000)
		{
			m_avgCount = 0;
			m_avgDt = (float)elapsedSeconds.count();
		}

		m_physicsEngine->update(deltaTime);
		
		onUpdate(deltaTime);

		for (auto& [key, entity] : m_world->getEntities())
		{
			entity->onUpdate(deltaTime);
		}

		onGraphicsUpdate(deltaTime);
	}
	void Game::onGraphicsUpdate(float deltaTime)
	{
		m_graphicsEngine->clearColor(
			m_display->getSwapChain(), Vector4d(0, 0, 0, 1));

		m_graphicsEngine->setViewportSize(m_display->getClientSize().width,
			m_display->getClientSize().height);


		GraphicsData gdata;

		{
			auto mainCamera = m_world->getEntity<Entity>(m_world->getMainCameraID());

			if (mainCamera)
			{
				CameraComponent camComp = mainCamera->getComponent<CameraComponent>();
				TransformComponent transComp = mainCamera->getComponent<TransformComponent>();
				transComp.proccessMatrix();
				camComp.proccessMatrix(transComp.m_worldMatrix);
				gdata.proj = camComp.m_ProjectionMatrix;
				gdata.view = camComp.m_ViewMatrix;
				gdata.cameraPosition = transComp.m_worldMatrix.getTranslation();
			}
		}

		
		{
			int i = 0;
			m_world->getECS()->ForEach<TransformComponent, LightComponent>([&](TransformComponent& transform, LightComponent& light)
			{
				transform.proccessMatrix();
				gdata.lights[i].lightPosition = transform.m_worldMatrix.getTranslation();
				gdata.lights[i].lightColor = light.m_Color;
				gdata.lights[i].lightDirection = transform.m_worldMatrix.getForwardDirection();
				i++;
			});
			gdata.numLights = i + 1;
		}

		{
			m_world->getECS()->ForEach<TransformComponent, MeshComponent>([&](TransformComponent& transform, MeshComponent& mesh)
				{;
					transform.proccessMatrix();
					gdata.world = transform.m_worldMatrix;

					for (int i = 0; i < mesh.m_Materials.size(); i++)
					{
						mesh.m_Materials[i]->setData(&gdata,sizeof(GraphicsData));
					}

					m_graphicsEngine->drawMesh(mesh.m_Mesh, mesh.m_Materials);
				});
		}

		m_display->getSwapChain()->present(false);
	}
	void Game::updateCamerasViewportSize()
	{
		auto mainCamera = m_world->getEntity<Entity>(m_world->getMainCameraID());
		if (mainCamera)
		{
			CameraComponent camComp = mainCamera->getComponent<CameraComponent>();
			auto displaySize = m_display->getClientSize();
			camComp.m_Screen.width = displaySize.width;
			camComp.m_Screen.height = displaySize.height;
			camComp.m_ProjectionMatrix.setPerspectiveFovLH(1.3f, (float)displaySize.width / (float)displaySize.height, 0.1f, camComp.m_FarClip);
		}
	}
}
