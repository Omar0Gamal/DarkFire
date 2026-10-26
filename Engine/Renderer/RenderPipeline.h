#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Renderer/RenderScene.h"
#include <functional>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace Core::Renderer
{
	class SceneRenderer;

	enum class RenderPipelineType : uint8_t
	{
		ForwardPlus = 0,
		Deferred = 1,
		Custom = 2
	};

	class DF_API IRenderPass
	{
	public:
		virtual ~IRenderPass() = default;
		[[nodiscard]] virtual std::string_view getName() const noexcept = 0;
		[[nodiscard]] virtual bool isEnabled() const noexcept { return m_enabled; }
		virtual void setEnabled(bool enabled) noexcept { m_enabled = enabled; }
		virtual void execute(SceneRenderer& renderer, const RenderScene& scene, const RenderView& view) = 0;

	protected:
		bool m_enabled = true;
	};

	class DF_API LambdaRenderPass final : public IRenderPass
	{
	public:
		using ExecuteFn = std::function<void(SceneRenderer&, const RenderScene&, const RenderView&)>;

		LambdaRenderPass(std::string name, ExecuteFn fn, bool enabled = true)
			: m_name(std::move(name)), m_fn(std::move(fn))
		{
			m_enabled = enabled;
		}

		[[nodiscard]] std::string_view getName() const noexcept override { return m_name; }

		void execute(SceneRenderer& renderer, const RenderScene& scene, const RenderView& view) override
		{
			if (m_enabled && m_fn)
			{
				m_fn(renderer, scene, view);
			}
		}

	private:
		std::string m_name;
		ExecuteFn m_fn;
	};

	class DF_API RenderPipeline
	{
	public:
		RenderPipeline() = default;
		explicit RenderPipeline(std::string name) : m_name(std::move(name)) {}
		explicit RenderPipeline(RenderPipelineType type)
			: m_type(type)
			, m_name(type == RenderPipelineType::Deferred ? "Deferred" : (type == RenderPipelineType::Custom ? "Custom" : "Forward+"))
		{
		}

		RenderPipeline(const RenderPipeline&) = delete;
		RenderPipeline& operator=(const RenderPipeline&) = delete;
		RenderPipeline(RenderPipeline&&) noexcept = default;
		RenderPipeline& operator=(RenderPipeline&&) noexcept = default;

		RenderPipeline& addPass(std::unique_ptr<IRenderPass> pass)
		{
			if (pass)
			{
				m_passes.push_back(std::move(pass));
			}
			return *this;
		}

		RenderPipeline& addLambdaPass(std::string name, LambdaRenderPass::ExecuteFn fn, bool enabled = true)
		{
			m_passes.push_back(std::make_unique<LambdaRenderPass>(std::move(name), std::move(fn), enabled));
			return *this;
		}

		void clear() noexcept
		{
			m_passes.clear();
		}

		void clearPasses() noexcept
		{
			m_passes.clear();
		}

		void execute(SceneRenderer& renderer, const RenderScene& scene, const RenderView& view)
		{
			for (auto& pass : m_passes)
			{
				if (pass && pass->isEnabled())
				{
					pass->execute(renderer, scene, view);
				}
			}
		}

		[[nodiscard]] std::string_view getName() const noexcept { return m_name; }
		void setName(std::string name) noexcept { m_name = std::move(name); }

		[[nodiscard]] const std::vector<std::unique_ptr<IRenderPass>>& getPasses() const noexcept { return m_passes; }
		[[nodiscard]] std::vector<std::unique_ptr<IRenderPass>>& getPasses() noexcept { return m_passes; }

	private:
		RenderPipelineType m_type = RenderPipelineType::ForwardPlus;
		std::string m_name = "Forward+";
		std::vector<std::unique_ptr<IRenderPass>> m_passes;
	};
}
