#pragma once

#include "Core/Graphics/Prerequisites.h"
#include "Core/Math/Box.h"

#include <map>
#include <chrono>
#include <set>
#include <string>

namespace Core
{
	class Game
	{
	public:
		Game(const wchar_t* titles, int width, int height);
		virtual ~Game();
	public:
		virtual void onCreate() {}
		virtual void onUpdate(float deltaTime) {}
		virtual void onQuit() {}
	public:
		void run();
		void quit();
	public:
		InputManager* getInputManager();
		GraphicsEngine* getGraphicsEngine();
		Display* getDisplay();
		PhysicsEngine* getPhysicsEngine();
		ResourceManager* getResourceManager();
		WorldManager* getWorldManager();
		WorldPtr getActiveWorld() { return m_world; }
	public:
		MaterialPtr createMaterial(const wchar_t* path);
		TexturePtr createTexture(const wchar_t* path);
		MeshPtr createMesh(const wchar_t* path);

	private:
		void onDisplaySize(const Box& size);
		void onUpdateInternal();
		void onGraphicsUpdate(float deltaTime);
		void updateCamerasViewportSize();
	private:
		bool m_isRunning = true;

		std::unique_ptr<InputManager> m_inputManager;
		std::unique_ptr<GraphicsEngine> m_graphicsEngine;
		std::unique_ptr<Display> m_display;
		std::unique_ptr<PhysicsEngine> m_physicsEngine;
		std::unique_ptr<ResourceManager> m_resourceManager;
		std::unique_ptr<WorldManager> m_worldManager;

		WorldPtr m_world;

		float m_avgDt = 0.0f;
		unsigned long long m_avgCount = 0;

		std::chrono::system_clock::time_point m_oldTime;
	private:
		friend class Display;
	
	};
}

