#include "SceneRenderer.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/Core/VFS/AssetPathResolver.h"
#include <nlohmann/json.hpp>
#include <filesystem>
#include <string>

namespace Core::Renderer
{
	GpuMesh SceneRenderer::createGpuMesh(const MeshData& meshData)
	{
		GpuMesh mesh;
		if (meshData.vertices.empty() || meshData.indices.empty())
		{
			return mesh;
		}

		// Vertex Buffer
		RHI::GpuBufferDesc vbDesc{};
		vbDesc.size = meshData.vertices.size() * sizeof(VertexMesh);
		vbDesc.usage = RHI::BufferUsage::Default;
		vbDesc.bindFlags = RHI::BufferBindFlags::VertexBuffer;
		auto vbRes = m_device.createBuffer(vbDesc, meshData.vertices.data());
		if (vbRes.isOk()) mesh.vertexBuffer = *vbRes;

		// Index Buffer
		RHI::GpuBufferDesc ibDesc{};
		ibDesc.size = meshData.indices.size() * sizeof(uint32_t);
		ibDesc.usage = RHI::BufferUsage::Default;
		ibDesc.bindFlags = RHI::BufferBindFlags::IndexBuffer;
		auto ibRes = m_device.createBuffer(ibDesc, meshData.indices.data());
		if (ibRes.isOk()) mesh.indexBuffer = *ibRes;

		mesh.indexCount = static_cast<uint32_t>(meshData.indices.size());
		mesh.submeshes = meshData.submeshes;
		mesh.lodLevels = meshData.lodLevels;
		mesh.localBounds = meshData.localBounds;

		return mesh;
	}

	GpuMesh SceneRenderer::getOrCreateMesh(const std::string& meshPath)
	{
		auto it = m_meshCache.find(meshPath);
		if (it != m_meshCache.end())
		{
			return it->second;
		}

		MeshData meshData = LoadMesh(meshPath);
		GpuMesh gpu = createGpuMesh(meshData);
		m_meshCache[meshPath] = gpu;
		return gpu;
	}

	std::shared_ptr<Material> SceneRenderer::getOrCreateMaterial(const std::string& materialPath)
	{
		if (materialPath.empty())
		{
			static auto s_defaultMat = std::make_shared<Material>("DefaultMaterial");
			return s_defaultMat;
		}

		auto it = m_materialCache.find(materialPath);
		if (it != m_materialCache.end())
		{
			return it->second;
		}

		if (materialPath.starts_with("fbx_mat:"))
		{
			std::string rest = materialPath.substr(8);
			std::string matName;
			std::string albedoPath;
			std::string normalPath;
			std::string roughnessPath;
			std::string metallicPath;
			std::string cutoutStr;
			std::string transmissionStr;
			std::string clothStr;
			std::string roughnessFactorStr;

			size_t firstPipe = rest.find('|');
			if (firstPipe == std::string::npos)
			{
				matName = rest;
			}
			else
			{
				matName = rest.substr(0, firstPipe);
				std::string params = rest.substr(firstPipe + 1);
				size_t start = 0;
				while (start < params.length())
				{
					size_t nextPipe = params.find('|', start);
					std::string token = (nextPipe == std::string::npos) ? params.substr(start) : params.substr(start, nextPipe - start);
					size_t eq = token.find('=');
					if (eq != std::string::npos)
					{
						std::string key = token.substr(0, eq);
						std::string val = token.substr(eq + 1);
						if (key == "albedo") albedoPath = val;
						else if (key == "normal") normalPath = val;
						else if (key == "roughness") roughnessPath = val;
						else if (key == "metallic") metallicPath = val;
						else if (key == "cutout") cutoutStr = val;
						else if (key == "transmission") transmissionStr = val;
						else if (key == "cloth") clothStr = val;
						else if (key == "roughnessFactor") roughnessFactorStr = val;
					}
					if (nextPipe == std::string::npos) break;
					start = nextPipe + 1;
				}
			}

			auto mat = std::make_shared<Material>(matName);
			if (!albedoPath.empty())
			{
				mat->setAlbedoPath(albedoPath);
				mat->setAlbedoTexture(getOrCreateTexture(albedoPath, true));
			}
			if (!normalPath.empty())
			{
				mat->setNormalPath(normalPath);
				mat->setNormalTexture(getOrCreateTexture(normalPath, false));
				// Unity FBX normal maps are authored in OpenGL (+Y) format; flip Y for DirectX (-Y)
				mat->setFlipNormalY(true);
			}
			if (!roughnessPath.empty() || !metallicPath.empty())
			{
				auto ormTex = getOrCreateORMTexture(roughnessPath, metallicPath);
				mat->setMetallicRoughnessPath("orm:" + roughnessPath + "|" + metallicPath);
				mat->setMetallicRoughnessTexture(ormTex);
				mat->setORMPacked(true);
				mat->setRoughness(1.0f);
				mat->setMetallic((!metallicPath.empty() && metallicPath.find("Dielectric") == std::string::npos) ? 1.0f : 0.0f);
			}

			if (cutoutStr == "1")
			{
				mat->setBlendMode(BlendMode::Masked);
				mat->setAlphaCutoff(0.45f);
				mat->setTwoSided(true);
				mat->setCullMode(RHI::CullMode::None);
			}

			if (transmissionStr == "1")
			{
				mat->setTwoSided(true);
				mat->setCullMode(RHI::CullMode::None);
				mat->setTransmission(true);
				mat->setTransmissionFactor(0.65f);
				mat->setTransmissionColor(Math::Vector3(0.42f, 0.82f, 0.28f));
			}

			if (clothStr == "1")
			{
				mat->setTwoSided(true);
				mat->setCullMode(RHI::CullMode::None);
				mat->setCloth(true);
				mat->setSheenFactor(0.6f);
				mat->setRoughness(0.70f);
				mat->setNormalScale(0.85f);
			}
			else
			{
				std::string lowerMat = matName;
				std::transform(lowerMat.begin(), lowerMat.end(), lowerMat.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });

				// Match Unity Classic Sponza authored normal scales:
				// Floor: 0.5, Ceiling: 0.5, Flagpoles: 0.5, Details: 0.75, Vase Octagonal: 0.5
				// Soften brick / column relief from harsh 1.0 to 0.80 so walls don't look overly rough or catch blinding light
				if (lowerMat.find("floor") != std::string::npos)
				{
					mat->setNormalScale(0.5f);
				}
				else if (lowerMat.find("ceiling") != std::string::npos || lowerMat.find("roof") != std::string::npos)
				{
					mat->setNormalScale(0.5f);
				}
				else if (lowerMat.find("detail") != std::string::npos)
				{
					mat->setNormalScale(0.75f);
				}
				else if (lowerMat.find("flagpole") != std::string::npos || lowerMat.find("chain") != std::string::npos)
				{
					mat->setNormalScale(0.5f);
				}
				else if (lowerMat.find("brick") != std::string::npos || lowerMat.find("arch") != std::string::npos || lowerMat.find("column") != std::string::npos)
				{
					mat->setNormalScale(0.80f);
				}
			}

			if (!roughnessFactorStr.empty())
			{
				try
				{
					mat->setRoughness(std::stof(roughnessFactorStr));
				}
				catch (...)
				{
				}
			}

			m_materialCache[materialPath] = mat;
			return mat;
		}

		auto mat = std::make_shared<Material>(materialPath);
		auto textRes = Core::AssetPathResolver::readText(materialPath);
		if (textRes.isOk() && !textRes.value().empty())
		{
			if (!mat->loadFromJson(textRes.value()))
			{
				DF_LOG_WARN("Renderer", "Failed to parse material: {}", materialPath);
			}
		}

		// Resolve texture references onto GPU textures
		if (!mat->getAlbedoTexturePath().empty())
		{
			mat->setAlbedoTexture(getOrCreateTexture(mat->getAlbedoTexturePath(), true));
		}
		if (!mat->getNormalTexturePath().empty())
		{
			mat->setNormalTexture(getOrCreateTexture(mat->getNormalTexturePath(), false));
		}
		if (!mat->getMetallicRoughnessTexturePath().empty())
		{
			mat->setMetallicRoughnessTexture(getOrCreateTexture(mat->getMetallicRoughnessTexturePath(), false));
		}
		if (!mat->getEmissiveTexturePath().empty())
		{
			mat->setEmissiveTexture(getOrCreateTexture(mat->getEmissiveTexturePath(), true));
		}
		if (!mat->getOcclusionTexturePath().empty())
		{
			mat->setOcclusionTexture(getOrCreateTexture(mat->getOcclusionTexturePath(), false));
		}
		if (!mat->getDetailTexturePath().empty())
		{
			mat->setDetailTexture(getOrCreateTexture(mat->getDetailTexturePath(), false));
		}

		m_materialCache[materialPath] = mat;
		return mat;
	}
}
