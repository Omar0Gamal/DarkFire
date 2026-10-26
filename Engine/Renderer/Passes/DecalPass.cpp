#include "Engine/Renderer/SceneRenderer.h"
#include "Engine/Core/Log/Log.h"
#include <d3d11.h>

namespace Core::Renderer
{
	// Dual-Pipeline (Forward+ & Deferred) OBB Projected Decal Pass
	void SceneRenderer::passDecals(const RenderScene& scene, bool isDeferred)
	{
		// Early-out if no decals in scene
		if (scene.decals.empty())
		{
			return;
		}

		// Depth buffer check (m_depthSRV / m_depthTexture)
		auto depthTex = m_depthTexture;
		if (!depthTex)
		{
			return;
		}

		// Obtain unit cube mesh [-0.5, 0.5]^3
		const auto cubeMesh = getOrCreateMesh("builtin:cube");
		if (!cubeMesh.vertexBuffer || !cubeMesh.indexBuffer || cubeMesh.indexCount == 0)
		{
			return;
		}

		// Select RTV for SV_Target0:
		// Deferred: m_gbufferAlbedoRTV (m_albedoMetallicTexture)
		// Forward+: m_hdrSceneRTV (m_hdrTexture)
		auto targetRTV = isDeferred ? m_albedoMetallicTexture : m_hdrTexture;
		if (!targetRTV)
		{
			return;
		}

		// Bind single target with nullptr DSV so depthSRV can be bound to PS slot 8 without D3D11 resource hazard
		m_context->setRenderTargets({ targetRTV }, nullptr);

		RHI::Viewport vp{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
		m_context->setViewport(vp);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		if (m_inputLayout) m_context->setInputLayout(m_inputLayout);
		if (m_vsDecal) m_context->setVertexShader(m_vsDecal);
		else if (m_vsStandard) m_context->setVertexShader(m_vsStandard);
		if (m_psDecal) m_context->setPixelShader(m_psDecal);

		// Rasterizer state: cull none allows decals to be projected from inside the box
		if (m_rsCullNone) m_context->setRasterizerState(m_rsCullNone);
		else if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);

		m_context->setDepthStencilState(nullptr, 0); // No hardware depth test since DSV is nullptr
		if (m_bsAlphaBlend) m_context->setBlendState(m_bsAlphaBlend);

		if (m_samplerLinearWrap) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_WRAP, m_samplerLinearWrap);
		if (m_samplerPointClamp) m_context->setSamplerPS(DF_SLOT_SMP_POINT_CLAMP, m_samplerPointClamp);

		// Bind depth SRV at PS slot 8
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, depthTex);

		m_context->setVertexBuffer(0, cubeMesh.vertexBuffer, sizeof(VertexMesh), 0);
		m_context->setIndexBuffer(cubeMesh.indexBuffer, RHI::PixelFormat::R32_UINT, 0);

		for (const auto& decal : scene.decals)
		{
			// CBPerObject: worldMatrix = decal.world, normalMatrix = decal.invWorld (shader invWorldMatrix)
			CBPerObject obj{};
			obj.worldMatrix = decal.world;
			obj.invWorldMatrix = decal.invWorld;
			obj.entityId = decal.entityId;

			auto alloc = m_uploadRing.allocate(sizeof(CBPerObject), &obj);
			if (alloc.isValid())
			{
				m_context->setConstantBufferSubrangeVS(DF_SLOT_CB_PER_OBJECT, alloc.buffer, alloc.firstConstant16, alloc.numConstants16);
				m_context->setConstantBufferSubrangePS(DF_SLOT_CB_PER_OBJECT, alloc.buffer, alloc.firstConstant16, alloc.numConstants16);
			}

			// CBPerMaterial: baseColorFactor = decal.color, roughnessFactor = decal.roughness, normalScale = decal.normalStrength
			CBPerMaterial mat{};
			mat.baseColorFactor = decal.color;
			mat.roughnessFactor = decal.roughness;
			mat.normalScale = decal.normalStrength;
			mat.tilingOffset = Math::Vector4(1.0f, 1.0f, 0.0f, 0.0f);

			if (m_cbMaterial)
			{
				m_context->updateSubresource(m_cbMaterial, &mat, sizeof(mat));
				m_context->setConstantBufferPS(DF_SLOT_CB_PER_MATERIAL, m_cbMaterial);
			}

			// Bind decal albedo texture or white texture fallback
			auto albedoTex = decal.albedoTexturePath.empty() ? m_whiteTexture : getOrCreateTexture(decal.albedoTexturePath, true);
			if (!albedoTex) albedoTex = m_whiteTexture;
			if (albedoTex)
			{
				m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO, albedoTex);
			}

			m_context->drawIndexed(cubeMesh.indexCount, 0, 0);
			m_drawCallCount++;
		}

		// Unbind PS slot 8 when done
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, nullptr);
	}
}
