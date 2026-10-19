#pragma once
#include "RenderPass.h"
#include "RenderQueue.h"
#include "Core/Game/ECS.h"
#include "Core/Game/Component/TransformComponent.h"
#include "Core/Game/Component/MeshComponent.h"
#include "Core/Resource/Material.h"
#include "Core/Math/Vector3d.h"
#include "GraphicsEngine.h"

namespace Core
{
	/// Modern state-sorted and depth-culled render pass for 3D meshes
	class MeshRenderPass : public IRenderPass
	{
	public:
		MeshRenderPass() = default;
		virtual ~MeshRenderPass() = default;

		void execute(GraphicsEngine* gfx, ECS* ecs, const RenderContext& ctx) override
		{
			if (!gfx || !ecs)
				return;

			RenderStateManager* stateMgr = gfx->getRenderStateManager();
			if (!stateMgr)
				return;

			m_queue.clear();
			Vector3d camPos(ctx.cameraPosition.x, ctx.cameraPosition.y, ctx.cameraPosition.z);

			ecs->ForEach<TransformComponent, MeshComponent>(
				[&](TransformComponent& transform, MeshComponent& mesh)
				{
					if (!mesh.m_Mesh || mesh.m_Materials.empty())
						return;

					transform.proccessMatrix();
					float depth = Vector3d::distance(transform.m_Position, camPos);

					GraphicsData gdata;
					gdata.view = ctx.view;
					gdata.proj = ctx.projection;
					gdata.world = transform.m_worldMatrix;
					gdata.cameraPosition = ctx.cameraPosition;
					gdata.time = ctx.totalTime;
					gdata.numLights = static_cast<int>((std::min)(ctx.lights.size(), size_t(32)));
					for (int i = 0; i < gdata.numLights; ++i)
					{
						gdata.lights[i] = ctx.lights[i];
					}

					for (size_t m = 0; m < mesh.m_Materials.size(); ++m)
					{
						auto& material = mesh.m_Materials[m];
						if (!material)
							continue;

						material->setData(&gdata, sizeof(GraphicsData));

						RenderItem item;
						item.mesh = mesh.m_Mesh;
						item.materialSlotIndex = m;
						item.material = material;
						item.worldMatrix = transform.m_worldMatrix;
						item.depth = depth;

						if (material->getBlendMode() == BlendMode::AlphaBlend ||
							material->getBlendMode() == BlendMode::Additive)
						{
							m_queue.submitTransparent(item);
						}
						else
						{
							m_queue.submitOpaque(item);
						}
					}
				});

			m_queue.execute(gfx, stateMgr);
		}

		int priority() const override { return 100; }
		const char* name() const override { return "MeshRenderPass"; }

	private:
		RenderQueue m_queue;
	};
}
