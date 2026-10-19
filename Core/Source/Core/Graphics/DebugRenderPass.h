#pragma once
#include "RenderPass.h"
#include "DebugRenderer.h"
#include "GraphicsEngine.h"

namespace Core
{
	class DebugRenderPass : public IRenderPass
	{
	public:
		DebugRenderPass() = default;
		virtual ~DebugRenderPass() = default;

		void execute(GraphicsEngine* gfx, ECS* ecs, const RenderContext& ctx) override
		{
			if (DebugRenderer::get().isEnabled())
			{
				DebugRenderer::get().render(gfx, ctx);
			}
		}

		int priority() const override { return 250; }
		const char* name() const override { return "DebugRenderPass"; }
	};
}
