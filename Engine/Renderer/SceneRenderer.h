#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/RHI/IRHI.h"
#include "Engine/RHI/UploadRing.h"
#include "Engine/Renderer/RenderScene.h"
#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/DrawItem.h"
#include "Engine/Renderer/MeshPrimitives.h"
#include "Engine/Shaders/Shared/Bindings.h"
#include <unordered_map>

#include <memory>
#include <vector>

namespace Core::Renderer
{
	struct GpuMesh
	{
		std::shared_ptr<RHI::IGpuBuffer> vertexBuffer;
		std::shared_ptr<RHI::IGpuBuffer> indexBuffer;
		uint32_t indexCount = 0;
	};

	struct DF_API SceneRendererConfig
	{
		bool enableBloom = true;
		bool enableFXAA = true;
		bool enableTonemapping = true;
		float exposure = 1.0f;
		float bloomThreshold = 1.0f;
		float bloomIntensity = 0.5f;
	};

	class DF_API SceneRenderer
	{
	public:
		explicit SceneRenderer(RHI::IRHIDevice& device);
		~SceneRenderer();

		void setConfig(const SceneRendererConfig& config) { m_config = config; }
		[[nodiscard]] const SceneRendererConfig& getConfig() const noexcept { return m_config; }

		void resize(uint32_t width, uint32_t height);
		void render(const RenderScene& scene, const RenderView& view);

		// Entity Picking & Reversed-Z Ray Unprojection
		[[nodiscard]] uint32_t readEntityId(uint32_t x, uint32_t y) const;
		static Math::Ray unprojectRayReversedZ(
			float screenX,
			float screenY,
			float viewportWidth,
			float viewportHeight,
			const Math::Matrix& invViewProj) noexcept;

		// Targets access
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getHDRTexture() const noexcept { return m_hdrTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getDepthTexture() const noexcept { return m_depthTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getEntityIdTexture() const noexcept { return m_entityIdTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getFinalLDRTexture() const noexcept { return m_finalLdrTexture; }

		// Mesh, Material & Texture Resources
		GpuMesh getOrCreateMesh(const std::string& meshPath);
		std::shared_ptr<Material> getOrCreateMaterial(const std::string& materialPath);
		std::shared_ptr<RHI::IGpuTexture> getOrCreateTexture(const std::string& texturePath, bool sRGB = false);

		// Stats
		[[nodiscard]] uint32_t getDrawCallCount() const noexcept { return m_drawCallCount; }
		[[nodiscard]] uint32_t getBatchCount() const noexcept { return m_batchCount; }
		[[nodiscard]] uint32_t getInstanceCount() const noexcept { return m_instanceCount; }
		[[nodiscard]] const RHI::UploadRing& getUploadRing() const noexcept { return m_uploadRing; }

	private:
		RHI::IRHIDevice& m_device;
		std::shared_ptr<RHI::IDeviceContext> m_context;
		RHI::UploadRing m_uploadRing;
		ShaderLibrary m_shaderLibrary;
		SceneRendererConfig m_config;

		uint32_t m_width = 1280;
		uint32_t m_height = 720;

		// Frame Targets
		std::shared_ptr<RHI::IGpuTexture> m_hdrTexture;       // R16G16B16A16_FLOAT
		std::shared_ptr<RHI::IGpuTexture> m_depthTexture;     // D32_FLOAT / R32_TYPELESS
		std::shared_ptr<RHI::IGpuTexture> m_entityIdTexture;  // R32_UINT
		std::shared_ptr<RHI::IGpuTexture> m_finalLdrTexture;  // R8G8B8A8_UNORM

		// Constant Buffers & Structured Buffers
		std::shared_ptr<RHI::IGpuBuffer> m_cbFrame;
		std::shared_ptr<RHI::IGpuBuffer> m_cbView;
		std::shared_ptr<RHI::IGpuBuffer> m_cbPass;
		std::shared_ptr<RHI::IGpuBuffer> m_bufLights;

		// Persistent Draw Item and Instance caches (zero per-frame allocations)
		std::vector<DrawItem> m_cachedDrawItems;
		std::vector<InstanceData> m_cachedInstances;
		std::vector<std::string> m_cachedMeshPaths;
		std::vector<std::string> m_cachedMaterialPaths;

		// Mesh, Material & Texture Caches
		std::unordered_map<std::string, GpuMesh> m_meshCache;
		std::unordered_map<std::string, std::shared_ptr<Material>> m_materialCache;
		std::unordered_map<std::string, std::shared_ptr<RHI::IGpuTexture>> m_textureCache;

		// Pipeline States & Shaders
		std::shared_ptr<RHI::IVertexShader> m_vsStandard;
		std::shared_ptr<RHI::IPixelShader> m_psStandard;
		std::shared_ptr<RHI::IInputLayout> m_inputLayout;
		std::shared_ptr<RHI::IVertexShader> m_vsFullscreen;
		std::shared_ptr<RHI::IPixelShader> m_psPostProcess;
		std::shared_ptr<RHI::IRasterizerState> m_rsSolid;
		std::shared_ptr<RHI::IDepthStencilState> m_dssOpaque;
		std::shared_ptr<RHI::ISamplerState> m_samplerLinearWrap;
		std::shared_ptr<RHI::ISamplerState> m_samplerLinearClamp;
		std::shared_ptr<RHI::ISamplerState> m_samplerPointClamp;

		// Default Textures
		std::shared_ptr<RHI::IGpuTexture> m_whiteTexture;
		std::shared_ptr<RHI::IGpuTexture> m_flatNormalTexture;
		std::shared_ptr<RHI::IGpuTexture> m_defaultMRTexture;
		std::shared_ptr<RHI::IGpuTexture> m_blackTexture;

		// Dedicated CBs
		std::shared_ptr<RHI::IGpuBuffer> m_cbMaterial;
		std::shared_ptr<RHI::IGpuBuffer> m_cbPostProcess;

		// Staging texture for readEntityId
		mutable void* m_stagingEntityId = nullptr;

		// Performance Stats
		uint32_t m_drawCallCount = 0;
		uint32_t m_batchCount = 0;
		uint32_t m_instanceCount = 0;

		// Frame & Animation state
		float m_totalTime = 0.0f;
		uint32_t m_frameIndex = 0;

		void initializeResources();
		void executePasses(const RenderScene& scene, const RenderView& view);
		void passOpaque(const RenderScene& scene);
		void passSkybox(const RenderScene& scene);
		void passTransparent(const RenderScene& scene);
		void passPostProcess(const RenderView& view);

		GpuMesh createGpuMesh(const MeshData& meshData);
	};
}
