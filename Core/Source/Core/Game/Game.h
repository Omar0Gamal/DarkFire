#pragma once

#include "Core/Graphics/Prerequisites.h"
#include "Core/Graphics/RenderPipeline.h"
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
		
		/// Get the render pipeline to add/remove custom render passes
		RenderPipeline* getRenderPipeline() { return m_renderPipeline.get(); }
		class PostProcessPipeline* getPostProcessPipeline();
		class UICanvas* getUICanvas();
		class TrackEditor* getTrackEditor();
		class InputActionMap* getInputActionMap();
		class ParticleSystem* getParticleSystem();
	public:
		MaterialPtr createMaterial(const wchar_t* path);
		TexturePtr createTexture(const wchar_t* path);
		MeshPtr createMesh(const wchar_t* path);
		FontPtr createFont(const wchar_t* path);
	protected:
		/// Build render context - override to customize camera/light gathering
		virtual RenderContext buildRenderContext();
	private:
		void onDisplaySize(const Box& size);
		void onUpdateInternal();
		void onGraphicsUpdate(float deltaTime);
		void updateCamerasViewportSize();
		void initDefaultRenderPasses();
	private:
		bool m_isRunning = true;

		std::unique_ptr<InputManager> m_inputManager;
		std::unique_ptr<GraphicsEngine> m_graphicsEngine;
		std::unique_ptr<Display> m_display;
		std::unique_ptr<PhysicsEngine> m_physicsEngine;
		std::unique_ptr<ResourceManager> m_resourceManager;
		std::unique_ptr<WorldManager> m_worldManager;
		std::unique_ptr<RenderPipeline> m_renderPipeline;
		std::shared_ptr<class PostProcessPipeline> m_postProcessPipeline;
		std::unique_ptr<class UICanvas> m_canvas;
		std::unique_ptr<class TrackEditor> m_trackEditor;
		std::unique_ptr<class InputActionMap> m_inputActionMap;
		std::unique_ptr<class ParticleSystem> m_particleSystem;

		WorldPtr m_world;

		float m_avgDt = 0.0f;
		float m_deltaTime = 0.0f;
		float m_totalTime = 0.0f;
		unsigned long long m_avgCount = 0;

		std::chrono::system_clock::time_point m_oldTime;
	private:
		friend class Display;
	
	};
}

