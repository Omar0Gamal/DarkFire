#include "Engine/Renderer/SceneRenderer.h"
#include "Engine/Core/Log/Log.h"
#include <algorithm>
#include <cmath>
#include <d3d11.h>

namespace Core::Renderer
{
	void SceneRenderer::passHiZBuild()
	{
		if (!m_depthTexture || !m_hiZTexture || !m_csHiZBuild.isValid())
		{
			return;
		}

		uint32_t width = m_hiZTexture->getDesc().width;
		uint32_t height = m_hiZTexture->getDesc().height;

		Math::Vector4 params(
			static_cast<float>(width),
			static_cast<float>(height),
			1.0f / static_cast<float>(width),
			1.0f / static_cast<float>(height)
		);

		m_csHiZBuild.setConstants(*m_context, params);
		m_csHiZBuild.setInput(*m_context, 0, m_depthTexture);
		m_csHiZBuild.setOutput(*m_context, 0, m_hiZTexture);
		m_csHiZBuild.dispatchThreads(*m_context, width, height, 1, 8, 8, 1);

		m_context->setUnorderedAccessViewCS(0, nullptr);
		m_context->setShaderResourceCS(0, nullptr);
	}

	void SceneRenderer::passSSR(const RenderScene& scene, const RenderView& view)
	{
		if (!m_ssrTexture) return;

		bool ssrActive = scene.environment.enableSSR;
		if (!ssrActive || !m_psSSR || !m_depthTexture || !m_normalRoughnessTexture || !m_albedoMetallicTexture)
		{
			m_context->clearRenderTarget(m_ssrTexture, Color(0.0f, 0.0f, 0.0f, 0.0f));
			return;
		}

		m_context->clearRenderTarget(m_ssrTexture, Color(0.0f, 0.0f, 0.0f, 0.0f));
		m_context->setRenderTargets({ m_ssrTexture }, nullptr);

		RHI::Viewport vp{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
		m_context->setViewport(vp);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		m_context->setInputLayout(nullptr);
		m_context->setVertexShader(m_vsFullscreen);
		m_context->setPixelShader(m_psSSR);
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		m_context->setDepthStencilState(nullptr, 0);
		m_context->setBlendState(nullptr);

		CBSSR cbSSR{};
		cbSSR.ssrParams = Math::Vector4(
			scene.environment.ssrMaxDistance > 0.1f ? scene.environment.ssrMaxDistance : 25.0f,
			scene.environment.ssrThickness > 0.01f ? scene.environment.ssrThickness : 0.5f,
			scene.environment.ssrRoughnessCutoff > 0.01f ? scene.environment.ssrRoughnessCutoff : 0.65f,
			scene.environment.ssrIntensity > 0.0f ? scene.environment.ssrIntensity : 1.0f
		);
		cbSSR.ssrExtra = Math::Vector4(1.0f, static_cast<float>(scene.environment.ssrStepCount > 0 ? scene.environment.ssrStepCount : 32), 0.0f, 0.0f);

		if (m_cbSSR)
		{
			m_context->updateSubresource(m_cbSSR, &cbSSR, sizeof(cbSSR));
			m_context->setConstantBufferPS(DF_SLOT_CB_SSR, m_cbSSR);
		}
		if (m_cbView)
		{
			m_context->setConstantBufferPS(DF_SLOT_CB_PER_VIEW, m_cbView);
		}

		// Slot 0: Scene color buffer to reflect (using previous frame HDR history for temporal stability)
		auto colorSource = m_prevHdrTexture ? m_prevHdrTexture : m_hdrTexture;
		m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO, colorSource);
		// Slot 9: Reversed-Z Scene Depth
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, m_depthTexture);
		// Slot 15: Normal + Roughness
		m_context->setShaderResourcePS(DF_SLOT_TEX_NORMAL_ROUGHNESS, m_normalRoughnessTexture);
		// Slot 16: Albedo + Metallic
		m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO_METALLIC, m_albedoMetallicTexture);
		// Slot 21: Screen-space Velocity for motion reprojection
		if (m_velocityTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_VELOCITY, m_velocityTexture);

		if (m_samplerLinearClamp) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_CLAMP, m_samplerLinearClamp);
		if (m_samplerPointClamp) m_context->setSamplerPS(DF_SLOT_SMP_POINT_CLAMP, m_samplerPointClamp);

		m_context->draw(3, 0);
		m_drawCallCount++;

		m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_NORMAL_ROUGHNESS, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO_METALLIC, nullptr);
		if (m_velocityTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_VELOCITY, nullptr);
		m_context->setRenderTargets({}, nullptr);
	}

	void SceneRenderer::passDDGI(const RenderScene& scene)
	{
		if (!scene.environment.enableDDGI || !m_ddgiRadianceAtlas || !m_ddgiDepthAtlas || !m_csDDGIRadianceGather.isValid() || !m_csDDGIDepthGather.isValid())
		{
			return;
		}

		CBDDGI cbDDGI{};
		cbDDGI.gridOriginSpacing = Math::Vector4(
			scene.environment.ddgiOrigin.x,
			scene.environment.ddgiOrigin.y,
			scene.environment.ddgiOrigin.z,
			scene.environment.ddgiProbeSpacing > 0.1f ? scene.environment.ddgiProbeSpacing : 2.0f
		);
		cbDDGI.gridCountsExtents = Math::Vector4(
			scene.environment.ddgiProbeCounts.x,
			scene.environment.ddgiProbeCounts.y,
			scene.environment.ddgiProbeCounts.z,
			scene.environment.ddgiNormalBias > 0.001f ? scene.environment.ddgiNormalBias : 0.15f
		);
		cbDDGI.ddgiParams = Math::Vector4(
			scene.environment.ddgiEnergyPreservation > 0.0f ? scene.environment.ddgiEnergyPreservation : 0.85f,
			scene.environment.ddgiHysteresis > 0.0f ? scene.environment.ddgiHysteresis : 0.95f,
			scene.environment.ddgiIntensity > 0.0f ? scene.environment.ddgiIntensity : 1.0f,
			1.0f
		);

		if (m_cbDDGI)
		{
			m_context->updateSubresource(m_cbDDGI, &cbDDGI, sizeof(cbDDGI));
			m_context->setConstantBufferCS(DF_SLOT_CB_DDGI, m_cbDDGI);
		}
		if (m_cbPass)
		{
			m_context->setConstantBufferCS(DF_SLOT_CB_PER_PASS, m_cbPass);
		}
		if (m_cbView)
		{
			m_context->setConstantBufferCS(DF_SLOT_CB_PER_VIEW, m_cbView);
		}

		uint32_t totalProbes = static_cast<uint32_t>(scene.environment.ddgiProbeCounts.x * scene.environment.ddgiProbeCounts.y * scene.environment.ddgiProbeCounts.z);
		if (totalProbes == 0) return;

		// 1. Radiance gather pass (8x8 threads per probe)
		m_csDDGIRadianceGather.setOutput(*m_context, 0, m_ddgiRadianceAtlas);
		m_csDDGIRadianceGather.dispatchGroups(*m_context, 1, 1, totalProbes);
		m_context->setUnorderedAccessViewCS(0, nullptr);

		// 2. Depth gather pass (16x16 threads per probe)
		m_csDDGIDepthGather.setOutput(*m_context, 0, m_ddgiDepthAtlas);
		m_csDDGIDepthGather.dispatchGroups(*m_context, 1, 1, totalProbes);
		m_context->setUnorderedAccessViewCS(0, nullptr);
	}

	void SceneRenderer::passSubsurface(const RenderScene& scene)
	{
		if (!scene.environment.enableSSS || !m_hdrTexture || !m_sssIntermediateTexture || !m_psSubsurface || !m_depthTexture)
		{
			return;
		}

		RHI::Viewport vp{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
		m_context->setViewport(vp);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		m_context->setInputLayout(nullptr);
		m_context->setVertexShader(m_vsFullscreen);
		m_context->setPixelShader(m_psSubsurface);
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		m_context->setDepthStencilState(nullptr, 0);
		m_context->setBlendState(nullptr);

		if (m_samplerLinearClamp) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_CLAMP, m_samplerLinearClamp);
		if (m_samplerPointClamp) m_context->setSamplerPS(DF_SLOT_SMP_POINT_CLAMP, m_samplerPointClamp);
		if (m_cbView) m_context->setConstantBufferPS(DF_SLOT_CB_PER_VIEW, m_cbView);

		// Pass A: Horizontal blur (direction = 1, 0)
		m_context->setRenderTargets({ m_sssIntermediateTexture }, nullptr);
		CBSSS cbSSS{};
		cbSSS.sssParams = Math::Vector4(scene.environment.sssRadius > 0.01f ? scene.environment.sssRadius : 1.5f, scene.environment.sssMaxDepthDiff > 0.001f ? scene.environment.sssMaxDepthDiff : 0.2f, 0.0f, 0.0f);
		cbSSS.sssDirection = Math::Vector4(1.0f, 0.0f, 0.0f, 0.0f);
		if (m_cbSSS)
		{
			m_context->updateSubresource(m_cbSSS, &cbSSS, sizeof(cbSSS));
			m_context->setConstantBufferPS(DF_SLOT_CB_SSS, m_cbSSS);
		}
		m_context->setShaderResourcePS(0, m_hdrTexture);
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, m_depthTexture);
		m_context->draw(3, 0);
		m_drawCallCount++;

		m_context->setShaderResourcePS(0, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, nullptr);

		// Pass B: Vertical blur (direction = 0, 1)
		m_context->setRenderTargets({ m_hdrTexture }, nullptr);
		cbSSS.sssDirection = Math::Vector4(0.0f, 1.0f, 0.0f, 0.0f);
		if (m_cbSSS)
		{
			m_context->updateSubresource(m_cbSSS, &cbSSS, sizeof(cbSSS));
			m_context->setConstantBufferPS(DF_SLOT_CB_SSS, m_cbSSS);
		}
		m_context->setShaderResourcePS(0, m_sssIntermediateTexture);
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, m_depthTexture);
		m_context->draw(3, 0);
		m_drawCallCount++;

		m_context->setShaderResourcePS(0, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, nullptr);
		m_context->setRenderTargets({}, nullptr);
	}
}
