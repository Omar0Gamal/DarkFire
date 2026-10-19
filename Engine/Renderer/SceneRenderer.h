#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/RHI/IRHI.h"
#include "Engine/RHI/UploadRing.h"
#include "Engine/Renderer/RenderScene.h"
#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/DrawItem.h"
#include "Engine/Shaders/Shared/Bindings.h"

#include <memory>
#include <vector>

namespace Core::Renderer
{
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
		~SceneRenderer() = default;

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

		// Stats
		[[nodiscard]] uint32_t getDrawCallCount() const noexcept { return m_drawCallCount; }
		[[nodiscard]] uint32_t getBatchCount() const noexcept { return m_batchCount; }
		[[nodiscard]] uint32_t getInstanceCount() const noexcept { return m_instanceCount; }

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

		// Performance Stats
		uint32_t m_drawCallCount = 0;
		uint32_t m_batchCount = 0;
		uint32_t m_instanceCount = 0;

		void initializeResources();
		void executePasses(const RenderScene& scene, const RenderView& view);
		void passOpaque(const RenderScene& scene);
		void passSkybox(const RenderScene& scene);
		void passTransparent(const RenderScene& scene);
		void passPostProcess(const RenderView& view);
	};
}
