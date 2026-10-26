#include "Engine/Renderer/SceneRenderer.h"
#include "Engine/Core/Log/Log.h"
#include <algorithm>
#include <cmath>
#include <d3d11.h>

namespace Core::Renderer
{
	const std::vector<Submesh>& SceneRenderer::selectLodSubmeshes(
		const GpuMesh& gpuMesh,
		float distanceToCamera,
		int32_t lodBias,
		uint32_t& outTotalIndexCount) const
	{
		if (gpuMesh.lodLevels.empty())
		{
			outTotalIndexCount = gpuMesh.indexCount;
			return gpuMesh.submeshes;
		}

		size_t lodIdx = 0;
		if (lodBias >= 1 && lodBias <= 3)
		{
			lodIdx = static_cast<size_t>(lodBias - 1);
		}
		else
		{
			if (distanceToCamera >= 48.0f) lodIdx = 2;
			else if (distanceToCamera >= 22.0f) lodIdx = 1;
			else lodIdx = 0;
		}

		lodIdx = std::min(lodIdx, gpuMesh.lodLevels.size() - 1);
		outTotalIndexCount = gpuMesh.lodLevels[lodIdx].indexCount;
		return gpuMesh.lodLevels[lodIdx].submeshes;
	}

	void SceneRenderer::executePasses(const RenderScene& scene, const RenderView& view)
	{
		RenderPipelineType pipeType = static_cast<RenderPipelineType>(std::clamp<uint32_t>(scene.environment.renderPipeline, 0u, 2u));
		if (pipeType == RenderPipelineType::Custom && m_customPipeline.getPasses().empty())
		{
			pipeType = RenderPipelineType::ForwardPlus;
		}
		getPipeline(pipeType).execute(*this, scene, view);
	}

	void SceneRenderer::passShadows(const RenderScene& scene, const RenderView& view)
	{
		if (!m_shadowMapTexture || !m_vsShadow || !m_dssShadow)
		{
			return;
		}

		bool shadowsEnabled = scene.environment.enableShadows;
		if (view.overrides.overrideShadows)
		{
			shadowsEnabled = view.overrides.enableShadows;
		}

		bool ssaoEnabled = scene.environment.enableSSAO;
		if (view.overrides.overrideSSAO)
		{
			ssaoEnabled = view.overrides.enableSSAO;
		}
		float ssaoDirectInfluence = ssaoEnabled ? scene.environment.ssaoDirectLightInfluence : 0.0f;

		// 1. Find primary light that casts shadows (directional light first, then spot light)
		const RenderLight* shadowLight = nullptr;
		if (shadowsEnabled)
		{
			for (const auto& l : scene.lights)
			{
				if (l.type == 0 && l.castShadows) // Directional Light
				{
					shadowLight = &l;
					break;
				}
			}
			if (!shadowLight)
			{
				for (const auto& l : scene.lights)
				{
					if (l.type == 2 && l.castShadows) // Spot Light
					{
						shadowLight = &l;
						break;
					}
				}
			}
		}

		Math::Matrix lightView = Math::Matrix::Identity;
		Math::Matrix lightProj = Math::Matrix::Identity;
		Math::Matrix lightViewProj = Math::Matrix::Identity;

		float shadowDist = shadowLight ? (shadowLight->type == 2 ? shadowLight->range : shadowLight->shadowDistance) : 60.0f;
		if (shadowDist <= 0.0f) shadowDist = 60.0f;
		float shadowBias = shadowLight ? shadowLight->shadowBias : 0.0015f;
		float shadowNormalBias = shadowLight ? shadowLight->shadowNormalBias : 0.02f;
		float shadowSoftness = shadowLight ? shadowLight->shadowSoftness : 1.0f;

		Math::Matrix cascadeMatrices[4] = {};
		float splits[5] = { std::max(scene.camera.nearZ, 0.1f), 0.0f, 0.0f, 0.0f, std::max(shadowDist, 10.0f) };
		bool isMultiCascade = (shadowLight && shadowLight->type == 0);

		if (isMultiCascade) // Directional Light 4-Cascade CSM
		{
			float fNear = splits[0];
			float fFar = splits[4];
			for (int i = 1; i <= 4; ++i)
			{
				float p = static_cast<float>(i) / 4.0f;
				float zLog = fNear * std::pow(fFar / fNear, p);
				float zUni = fNear + (fFar - fNear) * p;
				splits[i] = 0.75f * zLog + 0.25f * zUni;
			}

			Math::Vector3 lightDir = shadowLight->direction;
			if (lightDir.LengthSquared() < 1e-4f) lightDir = Math::Vector3(0.577f, -0.577f, 0.577f);
			lightDir.Normalize();
			Math::Vector3 up = Math::Vector3(0.0f, 1.0f, 0.0f);
			if (std::abs(lightDir.Dot(up)) > 0.99f) up = Math::Vector3(0.0f, 0.0f, 1.0f);

			float tanHalfFov = std::tan(scene.camera.fov * 0.5f);
			float aspect = static_cast<float>(m_width) / static_cast<float>(std::max(m_height, 1u));
			float cascadeSize = (kShadowMapResolution == 4096) ? 1024.0f : static_cast<float>(kShadowMapResolution) * 0.5f;

			for (int c = 0; c < 4; ++c)
			{
				float zNear_c = splits[c];
				float zFar_c = splits[c + 1];

				float xNear = zNear_c * tanHalfFov * aspect;
				float yNear = zNear_c * tanHalfFov;
				float xFar = zFar_c * tanHalfFov * aspect;
				float yFar = zFar_c * tanHalfFov;

				Math::Vector3 cornersCS[8] = {
					Math::Vector3(-xNear, -yNear, zNear_c),
					Math::Vector3( xNear, -yNear, zNear_c),
					Math::Vector3( xNear,  yNear, zNear_c),
					Math::Vector3(-xNear,  yNear, zNear_c),
					Math::Vector3(-xFar,  -yFar,  zFar_c),
					Math::Vector3( xFar,  -yFar,  zFar_c),
					Math::Vector3( xFar,   yFar,  zFar_c),
					Math::Vector3(-xFar,   yFar,  zFar_c)
				};

				Math::Vector3 center(0.0f, 0.0f, 0.0f);
				for (int k = 0; k < 8; ++k)
				{
					center += Math::Vector3::Transform(cornersCS[k], scene.camera.invViewMatrix);
				}
				center /= 8.0f;

				float radius = 0.0f;
				for (int k = 0; k < 8; ++k)
				{
					Math::Vector3 pWS = Math::Vector3::Transform(cornersCS[k], scene.camera.invViewMatrix);
					radius = std::max(radius, (pWS - center).Length());
				}
				radius = std::ceil(radius * 16.0f) / 16.0f;

				// Texel snapping in light projection space
				float texelSize = (radius * 2.0f) / cascadeSize;
				Math::Matrix lightViewZero = Math::lookAtLH(Math::Vector3(0, 0, 0), lightDir, up);
				Math::Vector3 centerLS = Math::Vector3::Transform(center, lightViewZero);
				centerLS.x = std::floor(centerLS.x / texelSize) * texelSize;
				centerLS.y = std::floor(centerLS.y / texelSize) * texelSize;

				Math::Matrix invLightViewZero = lightViewZero.Invert();
				center = Math::Vector3::Transform(centerLS, invLightViewZero);

				float casterMargin = 80.0f;
				Math::Vector3 eye = center - lightDir * (radius + casterMargin);
				Math::Vector3 target = center;
				Math::Matrix cView = Math::lookAtLH(eye, target, up);
				Math::Matrix cProj = Math::orthographicLH(radius * 2.0f, radius * 2.0f, 0.1f, (radius + casterMargin) * 2.0f);
				cascadeMatrices[c] = cView * cProj;
			}
		}
		else if (shadowLight && shadowLight->type == 2) // Spot Light Shadow Mapping (Perspective)
		{
			Math::Vector3 lightDir = shadowLight->direction;
			if (lightDir.LengthSquared() < 1e-4f) lightDir = Math::Vector3(0.0f, -1.0f, 0.0f);
			lightDir.Normalize();

			Math::Vector3 eye = shadowLight->position;
			Math::Vector3 center = shadowLight->position + lightDir * shadowDist;
			Math::Vector3 up = Math::Vector3(0.0f, 1.0f, 0.0f);
			if (std::abs(lightDir.Dot(up)) > 0.99f) up = Math::Vector3(0.0f, 0.0f, 1.0f);

			lightView = Math::lookAtLH(eye, center, up);
			float fov = std::clamp(shadowLight->outerConeAngle * 2.0f, 0.1f, 3.1f);
			lightProj = Math::perspectiveFovLH(fov, 1.0f, 0.2f, shadowDist);
			lightViewProj = lightView * lightProj;
		}

		// 2. Setup CBShadow constant buffer
		CBShadow cbShadow{};
		if (isMultiCascade)
		{
			cbShadow.lightViewProj = cascadeMatrices[0];
			cbShadow.cascadeViewProj1 = cascadeMatrices[1];
			cbShadow.cascadeViewProj2 = cascadeMatrices[2];
			cbShadow.cascadeViewProj3 = cascadeMatrices[3];
			cbShadow.cascadeSplits = Math::Vector4(splits[1], splits[2], splits[3], splits[4]);
			cbShadow.shadowParams = Math::Vector4(shadowBias, shadowNormalBias, 1.0f / static_cast<float>(kShadowMapResolution), shadowSoftness);
			cbShadow.shadowExtra = Math::Vector4(1.0f, ssaoDirectInfluence, 1.0f, 4.0f);
		}
		else
		{
			cbShadow.lightViewProj = lightViewProj;
			cbShadow.cascadeViewProj1 = lightViewProj;
			cbShadow.cascadeViewProj2 = lightViewProj;
			cbShadow.cascadeViewProj3 = lightViewProj;
			cbShadow.cascadeSplits = Math::Vector4(shadowDist * 0.15f, shadowDist * 0.35f, shadowDist * 0.65f, shadowDist);
			cbShadow.shadowParams = Math::Vector4(shadowBias, shadowNormalBias, 1.0f / static_cast<float>(kShadowMapResolution), shadowSoftness);
			cbShadow.shadowExtra = Math::Vector4(1.0f, ssaoDirectInfluence, shadowLight ? 1.0f : 0.0f, 1.0f);
		}

		if (m_cbShadow)
		{
			m_context->updateSubresource(m_cbShadow, &cbShadow, sizeof(cbShadow));
		}

		if (!shadowLight)
		{
			m_context->clearDepthStencil(m_shadowMapTexture, 1.0f, 0);
			return;
		}

		// Clear shadow map
		m_context->clearDepthStencil(m_shadowMapTexture, 1.0f, 0);
		m_context->setRenderTargets({}, m_shadowMapTexture);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		if (m_inputLayoutShadow) m_context->setInputLayout(m_inputLayoutShadow);
		else if (m_inputLayout) m_context->setInputLayout(m_inputLayout);

		m_context->setVertexShader(m_vsShadow);
		m_context->setPixelShader(nullptr);
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		if (m_dssShadow) m_context->setDepthStencilState(m_dssShadow, 0);
		m_context->setBlendState(nullptr);
		if (m_samplerLinearWrap) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_WRAP, m_samplerLinearWrap);
		if (m_samplerAnisoWrap) m_context->setSamplerPS(DF_SLOT_SMP_ANISO_WRAP, m_samplerAnisoWrap);

		m_context->setConstantBufferVS(DF_SLOT_CB_SHADOW, m_cbShadow);

		auto renderShadowCasters = [&](const Math::Matrix& vp, uint32_t cascadeIdx) {
			int32_t shadowLodBias = (scene.environment.meshLodBias == 0 && cascadeIdx >= 2) ? static_cast<int32_t>(cascadeIdx) : scene.environment.meshLodBias;

			for (const auto& mesh : scene.meshes)
			{
				if (!mesh.castShadows) continue;
				if (mesh.layer == RenderLayer::Transparent) continue;

				const auto gpuMesh = getOrCreateMesh(mesh.meshPath);
				if (!gpuMesh.vertexBuffer || !gpuMesh.indexBuffer || gpuMesh.indexCount == 0) continue;

				Math::Matrix worldLightVP = mesh.world * vp;

				if (gpuMesh.submeshes.size() > 1)
				{
					DirectX::XMFLOAT3 meshCorners[8];
					gpuMesh.localBounds.GetCorners(meshCorners);
					bool allNegX = true, allPosX = true;
					bool allNegY = true, allPosY = true;
					bool allNegZ = true, allPosZ = true;

					for (int i = 0; i < 8; ++i)
					{
						Math::Vector4 p(meshCorners[i].x, meshCorners[i].y, meshCorners[i].z, 1.0f);
						Math::Vector4 clip = Math::Vector4::Transform(p, worldLightVP);
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

				uint32_t lodTotalIdxCount = 0;
				const auto& lodSubmeshes = selectLodSubmeshes(gpuMesh, mesh.distanceToCamera, shadowLodBias, lodTotalIdxCount);

				if (!lodSubmeshes.empty())
				{
					for (const auto& sub : lodSubmeshes)
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
							Math::Vector4 clip = Math::Vector4::Transform(p, worldLightVP);
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

						if (sub.isAlphaCutout && m_psShadowCutout)
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
							m_context->setPixelShader(m_psShadowCutout);
							m_context->setRasterizerState(m_rsCullNone);
							m_context->drawIndexed(sub.indexCount, sub.startIndexLocation, sub.baseVertexLocation);
							m_drawCallCount++;
							m_context->setPixelShader(nullptr);
							m_context->setRasterizerState(m_rsSolid);
						}
						else
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
					m_context->drawIndexed(lodTotalIdxCount > 0 ? lodTotalIdxCount : gpuMesh.indexCount, 0, 0);
					m_drawCallCount++;
				}
			}

			// Terrain shadow casters
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
		};

		if (isMultiCascade)
		{
			float cascadeSize = (kShadowMapResolution == 4096) ? 1024.0f : static_cast<float>(kShadowMapResolution) * 0.5f;
			for (uint32_t c = 0; c < 4; ++c)
			{
				float vpX = static_cast<float>(c % 2) * cascadeSize;
				float vpY = static_cast<float>(c / 2) * cascadeSize;
				RHI::Viewport cascadeVp{ vpX, vpY, cascadeSize, cascadeSize, 0.0f, 1.0f };
				m_context->setViewport(cascadeVp);

				CBShadow cbPass = cbShadow;
				cbPass.lightViewProj = cascadeMatrices[c];
				m_context->updateSubresource(m_cbShadow, &cbPass, sizeof(cbPass));

				renderShadowCasters(cascadeMatrices[c], c);
			}
			// Restore full multi-cascade buffer
			m_context->updateSubresource(m_cbShadow, &cbShadow, sizeof(cbShadow));
		}
		else
		{
			RHI::Viewport shadowVp{ 0.0f, 0.0f, static_cast<float>(kShadowMapResolution), static_cast<float>(kShadowMapResolution), 0.0f, 1.0f };
			m_context->setViewport(shadowVp);
			renderShadowCasters(lightViewProj, 0);
		}

		// Render dynamic Spot & Point lights into Shadow Atlas
		m_shadowAtlasAllocator.reset();
		bool lightsUpdated = false;
		std::array<LightData, 64> lightArray{};

		uint32_t activeLightCount = static_cast<uint32_t>(std::min<size_t>(scene.lights.size(), 64));
		for (uint32_t i = 0; i < activeLightCount; ++i)
		{
			const auto& l = scene.lights[i];
			LightData ld{};
			ld.positionType = Math::Vector4(l.position.x, l.position.y, l.position.z, static_cast<float>(l.type));
			ld.directionRange = Math::Vector4(l.direction.x, l.direction.y, l.direction.z, l.range);
			ld.colorIntensity = Math::Vector4(l.color.x, l.color.y, l.color.z, l.intensity);
			ld.spotAnglesPad = Math::Vector4(std::cos(l.innerConeAngle), std::cos(l.outerConeAngle), 0.0f, 0.0f);
			ld.shadowViewProj = Math::Matrix::Identity;
			ld.shadowAtlasRect = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);

			if (l.castShadows && (&l != shadowLight || !isMultiCascade))
			{
				uint32_t outX = 0, outY = 0, outSize = 0;
				Math::Vector4 outUV{ 0.0f, 0.0f, 0.0f, 0.0f };

				if (l.type == 2) // Spot Light
				{
					if (m_shadowAtlasAllocator.allocate(i, 512, outX, outY, outSize, outUV) ||
					    m_shadowAtlasAllocator.allocate(i, 256, outX, outY, outSize, outUV))
					{
						Math::Vector3 lightDir = l.direction;
						if (lightDir.LengthSquared() < 1e-4f) lightDir = Math::Vector3(0.0f, -1.0f, 0.0f);
						lightDir.Normalize();

						Math::Vector3 eye = l.position;
						Math::Vector3 center = l.position + lightDir * (l.range > 0.0f ? l.range : 25.0f);
						Math::Vector3 up = Math::Vector3(0.0f, 1.0f, 0.0f);
						if (std::abs(lightDir.Dot(up)) > 0.99f) up = Math::Vector3(0.0f, 0.0f, 1.0f);

						Math::Matrix sView = Math::lookAtLH(eye, center, up);
						float fov = std::clamp(l.outerConeAngle * 2.0f, 0.1f, 3.1f);
						Math::Matrix sProj = Math::perspectiveFovLH(fov, 1.0f, 0.1f, l.range > 0.0f ? l.range : 25.0f);
						Math::Matrix sViewProj = sView * sProj;

						RHI::Viewport spotVp{ static_cast<float>(outX), static_cast<float>(outY), static_cast<float>(outSize), static_cast<float>(outSize), 0.0f, 1.0f };
						m_context->setViewport(spotVp);

						CBShadow cbSpot = cbShadow;
						cbSpot.lightViewProj = sViewProj;
						m_context->updateSubresource(m_cbShadow, &cbSpot, sizeof(cbSpot));

						m_context->setVertexShader(m_vsShadow);
						if (m_inputLayoutShadow) m_context->setInputLayout(m_inputLayoutShadow);
						renderShadowCasters(sViewProj, 0);

						ld.spotAnglesPad.z = 1.0f; // has shadow
						ld.shadowViewProj = sViewProj;
						ld.shadowAtlasRect = outUV;
						lightsUpdated = true;
					}
				}
				else if (l.type == 1) // Point Light Dual-Paraboloid
				{
					if (m_vsShadowParaboloid &&
					    (m_shadowAtlasAllocator.allocate(i, 512, outX, outY, outSize, outUV) ||
					     m_shadowAtlasAllocator.allocate(i, 256, outX, outY, outSize, outUV)))
					{
						Math::Vector3 eye = l.position;
						Math::Vector3 center = eye + Math::Vector3(0.0f, 0.0f, 1.0f);
						Math::Vector3 up = Math::Vector3(0.0f, 1.0f, 0.0f);
						Math::Matrix pView = Math::lookAtLH(eye, center, up);

						float pNear = 0.1f;
						float pFar = l.range > 0.0f ? l.range : 20.0f;
						float halfSlot = static_cast<float>(outSize) * 0.5f;

						// Render Front Hemisphere (zSign = +1.0)
						RHI::Viewport frontVp{ static_cast<float>(outX), static_cast<float>(outY), halfSlot, static_cast<float>(outSize), 0.0f, 1.0f };
						m_context->setViewport(frontVp);

						CBParaboloidShadow cbPara{};
						cbPara.lightViewMatrix = pView;
						cbPara.paraboloidParams = Math::Vector4(pNear, pFar, 1.0f, 0.0f);
						cbPara.atlasViewport = Math::Vector4(0.0f, 0.0f, 1.0f, 1.0f);
						if (m_cbParaboloidShadow)
						{
							m_context->updateSubresource(m_cbParaboloidShadow, &cbPara, sizeof(cbPara));
							m_context->setConstantBufferVS(0, m_cbParaboloidShadow);
						}

						m_context->setVertexShader(m_vsShadowParaboloid);
						if (m_inputLayoutShadowParaboloid) m_context->setInputLayout(m_inputLayoutShadowParaboloid);
						renderShadowCasters(pView, 0);

						// Render Back Hemisphere (zSign = -1.0)
						RHI::Viewport backVp{ static_cast<float>(outX) + halfSlot, static_cast<float>(outY), halfSlot, static_cast<float>(outSize), 0.0f, 1.0f };
						m_context->setViewport(backVp);

						cbPara.paraboloidParams.z = -1.0f;
						if (m_cbParaboloidShadow)
						{
							m_context->updateSubresource(m_cbParaboloidShadow, &cbPara, sizeof(cbPara));
						}
						renderShadowCasters(pView, 0);

						ld.spotAnglesPad.z = 1.0f; // has shadow
						ld.spotAnglesPad.w = 1.0f; // is point paraboloid
						ld.shadowViewProj = pView;
						ld.shadowAtlasRect = outUV;
						lightsUpdated = true;
					}
				}
			}

			lightArray[i] = ld;
		}

		if (lightsUpdated && m_bufLights)
		{
			m_context->updateSubresource(m_bufLights, lightArray.data(), sizeof(LightData) * 64);
		}

		m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO, nullptr);
		m_context->setPixelShader(nullptr);
		m_context->setRasterizerState(m_rsSolid);

		// Unbind DSV so shadow map can be read as SRV in passOpaque
		m_context->setRenderTargets({}, nullptr);

		// Restore full viewport
		RHI::Viewport fullVp{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
		m_context->setViewport(fullVp);
	}
}
