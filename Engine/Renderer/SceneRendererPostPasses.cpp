#include "SceneRenderer.h"
#include "Engine/Core/Log/Log.h"
#include <algorithm>
#include <cmath>
#include <d3d11.h>

namespace Core::Renderer
{
	void SceneRenderer::passSSAO(const RenderScene& scene, const RenderView& view)
	{
		bool ssaoEnabled = scene.environment.enableSSAO;
		if (view.overrides.overrideSSAO)
		{
			ssaoEnabled = view.overrides.enableSSAO;
		}
		if (!ssaoEnabled || !m_psSSAO || !m_psSSAOBlur || !m_depthTexture || !m_ssaoRawTexture || !m_ssaoBlurredTexture)
		{
			if (m_ssaoRawTexture) m_context->clearRenderTarget(m_ssaoRawTexture, Color(1.0f, 1.0f, 1.0f, 1.0f));
			if (m_ssaoBlurredTexture) m_context->clearRenderTarget(m_ssaoBlurredTexture, Color(1.0f, 1.0f, 1.0f, 1.0f));
			return;
		}

		uint32_t fullW = m_ssaoRawTexture->getDesc().width;
		uint32_t fullH = m_ssaoRawTexture->getDesc().height;

		RHI::Viewport vp{};
		vp.x = 0.0f;
		vp.y = 0.0f;
		vp.width = static_cast<float>(fullW);
		vp.height = static_cast<float>(fullH);
		vp.minDepth = 0.0f;
		vp.maxDepth = 1.0f;
		m_context->setViewport(vp);

		// Step A: Generate raw SSAO
		m_context->setRenderTargets({ m_ssaoRawTexture }, nullptr);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		m_context->setInputLayout(nullptr);
		m_context->setVertexShader(m_vsFullscreen);
		m_context->setPixelShader(m_psSSAO);
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		m_context->setDepthStencilState(nullptr, 0);
		m_context->setBlendState(nullptr);

		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, m_depthTexture);
		if (m_samplerPointClamp) m_context->setSamplerPS(DF_SLOT_SMP_POINT_CLAMP, m_samplerPointClamp);

		CBSSAO cbSSAO{};
		cbSSAO.radius = scene.environment.ssaoRadius > 0.001f ? scene.environment.ssaoRadius : 0.5f;
		cbSSAO.bias = scene.environment.ssaoBias > 0.0001f ? scene.environment.ssaoBias : 0.025f;
		cbSSAO.intensity = scene.environment.ssaoIntensity > 0.001f ? scene.environment.ssaoIntensity : 1.0f;
		cbSSAO.power = 1.0f;
		cbSSAO.sampleCount = 16;
		cbSSAO.invResolution = Math::Vector2(1.0f / fullW, 1.0f / fullH);
		for (size_t i = 0; i < 16; ++i) cbSSAO.kernel[i] = m_ssaoKernel[i];

		if (m_cbSSAO)
		{
			m_context->updateSubresource(m_cbSSAO, &cbSSAO, sizeof(cbSSAO));
			m_context->setConstantBufferPS(DF_SLOT_CB_POST_PROCESS, m_cbSSAO);
		}

		m_context->draw(3, 0);
		m_drawCallCount++;
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, nullptr);

		// Step B: Bilateral Blur
		m_context->setRenderTargets({ m_ssaoBlurredTexture }, nullptr);
		m_context->setPixelShader(m_psSSAOBlur);
		m_context->setShaderResourcePS(0, m_ssaoRawTexture);
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, m_depthTexture);
		if (m_samplerLinearClamp) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_CLAMP, m_samplerLinearClamp);
		if (m_samplerPointClamp) m_context->setSamplerPS(DF_SLOT_SMP_POINT_CLAMP, m_samplerPointClamp);

		struct { Math::Vector2 texelSize; Math::Vector2 pad; } cbBlur{
			Math::Vector2(1.0f / fullW, 1.0f / fullH),
			Math::Vector2(0.0f, 0.0f)
		};
		if (m_cbSSAOBlur)
		{
			m_context->updateSubresource(m_cbSSAOBlur, &cbBlur, sizeof(cbBlur));
			m_context->setConstantBufferPS(DF_SLOT_CB_POST_PROCESS, m_cbSSAOBlur);
		}

		m_context->draw(3, 0);
		m_drawCallCount++;

		m_context->setShaderResourcePS(0, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, nullptr);
		m_context->setRenderTargets({}, nullptr);

		// Restore full viewport
		RHI::Viewport fullVp{};
		fullVp.x = 0.0f;
		fullVp.y = 0.0f;
		fullVp.width = static_cast<float>(m_width);
		fullVp.height = static_cast<float>(m_height);
		fullVp.minDepth = 0.0f;
		fullVp.maxDepth = 1.0f;
		m_context->setViewport(fullVp);
	}

	void SceneRenderer::passBloom(const RenderScene& scene)
	{
		if (!scene.environment.enableBloom || !m_psBloomPrefilter || !m_psBloomDownsample || !m_psBloomUpsample || !m_bloomDown[0])
		{
			if (m_bloomUp[0]) m_context->clearRenderTarget(m_bloomUp[0], Color(0.0f, 0.0f, 0.0f, 0.0f));
			return;
		}

		m_context->setRenderTargets({}, nullptr);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		m_context->setInputLayout(nullptr);
		m_context->setVertexShader(m_vsFullscreen);
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		m_context->setDepthStencilState(nullptr, 0);
		m_context->setBlendState(nullptr);
		if (m_samplerLinearClamp) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_CLAMP, m_samplerLinearClamp);

		CBBloom cbBloom{};
		cbBloom.threshold = scene.environment.bloomThreshold > 0.001f ? scene.environment.bloomThreshold : 1.0f;
		cbBloom.knee = 0.5f;
		cbBloom.scatter = 0.7f;
		cbBloom.intensity = scene.environment.bloomIntensity > 0.0001f ? scene.environment.bloomIntensity : 0.04f;
		cbBloom.tint = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

		// Step A: Prefilter & Downsample Mip 0 from HDR Texture
		{
			uint32_t w = m_bloomDown[0]->getDesc().width;
			uint32_t h = m_bloomDown[0]->getDesc().height;
			RHI::Viewport vp{ 0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f };
			m_context->setViewport(vp);

			m_context->setRenderTargets({ m_bloomDown[0] }, nullptr);
			m_context->setPixelShader(m_psBloomPrefilter);
			m_context->setShaderResourcePS(0, m_hdrTexture);

			cbBloom.texelSize = Math::Vector2(1.0f / w, 1.0f / h);
			cbBloom.mipLevel = 0.0f;
			if (m_cbBloom)
			{
				m_context->updateSubresource(m_cbBloom, &cbBloom, sizeof(cbBloom));
				m_context->setConstantBufferPS(DF_SLOT_CB_POST_PROCESS, m_cbBloom);
			}

			m_context->draw(3, 0);
			m_drawCallCount++;
			m_context->setShaderResourcePS(0, nullptr);
		}

		// Step B: Downsample Pyramid Mips 1..4
		for (uint32_t i = 1; i < 5; ++i)
		{
			uint32_t w = m_bloomDown[i]->getDesc().width;
			uint32_t h = m_bloomDown[i]->getDesc().height;
			RHI::Viewport vp{ 0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f };
			m_context->setViewport(vp);

			m_context->setRenderTargets({ m_bloomDown[i] }, nullptr);
			m_context->setPixelShader(m_psBloomDownsample);
			m_context->setShaderResourcePS(0, m_bloomDown[i - 1]);

			cbBloom.texelSize = Math::Vector2(1.0f / w, 1.0f / h);
			cbBloom.mipLevel = static_cast<float>(i);
			if (m_cbBloom)
			{
				m_context->updateSubresource(m_cbBloom, &cbBloom, sizeof(cbBloom));
				m_context->setConstantBufferPS(DF_SLOT_CB_POST_PROCESS, m_cbBloom);
			}

			m_context->draw(3, 0);
			m_drawCallCount++;
			m_context->setShaderResourcePS(0, nullptr);
		}

		// Step C: Upsample Pyramid Mips 3..0
		for (int32_t i = 3; i >= 0; --i)
		{
			uint32_t w = m_bloomUp[i]->getDesc().width;
			uint32_t h = m_bloomUp[i]->getDesc().height;
			RHI::Viewport vp{ 0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f };
			m_context->setViewport(vp);

			m_context->setRenderTargets({ m_bloomUp[i] }, nullptr);
			m_context->setPixelShader(m_psBloomUpsample);

			auto lowerMip = (i == 3) ? m_bloomDown[4] : m_bloomUp[i + 1];
			auto currentMip = m_bloomDown[i];
			m_context->setShaderResourcePS(0, lowerMip);
			m_context->setShaderResourcePS(1, currentMip);

			cbBloom.texelSize = Math::Vector2(1.0f / w, 1.0f / h);
			cbBloom.mipLevel = static_cast<float>(i);
			if (m_cbBloom)
			{
				m_context->updateSubresource(m_cbBloom, &cbBloom, sizeof(cbBloom));
				m_context->setConstantBufferPS(DF_SLOT_CB_POST_PROCESS, m_cbBloom);
			}

			m_context->draw(3, 0);
			m_drawCallCount++;
			m_context->setShaderResourcePS(0, nullptr);
			m_context->setShaderResourcePS(1, nullptr);
		}

		m_context->setRenderTargets({}, nullptr);

		// Restore full viewport
		RHI::Viewport fullVp{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
		m_context->setViewport(fullVp);
	}

	void SceneRenderer::passTonemap(const RenderScene& scene, const RenderView& view, const std::shared_ptr<RHI::IGpuTexture>& destination)
	{
		if (!destination) return;

		std::vector<std::shared_ptr<RHI::IGpuTexture>> rtvs = { destination };
		m_context->setRenderTargets(rtvs, nullptr);

		RHI::Viewport vp{ 0.0f, 0.0f, static_cast<float>(view.width), static_cast<float>(view.height), 0.0f, 1.0f };
		m_context->setViewport(vp);

		auto psToUse = m_psTonemap;
		if (m_vsFullscreen && psToUse)
		{
			m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
			m_context->setInputLayout(nullptr);
			m_context->setVertexShader(m_vsFullscreen);
			m_context->setPixelShader(psToUse);
			if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
			m_context->setDepthStencilState(nullptr, 0);
			m_context->setBlendState(nullptr);

			// Check for debug shadow map view (debugView == 3)
			auto srcTex = (scene.environment.debugView == 3 && m_shadowMapTexture) ? m_shadowMapTexture : m_hdrTexture;
			m_context->setShaderResourcePS(0, srcTex);

			if (m_shadowMapTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_SHADOW_MAP, m_shadowMapTexture);

			// Bind Bloom Texture on slot 10
			auto bloomTex = (scene.environment.enableBloom && m_bloomUp[0]) ? m_bloomUp[0] : m_blackTexture;
			if (bloomTex) m_context->setShaderResourcePS(DF_SLOT_TEX_BLOOM, bloomTex);

			// Bind SSAO Texture on slot 11
			auto ssaoTex = (scene.environment.enableSSAO && m_ssaoBlurredTexture) ? m_ssaoBlurredTexture : m_whiteTexture;
			if (ssaoTex) m_context->setShaderResourcePS(DF_SLOT_TEX_SSAO, ssaoTex);

			// Bind Debug View textures (slots 15, 18, 20, 21, 24)
			if (m_normalRoughnessTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_NORMAL_ROUGHNESS, m_normalRoughnessTexture);
			if (m_albedoMetallicTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO_METALLIC, m_albedoMetallicTexture);
			if (m_colorLutTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_COLOR_LUT, m_colorLutTexture);
			if (m_velocityTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_VELOCITY, m_velocityTexture);
			if (m_ssrTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_SSR, m_ssrTexture);

			if (m_samplerLinearClamp) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_CLAMP, m_samplerLinearClamp);
			if (m_samplerPointClamp) m_context->setSamplerPS(DF_SLOT_SMP_POINT_CLAMP, m_samplerPointClamp);

			CBPostProcess cbPP{};

			float targetExposure = scene.environment.exposure > 0.001f ? scene.environment.exposure : 1.0f;
			if (scene.environment.enableAutoExposure)
			{
				float sceneLuma = std::max(0.05f, scene.environment.ambientIntensity * 0.45f);
				for (const auto& l : scene.lights)
				{
					if (l.type == 0)
					{
						sceneLuma += std::max(0.0f, -l.direction.y) * l.intensity * 0.18f;
					}
				}
				float evOffset = std::clamp(std::log2(0.55f / sceneLuma), scene.environment.autoExposureMinEV, scene.environment.autoExposureMaxEV);
				targetExposure *= std::exp2(evOffset);
				float alpha = 1.0f - std::exp(-0.01666f * std::max(scene.environment.autoExposureSpeed, 0.1f));
				m_adaptedExposure = m_adaptedExposure + (targetExposure - m_adaptedExposure) * alpha;
				cbPP.exposure = m_adaptedExposure;
			}
			else
			{
				m_adaptedExposure = targetExposure;
				cbPP.exposure = targetExposure;
			}

			cbPP.tonemapper = scene.environment.enableTonemapping
				? static_cast<uint32_t>(scene.environment.tonemapper)
				: static_cast<uint32_t>(TonemapperType::Linear);
			cbPP.contrast = scene.environment.contrast;
			cbPP.saturation = scene.environment.saturation;
			cbPP.vignetteIntensity = scene.environment.vignetteIntensity;
			cbPP.vignetteSmoothness = 0.4f;
			cbPP.bloomIntensity = scene.environment.enableBloom ? scene.environment.bloomIntensity : 0.0f;
			cbPP.debugView = static_cast<uint32_t>(scene.environment.debugView);

			float temp = std::clamp(scene.environment.colorTemperature, -1.0f, 1.0f);
			cbPP.colorFilter = Math::Vector4(1.0f + temp * 0.14f, 1.0f + temp * 0.02f, 1.0f - temp * 0.16f, 1.0f);
			cbPP.bloomTint = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			cbPP.texelSize = Math::Vector2(
				1.0f / (view.width > 0 ? static_cast<float>(view.width) : 1.0f),
				1.0f / (view.height > 0 ? static_cast<float>(view.height) : 1.0f)
			);
			cbPP.gamma = scene.environment.gamma > 0.1f ? scene.environment.gamma : 2.2f;

			if (m_cbPostProcess)
			{
				m_context->updateSubresource(m_cbPostProcess, &cbPP, sizeof(cbPP));
				m_context->setConstantBufferPS(0, m_cbPostProcess);
			}

			m_context->draw(3, 0);
			m_drawCallCount++;

			m_context->setShaderResourcePS(0, nullptr);
			m_context->setShaderResourcePS(DF_SLOT_TEX_BLOOM, nullptr);
			m_context->setShaderResourcePS(DF_SLOT_TEX_SSAO, nullptr);
			m_context->setShaderResourcePS(DF_SLOT_TEX_NORMAL_ROUGHNESS, nullptr);
			m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO_METALLIC, nullptr);
			m_context->setShaderResourcePS(DF_SLOT_TEX_COLOR_LUT, nullptr);
			m_context->setShaderResourcePS(DF_SLOT_TEX_VELOCITY, nullptr);
			m_context->setShaderResourcePS(DF_SLOT_TEX_SSR, nullptr);
			m_context->setShaderResourcePS(DF_SLOT_TEX_SHADOW_MAP, nullptr);
		}
		else
		{
			m_context->clearRenderTarget(destination, Color(0.05f, 0.05f, 0.08f, 1.0f));
		}

		m_context->setRenderTargets({}, nullptr);
	}

	void SceneRenderer::passTAA(const RenderScene& scene, const RenderView& view)
	{
		(void)scene;
		if (!m_enableTAA || !m_psTAA || !m_hdrTexture || !m_velocityTexture || !m_depthTexture ||
			!m_taaHistory[0] || !m_taaHistory[1] || !m_vsFullscreen)
		{
			return;
		}

		uint32_t currentHistIdx = m_taaHistoryIndex;
		uint32_t prevHistIdx = 1 - m_taaHistoryIndex;
		auto currentTarget = m_taaHistory[currentHistIdx];
		auto prevHistory = m_taaHistory[prevHistIdx];

		if (!currentTarget || !prevHistory) return;

		std::vector<std::shared_ptr<RHI::IGpuTexture>> rtvs = { currentTarget };
		m_context->setRenderTargets(rtvs, nullptr);

		RHI::Viewport vp{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
		m_context->setViewport(vp);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		m_context->setInputLayout(nullptr);
		m_context->setVertexShader(m_vsFullscreen);
		m_context->setPixelShader(m_psTAA);
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		m_context->setDepthStencilState(nullptr, 0);
		m_context->setBlendState(nullptr);

		// Slot 0: Current HDR color
		m_context->setShaderResourcePS(0, m_hdrTexture);
		// Slot 1: Previous history color
		m_context->setShaderResourcePS(1, prevHistory);
		// Slot 2: Screen velocity
		m_context->setShaderResourcePS(2, m_velocityTexture);
		// Slot 3: Reversed-Z Depth
		m_context->setShaderResourcePS(3, m_depthTexture);

		if (m_samplerLinearClamp) m_context->setSamplerPS(0, m_samplerLinearClamp);
		if (m_samplerPointClamp) m_context->setSamplerPS(1, m_samplerPointClamp);

		CBTAA cbTAA{};
		cbTAA.jitterAndBlend = Math::Vector4(m_jitterOffset.x, m_jitterOffset.y, 0.85f, m_isCameraMoving ? 1.0f : 0.0f);
		cbTAA.resolution = Math::Vector4(
			static_cast<float>(m_width),
			static_cast<float>(m_height),
			1.0f / (m_width > 0 ? static_cast<float>(m_width) : 1.0f),
			1.0f / (m_height > 0 ? static_cast<float>(m_height) : 1.0f)
		);

		if (m_cbTAA)
		{
			m_context->updateSubresource(m_cbTAA, &cbTAA, sizeof(cbTAA));
			m_context->setConstantBufferPS(0, m_cbTAA);
		}
		if (m_cbView)
		{
			m_context->setConstantBufferPS(DF_SLOT_CB_PER_VIEW, m_cbView);
		}

		m_context->draw(3, 0);
		m_drawCallCount++;

		// Unbind textures
		m_context->setShaderResourcePS(0, nullptr);
		m_context->setShaderResourcePS(1, nullptr);
		m_context->setShaderResourcePS(2, nullptr);
		m_context->setShaderResourcePS(3, nullptr);
		m_context->setRenderTargets({}, nullptr);

		// Advance ping-pong history index
		m_taaHistoryIndex = 1 - m_taaHistoryIndex;

		// Copy current resolved history to m_hdrTexture so all downstream passes (Bloom, Tonemap) use the pristine TAA color
		auto* nativeCtx = static_cast<ID3D11DeviceContext*>(m_device.getNativeContext());
		if (nativeCtx != nullptr && currentTarget != nullptr && m_hdrTexture != nullptr)
		{
			auto* srcRes = static_cast<ID3D11Resource*>(currentTarget->getNativeHandle());
			auto* dstRes = static_cast<ID3D11Resource*>(m_hdrTexture->getNativeHandle());
			if (srcRes != nullptr && dstRes != nullptr)
			{
				nativeCtx->CopyResource(dstRes, srcRes);
			}
		}
	}

	void SceneRenderer::passFXAA(const RenderScene& scene, const std::shared_ptr<RHI::IGpuTexture>& source, const std::shared_ptr<RHI::IGpuTexture>& destination)
	{
		if (!source || !destination || !m_vsFullscreen || !m_psFXAA) return;

		std::vector<std::shared_ptr<RHI::IGpuTexture>> rtvs = { destination };
		m_context->setRenderTargets(rtvs, nullptr);

		RHI::Viewport vp{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
		m_context->setViewport(vp);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		m_context->setInputLayout(nullptr);
		m_context->setVertexShader(m_vsFullscreen);
		m_context->setPixelShader(m_psFXAA);
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		m_context->setDepthStencilState(nullptr, 0);
		m_context->setBlendState(nullptr);

		m_context->setShaderResourcePS(0, source);
		if (m_samplerLinearClamp) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_CLAMP, m_samplerLinearClamp);

		CBFXAA cbFXAA{};
		cbFXAA.texelSize = Math::Vector2(1.0f / m_width, 1.0f / m_height);
		cbFXAA.subpix = scene.environment.fxaaSubpix > 0.001f ? scene.environment.fxaaSubpix : 0.75f;
		cbFXAA.edgeThreshold = scene.environment.fxaaEdgeThreshold > 0.001f ? scene.environment.fxaaEdgeThreshold : 0.166f;
		cbFXAA.edgeThresholdMin = scene.environment.fxaaEdgeThresholdMin > 0.001f ? scene.environment.fxaaEdgeThresholdMin : 0.0833f;
		cbFXAA.pad = Math::Vector3(scene.environment.casSharpening, 0.0f, 0.0f);
		if (m_cbFXAA)
		{
			m_context->updateSubresource(m_cbFXAA, &cbFXAA, sizeof(cbFXAA));
			m_context->setConstantBufferPS(0, m_cbFXAA);
		}

		m_context->draw(3, 0);
		m_drawCallCount++;

		m_context->setShaderResourcePS(0, nullptr);
		m_context->setRenderTargets({}, nullptr);
	}

	void SceneRenderer::passPostProcess(const RenderScene& scene, const RenderView& view)
	{
		std::shared_ptr<RHI::IGpuTexture> finalTarget = m_finalLdrTexture;
		if (view.targetTexture)
		{
			finalTarget = view.targetTexture;
		}
		else if (view.swapChain)
		{
			finalTarget = view.swapChain->getBackBufferTexture();
		}

		if (!finalTarget) return;

		m_context->clearRenderTarget(finalTarget, Color(0.05f, 0.05f, 0.08f, 1.0f));

		bool fxaaEnabled = scene.environment.enableFXAA;
		if (m_enableTAA)
		{
			// Never run FXAA on top of TAA: FXAA destroys resolved subpixel details and turns image to mud
			fxaaEnabled = false;
		}
		if (view.overrides.overrideFXAA)
		{
			fxaaEnabled = view.overrides.enableFXAA;
		}
		if (!m_psFXAA || !m_ldrIntermediateTexture)
		{
			fxaaEnabled = false;
		}

		auto tonemapDest = fxaaEnabled ? m_ldrIntermediateTexture : finalTarget;
		passTonemap(scene, view, tonemapDest);

		if (fxaaEnabled)
		{
			passFXAA(scene, m_ldrIntermediateTexture, finalTarget);
		}

		// Store completed HDR frame for next frame's temporal SSR raymarching
		auto* nativeCtx = static_cast<ID3D11DeviceContext*>(m_device.getNativeContext());
		if (nativeCtx != nullptr && m_prevHdrTexture != nullptr && m_hdrTexture != nullptr)
		{
			auto* srcRes = static_cast<ID3D11Resource*>(m_hdrTexture->getNativeHandle());
			auto* dstRes = static_cast<ID3D11Resource*>(m_prevHdrTexture->getNativeHandle());
			if (srcRes != nullptr && dstRes != nullptr)
			{
				nativeCtx->CopyResource(dstRes, srcRes);
			}
		}
	}

	void SceneRenderer::passMotionBlur(const RenderScene& scene, const RenderView& view)
	{
		if (!scene.environment.enableMotionBlur || !m_motionBlurTexture || !m_tileMaxTexture || !m_neighborMaxTexture || !m_velocityTexture || !m_hdrTexture || !m_psMotionBlur)
		{
			return;
		}

		uint32_t tileW = m_tileMaxTexture->getDesc().width;
		uint32_t tileH = m_tileMaxTexture->getDesc().height;

		// 1. TileMax reduction pass (20x20 thread group)
		struct { uint32_t screenW, screenH, tileW, tileH; } cbTile{ m_width, m_height, tileW, tileH };
		m_csTileMax.setConstants(*m_context, cbTile);
		m_csTileMax.setInput(*m_context, 0, m_velocityTexture);
		m_csTileMax.setOutput(*m_context, 0, m_tileMaxTexture);
		m_csTileMax.dispatchGroups(*m_context, tileW, tileH, 1);

		m_csTileMax.setOutput(*m_context, 0, nullptr);
		m_csTileMax.setInput(*m_context, 0, nullptr);

		// 2. NeighborMax 3x3 filter pass (8x8 thread group)
		struct { uint32_t tileCountX, tileCountY, pad0, pad1; } cbNeighbor{ tileW, tileH, 0, 0 };
		m_csNeighborMax.setConstants(*m_context, cbNeighbor);
		m_csNeighborMax.setInput(*m_context, 0, m_tileMaxTexture);
		m_csNeighborMax.setOutput(*m_context, 0, m_neighborMaxTexture);
		m_csNeighborMax.dispatchThreads(*m_context, tileW, tileH, 1, 8, 8, 1);

		m_csNeighborMax.setOutput(*m_context, 0, nullptr);
		m_csNeighborMax.setInput(*m_context, 0, nullptr);

		// 3. Motion blur reconstruction pixel shader pass
		m_context->setRenderTargets({ m_motionBlurTexture }, nullptr);

		RHI::Viewport vp{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
		m_context->setViewport(vp);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		m_context->setInputLayout(nullptr);
		m_context->setVertexShader(m_vsFullscreen);
		m_context->setPixelShader(m_psMotionBlur);
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		m_context->setDepthStencilState(nullptr, 0);
		m_context->setBlendState(nullptr);

		m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO, m_hdrTexture);
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, m_depthTexture);
		m_context->setShaderResourcePS(DF_SLOT_TEX_VELOCITY, m_velocityTexture);
		m_context->setShaderResourcePS(DF_SLOT_TEX_NEIGHBOR_MAX, m_neighborMaxTexture);

		if (m_samplerLinearClamp) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_CLAMP, m_samplerLinearClamp);
		if (m_samplerPointClamp) m_context->setSamplerPS(DF_SLOT_SMP_POINT_CLAMP, m_samplerPointClamp);

		CBMotionBlur cbMB{};
		cbMB.screenResolution = Math::Vector4(static_cast<float>(m_width), static_cast<float>(m_height), 1.0f / m_width, 1.0f / m_height);
		cbMB.motionBlurParams = Math::Vector4(
			scene.environment.motionBlurShutterAngle > 0.001f ? scene.environment.motionBlurShutterAngle : 0.5f,
			scene.environment.motionBlurMaxRadius > 1.0f ? scene.environment.motionBlurMaxRadius : 32.0f,
			0.0f,
			1.0f
		);

		if (m_cbMotionBlur)
		{
			m_context->updateSubresource(m_cbMotionBlur, &cbMB, sizeof(cbMB));
			m_context->setConstantBufferPS(DF_SLOT_CB_MOTION_BLUR, m_cbMotionBlur);
		}

		m_context->draw(3, 0);
		m_drawCallCount++;

		m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_VELOCITY, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_NEIGHBOR_MAX, nullptr);
		m_context->setRenderTargets({}, nullptr);

		auto* nativeCtx = static_cast<ID3D11DeviceContext*>(m_device.getNativeContext());
		if (nativeCtx != nullptr && m_motionBlurTexture != nullptr && m_hdrTexture != nullptr)
		{
			auto* srcRes = static_cast<ID3D11Resource*>(m_motionBlurTexture->getNativeHandle());
			auto* dstRes = static_cast<ID3D11Resource*>(m_hdrTexture->getNativeHandle());
			if (srcRes != nullptr && dstRes != nullptr)
			{
				nativeCtx->CopyResource(dstRes, srcRes);
			}
		}
	}

	void SceneRenderer::passAutoExposure(const RenderScene& scene, const RenderView& view)
	{
		if (!scene.environment.enableAutoExposure || !m_hdrTexture || !m_bufLuminanceHistogram || !m_bufAdaptedLuminance || !m_csLuminanceHistogram.isValid() || !m_csLuminanceAverage.isValid())
		{
			return;
		}

		// 1. Clear 64-bin histogram buffer with zeros
		uint32_t zeros[64] = { 0 };
		m_context->updateSubresource(m_bufLuminanceHistogram, zeros, sizeof(zeros));

		// 2. Dispatch LuminanceHistogramCS
		CBLuminanceHistogram cbHist{};
		cbHist.dimensions = { m_width, m_height, 0, 0 };
		float minEV = scene.environment.autoExposureMinEV;
		float maxEV = scene.environment.autoExposureMaxEV;
		if (maxEV - minEV < 0.1f) maxEV = minEV + 1.0f;
		cbHist.evRange = Math::Vector4(minEV, maxEV, 1.0f / (maxEV - minEV), 0.0f);

		m_csLuminanceHistogram.setConstants(*m_context, cbHist);
		m_csLuminanceHistogram.setInput(*m_context, 0, m_hdrTexture);
		m_csLuminanceHistogram.setOutputBuffer(*m_context, 0, m_bufLuminanceHistogram);
		m_csLuminanceHistogram.dispatchThreads(*m_context, m_width, m_height, 1, 16, 16, 1);

		m_csLuminanceHistogram.setOutputBuffer(*m_context, 0, nullptr);
		m_csLuminanceHistogram.setInput(*m_context, 0, nullptr);

		// 3. Dispatch LuminanceAverageCS
		CBLuminanceAverage cbAvg{};
		cbAvg.evRange = Math::Vector4(minEV, maxEV, 0.05f, 20.0f);
		float dt = 0.01666f;
		float speedUp = scene.environment.autoExposureSpeedUp > 0.01f ? scene.environment.autoExposureSpeedUp : 3.0f;
		float speedDown = scene.environment.autoExposureSpeedDown > 0.01f ? scene.environment.autoExposureSpeedDown : 1.0f;
		float middleGrey = 0.18f;
		cbAvg.adaptationParams = Math::Vector4(dt, speedUp, speedDown, middleGrey);
		cbAvg.pixelCount = { m_width * m_height, 0, 0, 0 };

		m_csLuminanceAverage.setConstants(*m_context, cbAvg);
		m_csLuminanceAverage.setInputBuffer(*m_context, 0, m_bufLuminanceHistogram);
		m_csLuminanceAverage.setOutputBuffer(*m_context, 0, m_bufAdaptedLuminance);
		m_csLuminanceAverage.dispatchGroups(*m_context, 1, 1, 1);

		m_csLuminanceAverage.setOutputBuffer(*m_context, 0, nullptr);
		m_csLuminanceAverage.setInputBuffer(*m_context, 0, nullptr);
	}
}
