#include "Engine/Renderer/SceneRenderer.h"
#include "Engine/Core/Log/Log.h"
#include <algorithm>
#include <cmath>

namespace Core::Renderer
{
	void SceneRenderer::passSkybox(const RenderScene& scene)
	{
		if (!m_vsFullscreen || !m_psSky || !m_hdrTexture || !m_depthTexture) return;

		std::vector<std::shared_ptr<RHI::IGpuTexture>> rtvs = { m_hdrTexture, m_entityIdTexture };
		m_context->setRenderTargets(rtvs, m_depthTexture);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		m_context->setInputLayout(nullptr);
		m_context->setVertexShader(m_vsFullscreen);
		m_context->setPixelShader(m_psSky);
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		if (m_dssSky) m_context->setDepthStencilState(m_dssSky, 0);
		m_context->setBlendState(nullptr);

		CBEnvironment envData{};
		envData.skyZenith = scene.environment.skyColor;
		envData.skyHorizon = scene.environment.horizonColor;
		envData.groundColor = scene.environment.groundColor;
		envData.ambientColor = scene.environment.ambientColor;
		envData.skyIntensity = 1.0f;
		envData.ambientIntensity = scene.environment.ambientIntensity;
		envData.skyType = static_cast<uint32_t>(scene.environment.skyType);

		if (scene.environment.skyType == SkyType::SkyboxTexture)
		{
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
		}

		// Find first directional light for sun disk
		bool foundDir = false;
		for (const auto& l : scene.lights)
		{
			if (l.type == 0) // Directional
			{
				envData.sunDirection = Math::Vector4(-l.direction.x, -l.direction.y, -l.direction.z, 1.0f);
				envData.sunColor = Math::Vector4(l.color.x, l.color.y, l.color.z, 0.9995f);
				foundDir = true;
				break;
			}
		}
		if (!foundDir)
		{
			envData.sunDirection = Math::Vector4(0.0f, 1.0f, 0.0f, 0.0f);
			envData.sunColor = Math::Vector4(1.0f, 1.0f, 1.0f, 0.9995f);
		}

		if (m_cbEnvironment)
		{
			m_context->updateSubresource(m_cbEnvironment, &envData, sizeof(envData));
			m_context->setConstantBufferPS(DF_SLOT_CB_POST_PROCESS, m_cbEnvironment);
		}

		m_context->draw(3, 0);
		m_drawCallCount++;

		m_context->setShaderResourcePS(DF_SLOT_TEX_SKY_EQUIRECT, nullptr);
		m_context->setRenderTargets({}, nullptr);
	}

	void SceneRenderer::passTransparent(const RenderScene& scene)
	{
		std::vector<RenderMeshInstance> transparentMeshes;
		for (const auto& mesh : scene.meshes)
		{
			const auto mat = mesh.material ? mesh.material : getOrCreateMaterial(mesh.materialPath);
			bool isTransparent = (mesh.layer == RenderLayer::Transparent) ||
				(mat && mat->getBlendMode() != BlendMode::Opaque);
			if (isTransparent)
			{
				transparentMeshes.push_back(mesh);
			}
		}

		if (transparentMeshes.empty() && scene.particles.empty()) return;

		// Sort back-to-front (farthest first)
		std::sort(transparentMeshes.begin(), transparentMeshes.end(), [](const RenderMeshInstance& a, const RenderMeshInstance& b) {
			return a.distanceToCamera > b.distanceToCamera;
		});

		std::vector<std::shared_ptr<RHI::IGpuTexture>> rtvs = { m_hdrTexture, m_entityIdTexture };
		m_context->setRenderTargets(rtvs, m_depthTexture);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		if (m_inputLayout) m_context->setInputLayout(m_inputLayout);
		if (m_vsStandard) m_context->setVertexShader(m_vsStandard);
		if (m_psStandard) m_context->setPixelShader(m_psStandard);
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		if (m_dssTransparent) m_context->setDepthStencilState(m_dssTransparent, 0);

		// Bind Samplers
		if (m_samplerLinearWrap) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_WRAP, m_samplerLinearWrap);
		if (m_samplerAnisoWrap) m_context->setSamplerPS(DF_SLOT_SMP_ANISO_WRAP, m_samplerAnisoWrap);
		if (m_samplerLinearClamp) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_CLAMP, m_samplerLinearClamp);
		if (m_samplerPointClamp) m_context->setSamplerPS(DF_SLOT_SMP_POINT_CLAMP, m_samplerPointClamp);
		if (m_samplerShadow) m_context->setSamplerPS(DF_SLOT_SMP_SHADOW, m_samplerShadow);

		// Bind Sky / Specular IBL Texture (slot 12)
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
		if (m_brdfLutTexture)
		{
			m_context->setShaderResourcePS(DF_SLOT_TEX_BRDF_LUT, m_brdfLutTexture);
		}

		// Bind default textures
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

		if (m_bufLights) m_context->setShaderResourceBufferPS(DF_SLOT_BUF_LIGHTS, m_bufLights);
		if (m_cbShadow) m_context->setConstantBufferPS(DF_SLOT_CB_SHADOW, m_cbShadow);
		if (m_shadowMapTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_SHADOW_MAP, m_shadowMapTexture);

		for (const auto& mesh : transparentMeshes)
		{
			const auto gpuMesh = getOrCreateMesh(mesh.meshPath);
			if (!gpuMesh.vertexBuffer || !gpuMesh.indexBuffer || gpuMesh.indexCount == 0) continue;

			const auto mat = mesh.material ? mesh.material : getOrCreateMaterial(mesh.materialPath);
			if (mat)
			{
				if (mat->getBlendMode() == BlendMode::Additive)
				{
					m_context->setBlendState(m_bsAdditive);
				}
				else
				{
					m_context->setBlendState(m_bsAlphaBlend);
				}

				if (m_cbMaterial)
				{
					CBPerMaterial matData = mat->getBufferData();
					m_context->updateSubresource(m_cbMaterial, &matData, sizeof(CBPerMaterial));
					m_context->setConstantBufferPS(DF_SLOT_CB_PER_MATERIAL, m_cbMaterial);
					m_context->setConstantBufferVS(DF_SLOT_CB_PER_MATERIAL, m_cbMaterial);
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
				m_context->setBlendState(m_bsAlphaBlend);
			}

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

			m_context->drawIndexed(gpuMesh.indexCount, 0, 0);
			m_drawCallCount++;
		}

		// Render Camera-Facing Billboard Particles
		if (!scene.particles.empty() && m_billboardQuadMesh.vertexBuffer)
		{
			Math::Vector3 camR(scene.camera.invViewMatrix._11, scene.camera.invViewMatrix._12, scene.camera.invViewMatrix._13);
			Math::Vector3 camU(scene.camera.invViewMatrix._21, scene.camera.invViewMatrix._22, scene.camera.invViewMatrix._23);
			Math::Vector3 camF(scene.camera.invViewMatrix._31, scene.camera.invViewMatrix._32, scene.camera.invViewMatrix._33);
			if (camR.LengthSquared() > 1e-6f) camR.Normalize();
			if (camU.LengthSquared() > 1e-6f) camU.Normalize();
			if (camF.LengthSquared() > 1e-6f) camF.Normalize();

			m_context->setVertexBuffer(0, m_billboardQuadMesh.vertexBuffer, sizeof(VertexMesh), 0);
			m_context->setIndexBuffer(m_billboardQuadMesh.indexBuffer, RHI::PixelFormat::R32_UINT, 0);

			for (const auto& p : scene.particles)
			{
				if (p.blendMode == BlendMode::Additive)
				{
					m_context->setBlendState(m_bsAdditive);
				}
				else
				{
					m_context->setBlendState(m_bsAlphaBlend);
				}

				auto tex = p.texturePath.empty() ? m_whiteTexture : getOrCreateTexture(p.texturePath, true);
				m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO, tex ? tex : m_whiteTexture);

				CBPerMaterial pMat{};
				pMat.baseColorFactor = p.color;
				pMat.roughnessFactor = 1.0f;
				pMat.metallicFactor = 0.0f;
				pMat.tilingOffset = Math::Vector4(1.0f, 1.0f, 0.0f, 0.0f);
				pMat.emissiveFactor = Math::Vector4(p.color.x * 2.0f, p.color.y * 2.0f, p.color.z * 2.0f, 0.0f);
				if (m_cbMaterial)
				{
					m_context->updateSubresource(m_cbMaterial, &pMat, sizeof(pMat));
					m_context->setConstantBufferPS(DF_SLOT_CB_PER_MATERIAL, m_cbMaterial);
					m_context->setConstantBufferVS(DF_SLOT_CB_PER_MATERIAL, m_cbMaterial);
				}

				Math::Matrix world = Math::Matrix::Identity;
				world._11 = camR.x * p.size; world._12 = camR.y * p.size; world._13 = camR.z * p.size;
				world._21 = camU.x * p.size; world._22 = camU.y * p.size; world._23 = camU.z * p.size;
				world._31 = camF.x * p.size; world._32 = camF.y * p.size; world._33 = camF.z * p.size;
				world._41 = p.position.x;    world._42 = p.position.y;    world._43 = p.position.z;

				CBPerObject obj{};
				obj.worldMatrix = world;
				obj.invWorldMatrix = world.Invert();
				obj.entityId = 0;

				auto alloc = m_uploadRing.allocate(sizeof(CBPerObject), &obj);
				if (alloc.isValid())
				{
					m_context->setConstantBufferSubrangeVS(DF_SLOT_CB_PER_OBJECT, alloc.buffer, alloc.firstConstant16, alloc.numConstants16);
				}

				m_context->drawIndexed(m_billboardQuadMesh.indexCount, 0, 0);
				m_drawCallCount++;
			}
		}

		m_context->setBlendState(nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_SKY_EQUIRECT, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_BRDF_LUT, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_SHADOW_MAP, nullptr);
		m_context->setShaderResourceBufferPS(DF_SLOT_BUF_LIGHTS, nullptr);
		m_context->setRenderTargets({}, nullptr);
	}
}
