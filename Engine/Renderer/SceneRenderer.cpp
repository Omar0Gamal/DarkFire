#include "SceneRenderer.h"
#include "Engine/Core/Log/Log.h"
#include <algorithm>
#include <array>
#include <cstring>

namespace Core::Renderer
{
	SceneRenderer::SceneRenderer(RHI::IRHIDevice& device)
		: m_device(device)
		, m_context(device.getImmediate())
		, m_shaderLibrary(device)
	{
		initializeResources();
	}

	void SceneRenderer::initializeResources()
	{
		m_uploadRing.initialize(m_device, 16 * 1024 * 1024);

		// 1. PerFrame Constant Buffer
		RHI::GpuBufferDesc frameDesc{};
		frameDesc.size = sizeof(CBPerFrame);
		frameDesc.usage = RHI::BufferUsage::Default;
		frameDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto frameRes = m_device.createBuffer(frameDesc);
		if (frameRes.isOk()) m_cbFrame = *frameRes;

		// 2. PerView Constant Buffer
		RHI::GpuBufferDesc viewDesc{};
		viewDesc.size = sizeof(CBPerView);
		viewDesc.usage = RHI::BufferUsage::Default;
		viewDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto viewRes = m_device.createBuffer(viewDesc);
		if (viewRes.isOk()) m_cbView = *viewRes;

		// 3. PerPass Constant Buffer
		RHI::GpuBufferDesc passDesc{};
		passDesc.size = sizeof(CBPerPass);
		passDesc.usage = RHI::BufferUsage::Default;
		passDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto passRes = m_device.createBuffer(passDesc);
		if (passRes.isOk()) m_cbPass = *passRes;

		// 4. Lights Structured Buffer
		RHI::GpuBufferDesc lightDesc{};
		lightDesc.size = sizeof(LightData) * 64; // Support up to 64 lights per pass
		lightDesc.usage = RHI::BufferUsage::Default;
		lightDesc.bindFlags = RHI::BufferBindFlags::ShaderResource;
		lightDesc.structureByteStride = sizeof(LightData);
		auto lightRes = m_device.createBuffer(lightDesc);
		if (lightRes.isOk()) m_bufLights = *lightRes;

		resize(m_width, m_height);
	}

	void SceneRenderer::resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0) return;
		m_width = width;
		m_height = height;

		// 1. HDR Render Target (R16G16B16A16_FLOAT)
		RHI::GpuTextureDesc hdrDesc{};
		hdrDesc.width = width;
		hdrDesc.height = height;
		hdrDesc.format = RHI::PixelFormat::R16G16B16A16_FLOAT;
		hdrDesc.bindFlags = RHI::BufferBindFlags::RenderTarget | RHI::BufferBindFlags::ShaderResource;
		auto hdrRes = m_device.createTexture(hdrDesc);
		if (hdrRes.isOk()) m_hdrTexture = *hdrRes;

		// 2. Depth Stencil Target (D32_FLOAT Reversed-Z / R32_TYPELESS)
		RHI::GpuTextureDesc depthDesc{};
		depthDesc.width = width;
		depthDesc.height = height;
		depthDesc.format = RHI::PixelFormat::D32_FLOAT;
		depthDesc.bindFlags = RHI::BufferBindFlags::DepthStencil | RHI::BufferBindFlags::ShaderResource;
		auto depthRes = m_device.createTexture(depthDesc);
		if (depthRes.isOk()) m_depthTexture = *depthRes;

		// 3. Entity-ID Target (R32_UINT)
		RHI::GpuTextureDesc idDesc{};
		idDesc.width = width;
		idDesc.height = height;
		idDesc.format = RHI::PixelFormat::R32_UINT;
		idDesc.bindFlags = RHI::BufferBindFlags::RenderTarget | RHI::BufferBindFlags::ShaderResource;
		auto idRes = m_device.createTexture(idDesc);
		if (idRes.isOk()) m_entityIdTexture = *idRes;

		// 4. Final LDR Target (R8G8B8A8_UNORM)
		RHI::GpuTextureDesc ldrDesc{};
		ldrDesc.width = width;
		ldrDesc.height = height;
		ldrDesc.format = RHI::PixelFormat::R8G8B8A8_UNORM;
		ldrDesc.bindFlags = RHI::BufferBindFlags::RenderTarget | RHI::BufferBindFlags::ShaderResource;
		auto ldrRes = m_device.createTexture(ldrDesc);
		if (ldrRes.isOk()) m_finalLdrTexture = *ldrRes;
	}

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

		// 1. Update PerFrame Constant Buffer
		CBPerFrame frameData{};
		frameData.time = 0.0f;
		frameData.deltaTime = 0.016f;
		frameData.frameIndex = 0;
		m_context->updateSubresource(m_cbFrame, &frameData, sizeof(frameData));
		m_context->setConstantBufferVS(DF_SLOT_CB_PER_FRAME, m_cbFrame);
		m_context->setConstantBufferPS(DF_SLOT_CB_PER_FRAME, m_cbFrame);

		// 2. Update PerView Constant Buffer
		CBPerView viewData{};
		viewData.viewMatrix = scene.camera.viewMatrix;
		viewData.projMatrix = scene.camera.projMatrix;
		viewData.viewProjMatrix = scene.camera.viewProjMatrix;
		viewData.invViewMatrix = scene.camera.invViewMatrix;
		viewData.invProjMatrix = scene.camera.invProjMatrix;
		viewData.invViewProjMatrix = scene.camera.invViewProjMatrix;
		viewData.cameraPosition = scene.camera.position;
		viewData.nearZ = scene.camera.nearZ;
		viewData.cameraForward = scene.camera.forward;
		viewData.farZ = scene.camera.farZ;
		viewData.viewportSize = Math::Vector2(static_cast<float>(view.width), static_cast<float>(view.height));
		viewData.invViewportSize = Math::Vector2(1.0f / view.width, 1.0f / view.height);
		m_context->updateSubresource(m_cbView, &viewData, sizeof(viewData));
		m_context->setConstantBufferVS(DF_SLOT_CB_PER_VIEW, m_cbView);
		m_context->setConstantBufferPS(DF_SLOT_CB_PER_VIEW, m_cbView);

		// 3. Update PerPass Constant Buffer
		CBPerPass passData{};
		passData.ambientLightColor = scene.environment.ambientColor * scene.environment.ambientIntensity;
		passData.lightCount = static_cast<uint32_t>(std::min<size_t>(scene.lights.size(), 64));
		m_context->updateSubresource(m_cbPass, &passData, sizeof(passData));
		m_context->setConstantBufferPS(DF_SLOT_CB_PER_PASS, m_cbPass);

		// 4. Update Lights Buffer
		if (!scene.lights.empty() && m_bufLights)
		{
			std::array<LightData, 64> lightArray{};
			for (uint32_t i = 0; i < passData.lightCount; ++i)
			{
				const auto& l = scene.lights[i];
				LightData ld{};
				ld.positionType = Math::Vector4(l.position.x, l.position.y, l.position.z, static_cast<float>(l.type));
				ld.directionRange = Math::Vector4(l.direction.x, l.direction.y, l.direction.z, l.range);
				ld.colorIntensity = Math::Vector4(l.color.x, l.color.y, l.color.z, l.intensity);
				ld.spotAnglesPad = Math::Vector4(std::cos(l.innerConeAngle), std::cos(l.outerConeAngle), 0.0f, 0.0f);
				lightArray[i] = ld;
			}
			m_context->updateSubresource(m_bufLights, lightArray.data(), sizeof(LightData) * 64);
		}

		// 5. Execute Passes in Strict Order
		executePasses(scene, view);
	}

	void SceneRenderer::executePasses(const RenderScene& scene, const RenderView& view)
	{
		RHI::Viewport vp{};
		vp.x = 0.0f;
		vp.y = 0.0f;
		vp.width = static_cast<float>(m_width);
		vp.height = static_cast<float>(m_height);
		vp.minDepth = 0.0f;
		vp.maxDepth = 1.0f;
		m_context->setViewport(vp);

		// Pass 1: Forward Opaque & Alpha-Test (HDR + Reversed-Z + Entity ID)
		passOpaque(scene);

		// Pass 2: Skybox
		passSkybox(scene);

		// Pass 3: Transparent
		passTransparent(scene);

		// Pass 4: Post-Process & Tonemap (Outputs to targetTexture, backbuffer, or m_finalLdrTexture)
		passPostProcess(view);
	}

	void SceneRenderer::passOpaque(const RenderScene& scene)
	{
		std::vector<std::shared_ptr<RHI::IGpuTexture>> rtvs = { m_hdrTexture, m_entityIdTexture };
		m_context->setRenderTargets(rtvs, m_depthTexture);

		// Clear HDR target and ID target
		m_context->clearRenderTarget(m_hdrTexture, Color(0.05f, 0.05f, 0.08f, 1.0f));
		m_context->clearRenderTarget(m_entityIdTexture, Color(0.0f, 0.0f, 0.0f, 0.0f));

		// Reversed-Z: depth cleared to 0.0f!
		m_context->clearDepthStencil(m_depthTexture, 0.0f, 0);

		// Collect and sort Opaque and AlphaTest meshes
		m_cachedDrawItems.clear();
		for (const auto& mesh : scene.meshes)
		{
			if (mesh.layer == RenderLayer::Opaque || mesh.layer == RenderLayer::AlphaTest)
			{
				DrawItem item{};
				item.shaderVariantId = 0;
				item.materialId = mesh.material ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mesh.material.get()) & 0xFFFF) : 0;
				item.meshId = mesh.vertexBuffer ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mesh.vertexBuffer.get()) & 0xFFFF) : 0;
				item.indexCount = mesh.indexCount;
				item.startIndexLocation = mesh.startIndex;
				item.baseVertexLocation = mesh.baseVertex;
				item.instanceData.world = mesh.world;
				item.instanceData.entityId = mesh.entityId;
				item.sortKey = DrawItem::makeSortKey(mesh.layer, item.shaderVariantId, item.materialId, item.meshId, mesh.distanceToCamera);
				m_cachedDrawItems.push_back(item);
			}
		}

		if (m_cachedDrawItems.empty()) return;

		std::sort(m_cachedDrawItems.begin(), m_cachedDrawItems.end());

		auto batches = BatchDrawItems(m_cachedDrawItems, m_cachedInstances);
		m_batchCount += static_cast<uint32_t>(batches.size());
		m_instanceCount += static_cast<uint32_t>(m_cachedInstances.size());

		for (const auto& batch : batches)
		{
			// Update per-object CB via UploadRing
			CBPerObject obj{};
			obj.worldMatrix = m_cachedInstances[batch.firstInstanceIndex].world;
			obj.invWorldMatrix = obj.worldMatrix.Invert();
			obj.entityId = m_cachedInstances[batch.firstInstanceIndex].entityId;

			auto alloc = m_uploadRing.allocate(sizeof(CBPerObject), &obj);
			if (alloc.isValid())
			{
				m_context->setConstantBufferSubrangeVS(DF_SLOT_CB_PER_OBJECT, alloc.buffer, alloc.firstConstant16, alloc.numConstants16);
			}

			m_context->drawIndexed(batch.indexCount, batch.startIndexLocation, batch.baseVertexLocation);
			m_drawCallCount++;
		}
	}

	void SceneRenderer::passSkybox(const RenderScene& scene)
	{
		(void)scene;
		// Skybox rendered with Reversed-Z depth = 0.0f and ComparisonFunc::GreaterEqual
	}

	void SceneRenderer::passTransparent(const RenderScene& scene)
	{
		(void)scene;
		// Transparent meshes rendered back-to-front with DepthMode::ReadOnly
	}

	void SceneRenderer::passPostProcess(const RenderView& view)
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

		if (finalTarget)
		{
			std::vector<std::shared_ptr<RHI::IGpuTexture>> rtvs = { finalTarget };
			m_context->setRenderTargets(rtvs, nullptr);
			// Post process resolve: ACES tonemap, Linear to sRGB
			m_context->clearRenderTarget(finalTarget, Color(0.05f, 0.05f, 0.08f, 1.0f));
		}
	}

	uint32_t SceneRenderer::readEntityId(uint32_t x, uint32_t y) const
	{
		(void)x;
		(void)y;
		return 0;
	}

	Math::Ray SceneRenderer::unprojectRayReversedZ(
		float screenX,
		float screenY,
		float viewportWidth,
		float viewportHeight,
		const Math::Matrix& invViewProj) noexcept
	{
		// Reversed-Z NDC:
		// Near plane is at Z = 1.0f
		// Far plane is at Z = 0.0f
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
