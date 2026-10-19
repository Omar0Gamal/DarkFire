#include "Material.h"

namespace Core::Renderer
{
	Material::Material()
		: Material("DefaultMaterial")
	{
	}

	Material::Material(std::string name)
		: m_name(std::move(name))
	{
		m_bufferData.baseColorFactor = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		m_bufferData.roughnessFactor = 0.5f;
		m_bufferData.metallicFactor = 0.0f;
		m_bufferData.tilingOffset = Math::Vector2(1.0f, 1.0f);
		m_bufferData.emissiveFactor = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
		m_bufferData.materialFlags = 0;
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
			context.setConstantBufferPS(DF_SLOT_CB_PER_MATERIAL, materialCb);
		}

		// Bind Material Textures (t0..t4)
		if (m_albedoTexture)
		{
			context.setShaderResourcePS(DF_SLOT_TEX_ALBEDO, m_albedoTexture);
		}
		if (m_normalTexture)
		{
			context.setShaderResourcePS(DF_SLOT_TEX_NORMAL, m_normalTexture);
		}
		if (m_metallicRoughnessTexture)
		{
			context.setShaderResourcePS(DF_SLOT_TEX_METALLIC_ROUGHNESS, m_metallicRoughnessTexture);
		}
		if (m_emissiveTexture)
		{
			context.setShaderResourcePS(DF_SLOT_TEX_EMISSIVE, m_emissiveTexture);
		}
		if (m_occlusionTexture)
		{
			context.setShaderResourcePS(DF_SLOT_TEX_OCCLUSION, m_occlusionTexture);
		}

		// Rasterizer State
		RHI::RasterizerStateDesc rsDesc{};
		rsDesc.cullMode = m_cullMode;
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
