#include "Engine/Renderer/SceneRenderer.h"
#include "Engine/Renderer/TerrainGenerator.h"
#include "Engine/Renderer/DebugRenderer.h"
#include "Engine/Core/Log/Log.h"
#include <algorithm>
#include <bit>
#include <cmath>
#include <cstring>
#include <d3d11.h>

namespace Core::Renderer
{
	GpuMesh SceneRenderer::getOrCreateTerrainMesh(const Core::TerrainComponent& tc)
	{
		uint64_t key = (static_cast<uint64_t>(tc.seed) << 32)
			^ (static_cast<uint64_t>(tc.resolution) << 16)
			^ static_cast<uint64_t>(std::bit_cast<uint32_t>(tc.worldSize))
			^ (static_cast<uint64_t>(std::bit_cast<uint32_t>(tc.maxHeight)) << 8)
			^ (static_cast<uint64_t>(tc.isIsland ? 1 : 0) << 48);

		auto it = m_terrainMeshCache.find(key);
		if (it != m_terrainMeshCache.end())
		{
			return it->second;
		}

		MeshData data = TerrainGenerator::generateTerrainMesh(tc);
		GpuMesh gpu = createGpuMesh(data);
		m_terrainMeshCache[key] = gpu;
		return gpu;
	}

	GpuMesh SceneRenderer::getOrCreateWaterMesh(const Core::WaterComponent& wc)
	{
		uint64_t key = (static_cast<uint64_t>(wc.gridResolution) << 32)
			^ static_cast<uint64_t>(std::bit_cast<uint32_t>(wc.planeSize));

		auto it = m_waterMeshCache.find(key);
		if (it != m_waterMeshCache.end())
		{
			return it->second;
		}

		MeshData data = CreateSubdividedPlaneMesh(wc.planeSize, wc.gridResolution);
		GpuMesh gpu = createGpuMesh(data);
		m_waterMeshCache[key] = gpu;
		return gpu;
	}

	void SceneRenderer::passTerrain(const RenderScene& scene)
	{
		if (scene.terrains.empty() || !m_vsTerrain || !m_psTerrain || !m_depthTexture)
		{
			return;
		}

		std::vector<std::shared_ptr<RHI::IGpuTexture>> rtvs = {
			m_hdrTexture,
			m_entityIdTexture
		};
		m_context->setRenderTargets(rtvs, m_depthTexture);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		if (m_inputLayout) m_context->setInputLayout(m_inputLayout);
		m_context->setVertexShader(m_vsTerrain);
		m_context->setPixelShader(m_psTerrain);
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		if (m_dssOpaque) m_context->setDepthStencilState(m_dssOpaque, 0);
		m_context->setBlendState(nullptr);

		if (m_shadowMapTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_SHADOW_MAP, m_shadowMapTexture);
		auto ssaoTex = (scene.environment.enableSSAO && m_ssaoBlurredTexture) ? m_ssaoBlurredTexture : m_whiteTexture;
		if (ssaoTex) m_context->setShaderResourcePS(DF_SLOT_TEX_SSAO, ssaoTex);
		if (m_bufLights) m_context->setShaderResourceBufferPS(DF_SLOT_BUF_LIGHTS, m_bufLights);

		// Bind Sky / Specular IBL Texture (slot 12)
		std::shared_ptr<RHI::IGpuTexture> skyTex = scene.environment.skyboxTexture;
		if (!skyTex && !scene.environment.skyboxTexturePath.empty())
		{
			skyTex = getOrCreateTexture(scene.environment.skyboxTexturePath);
		}
		if (!skyTex) skyTex = m_proceduralEnvIBLTexture ? m_proceduralEnvIBLTexture : m_blackTexture;
		if (skyTex) m_context->setShaderResourcePS(DF_SLOT_TEX_SKY_EQUIRECT, skyTex);
		if (m_brdfLutTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_BRDF_LUT, m_brdfLutTexture);

		if (scene.environment.enableContactShadows && m_depthCopyTexture)
		{
			m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, m_depthCopyTexture);
		}

		if (m_samplerLinearWrap) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_WRAP, m_samplerLinearWrap);
		if (m_samplerAnisoWrap) m_context->setSamplerPS(DF_SLOT_SMP_ANISO_WRAP, m_samplerAnisoWrap);
		if (m_samplerLinearClamp) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_CLAMP, m_samplerLinearClamp);
		if (m_samplerPointClamp) m_context->setSamplerPS(DF_SLOT_SMP_POINT_CLAMP, m_samplerPointClamp);
		if (m_samplerShadow) m_context->setSamplerPS(DF_SLOT_SMP_SHADOW, m_samplerShadow);

		if (m_cbShadow) m_context->setConstantBufferPS(DF_SLOT_CB_SHADOW, m_cbShadow);

		for (const auto& ti : scene.terrains)
		{
			GpuMesh gpuMesh = getOrCreateTerrainMesh(ti.component);
			if (!gpuMesh.vertexBuffer || !gpuMesh.indexBuffer || gpuMesh.indexCount == 0) continue;

			CBTerrain cbTerrainData{};
			cbTerrainData.heightParams = Math::Vector4(ti.component.sandHeight, ti.component.cliffSlopeThreshold, ti.component.snowHeight, ti.component.maxHeight);
			cbTerrainData.worldParams = Math::Vector4(ti.component.worldSize, ti.component.seaFloorDepth, 1.0f, 0.0f);
			if (m_cbTerrain)
			{
				m_context->updateSubresource(m_cbTerrain, &cbTerrainData, sizeof(cbTerrainData));
				m_context->setConstantBufferVS(DF_SLOT_CB_TERRAIN, m_cbTerrain);
				m_context->setConstantBufferPS(DF_SLOT_CB_TERRAIN, m_cbTerrain);
			}

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

		m_context->setShaderResourcePS(DF_SLOT_TEX_SHADOW_MAP, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_SSAO, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_SKY_EQUIRECT, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_BRDF_LUT, nullptr);
		m_context->setShaderResourceBufferPS(DF_SLOT_BUF_LIGHTS, nullptr);
		m_context->setRenderTargets({}, nullptr);
	}

	void SceneRenderer::passFoliage(const RenderScene& scene)
	{
		if (scene.foliage.empty() || !m_vsGrass || !m_psGrass || !m_grassBladeMesh.vertexBuffer || !m_depthTexture)
		{
			return;
		}

		std::vector<std::shared_ptr<RHI::IGpuTexture>> rtvs = {
			m_hdrTexture,
			m_entityIdTexture
		};
		m_context->setRenderTargets(rtvs, m_depthTexture);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		if (m_inputLayout) m_context->setInputLayout(m_inputLayout);
		m_context->setVertexShader(m_vsGrass);
		m_context->setPixelShader(m_psGrass);
		if (m_rsCullNone) m_context->setRasterizerState(m_rsCullNone);
		if (m_dssOpaque) m_context->setDepthStencilState(m_dssOpaque, 0);
		m_context->setBlendState(nullptr);

		if (m_shadowMapTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_SHADOW_MAP, m_shadowMapTexture);
		auto ssaoTex = (scene.environment.enableSSAO && m_ssaoBlurredTexture) ? m_ssaoBlurredTexture : m_whiteTexture;
		if (ssaoTex) m_context->setShaderResourcePS(DF_SLOT_TEX_SSAO, ssaoTex);
		if (m_bufLights) m_context->setShaderResourceBufferPS(DF_SLOT_BUF_LIGHTS, m_bufLights);

		// Bind Sky / Specular IBL Texture (slot 12)
		std::shared_ptr<RHI::IGpuTexture> skyTex = scene.environment.skyboxTexture;
		if (!skyTex && !scene.environment.skyboxTexturePath.empty())
		{
			skyTex = getOrCreateTexture(scene.environment.skyboxTexturePath);
		}
		if (!skyTex) skyTex = m_proceduralEnvIBLTexture ? m_proceduralEnvIBLTexture : m_blackTexture;
		if (skyTex) m_context->setShaderResourcePS(DF_SLOT_TEX_SKY_EQUIRECT, skyTex);
		if (m_brdfLutTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_BRDF_LUT, m_brdfLutTexture);

		if (scene.environment.enableContactShadows && m_depthCopyTexture)
		{
			m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, m_depthCopyTexture);
		}

		if (m_samplerLinearWrap) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_WRAP, m_samplerLinearWrap);
		if (m_samplerAnisoWrap) m_context->setSamplerPS(DF_SLOT_SMP_ANISO_WRAP, m_samplerAnisoWrap);
		if (m_samplerLinearClamp) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_CLAMP, m_samplerLinearClamp);
		if (m_samplerPointClamp) m_context->setSamplerPS(DF_SLOT_SMP_POINT_CLAMP, m_samplerPointClamp);
		if (m_samplerShadow) m_context->setSamplerPS(DF_SLOT_SMP_SHADOW, m_samplerShadow);

		if (m_cbShadow) m_context->setConstantBufferPS(DF_SLOT_CB_SHADOW, m_cbShadow);

		const RenderTerrainInstance* terrain = scene.terrains.empty() ? nullptr : &scene.terrains[0];

		for (const auto& fi : scene.foliage)
		{
			uint32_t count = 0;
			auto it = m_foliageBufferCache.find(fi.entityId);
			if (it == m_foliageBufferCache.end())
			{
				uint32_t rngState = 2166136261u ^ (fi.entityId * 16777619u);
				auto lcgRand = [&]() -> float {
					rngState = rngState * 1664525u + 1013904223u;
					return static_cast<float>(rngState & 0x00FFFFFFu) / static_cast<float>(0x00FFFFFFu);
				};

				std::vector<GrassInstanceData> instances;
				instances.reserve(fi.component.bladeCount);

				Math::Vector3 foliageCenter = Math::getTranslation(fi.world);
				float halfDimX = fi.component.patchDimensions.x * 0.5f;
				float halfDimZ = fi.component.patchDimensions.y * 0.5f;

				for (uint32_t b = 0; b < fi.component.bladeCount; ++b)
				{
					float rx = (lcgRand() * 2.0f - 1.0f) * halfDimX;
					float rz = (lcgRand() * 2.0f - 1.0f) * halfDimZ;
					Math::Vector3 worldPos = foliageCenter + Math::Vector3(rx, 0.0f, rz);

					if (fi.component.snapToTerrain && terrain)
					{
						Math::Vector3 terrainCenter = Math::getTranslation(terrain->world);
						float localTerrainX = worldPos.x - terrainCenter.x;
						float localTerrainZ = worldPos.z - terrainCenter.z;

						float h = TerrainGenerator::sampleHeight(terrain->component, localTerrainX, localTerrainZ) + terrainCenter.y;
						Math::Vector3 normal = TerrainGenerator::sampleNormal(terrain->component, localTerrainX, localTerrainZ);
						float slope = 1.0f - normal.y;

						if (h < fi.component.minAltitude || h > fi.component.maxAltitude || slope > fi.component.maxSlope)
						{
							continue;
						}
						worldPos.y = h;
					}

					float hScale = std::lerp(fi.component.bladeHeightMin, fi.component.bladeHeightMax, lcgRand());
					float yaw = lcgRand() * 6.2831853f;
					float colorVar = lcgRand();

					GrassInstanceData gid{};
					gid.positionScale = Math::Vector4(worldPos.x, worldPos.y, worldPos.z, hScale);
					gid.rotationColor = Math::Vector4(std::sin(yaw), std::cos(yaw), fi.component.bladeWidth, colorVar);
					instances.push_back(gid);
				}

				if (instances.empty())
				{
					GrassInstanceData dummy{};
					dummy.positionScale = Math::Vector4(0.0f, -9999.0f, 0.0f, 0.0f);
					dummy.rotationColor = Math::Vector4(0.0f, 1.0f, 0.0f, 0.0f);
					instances.push_back(dummy);
				}

				RHI::GpuBufferDesc instBufDesc{};
				instBufDesc.size = static_cast<uint32_t>(instances.size() * sizeof(GrassInstanceData));
				instBufDesc.usage = RHI::BufferUsage::Immutable;
				instBufDesc.bindFlags = RHI::BufferBindFlags::ShaderResource;
				instBufDesc.structureByteStride = sizeof(GrassInstanceData);
				auto bufRes = m_device.createBuffer(instBufDesc, instances.data());
				if (bufRes.isOk())
				{
					m_foliageBufferCache[fi.entityId] = *bufRes;
					m_foliageCountCache[fi.entityId] = static_cast<uint32_t>(instances.size());
					count = static_cast<uint32_t>(instances.size());
				}
			}
			else
			{
				count = m_foliageCountCache[fi.entityId];
			}

			if (count == 0) continue;

			CBFoliage cb{};
			cb.windParams = Math::Vector4(fi.component.windSpeed, fi.component.windStrength, fi.component.windFrequency, fi.component.maxDrawDistance);
			cb.rootColor = Math::Vector4(fi.component.rootColor.R(), fi.component.rootColor.G(), fi.component.rootColor.B(), fi.component.rootColor.A());
			cb.tipColor = Math::Vector4(fi.component.tipColor.R(), fi.component.tipColor.G(), fi.component.tipColor.B(), fi.component.tipColor.A());
			if (m_cbFoliage)
			{
				m_context->updateSubresource(m_cbFoliage, &cb, sizeof(cb));
				m_context->setConstantBufferVS(DF_SLOT_CB_FOLIAGE, m_cbFoliage);
				m_context->setConstantBufferPS(DF_SLOT_CB_FOLIAGE, m_cbFoliage);
			}

			CBPerObject obj{};
			obj.worldMatrix = fi.world;
			obj.invWorldMatrix = fi.world.Invert();
			obj.entityId = fi.entityId;

			auto alloc = m_uploadRing.allocate(sizeof(CBPerObject), &obj);
			if (alloc.isValid())
			{
				m_context->setConstantBufferSubrangeVS(DF_SLOT_CB_PER_OBJECT, alloc.buffer, alloc.firstConstant16, alloc.numConstants16);
			}

			m_context->setVertexBuffer(0, m_grassBladeMesh.vertexBuffer, sizeof(VertexMesh), 0);
			m_context->setIndexBuffer(m_grassBladeMesh.indexBuffer, RHI::PixelFormat::R32_UINT, 0);

			m_context->setShaderResourceBufferVS(0, m_foliageBufferCache[fi.entityId]);
			m_context->drawIndexedInstanced(m_grassBladeMesh.indexCount, count, 0, 0, 0);
			m_drawCallCount++;
			m_instanceCount += count;
			m_context->setShaderResourceBufferVS(0, nullptr);
		}

		m_context->setShaderResourcePS(DF_SLOT_TEX_SHADOW_MAP, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_SSAO, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_SKY_EQUIRECT, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_BRDF_LUT, nullptr);
		m_context->setShaderResourceBufferPS(DF_SLOT_BUF_LIGHTS, nullptr);
		m_context->setRenderTargets({}, nullptr);
	}

	void SceneRenderer::passWater(const RenderScene& scene)
	{
		if (scene.waters.empty() || !m_vsWater || !m_psWater || !m_hdrTexture || !m_depthTexture)
		{
			return;
		}

		// 1. Copy m_hdrTexture to m_refractionHdrCopyTexture for hazard-free seabed refraction
		auto* nativeCtx = static_cast<ID3D11DeviceContext*>(m_device.getNativeContext());
		if (nativeCtx && m_refractionHdrCopyTexture && m_hdrTexture)
		{
			auto* dstRes = static_cast<ID3D11Resource*>(m_refractionHdrCopyTexture->getNativeHandle());
			auto* srcRes = static_cast<ID3D11Resource*>(m_hdrTexture->getNativeHandle());
			if (dstRes && srcRes)
			{
				nativeCtx->CopyResource(dstRes, srcRes);
			}
		}

		// 2. Set render target to HDR + entity ID without DSV, so m_depthTexture can be read in PS as SRV (t9)
		std::vector<std::shared_ptr<RHI::IGpuTexture>> rtvs = { m_hdrTexture, m_entityIdTexture };
		m_context->setRenderTargets(rtvs, nullptr);

		RHI::Viewport fullVp{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
		m_context->setViewport(fullVp);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		if (m_inputLayout) m_context->setInputLayout(m_inputLayout);
		m_context->setVertexShader(m_vsWater);
		m_context->setPixelShader(m_psWater);
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		m_context->setDepthStencilState(nullptr, 0);
		m_context->setBlendState(nullptr);

		if (m_refractionHdrCopyTexture)
		{
			m_context->setShaderResourcePS(DF_SLOT_TEX_CUSTOM0, m_refractionHdrCopyTexture);
		}
		else
		{
			m_context->setShaderResourcePS(DF_SLOT_TEX_CUSTOM0, m_whiteTexture);
		}
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, m_depthTexture);

		// Bind Sky / Specular IBL Texture (slot 12)
		std::shared_ptr<RHI::IGpuTexture> skyTex = scene.environment.skyboxTexture;
		if (!skyTex && !scene.environment.skyboxTexturePath.empty())
		{
			skyTex = getOrCreateTexture(scene.environment.skyboxTexturePath);
		}
		if (!skyTex) skyTex = m_proceduralEnvIBLTexture ? m_proceduralEnvIBLTexture : m_blackTexture;
		if (skyTex) m_context->setShaderResourcePS(DF_SLOT_TEX_SKY_EQUIRECT, skyTex);
		if (m_brdfLutTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_BRDF_LUT, m_brdfLutTexture);
		if (m_bufLights) m_context->setShaderResourceBufferPS(DF_SLOT_BUF_LIGHTS, m_bufLights);

		if (m_samplerLinearClamp) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_CLAMP, m_samplerLinearClamp);
		if (m_samplerPointClamp) m_context->setSamplerPS(DF_SLOT_SMP_POINT_CLAMP, m_samplerPointClamp);

		for (const auto& wi : scene.waters)
		{
			GpuMesh gpuMesh = getOrCreateWaterMesh(wi.component);
			if (!gpuMesh.vertexBuffer || !gpuMesh.indexBuffer || gpuMesh.indexCount == 0) continue;

			CBWater cb{};
			cb.waveParams = Math::Vector4(wi.component.waveAmplitude, wi.component.waveFrequency, wi.component.waveSpeed, wi.component.waveSteepness);
			cb.shallowColor = Math::Vector4(wi.component.shallowColor.R(), wi.component.shallowColor.G(), wi.component.shallowColor.B(), wi.component.shallowColor.A());
			cb.deepColor = Math::Vector4(wi.component.deepColor.R(), wi.component.deepColor.G(), wi.component.deepColor.B(), wi.component.deepColor.A());
			cb.foamColor = Math::Vector4(wi.component.foamColor.R(), wi.component.foamColor.G(), wi.component.foamColor.B(), wi.component.foamColor.A());
			cb.opticalParams = Math::Vector4(wi.component.depthAbsorption, wi.component.foamDepthThreshold, wi.component.refractionStrength, wi.component.causticsIntensity);
			cb.waterExtra = Math::Vector4(wi.component.waterLevel, wi.component.planeSize, 0.0f, 0.0f);

			if (m_cbWater)
			{
				m_context->updateSubresource(m_cbWater, &cb, sizeof(cb));
				m_context->setConstantBufferVS(DF_SLOT_CB_WATER, m_cbWater);
				m_context->setConstantBufferPS(DF_SLOT_CB_WATER, m_cbWater);
			}

			CBPerObject obj{};
			obj.worldMatrix = wi.world;
			obj.invWorldMatrix = wi.world.Invert();
			obj.entityId = wi.entityId;

			auto alloc = m_uploadRing.allocate(sizeof(CBPerObject), &obj);
			if (alloc.isValid())
			{
				m_context->setConstantBufferSubrangeVS(DF_SLOT_CB_PER_OBJECT, alloc.buffer, alloc.firstConstant16, alloc.numConstants16);
			}

			m_context->setVertexBuffer(0, gpuMesh.vertexBuffer, sizeof(VertexMesh), 0);
			m_context->setIndexBuffer(gpuMesh.indexBuffer, RHI::PixelFormat::R32_UINT, 0);

			m_context->drawIndexed(gpuMesh.indexCount, 0, 0);
			m_drawCallCount++;
		}

		m_context->setShaderResourcePS(DF_SLOT_TEX_CUSTOM0, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_SKY_EQUIRECT, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_BRDF_LUT, nullptr);
		m_context->setShaderResourceBufferPS(DF_SLOT_BUF_LIGHTS, nullptr);
		m_context->setRenderTargets({}, nullptr);
	}

	void SceneRenderer::passDebugLines([[maybe_unused]] const RenderScene& scene, [[maybe_unused]] const RenderView& view)
	{
		const auto& vertices = DebugRenderer::getVertices();
		if (vertices.empty() || !m_vsDebugLine || !m_psDebugLine || !m_hdrTexture)
		{
			DebugRenderer::clear();
			return;
		}

		uint32_t vertexCount = static_cast<uint32_t>(vertices.size());
		if (vertexCount > m_vbDebugLinesCapacity || !m_vbDebugLines)
		{
			m_vbDebugLinesCapacity = std::max(vertexCount * 2, 4096u);
			RHI::GpuBufferDesc desc{};
			desc.size = m_vbDebugLinesCapacity * sizeof(DebugVertex);
			desc.usage = RHI::BufferUsage::Dynamic;
			desc.bindFlags = RHI::BufferBindFlags::VertexBuffer;
			auto res = m_device.createBuffer(desc);
			if (res.isOk())
			{
				m_vbDebugLines = *res;
			}
			else
			{
				DebugRenderer::clear();
				return;
			}
		}

		auto mapRes = m_context->map(m_vbDebugLines, true);
		if (mapRes.isOk())
		{
			std::memcpy(mapRes.value(), vertices.data(), vertexCount * sizeof(DebugVertex));
			m_context->unmap(m_vbDebugLines);
		}
		else
		{
			m_context->updateSubresource(m_vbDebugLines, vertices.data(), vertexCount * sizeof(DebugVertex));
		}

		std::vector<std::shared_ptr<RHI::IGpuTexture>> rtvs = { m_hdrTexture };
		m_context->setRenderTargets(rtvs, m_depthTexture);

		RHI::Viewport vp{};
		vp.x = 0.0f;
		vp.y = 0.0f;
		vp.width = static_cast<float>(m_width);
		vp.height = static_cast<float>(m_height);
		vp.minDepth = 0.0f;
		vp.maxDepth = 1.0f;
		m_context->setViewport(vp);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::LineList);
		if (m_inputLayoutDebugLine) m_context->setInputLayout(m_inputLayoutDebugLine);
		m_context->setVertexShader(m_vsDebugLine);
		m_context->setPixelShader(m_psDebugLine);

		if (m_rsCullNone) m_context->setRasterizerState(m_rsCullNone);
		if (m_dssTransparent) m_context->setDepthStencilState(m_dssTransparent, 0);
		if (m_bsAlphaBlend) m_context->setBlendState(m_bsAlphaBlend);

		if (m_cbView)
		{
			m_context->setConstantBufferVS(DF_SLOT_CB_PER_VIEW, m_cbView);
		}

		m_context->setVertexBuffer(0, m_vbDebugLines, sizeof(DebugVertex), 0);
		m_context->draw(vertexCount, 0);
		m_drawCallCount++;

		m_context->setRenderTargets({}, nullptr);
		DebugRenderer::clear();
	}
}
