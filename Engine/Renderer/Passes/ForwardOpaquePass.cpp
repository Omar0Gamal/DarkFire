#include "Engine/Renderer/SceneRenderer.h"
#include "Engine/Core/Log/Log.h"
#include <algorithm>
#include <cmath>
#include <d3d11.h>

namespace Core::Renderer
{
	void SceneRenderer::passDepthPrepass(const RenderScene& scene)
	{
		if (!m_depthTexture) return;

		// Reversed-Z: clear depth to 0.0f
		m_context->clearDepthStencil(m_depthTexture, 0.0f, 0);

		// Bind depth-only render target
		m_context->setRenderTargets({}, m_depthTexture);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		if (m_inputLayout) m_context->setInputLayout(m_inputLayout);
		if (m_vsStandard) m_context->setVertexShader(m_vsStandard);
		m_context->setPixelShader(nullptr); // Hardware fast depth-only
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		if (m_dssDepthPrepass) m_context->setDepthStencilState(m_dssDepthPrepass, 0);
		m_context->setBlendState(nullptr);
		if (m_samplerLinearWrap) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_WRAP, m_samplerLinearWrap);
		if (m_samplerAnisoWrap) m_context->setSamplerPS(DF_SLOT_SMP_ANISO_WRAP, m_samplerAnisoWrap);

		for (const auto& mesh : scene.meshes)
		{
			if (mesh.layer != RenderLayer::Opaque && mesh.layer != RenderLayer::AlphaTest) continue;

			const auto gpuMesh = getOrCreateMesh(mesh.meshPath);
			if (!gpuMesh.vertexBuffer || !gpuMesh.indexBuffer || gpuMesh.indexCount == 0) continue;

			m_context->setVertexBuffer(0, gpuMesh.vertexBuffer, sizeof(VertexMesh), 0);
			m_context->setIndexBuffer(gpuMesh.indexBuffer, RHI::PixelFormat::R32_UINT, 0);

			CBPerObject obj{};
			obj.worldMatrix = mesh.world;
			obj.invWorldMatrix = mesh.world.Invert();
			obj.entityId = mesh.entityId;

			auto alloc = m_uploadRing.allocate(sizeof(CBPerObject), &obj);
			if (alloc.isValid())
			{
				m_context->setConstantBufferSubrangeVS(DF_SLOT_CB_PER_OBJECT, alloc.buffer, alloc.firstConstant16, alloc.numConstants16);
			}

			uint32_t totalIdxCount = 0;
			const auto& submeshes = selectLodSubmeshes(gpuMesh, mesh.distanceToCamera, scene.environment.meshLodBias, totalIdxCount);

			if (!submeshes.empty())
			{
				Math::Matrix worldViewProj = mesh.world * scene.camera.viewProjMatrix;

				for (const auto& sub : submeshes)
				{
					if (sub.indexCount == 0) continue;

					DirectX::XMFLOAT3 corners[8];
					sub.localBounds.GetCorners(corners);
					bool allNegX = true, allPosX = true;
					bool allNegY = true, allPosY = true;
					bool allNegZ = true, allPosZ = true;

					for (int i = 0; i < 8; ++i)
					{
						Math::Vector4 p(corners[i].x, corners[i].y, corners[i].z, 1.0f);
						Math::Vector4 clip = Math::Vector4::Transform(p, worldViewProj);
						if (clip.x >= -clip.w) allNegX = false;
						if (clip.x <= clip.w) allPosX = false;
						if (clip.y >= -clip.w) allNegY = false;
						if (clip.y <= clip.w) allPosY = false;
						if (clip.z >= 0.0f) allNegZ = false;
						if (clip.z <= clip.w) allPosZ = false;
					}

					if (allNegX || allPosX || allNegY || allPosY || allNegZ || allPosZ)
					{
						continue;
					}

					if (sub.isAlphaCutout && m_psDepthCutout)
					{
						std::string subMatPath = mesh.materialPath;
						if (subMatPath.empty() || subMatPath == "/engine/Content/Default.mat" || subMatPath == "Default.mat")
						{
							if (!sub.defaultMaterialPath.empty()) subMatPath = sub.defaultMaterialPath;
						}
						const auto mat = subMatPath.empty() ? mesh.material : getOrCreateMaterial(subMatPath);
						if (mat)
						{
							if (m_cbMaterial)
							{
								CBPerMaterial matData = mat->getBufferData();
								m_context->updateSubresource(m_cbMaterial, &matData, sizeof(matData));
								m_context->setConstantBufferPS(DF_SLOT_CB_PER_MATERIAL, m_cbMaterial);
							}
							auto albedoTex = mat->getAlbedoTexture();
							m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO, albedoTex ? albedoTex : m_whiteTexture);
						}
						m_context->setPixelShader(m_psDepthCutout);
						m_context->setRasterizerState(m_rsCullNone);
						m_context->drawIndexed(sub.indexCount, sub.startIndexLocation, sub.baseVertexLocation);
						m_drawCallCount++;
						m_context->setPixelShader(nullptr);
						m_context->setRasterizerState(m_rsSolid);
					}
					else if (!sub.isAlphaCutout)
					{
						std::string subMatPath = mesh.materialPath;
						if (subMatPath.empty() || subMatPath == "/engine/Content/Default.mat" || subMatPath == "Default.mat")
						{
							if (!sub.defaultMaterialPath.empty()) subMatPath = sub.defaultMaterialPath;
						}
						const auto mat = subMatPath.empty() ? mesh.material : getOrCreateMaterial(subMatPath);
						bool isTwoSided = mat && mat->isTwoSided();
						if (isTwoSided) m_context->setRasterizerState(m_rsCullNone);

						m_context->drawIndexed(sub.indexCount, sub.startIndexLocation, sub.baseVertexLocation);
						m_drawCallCount++;

						if (isTwoSided) m_context->setRasterizerState(m_rsSolid);
					}
				}
			}
			else
			{
				m_context->drawIndexed(totalIdxCount > 0 ? totalIdxCount : gpuMesh.indexCount, 0, 0);
				m_drawCallCount++;
			}
		}

		// Terrain depth prepass
		for (const auto& ti : scene.terrains)
		{
			const auto gpuMesh = getOrCreateTerrainMesh(ti.component);
			if (!gpuMesh.vertexBuffer || !gpuMesh.indexBuffer || gpuMesh.indexCount == 0) continue;

			m_context->setVertexBuffer(0, gpuMesh.vertexBuffer, sizeof(VertexMesh), 0);
			m_context->setIndexBuffer(gpuMesh.indexBuffer, RHI::PixelFormat::R32_UINT, 0);

			CBPerObject obj{};
			obj.worldMatrix = ti.world;
			obj.invWorldMatrix = ti.world.Invert();
			obj.entityId = ti.entityId;

			auto alloc = m_uploadRing.allocate(sizeof(CBPerObject), &obj);
			if (alloc.isValid())
			{
				m_context->setConstantBufferSubrangeVS(DF_SLOT_CB_PER_OBJECT, alloc.buffer, alloc.firstConstant16, alloc.numConstants16);
			}

			m_context->drawIndexed(gpuMesh.indexCount, 0, 0);
			m_drawCallCount++;
		}

		m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO, nullptr);
		m_context->setPixelShader(nullptr);
		m_context->setRasterizerState(m_rsSolid);

		// Unbind DSV so depth can be read as SRV in SSAO
		m_context->setRenderTargets({}, nullptr);

		// Copy depth for contact shadows if enabled
		if (scene.environment.enableContactShadows && m_depthCopyTexture && m_depthTexture)
		{
			auto* nativeCtx = static_cast<ID3D11DeviceContext*>(m_device.getNativeContext());
			if (nativeCtx)
			{
				auto* dstRes = static_cast<ID3D11Resource*>(m_depthCopyTexture->getNativeHandle());
				auto* srcRes = static_cast<ID3D11Resource*>(m_depthTexture->getNativeHandle());
				if (dstRes && srcRes)
				{
					nativeCtx->CopyResource(dstRes, srcRes);
				}
			}
		}
	}

	void SceneRenderer::passOpaque(const RenderScene& scene)
	{
		std::vector<std::shared_ptr<RHI::IGpuTexture>> rtvs = {
			m_hdrTexture,
			m_entityIdTexture,
			m_normalRoughnessTexture,
			m_albedoMetallicTexture,
			m_velocityTexture
		};
		m_context->setRenderTargets(rtvs, m_depthTexture);

		// If neither SSAO nor contact shadows is enabled, depth prepass didn't run; clear depth to 0.0f
		if (!scene.environment.enableSSAO && !scene.environment.enableContactShadows)
		{
			m_context->clearDepthStencil(m_depthTexture, 0.0f, 0);
		}

		// Clear HDR target and ID target using scene environment ambient lighting
		Color envClearColor(
			scene.environment.ambientColor.x * scene.environment.ambientIntensity * 0.35f,
			scene.environment.ambientColor.y * scene.environment.ambientIntensity * 0.38f,
			scene.environment.ambientColor.z * scene.environment.ambientIntensity * 0.44f,
			1.0f
		);
		m_context->clearRenderTarget(m_hdrTexture, envClearColor);
		m_context->clearRenderTarget(m_entityIdTexture, Color(0.0f, 0.0f, 0.0f, 0.0f));
		if (m_velocityTexture) m_context->clearRenderTarget(m_velocityTexture, Color(0.0f, 0.0f, 0.0f, 0.0f));

		// Collect and sort Opaque and AlphaTest meshes
		m_cachedDrawItems.clear();
		m_cachedMeshPaths.clear();
		m_cachedMaterialPaths.clear();

		for (const auto& mesh : scene.meshes)
		{
			if (mesh.layer == RenderLayer::Opaque || mesh.layer == RenderLayer::AlphaTest)
			{
				const auto gpu = getOrCreateMesh(mesh.meshPath);
				if (!gpu.vertexBuffer || !gpu.indexBuffer) continue;

				uint32_t totalIdxCount = 0;
				const auto& submeshes = selectLodSubmeshes(gpu, mesh.distanceToCamera, scene.environment.meshLodBias, totalIdxCount);

				if (!submeshes.empty())
				{
					Math::Matrix worldViewProj = mesh.world * scene.camera.viewProjMatrix;

					for (const auto& sub : submeshes)
					{
						if (sub.indexCount == 0) continue;

						DirectX::XMFLOAT3 corners[8];
						sub.localBounds.GetCorners(corners);
						bool allNegX = true, allPosX = true;
						bool allNegY = true, allPosY = true;
						bool allNegZ = true, allPosZ = true;

						for (int i = 0; i < 8; ++i)
						{
							Math::Vector4 p(corners[i].x, corners[i].y, corners[i].z, 1.0f);
							Math::Vector4 clip = Math::Vector4::Transform(p, worldViewProj);
							if (clip.x >= -clip.w) allNegX = false;
							if (clip.x <= clip.w) allPosX = false;
							if (clip.y >= -clip.w) allNegY = false;
							if (clip.y <= clip.w) allPosY = false;
							if (clip.z >= 0.0f) allNegZ = false;
							if (clip.z <= clip.w) allPosZ = false;
						}

						if (allNegX || allPosX || allNegY || allPosY || allNegZ || allPosZ)
						{
							continue;
						}

						std::string subMatPath = mesh.materialPath;
						if (subMatPath.empty() || subMatPath == "/engine/Content/Default.mat" || subMatPath == "Default.mat")
						{
							if (!sub.defaultMaterialPath.empty())
							{
								subMatPath = sub.defaultMaterialPath;
							}
						}

						const auto mat = subMatPath.empty() ? mesh.material : getOrCreateMaterial(subMatPath);
						RenderLayer subLayer = sub.isAlphaCutout ? RenderLayer::AlphaTest : mesh.layer;

						DrawItem item{};
						item.shaderVariantId = 0;
						item.materialId = mat ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mat.get()) & 0xFFFF) : 0;
						item.meshId = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gpu.vertexBuffer.get()) & 0xFFFF);
						item.indexCount = sub.indexCount;
						item.startIndexLocation = sub.startIndexLocation;
						item.baseVertexLocation = sub.baseVertexLocation;
						item.instanceData.world = mesh.world;
						item.instanceData.entityId = mesh.entityId;
						item.sortKey = DrawItem::makeSortKey(subLayer, item.shaderVariantId, item.materialId, item.meshId, mesh.distanceToCamera);

						m_cachedDrawItems.push_back(item);
						m_cachedMeshPaths.push_back(mesh.meshPath);
						m_cachedMaterialPaths.push_back(subMatPath);
					}
				}
				else
				{
					uint32_t idxCount = totalIdxCount > 0 ? totalIdxCount : gpu.indexCount;
					if (idxCount == 0) continue;
					const auto mat = mesh.material ? mesh.material : getOrCreateMaterial(mesh.materialPath);

					DrawItem item{};
					item.shaderVariantId = 0;
					item.materialId = mat ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mat.get()) & 0xFFFF) : 0;
					item.meshId = static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gpu.vertexBuffer.get()) & 0xFFFF);
					item.indexCount = idxCount;
					item.startIndexLocation = 0;
					item.baseVertexLocation = 0;
					item.instanceData.world = mesh.world;
					item.instanceData.entityId = mesh.entityId;
					item.sortKey = DrawItem::makeSortKey(mesh.layer, item.shaderVariantId, item.materialId, item.meshId, mesh.distanceToCamera);

					m_cachedDrawItems.push_back(item);
					m_cachedMeshPaths.push_back(mesh.meshPath);
					m_cachedMaterialPaths.push_back(mesh.materialPath);
				}
			}
		}

		if (m_cachedDrawItems.empty()) return;

		// Bind Pipeline State for Opaque pass (with hardware tessellation support)
		bool useTessellation = scene.environment.enableTessellation && m_hsStandard && m_dsStandard;
		m_context->setPrimitiveTopology(useTessellation ? RHI::PrimitiveTopology::PatchList3ControlPoints : RHI::PrimitiveTopology::TriangleList);
		if (m_inputLayout) m_context->setInputLayout(m_inputLayout);
		if (m_vsStandard) m_context->setVertexShader(m_vsStandard);
		if (m_psStandard) m_context->setPixelShader(m_psStandard);
		if (useTessellation)
		{
			m_context->setHullShader(m_hsStandard);
			m_context->setDomainShader(m_dsStandard);
		}
		else
		{
			m_context->setHullShader(nullptr);
			m_context->setDomainShader(nullptr);
		}

		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		if (m_dssOpaque) m_context->setDepthStencilState(m_dssOpaque, 0);
		m_context->setBlendState(nullptr);

		// Bind Samplers
		if (m_samplerLinearWrap) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_WRAP, m_samplerLinearWrap);
		if (m_samplerAnisoWrap) m_context->setSamplerPS(DF_SLOT_SMP_ANISO_WRAP, m_samplerAnisoWrap);
		if (m_samplerLinearClamp) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_CLAMP, m_samplerLinearClamp);
		if (m_samplerPointClamp) m_context->setSamplerPS(DF_SLOT_SMP_POINT_CLAMP, m_samplerPointClamp);
		if (m_samplerShadow) m_context->setSamplerPS(DF_SLOT_SMP_SHADOW, m_samplerShadow);

		// Bind Lights Structured Buffer & Cluster Buffers
		if (m_bufLights)
		{
			m_context->setShaderResourceBufferPS(DF_SLOT_BUF_LIGHTS, m_bufLights);
		}
		if (m_bufClusterGrid)
		{
			m_context->setShaderResourceBufferPS(DF_SLOT_BUF_CLUSTER_GRID, m_bufClusterGrid);
		}
		if (m_bufClusterLightIndices)
		{
			m_context->setShaderResourceBufferPS(DF_SLOT_BUF_CLUSTER_INDICES, m_bufClusterLightIndices);
		}

		// Bind Volumetric Fog (slot 29)
		if (m_volumetricFogIntegrated)
		{
			m_context->setShaderResourcePS(DF_SLOT_TEX_VOLUMETRIC_FOG, m_volumetricFogIntegrated);
		}

		// Bind Shadow Map & CBShadow
		if (m_cbShadow)
		{
			m_context->setConstantBufferPS(DF_SLOT_CB_SHADOW, m_cbShadow);
		}
		if (m_shadowMapTexture)
		{
			m_context->setShaderResourcePS(DF_SLOT_TEX_SHADOW_MAP, m_shadowMapTexture);
		}

		// Bind SSAO Texture (slot 11)
		bool ssaoActive = scene.environment.enableSSAO && m_ssaoBlurredTexture;
		auto ssaoTex = ssaoActive ? m_ssaoBlurredTexture : m_whiteTexture;
		if (ssaoTex)
		{
			m_context->setShaderResourcePS(DF_SLOT_TEX_SSAO, ssaoTex);
		}

		// Bind Depth Copy Texture for Contact Shadows (slot 9)
		if (scene.environment.enableContactShadows && m_depthCopyTexture)
		{
			m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, m_depthCopyTexture);
		}

		// Bind Sky (slot 12), Specular IBL (slot 17), and Irradiance IBL (slot 18)
		std::shared_ptr<RHI::IGpuTexture> skyTex = scene.environment.skyboxTexture;
		if (!skyTex && !scene.environment.skyboxTexturePath.empty())
		{
			skyTex = getOrCreateTexture(scene.environment.skyboxTexturePath);
		}
		if (!skyTex)
		{
			skyTex = m_proceduralEnvIBLTexture ? m_proceduralEnvIBLTexture : m_blackTexture;
		}
		if (skyTex)
		{
			m_context->setShaderResourcePS(DF_SLOT_TEX_SKY_EQUIRECT, skyTex);
		}

		auto specProbe = m_specularIBLTexture ? m_specularIBLTexture : (m_proceduralEnvIBLTexture ? m_proceduralEnvIBLTexture : skyTex);
		if (specProbe)
		{
			m_context->setShaderResourcePS(DF_SLOT_TEX_SPECULAR_IBL, specProbe);
		}

		auto irradProbe = m_irradianceIBLTexture ? m_irradianceIBLTexture : specProbe;
		if (irradProbe)
		{
			m_context->setShaderResourcePS(DF_SLOT_TEX_IRRADIANCE_IBL, irradProbe);
		}

		if (m_brdfLutTexture)
		{
			m_context->setShaderResourcePS(DF_SLOT_TEX_BRDF_LUT, m_brdfLutTexture);
		}

		if (m_ssrTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_SSR, m_ssrTexture);
		if (m_cbSSR) m_context->setConstantBufferPS(DF_SLOT_CB_SSR, m_cbSSR);
		if (m_ddgiRadianceAtlas) m_context->setShaderResourcePS(DF_SLOT_TEX_DDGI_IRRADIANCE, m_ddgiRadianceAtlas);
		if (m_ddgiDepthAtlas) m_context->setShaderResourcePS(DF_SLOT_TEX_DDGI_DEPTH, m_ddgiDepthAtlas);
		if (m_cbDDGI) m_context->setConstantBufferPS(DF_SLOT_CB_DDGI, m_cbDDGI);

		// Bind Default Textures for all slots (Occlusion slot 4 must never be unbound)
		if (m_whiteTexture)
		{
			m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO, m_whiteTexture);
			m_context->setShaderResourcePS(DF_SLOT_TEX_OCCLUSION, m_whiteTexture);
		}
		if (m_flatNormalTexture)
		{
			m_context->setShaderResourcePS(DF_SLOT_TEX_NORMAL, m_flatNormalTexture);
			m_context->setShaderResourcePS(DF_SLOT_TEX_DETAIL, m_flatNormalTexture);
		}
		if (m_defaultMRTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_METALLIC_ROUGHNESS, m_defaultMRTexture);
		if (m_blackTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_EMISSIVE, m_blackTexture);

		// Sort and batch
		std::vector<size_t> indices(m_cachedDrawItems.size());
		for (size_t i = 0; i < indices.size(); ++i) indices[i] = i;
		std::sort(indices.begin(), indices.end(), [this](size_t a, size_t b) {
			return m_cachedDrawItems[a] < m_cachedDrawItems[b];
		});

		std::vector<DrawItem> sortedItems;
		sortedItems.reserve(indices.size());
		std::vector<std::string> sortedPaths;
		sortedPaths.reserve(indices.size());
		std::vector<std::string> sortedMatPaths;
		sortedMatPaths.reserve(indices.size());
		for (size_t idx : indices)
		{
			sortedItems.push_back(m_cachedDrawItems[idx]);
			sortedPaths.push_back(m_cachedMeshPaths[idx]);
			sortedMatPaths.push_back(m_cachedMaterialPaths[idx]);
		}
		m_cachedDrawItems = std::move(sortedItems);
		m_cachedMeshPaths = std::move(sortedPaths);
		m_cachedMaterialPaths = std::move(sortedMatPaths);

		auto batches = BatchDrawItems(m_cachedDrawItems, m_cachedInstances);
		m_batchCount += static_cast<uint32_t>(batches.size());
		m_instanceCount += static_cast<uint32_t>(m_cachedInstances.size());

		for (const auto& batch : batches)
		{
			const std::string& meshPath = m_cachedMeshPaths[batch.firstInstanceIndex];
			const std::string& matPath = m_cachedMaterialPaths[batch.firstInstanceIndex];
			const auto gpuMesh = getOrCreateMesh(meshPath);
			if (!gpuMesh.vertexBuffer || !gpuMesh.indexBuffer || batch.indexCount == 0) continue;

			// Update material constant buffer and bind textures for this batch
			const auto mat = getOrCreateMaterial(matPath);
			if (mat)
			{
				if (mat->isTwoSided() || mat->getCullMode() == RHI::CullMode::None)
				{
					m_context->setRasterizerState(m_rsCullNone);
				}
				else
				{
					m_context->setRasterizerState(m_rsSolid);
				}

				if (m_cbMaterial)
				{
					CBPerMaterial matData = mat->getBufferData();
					m_context->updateSubresource(m_cbMaterial, &matData, sizeof(CBPerMaterial));
					m_context->setConstantBufferPS(DF_SLOT_CB_PER_MATERIAL, m_cbMaterial);
					m_context->setConstantBufferVS(DF_SLOT_CB_PER_MATERIAL, m_cbMaterial);
					if (useTessellation)
					{
						m_context->setConstantBufferDS(DF_SLOT_CB_PER_MATERIAL, m_cbMaterial);
					}
				}

				auto albedoTex = mat->getAlbedoTexture() ? mat->getAlbedoTexture() : m_whiteTexture;
				auto normalTex = mat->getNormalTexture() ? mat->getNormalTexture() : m_flatNormalTexture;
				auto mrTex = mat->getMetallicRoughnessTexture() ? mat->getMetallicRoughnessTexture() : m_defaultMRTexture;
				auto emissiveTex = mat->getEmissiveTexture() ? mat->getEmissiveTexture() : m_blackTexture;
				auto occlusionTex = mat->getOcclusionTexture() ? mat->getOcclusionTexture() : m_whiteTexture;
				auto detailTex = mat->getDetailTexture() ? mat->getDetailTexture() : m_flatNormalTexture;

				if (albedoTex) m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO, albedoTex);
				if (normalTex) m_context->setShaderResourcePS(DF_SLOT_TEX_NORMAL, normalTex);
				if (mrTex) m_context->setShaderResourcePS(DF_SLOT_TEX_METALLIC_ROUGHNESS, mrTex);
				if (emissiveTex) m_context->setShaderResourcePS(DF_SLOT_TEX_EMISSIVE, emissiveTex);
				if (occlusionTex) m_context->setShaderResourcePS(DF_SLOT_TEX_OCCLUSION, occlusionTex);
				if (detailTex) m_context->setShaderResourcePS(DF_SLOT_TEX_DETAIL, detailTex);
			}
			else
			{
				m_context->setRasterizerState(m_rsSolid);
			}

			m_context->setVertexBuffer(0, gpuMesh.vertexBuffer, sizeof(VertexMesh), 0);
			m_context->setIndexBuffer(gpuMesh.indexBuffer, RHI::PixelFormat::R32_UINT, 0);

			for (uint32_t i = 0; i < batch.instanceCount; ++i)
			{
				const auto& inst = m_cachedInstances[batch.firstInstanceIndex + i];

				CBPerObject obj{};
				obj.worldMatrix = inst.world;
				obj.invWorldMatrix = inst.world.Invert();
				obj.entityId = inst.entityId;

				auto alloc = m_uploadRing.allocate(sizeof(CBPerObject), &obj);
				if (alloc.isValid())
				{
					m_context->setConstantBufferSubrangeVS(DF_SLOT_CB_PER_OBJECT, alloc.buffer, alloc.firstConstant16, alloc.numConstants16);
					if (useTessellation)
					{
						m_context->setConstantBufferSubrangeDS(DF_SLOT_CB_PER_OBJECT, alloc.buffer, alloc.firstConstant16, alloc.numConstants16);
					}
				}

				m_context->drawIndexed(batch.indexCount, batch.startIndexLocation, batch.baseVertexLocation);
				m_drawCallCount++;
			}
		}

		// Unbind DF_SLOT_TEX_DEPTH, HS, DS, and restore TriangleList at end of pass
		m_context->setShaderResourcePS(DF_SLOT_TEX_SHADOW_MAP, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_SSAO, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_DETAIL, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_SKY_EQUIRECT, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_SPECULAR_IBL, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_IRRADIANCE_IBL, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_BRDF_LUT, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_SSR, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_DDGI_IRRADIANCE, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_DDGI_DEPTH, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_VOLUMETRIC_FOG, nullptr);
		m_context->setShaderResourceBufferPS(DF_SLOT_BUF_CLUSTER_GRID, nullptr);
		m_context->setShaderResourceBufferPS(DF_SLOT_BUF_CLUSTER_INDICES, nullptr);
		m_context->setShaderResourceBufferPS(DF_SLOT_BUF_LIGHTS, nullptr);
		m_context->setHullShader(nullptr);
		m_context->setDomainShader(nullptr);
		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		m_context->setRenderTargets({}, nullptr);
	}
}
