#pragma once
#include "RenderPass.h"
#include "RenderQueue.h"
#include "Core/Game/ECS.h"
#include "Core/Game/Component/TransformComponent.h"
#include "Core/Game/Component/MeshComponent.h"
#include "GraphicsEngine.h"
#include <unordered_map>
#include <vector>

namespace Core
{
	struct InstanceTransformData
	{
		Mat4x4 world;
		Vector4d colorTint;
	};

	class InstancedMeshRenderPass : public IRenderPass
	{
	public:
		InstancedMeshRenderPass() = default;
		virtual ~InstancedMeshRenderPass() = default;

		void execute(GraphicsEngine* gfx, ECS* ecs, const RenderContext& ctx) override
		{
			if (!gfx || !ecs)
				return;

			RenderStateManager* stateMgr = gfx->getRenderStateManager();
			if (!stateMgr)
				return;

			// Group instances by Mesh and Material for batch rendering
			struct BatchKey
			{
				MeshPtr mesh;
				MaterialPtr material;
				bool operator==(const BatchKey& other) const
				{
					return mesh == other.mesh && material == other.material;
				}
			};

			struct BatchKeyHash
			{
				size_t operator()(const BatchKey& k) const
				{
					return reinterpret_cast<size_t>(k.mesh.get()) ^ (reinterpret_cast<size_t>(k.material.get()) << 1);
				}
			};

			std::unordered_map<BatchKey, std::vector<Mat4x4>, BatchKeyHash> batches;

			ecs->ForEach<TransformComponent, MeshComponent>(
				[&](TransformComponent& transform, MeshComponent& mesh)
				{
					if (!mesh.m_Mesh || mesh.m_Materials.empty())
						return;

					transform.proccessMatrix();
					for (auto& mat : mesh.m_Materials)
					{
						if (mat)
						{
							BatchKey key = { mesh.m_Mesh, mat };
							batches[key].push_back(transform.m_worldMatrix);
						}
					}
				});

			// Render batches
			stateMgr->applyRasterizerState(RasterizerStateType::CullBack);
			stateMgr->applyBlendState(BlendStateType::Opaque);
			stateMgr->applyDepthStencilState(DepthStencilStateType::Default);

			for (auto& [key, transforms] : batches)
			{
				if (!key.mesh || !key.material || transforms.empty())
					continue;

				// Bind mesh and material once per batch
				gfx->setMaterial(key.material);
				gfx->setVertexBuffer(key.mesh->getVertexBuffer());
				gfx->setIndexBuffer(key.mesh->getIndexBuffer());

				for (const auto& world : transforms)
				{
					GraphicsData gdata;
					gdata.view = ctx.view;
					gdata.proj = ctx.projection;
					gdata.world = world;
					gdata.cameraPosition = ctx.cameraPosition;
					gdata.time = ctx.totalTime;
					gdata.numLights = static_cast<int>((std::min)(ctx.lights.size(), size_t(32)));
					for (int i = 0; i < gdata.numLights; ++i)
					{
						gdata.lights[i] = ctx.lights[i];
					}

					key.material->setData(&gdata, sizeof(GraphicsData));

					for (size_t s = 0; s < key.mesh->getNumMaterialSlots(); ++s)
					{
						const MaterialSlot& slot = key.mesh->getMaterialSlot((UINT)s);
						gfx->drawIndexedTriangleList((int)slot.num_indices, 0, (int)slot.start_index);
					}
				}
			}
		}

		int priority() const override { return 105; }
		const char* name() const override { return "InstancedMeshRenderPass"; }
	};
}
