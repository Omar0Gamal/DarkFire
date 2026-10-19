#pragma once

#include "Engine/Runtime/Layer.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Renderer/RenderScene.h"
#include "Engine/Core/Scripting/ScriptRegistry.h"
#include <memory>
#include <filesystem>

namespace Runtime
{
	class DF_API SceneRuntimeLayer : public Layer
	{
	public:
		explicit SceneRuntimeLayer(std::shared_ptr<Core::Scene> scene = nullptr);
		~SceneRuntimeLayer() override;

		void onAttach() override;
		void onDetach() override;
		void onFixedUpdate(float fixedDeltaTime) override;
		void onUpdate(float deltaTime) override;

		bool loadScene(const std::filesystem::path& scenePath);
		void setScene(std::shared_ptr<Core::Scene> scene);
		std::shared_ptr<Core::Scene> getScene() const noexcept { return m_scene; }

	private:
		void startPendingScripts();

		std::shared_ptr<Core::Scene> m_scene;
		Core::Renderer::RenderScene m_renderScene;
	};
}
