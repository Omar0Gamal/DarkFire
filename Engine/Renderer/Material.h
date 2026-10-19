#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/RHI/IRHI.h"
#include "Engine/Shaders/Shared/Bindings.h"
#include <memory>
#include <string>

namespace Core::Renderer
{
	enum class BlendMode : uint8_t
	{
		Opaque = 0,
		Masked,
		Additive,
		Translucent
	};

	enum class DepthMode : uint8_t
	{
		ReadWrite = 0,
		ReadOnly,
		None
	};

	class DF_API Material
	{
	public:
		Material();
		explicit Material(std::string name);
		~Material() = default;

		void setName(std::string name) { m_name = std::move(name); }
		[[nodiscard]] const std::string& getName() const noexcept { return m_name; }

		// Material Parameters (b3)
		void setBaseColor(const Math::Vector4& color) noexcept { m_bufferData.baseColorFactor = color; }
		void setRoughness(float roughness) noexcept { m_bufferData.roughnessFactor = roughness; }
		void setMetallic(float metallic) noexcept { m_bufferData.metallicFactor = metallic; }
		void setTilingOffset(const Math::Vector2& tiling) noexcept { m_bufferData.tilingOffset = tiling; }
		void setEmissive(const Math::Vector4& emissive) noexcept { m_bufferData.emissiveFactor = emissive; }
		void setFlags(uint32_t flags) noexcept { m_bufferData.materialFlags = flags; }

		[[nodiscard]] const Math::Vector4& getBaseColor() const noexcept { return m_bufferData.baseColorFactor; }
		[[nodiscard]] float getRoughness() const noexcept { return m_bufferData.roughnessFactor; }
		[[nodiscard]] float getMetallic() const noexcept { return m_bufferData.metallicFactor; }
		[[nodiscard]] const Math::Vector2& getTilingOffset() const noexcept { return m_bufferData.tilingOffset; }
		[[nodiscard]] const Math::Vector4& getEmissive() const noexcept { return m_bufferData.emissiveFactor; }
		[[nodiscard]] const CBPerMaterial& getBufferData() const noexcept { return m_bufferData; }

		// Render States
		void setBlendMode(BlendMode mode) noexcept { m_blendMode = mode; }
		void setDepthMode(DepthMode mode) noexcept { m_depthMode = mode; }
		void setCullMode(RHI::CullMode mode) noexcept { m_cullMode = mode; }

		[[nodiscard]] BlendMode getBlendMode() const noexcept { return m_blendMode; }
		[[nodiscard]] DepthMode getDepthMode() const noexcept { return m_depthMode; }
		[[nodiscard]] RHI::CullMode getCullMode() const noexcept { return m_cullMode; }

		// Textures (t0..t4)
		void setAlbedoTexture(std::shared_ptr<RHI::IGpuTexture> tex) { m_albedoTexture = std::move(tex); }
		void setNormalTexture(std::shared_ptr<RHI::IGpuTexture> tex) { m_normalTexture = std::move(tex); }
		void setMetallicRoughnessTexture(std::shared_ptr<RHI::IGpuTexture> tex) { m_metallicRoughnessTexture = std::move(tex); }
		void setEmissiveTexture(std::shared_ptr<RHI::IGpuTexture> tex) { m_emissiveTexture = std::move(tex); }
		void setOcclusionTexture(std::shared_ptr<RHI::IGpuTexture> tex) { m_occlusionTexture = std::move(tex); }

		[[nodiscard]] const std::shared_ptr<RHI::IGpuTexture>& getAlbedoTexture() const noexcept { return m_albedoTexture; }
		[[nodiscard]] const std::shared_ptr<RHI::IGpuTexture>& getNormalTexture() const noexcept { return m_normalTexture; }
		[[nodiscard]] const std::shared_ptr<RHI::IGpuTexture>& getMetallicRoughnessTexture() const noexcept { return m_metallicRoughnessTexture; }
		[[nodiscard]] const std::shared_ptr<RHI::IGpuTexture>& getEmissiveTexture() const noexcept { return m_emissiveTexture; }
		[[nodiscard]] const std::shared_ptr<RHI::IGpuTexture>& getOcclusionTexture() const noexcept { return m_occlusionTexture; }

		// Shaders
		void setVertexShader(std::shared_ptr<RHI::IVertexShader> vs) { m_vertexShader = std::move(vs); }
		void setPixelShader(std::shared_ptr<RHI::IPixelShader> ps) { m_pixelShader = std::move(ps); }
		[[nodiscard]] const std::shared_ptr<RHI::IVertexShader>& getVertexShader() const noexcept { return m_vertexShader; }
		[[nodiscard]] const std::shared_ptr<RHI::IPixelShader>& getPixelShader() const noexcept { return m_pixelShader; }

		// Bind Material to Context
		void bind(RHI::IDeviceContext& context, RHI::IRHIDevice& device, const std::shared_ptr<RHI::IGpuBuffer>& materialCb = nullptr) const;

	private:
		std::string m_name;
		CBPerMaterial m_bufferData{};

		BlendMode m_blendMode = BlendMode::Opaque;
		DepthMode m_depthMode = DepthMode::ReadWrite;
		RHI::CullMode m_cullMode = RHI::CullMode::Back;

		std::shared_ptr<RHI::IGpuTexture> m_albedoTexture;
		std::shared_ptr<RHI::IGpuTexture> m_normalTexture;
		std::shared_ptr<RHI::IGpuTexture> m_metallicRoughnessTexture;
		std::shared_ptr<RHI::IGpuTexture> m_emissiveTexture;
		std::shared_ptr<RHI::IGpuTexture> m_occlusionTexture;

		std::shared_ptr<RHI::IVertexShader> m_vertexShader;
		std::shared_ptr<RHI::IPixelShader> m_pixelShader;
	};
}
