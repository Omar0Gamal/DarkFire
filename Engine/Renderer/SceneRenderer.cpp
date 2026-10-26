#include "SceneRenderer.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/Runtime/EngineContext.h"
#include "Engine/Renderer/DebugRenderer.h"
#include <algorithm>
#include <array>
#include <cstring>
#include <d3d11.h>

namespace Core::Renderer
{
	void SceneRenderer::render(const RenderScene& scene, const RenderView& view)
	{
		if (view.width != m_width || view.height != m_height)
		{
			resize(view.width, view.height);
		}

		m_drawCallCount = 0;
		m_batchCount = 0;
		m_instanceCount = 0;
		m_uploadRing.reset();

		if (!m_context) return;

		// 1. Update PerFrame Constant Buffer with real elapsed time and frame index
		float dt = 0.016666f;
		m_totalTime += dt;
		m_frameIndex++;

		CBPerFrame frameData{};
		frameData.time = m_totalTime;
		frameData.deltaTime = dt;
		frameData.frameIndex = m_frameIndex;
		m_context->updateSubresource(m_cbFrame, &frameData, sizeof(frameData));
		m_context->setConstantBufferVS(DF_SLOT_CB_PER_FRAME, m_cbFrame);
		m_context->setConstantBufferPS(DF_SLOT_CB_PER_FRAME, m_cbFrame);
		m_context->setConstantBufferHS(DF_SLOT_CB_PER_FRAME, m_cbFrame);
		m_context->setConstantBufferDS(DF_SLOT_CB_PER_FRAME, m_cbFrame);

		// 2. Update PerView Constant Buffer with Halton(2, 3) 8-Phase Subpixel Jitter for TAA
		m_enableTAA = scene.environment.enableTAA;
		Math::Matrix proj = scene.camera.projMatrix;

		if (m_prevCameraViewMatrix == Math::Matrix::Identity)
		{
			m_isCameraMoving = false;
		}
		else
		{
			float diff = 0.0f;
			const float* m1 = reinterpret_cast<const float*>(&scene.camera.viewMatrix);
			const float* m2 = reinterpret_cast<const float*>(&m_prevCameraViewMatrix);
			for (int i = 0; i < 16; ++i) diff += std::abs(m1[i] - m2[i]);
			m_isCameraMoving = (diff > 1e-4f);
		}
		m_prevCameraViewMatrix = scene.camera.viewMatrix;

		if (m_enableTAA && view.width > 0 && view.height > 0)
		{
			uint32_t jitterPhase = (m_frameIndex % 8) + 1;
			float jx = halton(jitterPhase, 2) - 0.5f;
			float jy = halton(jitterPhase, 3) - 0.5f;
			m_jitterOffset = Math::Vector2(jx / static_cast<float>(view.width), jy / static_cast<float>(view.height));
			proj._31 += 2.0f * m_jitterOffset.x;
			proj._32 += -2.0f * m_jitterOffset.y;
		}
		else
		{
			m_jitterOffset = Math::Vector2(0.0f, 0.0f);
		}

		Math::Matrix viewProj = scene.camera.viewMatrix * proj;

		CBPerView viewData{};
		viewData.viewMatrix = scene.camera.viewMatrix;
		viewData.projMatrix = proj;
		viewData.viewProjMatrix = viewProj;
		viewData.invViewMatrix = scene.camera.invViewMatrix;
		viewData.invProjMatrix = proj.Invert();
		viewData.invViewProjMatrix = viewProj.Invert();
		viewData.cameraPosition = scene.camera.position;
		viewData.nearZ = scene.camera.nearZ;
		viewData.cameraForward = scene.camera.forward;
		viewData.farZ = scene.camera.farZ;
		viewData.viewportSize = Math::Vector2(static_cast<float>(view.width), static_cast<float>(view.height));
		viewData.invViewportSize = Math::Vector2(1.0f / view.width, 1.0f / view.height);
		viewData.prevViewProjMatrix = (m_prevViewProjMatrix == Math::Matrix::Identity) ? viewProj : m_prevViewProjMatrix;
		viewData.jitterOffset = m_jitterOffset;
		viewData.prevJitterOffset = m_prevJitterOffset;
		m_prevJitterOffset = m_jitterOffset;
		m_prevViewProjMatrix = viewProj;

		m_context->updateSubresource(m_cbView, &viewData, sizeof(viewData));
		m_context->setConstantBufferVS(DF_SLOT_CB_PER_VIEW, m_cbView);
		m_context->setConstantBufferPS(DF_SLOT_CB_PER_VIEW, m_cbView);
		m_context->setConstantBufferHS(DF_SLOT_CB_PER_VIEW, m_cbView);
		m_context->setConstantBufferDS(DF_SLOT_CB_PER_VIEW, m_cbView);
		m_context->setConstantBufferCS(DF_SLOT_CB_PER_VIEW, m_cbView);

		// 3. Update PerPass Constant Buffer
		CBPerPass passData{};
		passData.ambientLightColor = Math::Vector4(
			scene.environment.ambientColor.x,
			scene.environment.ambientColor.y,
			scene.environment.ambientColor.z,
			scene.environment.ambientIntensity
		);
		passData.skyZenithColor = scene.environment.skyColor;
		passData.skyHorizonColor = scene.environment.horizonColor;
		passData.skyGroundColor = scene.environment.groundColor;

		if (scene.environment.enableFog)
		{
			passData.fogColorDensity = Math::Vector4(
				scene.environment.fogColor.x,
				scene.environment.fogColor.y,
				scene.environment.fogColor.z,
				scene.environment.fogDensity
			);
			passData.fogParams = Math::Vector4(
				scene.environment.fogHeightFalloff,
				scene.environment.fogSunScattering,
				scene.environment.enablePCSS ? 1.0f : 0.0f,
				scene.environment.enableContactShadows ? 1.0f : 0.0f
			);
		}
		else
		{
			passData.fogColorDensity = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
			passData.fogParams = Math::Vector4(
				0.08f,
				0.35f,
				scene.environment.enablePCSS ? 1.0f : 0.0f,
				scene.environment.enableContactShadows ? 1.0f : 0.0f
			);
		}

		float tessFactor = std::clamp(scene.environment.tessellationFactor, 1.0f, 16.0f);
		passData.shGridMin = Math::Vector4(tessFactor, 2.0f, 28.0f, 0.45f);
		passData.shGridMax = Math::Vector4(
			scene.environment.iblBoxCenter.x,
			scene.environment.iblBoxCenter.y,
			scene.environment.iblBoxCenter.z,
			scene.environment.enableBoxProjectedIBL ? 1.0f : 0.0f
		);
		passData.probeBoxExtents = Math::Vector4(
			scene.environment.iblBoxExtents.x,
			scene.environment.iblBoxExtents.y,
			scene.environment.iblBoxExtents.z,
			scene.environment.iblBoxIntensity
		);

		passData.lightCount = static_cast<uint32_t>(std::min<size_t>(scene.lights.size(), 64));
		m_context->updateSubresource(m_cbPass, &passData, sizeof(passData));
		m_context->setConstantBufferPS(DF_SLOT_CB_PER_PASS, m_cbPass);
		m_context->setConstantBufferHS(DF_SLOT_CB_PER_PASS, m_cbPass);
		m_context->setConstantBufferDS(DF_SLOT_CB_PER_PASS, m_cbPass);

		// 4. Update Lights Buffer (Primary shadow caster placed at index 0)
		if (!scene.lights.empty() && m_bufLights)
		{
			std::vector<RenderLight> orderedLights = scene.lights;
			// Find primary shadow caster
			int primaryShadowIdx = -1;
			for (size_t i = 0; i < orderedLights.size(); ++i)
			{
				if (orderedLights[i].type == 0 && orderedLights[i].castShadows)
				{
					primaryShadowIdx = static_cast<int>(i);
					break;
				}
			}
			if (primaryShadowIdx < 0)
			{
				for (size_t i = 0; i < orderedLights.size(); ++i)
				{
					if (orderedLights[i].type == 2 && orderedLights[i].castShadows)
					{
						primaryShadowIdx = static_cast<int>(i);
						break;
					}
				}
			}
			if (primaryShadowIdx > 0)
			{
				std::swap(orderedLights[0], orderedLights[primaryShadowIdx]);
			}

			std::array<LightData, 64> lightArray{};
			for (uint32_t i = 0; i < passData.lightCount; ++i)
			{
				const auto& l = orderedLights[i];
				LightData ld{};
				ld.positionType = Math::Vector4(l.position.x, l.position.y, l.position.z, static_cast<float>(l.type));
				ld.directionRange = Math::Vector4(l.direction.x, l.direction.y, l.direction.z, l.range);
				ld.colorIntensity = Math::Vector4(l.color.x, l.color.y, l.color.z, l.intensity);
				ld.spotAnglesPad = Math::Vector4(std::cos(l.innerConeAngle), std::cos(l.outerConeAngle), 0.0f, 0.0f);
				lightArray[i] = ld;
			}
			m_context->updateSubresource(m_bufLights, lightArray.data(), sizeof(LightData) * 64);
		}

		// 5. Update Pre-Convolved Environment IBL Texture
		updateProceduralEnvIBL(scene);

		// 6. Update Clustered Dynamic Lighting (16x16x24 Clusters)
		updateClusters(scene, view);

		// 7. Execute Active Render Pipeline
		executePasses(scene, view);
	}

	float SceneRenderer::halton(uint32_t index, uint32_t base)
	{
		float f = 1.0f;
		float r = 0.0f;
		while (index > 0)
		{
			f /= static_cast<float>(base);
			r += f * static_cast<float>(index % base);
			index /= base;
		}
		return r;
	}

	void SceneRenderer::updateClusters(const RenderScene& scene, const RenderView& view)
	{
		if (!scene.environment.enableClusteredLighting) return;
		if (!m_bufClusterAABBs || !m_bufClusterGrid || !m_bufClusterLightIndices || !m_csClusterBuild.isValid() || !m_csClusterAssign.isValid())
		{
			return;
		}

		// 1. Rebuild Cluster AABBs if viewport dimensions or near/far planes changed
		bool needBuild = !m_clustersBuilt ||
			m_prevClusterWidth != view.width ||
			m_prevClusterHeight != view.height ||
			std::abs(m_prevClusterNearZ - scene.camera.nearZ) > 0.001f ||
			std::abs(m_prevClusterFarZ - scene.camera.farZ) > 0.001f;

		if (needBuild)
		{
			CBClusterBuild buildData{};
			buildData.invProjection = scene.camera.projMatrix.Invert();
			buildData.tileDimensions = Math::Vector2(static_cast<float>(view.width) / 16.0f, static_cast<float>(view.height) / 16.0f);
			buildData.viewportSize = Math::Vector2(static_cast<float>(view.width), static_cast<float>(view.height));
			buildData.nearZ = scene.camera.nearZ;
			buildData.farZ = scene.camera.farZ;

			m_csClusterBuild.setConstants(*m_context, buildData);
			m_csClusterBuild.setOutputBuffer(*m_context, 0, m_bufClusterAABBs);
			m_csClusterBuild.dispatchThreads(*m_context, 16, 16, 24, 16, 16, 1);

			m_clustersBuilt = true;
			m_prevClusterWidth = view.width;
			m_prevClusterHeight = view.height;
			m_prevClusterNearZ = scene.camera.nearZ;
			m_prevClusterFarZ = scene.camera.farZ;
		}

		// 2. Assign dynamic lights to clusters
		uint32_t zero = 0;
		if (m_bufClusterCounter)
		{
			m_context->updateSubresource(m_bufClusterCounter, &zero, sizeof(zero));
		}

		CBClusterAssign assignData{};
		assignData.viewMatrix = scene.camera.viewMatrix;
		assignData.lightCount = static_cast<uint32_t>(std::min<size_t>(scene.lights.size(), 64));
		assignData.maxIndices = 6144 * 64;

		m_csClusterAssign.setConstants(*m_context, assignData);
		m_csClusterAssign.setInputBuffer(*m_context, 0, m_bufClusterAABBs);
		m_csClusterAssign.setInputBuffer(*m_context, 1, m_bufLights);
		m_csClusterAssign.setOutputBuffer(*m_context, 0, m_bufClusterGrid);
		m_csClusterAssign.setOutputBuffer(*m_context, 1, m_bufClusterLightIndices);
		m_csClusterAssign.setOutputBuffer(*m_context, 2, m_bufClusterCounter);
		m_csClusterAssign.dispatchThreads(*m_context, 16, 16, 24, 8, 8, 4);

		// Unbind UAVs and bind cluster buffers to Pixel Shader slots
		m_context->setUnorderedAccessBufferCS(0, nullptr);
		m_context->setUnorderedAccessBufferCS(1, nullptr);
		m_context->setUnorderedAccessBufferCS(2, nullptr);

		m_context->setShaderResourceBufferPS(DF_SLOT_BUF_CLUSTER_GRID, m_bufClusterGrid);
		m_context->setShaderResourceBufferPS(DF_SLOT_BUF_CLUSTER_INDICES, m_bufClusterLightIndices);
	}

	uint32_t SceneRenderer::readEntityId(uint32_t x, uint32_t y) const
	{
		if (!m_entityIdTexture || x >= m_width || y >= m_height)
		{
			return 0;
		}

		auto* nativeDev = static_cast<ID3D11Device*>(m_device.getNativeDevice());
		auto* nativeCtx = static_cast<ID3D11DeviceContext*>(m_device.getNativeContext());
		if (!nativeDev || !nativeCtx)
		{
			return 0;
		}

		auto* staging = static_cast<ID3D11Texture2D*>(m_stagingEntityId);
		if (!staging)
		{
			D3D11_TEXTURE2D_DESC desc{};
			desc.Width = 1;
			desc.Height = 1;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R32_UINT;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_STAGING;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			ID3D11Texture2D* newTex = nullptr;
			HRESULT hr = nativeDev->CreateTexture2D(&desc, nullptr, &newTex);
			if (SUCCEEDED(hr) && newTex)
			{
				m_stagingEntityId = newTex;
				staging = newTex;
			}
		}

		if (!staging) return 0;

		D3D11_BOX srcBox{};
		srcBox.left = x;
		srcBox.right = x + 1;
		srcBox.top = y;
		srcBox.bottom = y + 1;
		srcBox.front = 0;
		srcBox.back = 1;

		auto* srcTex = static_cast<ID3D11Resource*>(m_entityIdTexture->getNativeHandle());
		if (!srcTex) return 0;

		nativeCtx->CopySubresourceRegion(staging, 0, 0, 0, 0, srcTex, 0, &srcBox);

		D3D11_MAPPED_SUBRESOURCE mapped{};
		HRESULT hr = nativeCtx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);
		if (FAILED(hr)) return 0;

		uint32_t id = *reinterpret_cast<const uint32_t*>(mapped.pData);
		nativeCtx->Unmap(staging, 0);
		return id;
	}

	Math::Ray SceneRenderer::unprojectRayReversedZ(
		float screenX,
		float screenY,
		float viewportWidth,
		float viewportHeight,
		const Math::Matrix& invViewProj) noexcept
	{
		// Reversed-Z NDC: Near plane is at Z = 1.0f, Far plane is at Z = 0.0f
		const float ndcX = (2.0f * screenX) / viewportWidth - 1.0f;
		const float ndcY = 1.0f - (2.0f * screenY) / viewportHeight;

		Math::Vector4 nearCS(ndcX, ndcY, 1.0f, 1.0f);
		Math::Vector4 farCS(ndcX, ndcY, 0.0f, 1.0f);

		Math::Vector4 nearWS = Math::Vector4::Transform(nearCS, invViewProj);
		Math::Vector4 farWS = Math::Vector4::Transform(farCS, invViewProj);

		if (std::abs(nearWS.w) > 0.00001f) nearWS /= nearWS.w;
		if (std::abs(farWS.w) > 0.00001f) farWS /= farWS.w;

		Math::Vector3 rayOrigin(nearWS.x, nearWS.y, nearWS.z);
		Math::Vector3 rayDir = Math::Vector3(farWS.x - nearWS.x, farWS.y - nearWS.y, farWS.z - nearWS.z);
		rayDir.Normalize();

		return Math::Ray(rayOrigin, rayDir);
	}
}
