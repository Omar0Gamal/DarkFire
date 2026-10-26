#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/RHI/IRHI.h"
#include "Engine/Shaders/Shared/Bindings.h"
#include <memory>
#include <string>
#include <filesystem>

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

		void setName(std::string name) { m_name = std::move(name); m_isDirty = true; }
		[[nodiscard]] const std::string& getName() const noexcept { return m_name; }

		void setAssetPath(std::string path) { m_assetPath = std::move(path); }
		[[nodiscard]] const std::string& getAssetPath() const noexcept { return m_assetPath; }

		bool isDirty() const noexcept { return m_isDirty; }
		void setDirty(bool dirty) noexcept { m_isDirty = dirty; }

		// Material Parameters
		void setBaseColor(const Math::Vector4& color) noexcept { m_baseColor = color; m_isDirty = true; }
		void setRoughness(float roughness) noexcept { m_roughness = std::clamp(roughness, 0.04f, 1.0f); m_isDirty = true; }
		void setMetallic(float metallic) noexcept { m_metallic = std::clamp(metallic, 0.0f, 1.0f); m_isDirty = true; }
		void setNormalScale(float scale) noexcept { m_normalScale = scale; m_isDirty = true; }
		void setOcclusionStrength(float strength) noexcept { m_occlusionStrength = std::clamp(strength, 0.0f, 1.0f); m_isDirty = true; }
		void setAlphaCutoff(float cutoff) noexcept { m_alphaCutoff = std::clamp(cutoff, 0.0f, 1.0f); m_isDirty = true; }
		void setEmissiveColor(const Math::Vector3& color) noexcept { m_emissiveColor = color; m_isDirty = true; }
		void setEmissiveIntensity(float intensity) noexcept { m_emissiveIntensity = std::max(intensity, 0.0f); m_isDirty = true; }
		void setTiling(const Math::Vector2& tiling) noexcept { m_uvTiling = tiling; m_isDirty = true; }
		void setOffset(const Math::Vector2& offset) noexcept { m_uvOffset = offset; m_isDirty = true; }
		void setTilingOffset(const Math::Vector2& to) noexcept { m_uvTiling = to; m_isDirty = true; }
		void setFlipNormalY(bool flip) noexcept { m_flipNormalY = flip; m_isDirty = true; }
		void setORMPacked(bool packed) noexcept { m_isORMPacked = packed; m_isDirty = true; }
		void setTwoSided(bool twoSided) noexcept { m_twoSided = twoSided; m_isDirty = true; }
		void setMaskMapPacked(bool packed) noexcept { m_isMaskMapPacked = packed; m_isDirty = true; }
		void setCloth(bool cloth) noexcept { m_isCloth = cloth; m_isDirty = true; }
		void setTransmission(bool trans) noexcept { m_isTransmission = trans; m_isDirty = true; }
		void setTransmissionFactor(float factor) noexcept { m_transmissionFactor = std::clamp(factor, 0.0f, 1.0f); m_isDirty = true; }
		void setSheenFactor(float factor) noexcept { m_sheenFactor = std::clamp(factor, 0.0f, 1.0f); m_isDirty = true; }
		void setTransmissionColor(const Math::Vector3& col) noexcept { m_transmissionColor = col; m_isDirty = true; }
		void setClearCoat(bool cc) noexcept { m_isClearCoat = cc; m_isDirty = true; }
		void setClearCoatFactor(float factor) noexcept { m_clearCoatFactor = std::clamp(factor, 0.0f, 1.0f); m_isDirty = true; }
		void setClearCoatRoughness(float roughness) noexcept { m_clearCoatRoughness = std::clamp(roughness, 0.001f, 1.0f); m_isDirty = true; }
		void setClearcoatFactor(float factor) noexcept { setClearCoatFactor(factor); }
		void setClearcoatRoughness(float r) noexcept { setClearCoatRoughness(r); }
		void setAnisotropy(bool aniso) noexcept { m_isAnisotropy = aniso; m_isDirty = true; }
		void setAnisotropyFactor(float factor) noexcept { m_anisotropyFactor = std::clamp(factor, -1.0f, 1.0f); m_isDirty = true; }
		void setAnisotropyRotation(float rot) noexcept { m_anisotropyRotation = std::clamp(rot, 0.0f, 1.0f); m_isDirty = true; }
		void setSubsurface(bool sss) noexcept { m_isSubsurface = sss; m_isDirty = true; }
		void setSubsurfaceFactor(float factor) noexcept { m_subsurfaceFactor = std::clamp(factor, 0.0f, 1.0f); m_isDirty = true; }
		void setDetailTiling(const Math::Vector2& dt) noexcept { m_detailTiling = dt; m_isDirty = true; }
		void setDetailNormalScale(float scale) noexcept { m_detailNormalScale = scale; m_isDirty = true; }
		void setDetailAlbedoScale(float scale) noexcept { m_detailAlbedoScale = scale; m_isDirty = true; }

		[[nodiscard]] const Math::Vector4& getBaseColor() const noexcept { return m_baseColor; }
		[[nodiscard]] float getRoughness() const noexcept { return m_roughness; }
		[[nodiscard]] float getMetallic() const noexcept { return m_metallic; }
		[[nodiscard]] float getNormalScale() const noexcept { return m_normalScale; }
		[[nodiscard]] float getOcclusionStrength() const noexcept { return m_occlusionStrength; }
		[[nodiscard]] float getAlphaCutoff() const noexcept { return m_alphaCutoff; }
		[[nodiscard]] const Math::Vector3& getEmissiveColor() const noexcept { return m_emissiveColor; }
		[[nodiscard]] float getEmissiveIntensity() const noexcept { return m_emissiveIntensity; }
		[[nodiscard]] const Math::Vector2& getTiling() const noexcept { return m_uvTiling; }
		[[nodiscard]] const Math::Vector2& getOffset() const noexcept { return m_uvOffset; }
		[[nodiscard]] const Math::Vector2& getTilingOffset() const noexcept { return m_uvTiling; }
		[[nodiscard]] bool isFlipNormalY() const noexcept { return m_flipNormalY; }
		[[nodiscard]] bool isORMPacked() const noexcept { return m_isORMPacked; }
		[[nodiscard]] bool isTwoSided() const noexcept { return m_twoSided; }
		[[nodiscard]] bool isMaskMapPacked() const noexcept { return m_isMaskMapPacked; }
		[[nodiscard]] bool isCloth() const noexcept { return m_isCloth; }
		[[nodiscard]] bool isTransmission() const noexcept { return m_isTransmission; }
		[[nodiscard]] float getTransmissionFactor() const noexcept { return m_transmissionFactor; }
		[[nodiscard]] float getSheenFactor() const noexcept { return m_sheenFactor; }
		[[nodiscard]] const Math::Vector3& getTransmissionColor() const noexcept { return m_transmissionColor; }
		[[nodiscard]] bool isClearCoat() const noexcept { return m_isClearCoat; }
		[[nodiscard]] float getClearCoatFactor() const noexcept { return m_clearCoatFactor; }
		[[nodiscard]] float getClearCoatRoughness() const noexcept { return m_clearCoatRoughness; }
		[[nodiscard]] float getClearcoatFactor() const noexcept { return getClearCoatFactor(); }
		[[nodiscard]] float getClearcoatRoughness() const noexcept { return getClearCoatRoughness(); }
		[[nodiscard]] bool isAnisotropy() const noexcept { return m_isAnisotropy; }
		[[nodiscard]] float getAnisotropyFactor() const noexcept { return m_anisotropyFactor; }
		[[nodiscard]] float getAnisotropyRotation() const noexcept { return m_anisotropyRotation; }
		[[nodiscard]] bool isSubsurface() const noexcept { return m_isSubsurface; }
		[[nodiscard]] float getSubsurfaceFactor() const noexcept { return m_subsurfaceFactor; }
		[[nodiscard]] bool hasFlag(uint32_t flag) const noexcept { return (getBufferData().materialFlags & flag) != 0; }
		[[nodiscard]] const Math::Vector2& getDetailTiling() const noexcept { return m_detailTiling; }
		[[nodiscard]] float getDetailNormalScale() const noexcept { return m_detailNormalScale; }
		[[nodiscard]] float getDetailAlbedoScale() const noexcept { return m_detailAlbedoScale; }

		// Constant Buffer Representation (b3)
		[[nodiscard]] CBPerMaterial getBufferData() const noexcept;

		// Render States
		void setBlendMode(BlendMode mode) noexcept { m_blendMode = mode; m_isDirty = true; }
		void setDepthMode(DepthMode mode) noexcept { m_depthMode = mode; m_isDirty = true; }
		void setCullMode(RHI::CullMode mode) noexcept { m_cullMode = mode; m_isDirty = true; }

		[[nodiscard]] BlendMode getBlendMode() const noexcept { return m_blendMode; }
		[[nodiscard]] DepthMode getDepthMode() const noexcept { return m_depthMode; }
		[[nodiscard]] RHI::CullMode getCullMode() const noexcept { return m_cullMode; }

		// Texture Paths
		void setAlbedoPath(std::string path) { m_albedoPath = std::move(path); m_isDirty = true; }
		void setNormalPath(std::string path) { m_normalPath = std::move(path); m_isDirty = true; }
		void setMetallicRoughnessPath(std::string path) { m_metallicRoughnessPath = std::move(path); m_isDirty = true; }
		void setEmissivePath(std::string path) { m_emissivePath = std::move(path); m_isDirty = true; }
		void setOcclusionPath(std::string path) { m_occlusionPath = std::move(path); m_isDirty = true; }
		void setDetailPath(std::string path) { m_detailPath = std::move(path); m_isDirty = true; }

		[[nodiscard]] const std::string& getAlbedoPath() const noexcept { return m_albedoPath; }
		[[nodiscard]] const std::string& getNormalPath() const noexcept { return m_normalPath; }
		[[nodiscard]] const std::string& getMetallicRoughnessPath() const noexcept { return m_metallicRoughnessPath; }
		[[nodiscard]] const std::string& getEmissivePath() const noexcept { return m_emissivePath; }
		[[nodiscard]] const std::string& getOcclusionPath() const noexcept { return m_occlusionPath; }
		[[nodiscard]] const std::string& getDetailPath() const noexcept { return m_detailPath; }

		[[nodiscard]] const std::string& getAlbedoTexturePath() const noexcept { return m_albedoPath; }
		[[nodiscard]] const std::string& getNormalTexturePath() const noexcept { return m_normalPath; }
		[[nodiscard]] const std::string& getMetallicRoughnessTexturePath() const noexcept { return m_metallicRoughnessPath; }
		[[nodiscard]] const std::string& getEmissiveTexturePath() const noexcept { return m_emissivePath; }
		[[nodiscard]] const std::string& getOcclusionTexturePath() const noexcept { return m_occlusionPath; }
		[[nodiscard]] const std::string& getDetailTexturePath() const noexcept { return m_detailPath; }

		// GPU Textures (t0..t5)
		void setAlbedoTexture(std::shared_ptr<RHI::IGpuTexture> tex) { m_albedoTexture = std::move(tex); }
		void setNormalTexture(std::shared_ptr<RHI::IGpuTexture> tex) { m_normalTexture = std::move(tex); }
		void setMetallicRoughnessTexture(std::shared_ptr<RHI::IGpuTexture> tex) { m_metallicRoughnessTexture = std::move(tex); }
		void setEmissiveTexture(std::shared_ptr<RHI::IGpuTexture> tex) { m_emissiveTexture = std::move(tex); }
		void setOcclusionTexture(std::shared_ptr<RHI::IGpuTexture> tex) { m_occlusionTexture = std::move(tex); }
		void setDetailTexture(std::shared_ptr<RHI::IGpuTexture> tex) { m_detailTexture = std::move(tex); }

		[[nodiscard]] const std::shared_ptr<RHI::IGpuTexture>& getAlbedoTexture() const noexcept { return m_albedoTexture; }
		[[nodiscard]] const std::shared_ptr<RHI::IGpuTexture>& getNormalTexture() const noexcept { return m_normalTexture; }
		[[nodiscard]] const std::shared_ptr<RHI::IGpuTexture>& getMetallicRoughnessTexture() const noexcept { return m_metallicRoughnessTexture; }
		[[nodiscard]] const std::shared_ptr<RHI::IGpuTexture>& getEmissiveTexture() const noexcept { return m_emissiveTexture; }
		[[nodiscard]] const std::shared_ptr<RHI::IGpuTexture>& getOcclusionTexture() const noexcept { return m_occlusionTexture; }
		[[nodiscard]] const std::shared_ptr<RHI::IGpuTexture>& getDetailTexture() const noexcept { return m_detailTexture; }

		// Shaders
		void setVertexShader(std::shared_ptr<RHI::IVertexShader> vs) { m_vertexShader = std::move(vs); }
		void setPixelShader(std::shared_ptr<RHI::IPixelShader> ps) { m_pixelShader = std::move(ps); }
		[[nodiscard]] const std::shared_ptr<RHI::IVertexShader>& getVertexShader() const noexcept { return m_vertexShader; }
		[[nodiscard]] const std::shared_ptr<RHI::IPixelShader>& getPixelShader() const noexcept { return m_pixelShader; }

		// JSON Serialization / Deserialization
		bool loadFromJson(const std::string& jsonContent);
		std::string toJson() const;
		bool saveToFile(const std::filesystem::path& path);

		// Bind Material to Context
		void bind(RHI::IDeviceContext& context, RHI::IRHIDevice& device, const std::shared_ptr<RHI::IGpuBuffer>& materialCb = nullptr) const;

	private:
		std::string m_name;
		std::string m_assetPath;
		bool m_isDirty = false;

		Math::Vector4 m_baseColor{ 1.0f, 1.0f, 1.0f, 1.0f };
		Math::Vector3 m_emissiveColor{ 0.0f, 0.0f, 0.0f };
		float m_emissiveIntensity = 1.0f;
		float m_roughness = 0.5f;
		float m_metallic = 0.0f;
		float m_normalScale = 1.0f;
		float m_occlusionStrength = 1.0f;
		float m_alphaCutoff = 0.5f;
		Math::Vector2 m_uvTiling{ 1.0f, 1.0f };
		Math::Vector2 m_uvOffset{ 0.0f, 0.0f };

		bool m_flipNormalY = false;
		bool m_isORMPacked = false;
		bool m_twoSided = false;
		bool m_isMaskMapPacked = false;
		bool m_isCloth = false;
		bool m_isTransmission = false;
		float m_transmissionFactor = 0.0f;
		float m_sheenFactor = 0.0f;
		Math::Vector3 m_transmissionColor{ 1.0f, 1.0f, 1.0f };
		bool m_isClearCoat = false;
		float m_clearCoatFactor = 0.0f;
		float m_clearCoatRoughness = 0.05f;
		bool m_isAnisotropy = false;
		float m_anisotropyFactor = 0.0f;
		float m_anisotropyRotation = 0.0f;
		bool m_isSubsurface = false;
		float m_subsurfaceFactor = 0.0f;
		Math::Vector2 m_detailTiling{ 1.0f, 1.0f };
		float m_detailNormalScale = 1.0f;
		float m_detailAlbedoScale = 1.0f;

		BlendMode m_blendMode = BlendMode::Opaque;
		DepthMode m_depthMode = DepthMode::ReadWrite;
		RHI::CullMode m_cullMode = RHI::CullMode::Back;

		std::string m_albedoPath;
		std::string m_normalPath;
		std::string m_metallicRoughnessPath;
		std::string m_emissivePath;
		std::string m_occlusionPath;
		std::string m_detailPath;

		std::shared_ptr<RHI::IGpuTexture> m_albedoTexture;
		std::shared_ptr<RHI::IGpuTexture> m_normalTexture;
		std::shared_ptr<RHI::IGpuTexture> m_metallicRoughnessTexture;
		std::shared_ptr<RHI::IGpuTexture> m_emissiveTexture;
		std::shared_ptr<RHI::IGpuTexture> m_occlusionTexture;
		std::shared_ptr<RHI::IGpuTexture> m_detailTexture;

		std::shared_ptr<RHI::IVertexShader> m_vertexShader;
		std::shared_ptr<RHI::IPixelShader> m_pixelShader;
	};
}
