#include "Material.h"
#include <nlohmann/json.hpp>
#include <fstream>
#include <algorithm>

namespace Core::Renderer
{
	Material::Material()
		: Material("DefaultMaterial")
	{
	}

	Material::Material(std::string name)
		: m_name(std::move(name))
	{
	}

	CBPerMaterial Material::getBufferData() const noexcept
	{
		CBPerMaterial data{};
		data.baseColorFactor = m_baseColor;
		data.emissiveFactor = Math::Vector4(m_emissiveColor.x, m_emissiveColor.y, m_emissiveColor.z, m_emissiveIntensity);
		data.tilingOffset = Math::Vector4(m_uvTiling.x, m_uvTiling.y, m_uvOffset.x, m_uvOffset.y);
		data.roughnessFactor = m_roughness;
		data.metallicFactor = m_metallic;
		data.normalScale = m_normalScale;
		data.occlusionStrength = m_occlusionStrength;
		data.alphaCutoff = m_alphaCutoff;
		data.transmissionFactor = m_transmissionFactor;
		data.sheenFactor = m_sheenFactor;
		data.transmissionColor = Math::Vector4(m_transmissionColor.x, m_transmissionColor.y, m_transmissionColor.z, m_clearCoatFactor);
		data.detailTilingScale = Math::Vector4(m_detailTiling.x, m_detailTiling.y, m_detailNormalScale, m_detailAlbedoScale);
		data.extraMaterialParams = Math::Vector4(m_clearCoatFactor, m_clearCoatRoughness, m_anisotropyFactor, m_anisotropyRotation);

		uint32_t flags = 0;
		if (!m_albedoPath.empty() || m_albedoTexture) flags |= DF_MAT_FLAG_HAS_ALBEDO_MAP;
		if (!m_normalPath.empty() || m_normalTexture) flags |= DF_MAT_FLAG_HAS_NORMAL_MAP;
		if (!m_metallicRoughnessPath.empty() || m_metallicRoughnessTexture) flags |= DF_MAT_FLAG_HAS_MR_MAP;
		if (!m_emissivePath.empty() || m_emissiveTexture) flags |= DF_MAT_FLAG_HAS_EMISSIVE_MAP;
		if (!m_occlusionPath.empty() || m_occlusionTexture) flags |= DF_MAT_FLAG_HAS_OCCLUSION_MAP;
		if (m_flipNormalY) flags |= DF_MAT_FLAG_FLIP_NORMAL_Y;
		if (m_isORMPacked) flags |= DF_MAT_FLAG_ORM_PACKED;
		if (m_twoSided) flags |= DF_MAT_FLAG_TWO_SIDED;
		if (m_isMaskMapPacked) flags |= DF_MAT_FLAG_MASK_MAP_PACKED;
		if (m_isCloth || m_sheenFactor > 0.001f) flags |= DF_MAT_FLAG_CLOTH;
		if (m_isTransmission || m_transmissionFactor > 0.001f) flags |= DF_MAT_FLAG_TRANSMISSION;
		if (m_isClearCoat || m_clearCoatFactor > 0.001f) flags |= DF_MAT_FLAG_CLEAR_COAT;
		if (m_isAnisotropy || std::abs(m_anisotropyFactor) > 0.001f) flags |= DF_MAT_FLAG_ANISOTROPY;
		if (m_isSubsurface || m_subsurfaceFactor > 0.001f) flags |= DF_MAT_FLAG_SUBSURFACE;
		if (!m_detailPath.empty() || m_detailTexture) flags |= DF_MAT_FLAG_HAS_DETAIL_MAP;

		data.materialFlags = flags;
		return data;
	}

	bool Material::loadFromJson(const std::string& jsonContent)
	{
		if (jsonContent.empty()) return false;

		try
		{
			auto j = nlohmann::json::parse(jsonContent);
			if (j.contains("name") && j["name"].is_string()) m_name = j["name"].get<std::string>();

			if (j.contains("baseColor") && j["baseColor"].is_array() && j["baseColor"].size() >= 3)
			{
				float r = j["baseColor"][0].get<float>();
				float g = j["baseColor"][1].get<float>();
				float b = j["baseColor"][2].get<float>();
				float a = (j["baseColor"].size() >= 4) ? j["baseColor"][3].get<float>() : 1.0f;
				m_baseColor = Math::Vector4(r, g, b, a);
			}

			if (j.contains("roughness") && j["roughness"].is_number()) m_roughness = j["roughness"].get<float>();
			if (j.contains("metallic") && j["metallic"].is_number()) m_metallic = j["metallic"].get<float>();
			if (j.contains("normalScale") && j["normalScale"].is_number()) m_normalScale = j["normalScale"].get<float>();
			if (j.contains("occlusionStrength") && j["occlusionStrength"].is_number()) m_occlusionStrength = j["occlusionStrength"].get<float>();
			if (j.contains("alphaCutoff") && j["alphaCutoff"].is_number()) m_alphaCutoff = j["alphaCutoff"].get<float>();
			if (j.contains("emissiveIntensity") && j["emissiveIntensity"].is_number()) m_emissiveIntensity = j["emissiveIntensity"].get<float>();

			if (j.contains("emissive") && j["emissive"].is_array() && j["emissive"].size() >= 3)
			{
				m_emissiveColor.x = j["emissive"][0].get<float>();
				m_emissiveColor.y = j["emissive"][1].get<float>();
				m_emissiveColor.z = j["emissive"][2].get<float>();
				if (j["emissive"].size() >= 4 && !j.contains("emissiveIntensity"))
				{
					float a = j["emissive"][3].get<float>();
					if (a > 0.0f) m_emissiveIntensity = a;
				}
			}

			if (j.contains("tiling") && j["tiling"].is_array() && j["tiling"].size() >= 2)
			{
				m_uvTiling.x = j["tiling"][0].get<float>();
				m_uvTiling.y = j["tiling"][1].get<float>();
			}

			if (j.contains("offset") && j["offset"].is_array() && j["offset"].size() >= 2)
			{
				m_uvOffset.x = j["offset"][0].get<float>();
				m_uvOffset.y = j["offset"][1].get<float>();
			}

			if (j.contains("flipNormalY") && j["flipNormalY"].is_boolean()) m_flipNormalY = j["flipNormalY"].get<bool>();
			if (j.contains("isORMPacked") && j["isORMPacked"].is_boolean()) m_isORMPacked = j["isORMPacked"].get<bool>();
			if (j.contains("twoSided") && j["twoSided"].is_boolean()) m_twoSided = j["twoSided"].get<bool>();
			if (j.contains("isMaskMapPacked") && j["isMaskMapPacked"].is_boolean()) m_isMaskMapPacked = j["isMaskMapPacked"].get<bool>();
			if (j.contains("isCloth") && j["isCloth"].is_boolean()) m_isCloth = j["isCloth"].get<bool>();
			if (j.contains("isTransmission") && j["isTransmission"].is_boolean()) m_isTransmission = j["isTransmission"].get<bool>();
			if (j.contains("clearCoat") && j["clearCoat"].is_boolean()) m_isClearCoat = j["clearCoat"].get<bool>();
			if (j.contains("clearCoatFactor") && j["clearCoatFactor"].is_number()) m_clearCoatFactor = j["clearCoatFactor"].get<float>();
			if (j.contains("clearCoatRoughness") && j["clearCoatRoughness"].is_number()) m_clearCoatRoughness = j["clearCoatRoughness"].get<float>();
			if (j.contains("anisotropy") && j["anisotropy"].is_boolean()) m_isAnisotropy = j["anisotropy"].get<bool>();
			if (j.contains("anisotropyFactor") && j["anisotropyFactor"].is_number()) m_anisotropyFactor = j["anisotropyFactor"].get<float>();
			if (j.contains("anisotropyRotation") && j["anisotropyRotation"].is_number()) m_anisotropyRotation = j["anisotropyRotation"].get<float>();
			if (j.contains("subsurface") && j["subsurface"].is_boolean()) m_isSubsurface = j["subsurface"].get<bool>();
			if (j.contains("subsurfaceFactor") && j["subsurfaceFactor"].is_number()) m_subsurfaceFactor = j["subsurfaceFactor"].get<float>();
			if (j.contains("transmissionFactor") && j["transmissionFactor"].is_number()) m_transmissionFactor = j["transmissionFactor"].get<float>();
			if (j.contains("sheenFactor") && j["sheenFactor"].is_number()) m_sheenFactor = j["sheenFactor"].get<float>();
			if (j.contains("detailNormalScale") && j["detailNormalScale"].is_number()) m_detailNormalScale = j["detailNormalScale"].get<float>();
			if (j.contains("detailAlbedoScale") && j["detailAlbedoScale"].is_number()) m_detailAlbedoScale = j["detailAlbedoScale"].get<float>();

			if (j.contains("transmissionColor") && j["transmissionColor"].is_array() && j["transmissionColor"].size() >= 3)
			{
				m_transmissionColor.x = j["transmissionColor"][0].get<float>();
				m_transmissionColor.y = j["transmissionColor"][1].get<float>();
				m_transmissionColor.z = j["transmissionColor"][2].get<float>();
			}

			if (j.contains("detailTiling") && j["detailTiling"].is_array() && j["detailTiling"].size() >= 2)
			{
				m_detailTiling.x = j["detailTiling"][0].get<float>();
				m_detailTiling.y = j["detailTiling"][1].get<float>();
			}

			// Texture Paths
			if (j.contains("albedoTexture") && j["albedoTexture"].is_string()) m_albedoPath = j["albedoTexture"].get<std::string>();
			if (j.contains("normalTexture") && j["normalTexture"].is_string()) m_normalPath = j["normalTexture"].get<std::string>();
			if (j.contains("metallicRoughnessTexture") && j["metallicRoughnessTexture"].is_string()) m_metallicRoughnessPath = j["metallicRoughnessTexture"].get<std::string>();
			if (j.contains("emissiveTexture") && j["emissiveTexture"].is_string()) m_emissivePath = j["emissiveTexture"].get<std::string>();
			if (j.contains("occlusionTexture") && j["occlusionTexture"].is_string()) m_occlusionPath = j["occlusionTexture"].get<std::string>();
			if (j.contains("detailTexture") && j["detailTexture"].is_string()) m_detailPath = j["detailTexture"].get<std::string>();

			// Render States
			if (j.contains("blendMode") && j["blendMode"].is_number_integer()) m_blendMode = static_cast<BlendMode>(j["blendMode"].get<uint8_t>());
			if (j.contains("depthMode") && j["depthMode"].is_number_integer()) m_depthMode = static_cast<DepthMode>(j["depthMode"].get<uint8_t>());
			if (j.contains("cullMode") && j["cullMode"].is_number_integer()) m_cullMode = static_cast<RHI::CullMode>(j["cullMode"].get<uint8_t>());

			m_isDirty = false;
			return true;
		}
		catch (...)
		{
			return false;
		}
	}

	std::string Material::toJson() const
	{
		nlohmann::json j;
		j["name"] = m_name;
		j["baseColor"] = { m_baseColor.x, m_baseColor.y, m_baseColor.z, m_baseColor.w };
		j["roughness"] = m_roughness;
		j["metallic"] = m_metallic;
		j["normalScale"] = m_normalScale;
		j["occlusionStrength"] = m_occlusionStrength;
		j["alphaCutoff"] = m_alphaCutoff;
		j["emissive"] = { m_emissiveColor.x, m_emissiveColor.y, m_emissiveColor.z, m_emissiveIntensity };
		j["emissiveIntensity"] = m_emissiveIntensity;
		j["tiling"] = { m_uvTiling.x, m_uvTiling.y };
		j["offset"] = { m_uvOffset.x, m_uvOffset.y };
		j["flipNormalY"] = m_flipNormalY;
		j["isORMPacked"] = m_isORMPacked;
		j["twoSided"] = m_twoSided;
		j["isMaskMapPacked"] = m_isMaskMapPacked;
		j["isCloth"] = m_isCloth;
		j["isTransmission"] = m_isTransmission;
		j["transmissionFactor"] = m_transmissionFactor;
		j["sheenFactor"] = m_sheenFactor;
		j["clearCoat"] = m_isClearCoat;
		j["clearCoatFactor"] = m_clearCoatFactor;
		j["clearCoatRoughness"] = m_clearCoatRoughness;
		j["anisotropy"] = m_isAnisotropy;
		j["anisotropyFactor"] = m_anisotropyFactor;
		j["anisotropyRotation"] = m_anisotropyRotation;
		j["subsurface"] = m_isSubsurface;
		j["subsurfaceFactor"] = m_subsurfaceFactor;
		j["transmissionColor"] = { m_transmissionColor.x, m_transmissionColor.y, m_transmissionColor.z };
		j["detailTiling"] = { m_detailTiling.x, m_detailTiling.y };
		j["detailNormalScale"] = m_detailNormalScale;
		j["detailAlbedoScale"] = m_detailAlbedoScale;

		j["albedoTexture"] = m_albedoPath;
		j["normalTexture"] = m_normalPath;
		j["metallicRoughnessTexture"] = m_metallicRoughnessPath;
		j["emissiveTexture"] = m_emissivePath;
		j["occlusionTexture"] = m_occlusionPath;
		j["detailTexture"] = m_detailPath;

		j["blendMode"] = static_cast<uint8_t>(m_blendMode);
		j["depthMode"] = static_cast<uint8_t>(m_depthMode);
		j["cullMode"] = static_cast<uint8_t>(m_cullMode);

		return j.dump(2);
	}

	bool Material::saveToFile(const std::filesystem::path& path)
	{
		std::error_code ec;
		std::filesystem::create_directories(path.parent_path(), ec);

		std::ofstream outFile(path, std::ios::trunc);
		if (!outFile.is_open()) return false;

		outFile << toJson();
		outFile.close();
		m_assetPath = path.generic_string();
		m_isDirty = false;
		return true;
	}

	void Material::bind(RHI::IDeviceContext& context, RHI::IRHIDevice& device, const std::shared_ptr<RHI::IGpuBuffer>& materialCb) const
	{
		if (m_vertexShader)
		{
			context.setVertexShader(m_vertexShader);
		}

		if (m_pixelShader)
		{
			context.setPixelShader(m_pixelShader);
		}

		if (materialCb)
		{
			auto data = getBufferData();
			context.updateSubresource(materialCb, &data, sizeof(data));
			context.setConstantBufferVS(DF_SLOT_CB_PER_MATERIAL, materialCb);
			context.setConstantBufferPS(DF_SLOT_CB_PER_MATERIAL, materialCb);
		}

		// Bind Material Textures (t0..t5)
		if (m_albedoTexture) context.setShaderResourcePS(DF_SLOT_TEX_ALBEDO, m_albedoTexture);
		if (m_normalTexture) context.setShaderResourcePS(DF_SLOT_TEX_NORMAL, m_normalTexture);
		if (m_metallicRoughnessTexture) context.setShaderResourcePS(DF_SLOT_TEX_METALLIC_ROUGHNESS, m_metallicRoughnessTexture);
		if (m_emissiveTexture) context.setShaderResourcePS(DF_SLOT_TEX_EMISSIVE, m_emissiveTexture);
		if (m_occlusionTexture) context.setShaderResourcePS(DF_SLOT_TEX_OCCLUSION, m_occlusionTexture);
		if (m_detailTexture) context.setShaderResourcePS(DF_SLOT_TEX_DETAIL, m_detailTexture);

		// Rasterizer State
		RHI::RasterizerStateDesc rsDesc{};
		rsDesc.cullMode = m_twoSided ? RHI::CullMode::None : m_cullMode;
		rsDesc.fillMode = RHI::FillMode::Solid;
		context.setRasterizerState(device.getOrCreateRasterizerState(rsDesc));

		// Depth Stencil State (Reversed-Z)
		RHI::DepthStencilStateDesc dsDesc{};
		if (m_depthMode == DepthMode::ReadWrite)
		{
			dsDesc.depthEnable = true;
			dsDesc.depthWrite = true;
			dsDesc.depthFunc = RHI::ComparisonFunc::GreaterEqual;
		}
		else if (m_depthMode == DepthMode::ReadOnly)
		{
			dsDesc.depthEnable = true;
			dsDesc.depthWrite = false;
			dsDesc.depthFunc = RHI::ComparisonFunc::GreaterEqual;
		}
		else
		{
			dsDesc.depthEnable = false;
			dsDesc.depthWrite = false;
		}
		context.setDepthStencilState(device.getOrCreateDepthStencilState(dsDesc));

		// Blend State
		RHI::BlendStateDesc bDesc{};
		if (m_blendMode == BlendMode::Additive)
		{
			bDesc.blendEnable = true;
			bDesc.srcBlend = RHI::BlendFactor::One;
			bDesc.destBlend = RHI::BlendFactor::One;
			bDesc.blendOp = RHI::BlendOp::Add;
			bDesc.srcBlendAlpha = RHI::BlendFactor::One;
			bDesc.destBlendAlpha = RHI::BlendFactor::Zero;
			bDesc.blendOpAlpha = RHI::BlendOp::Add;
		}
		else if (m_blendMode == BlendMode::Translucent)
		{
			bDesc.blendEnable = true;
			bDesc.srcBlend = RHI::BlendFactor::SrcAlpha;
			bDesc.destBlend = RHI::BlendFactor::InvSrcAlpha;
			bDesc.blendOp = RHI::BlendOp::Add;
			bDesc.srcBlendAlpha = RHI::BlendFactor::One;
			bDesc.destBlendAlpha = RHI::BlendFactor::Zero;
			bDesc.blendOpAlpha = RHI::BlendOp::Add;
		}
		else
		{
			bDesc.blendEnable = false;
		}
		context.setBlendState(device.getOrCreateBlendState(bDesc));
	}
}
