#include "Engine/Runtime/SceneRuntimeLayer.h"
#include "Engine/Runtime/Application.h"
#include "Engine/Core/Scene/SceneRendererBridge.h"
#include "Engine/Core/Serialization/SceneSerializer.h"
#include "Engine/Core/Scripting/ScriptableEntity.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Math/Math.h"

namespace Runtime
{
	SceneRuntimeLayer::SceneRuntimeLayer(std::shared_ptr<Core::Scene> scene)
		: Layer("SceneRuntimeLayer")
		, m_scene(std::move(scene))
	{
		if (!m_scene)
		{
			m_scene = std::make_shared<Core::Scene>();
		}
	}

	SceneRuntimeLayer::~SceneRuntimeLayer()
	{
		onDetach();
	}

	void SceneRuntimeLayer::onAttach()
	{
		startPendingScripts();
	}

	void SceneRuntimeLayer::onDetach()
	{
		if (m_scene)
		{
			auto view = m_scene->view<Core::ScriptsComponent>();
			for (auto entity : view)
			{
				auto& sc = view.get<Core::ScriptsComponent>(entity);
				for (auto& s : sc.scripts)
				{
					if (s.isStarted && s.instance)
					{
						s.instance->onDestroy();
						s.isStarted = false;
					}
				}
			}
		}
	}

	void SceneRuntimeLayer::setScene(std::shared_ptr<Core::Scene> scene)
	{
		onDetach();
		m_scene = std::move(scene);
		if (m_scene)
		{
			startPendingScripts();
		}
	}

	bool SceneRuntimeLayer::loadScene(const std::filesystem::path& scenePath)
	{
		auto newScene = std::make_shared<Core::Scene>();
		Core::SceneSerializer serializer(newScene.get());

		// 1. Try loading from physical disk
		if (std::filesystem::exists(scenePath))
		{
			if (serializer.deserializeFromFile(scenePath))
			{
				setScene(std::move(newScene));
				return true;
			}
		}

		// 2. Try loading from VFS / Pak
		auto& vfs = Core::EngineContext::Get().vfs;
		std::string vpath = scenePath.string();
		std::replace(vpath.begin(), vpath.end(), '\\', '/');
		if (!vpath.empty() && vpath.front() != '/')
		{
			vpath = "/" + vpath;
		}

		auto textRes = vfs.readTextFile(vpath);
		if (textRes.isErr())
		{
			textRes = vfs.readTextFile("/Scenes/" + scenePath.filename().string());
		}

		if (textRes.isOk())
		{
			if (serializer.deserializeFromString(textRes.value()))
			{
				setScene(std::move(newScene));
				return true;
			}
		}

		Core::Log::Get().error("Runtime", "Failed to deserialize scene from: {}", scenePath.string());
		return false;
	}

	void SceneRuntimeLayer::startPendingScripts()
	{
		if (!m_scene)
		{
			return;
		}

		auto view = m_scene->view<Core::ScriptsComponent>();
		for (auto entityHandle : view)
		{
			Core::Entity entity(entityHandle, m_scene.get());
			auto& sc = view.get<Core::ScriptsComponent>(entityHandle);
			for (auto& s : sc.scripts)
			{
				if (!s.instance)
				{
					s.instance = Core::ScriptRegistry::Get().instantiate(s.scriptName);
					if (s.instance)
					{
						s.instance->bind(entity, m_scene.get());
					}
				}

				if (s.instance && !s.isStarted)
				{
					s.instance->onCreate();
					if (s.isEnabled)
					{
						s.instance->onStart();
					}
					s.isStarted = true;
				}
			}
		}
	}

	void SceneRuntimeLayer::onFixedUpdate(float fixedDeltaTime)
	{
		if (!m_scene)
		{
			return;
		}

		startPendingScripts();

		// 1. Physics Step
		Application::Get().getPhysicsWorld().step(*m_scene, fixedDeltaTime);

		// 2. FixedUpdate scripts
		auto view = m_scene->view<Core::ScriptsComponent>();
		for (auto entityHandle : view)
		{
			auto& sc = view.get<Core::ScriptsComponent>(entityHandle);
			for (auto& s : sc.scripts)
			{
				if (s.isEnabled && s.isStarted && s.instance)
				{
					s.instance->onFixedUpdate(fixedDeltaTime);
				}
			}
		}
	}

	void SceneRuntimeLayer::onUpdate(float deltaTime)
	{
		if (!m_scene)
		{
			return;
		}

		startPendingScripts();

		// 1. Script onUpdate
		auto view = m_scene->view<Core::ScriptsComponent>();
		for (auto entityHandle : view)
		{
			auto& sc = view.get<Core::ScriptsComponent>(entityHandle);
			for (auto& s : sc.scripts)
			{
				if (s.isEnabled && s.isStarted && s.instance)
				{
					s.instance->onUpdate(deltaTime);
				}
			}
		}

		// 2. Script onLateUpdate
		for (auto entityHandle : view)
		{
			auto& sc = view.get<Core::ScriptsComponent>(entityHandle);
			for (auto& s : sc.scripts)
			{
				if (s.isEnabled && s.isStarted && s.instance)
				{
					s.instance->onLateUpdate(deltaTime);
				}
			}
		}

		// 3. Update transforms
		m_scene->updateTransforms();

		// 4. Render Snapshot & Extraction with window aspect
		float aspect = Application::Get().getWindow().getAspectRatio();
		Core::ExtractRenderScene(*m_scene, m_renderScene, aspect);
		m_renderScene.camera.aspect = aspect;
		m_renderScene.camera.projMatrix = (m_renderScene.camera.isOrthographic)
			? Core::Math::orthographicLH_ReversedZ(10.0f, 10.0f / aspect, m_renderScene.camera.nearZ, m_renderScene.camera.farZ)
			: Core::Math::perspectiveFovLH_ReversedZ(m_renderScene.camera.fov, aspect, m_renderScene.camera.nearZ, m_renderScene.camera.farZ);
		m_renderScene.camera.viewProjMatrix = m_renderScene.camera.viewMatrix * m_renderScene.camera.projMatrix;
		m_renderScene.camera.invProjMatrix = m_renderScene.camera.projMatrix.Invert();
		m_renderScene.camera.invViewProjMatrix = m_renderScene.camera.viewProjMatrix.Invert();

		Core::Renderer::RenderView renderView;
		renderView.width = Application::Get().getWindow().getWidth();
		renderView.height = Application::Get().getWindow().getHeight();
		renderView.swapChain = Application::Get().getSwapChainShared();
		renderView.isEditorView = false;

		Application::Get().getRenderer().render(m_renderScene, renderView);
	}
}
