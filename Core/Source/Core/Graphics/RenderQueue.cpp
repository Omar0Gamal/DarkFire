#include "RenderQueue.h"
#include "GraphicsEngine.h"
#include "Core/Resource/Mesh.h"
#include "Core/Resource/Material.h"
#include "ConstantBuffer.h"
#include <algorithm>

namespace Core
{
	void RenderQueue::clear()
	{
		m_opaqueItems.clear();
		m_transparentItems.clear();
		m_debugItems.clear();
	}

	void RenderQueue::submitOpaque(const RenderItem& item)
	{
		m_opaqueItems.push_back(item);
	}

	void RenderQueue::submitTransparent(const RenderItem& item)
	{
		m_transparentItems.push_back(item);
	}

	void RenderQueue::submitDebug(const RenderItem& item)
	{
		m_debugItems.push_back(item);
	}

	void RenderQueue::sort()
	{
		// Opaque: Sort by material pointer first (batching shaders/textures), then front-to-back depth (early-Z)
		std::sort(m_opaqueItems.begin(), m_opaqueItems.end(),
			[](const RenderItem& a, const RenderItem& b)
			{
				if (a.material.get() != b.material.get())
				{
					return a.material.get() < b.material.get();
				}
				return a.depth < b.depth;
			});

		// Transparent: Sort strictly back-to-front by depth
		std::sort(m_transparentItems.begin(), m_transparentItems.end(),
			[](const RenderItem& a, const RenderItem& b)
			{
				return a.depth > b.depth;
			});
	}

	void RenderQueue::execute(GraphicsEngine* gfx, RenderStateManager* stateMgr)
	{
		if (!gfx || !stateMgr)
			return;

		sort();

		// -------------------------------------------------------------
		// 1. Render Opaque Bucket
		// -------------------------------------------------------------
		if (!m_opaqueItems.empty())
		{
			stateMgr->applyRasterizerState(RasterizerStateType::CullBack);
			stateMgr->applyBlendState(BlendStateType::Opaque);
			stateMgr->applyDepthStencilState(DepthStencilStateType::Default);

			Material* currentMat = nullptr;
			Mesh* currentMesh = nullptr;

			for (const auto& item : m_opaqueItems)
			{
				if (!item.mesh || !item.material)
					continue;

				if (item.material.get() != currentMat)
				{
					gfx->setMaterial(item.material);
					currentMat = item.material.get();
				}

				if (item.mesh.get() != currentMesh)
				{
					gfx->setVertexBuffer(item.mesh->getVertexBuffer());
					gfx->setIndexBuffer(item.mesh->getIndexBuffer());
					currentMesh = item.mesh.get();
				}

				if (item.materialSlotIndex < item.mesh->getNumMaterialSlots())
				{
					const MaterialSlot& slot = item.mesh->getMaterialSlot((UINT)item.materialSlotIndex);
					gfx->drawIndexedTriangleList((int)slot.num_indices, 0, (int)slot.start_index);
				}
			}
		}

		// -------------------------------------------------------------
		// 2. Render Transparent Bucket
		// -------------------------------------------------------------
		if (!m_transparentItems.empty())
		{
			stateMgr->applyBlendState(BlendStateType::AlphaBlend);
			stateMgr->applyDepthStencilState(DepthStencilStateType::ReadOnly);

			Material* currentMat = nullptr;
			Mesh* currentMesh = nullptr;

			for (const auto& item : m_transparentItems)
			{
				if (!item.mesh || !item.material)
					continue;

				if (item.material.get() != currentMat)
				{
					gfx->setMaterial(item.material);
					currentMat = item.material.get();
				}

				if (item.mesh.get() != currentMesh)
				{
					gfx->setVertexBuffer(item.mesh->getVertexBuffer());
					gfx->setIndexBuffer(item.mesh->getIndexBuffer());
					currentMesh = item.mesh.get();
				}

				if (item.materialSlotIndex < item.mesh->getNumMaterialSlots())
				{
					const MaterialSlot& slot = item.mesh->getMaterialSlot((UINT)item.materialSlotIndex);
					gfx->drawIndexedTriangleList((int)slot.num_indices, 0, (int)slot.start_index);
				}
			}
		}

		// -------------------------------------------------------------
		// 3. Render Debug Bucket (Wireframe)
		// -------------------------------------------------------------
		if (!m_debugItems.empty())
		{
			stateMgr->applyRasterizerState(RasterizerStateType::Wireframe);
			stateMgr->applyBlendState(BlendStateType::Opaque);
			stateMgr->applyDepthStencilState(DepthStencilStateType::Default);

			for (const auto& item : m_debugItems)
			{
				if (!item.mesh)
					continue;

				gfx->setVertexBuffer(item.mesh->getVertexBuffer());
				gfx->setIndexBuffer(item.mesh->getIndexBuffer());

				if (item.material)
					gfx->setMaterial(item.material);

				for (size_t s = 0; s < item.mesh->getNumMaterialSlots(); ++s)
				{
					const MaterialSlot& slot = item.mesh->getMaterialSlot((UINT)s);
					gfx->drawIndexedTriangleList((int)slot.num_indices, 0, (int)slot.start_index);
				}
			}
		}

		clear();
	}
}
