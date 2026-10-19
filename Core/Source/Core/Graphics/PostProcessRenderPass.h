#pragma once
#include "RenderPass.h"
#include "PostProcessPipeline.h"
#include "SwapChain.h"
#include "GraphicsEngine.h"

namespace Core
{
	/// Render pass that processes the HDR scene texture through ACES tonemapping,
	/// speed radial blur, bloom, and vignette, compositing the result to the SwapChain.
	class PostProcessRenderPass : public IRenderPass
	{
	public:
		PostProcessRenderPass(std::shared_ptr<PostProcessPipeline> pipeline, SwapChainPtr swapChain)
			: m_pipeline(pipeline), m_swapChain(swapChain)
		{
		}

		virtual ~PostProcessRenderPass() = default;

		void execute(GraphicsEngine* gfx, ECS* ecs, const RenderContext& ctx) override
		{
			if (!m_pipeline || !m_swapChain || !m_pipeline->isEnabled())
				return;

			ID3D11RenderTargetView* rtv = static_cast<ID3D11RenderTargetView*>(m_swapChain->getRenderTargetView());
			m_pipeline->renderPostProcess(rtv, ctx.viewportWidth, ctx.viewportHeight);
		}

		int priority() const override { return 300; }
		const char* name() const override { return "PostProcessRenderPass"; }

	private:
		std::shared_ptr<PostProcessPipeline> m_pipeline;
		SwapChainPtr m_swapChain;
	};
}
