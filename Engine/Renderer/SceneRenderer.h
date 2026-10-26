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
#include "Engine/Renderer/RenderPipeline.h"
#include "Engine/Renderer/ComputeKernel.h"
#include "Engine/Shaders/Shared/Bindings.h"
#include "Engine/Renderer/ShadowAtlasAllocator.h"
#include <unordered_map>

#include <memory>
#include <vector>

namespace Core::Renderer
{
	struct GpuInstanceData
	{
		Math::Matrix world;
		Math::Matrix prevWorld;
		Math::Vector3 aabbMin;
		float pad0 = 0.0f;
		Math::Vector3 aabbMax;
		uint32_t entityId = 0;
		uint32_t materialId = 0;
		uint32_t batchId = 0;
		uint32_t startIndex = 0;
		uint32_t indexCount = 0;
	};

	struct DrawIndexedInstancedIndirectArgs
	{
		uint32_t indexCountPerInstance = 0;
		uint32_t instanceCount = 0;
		uint32_t startIndexLocation = 0;
		int32_t  baseVertexLocation = 0;
		uint32_t startInstanceLocation = 0;
	};

	struct ClusterAABB
	{
		Math::Vector4 minPoint;
		Math::Vector4 maxPoint;
	};

	struct CBClusterBuild
	{
		Math::Matrix invProjection;
		Math::Vector2 tileDimensions;
		Math::Vector2 viewportSize;
		float nearZ = 0.1f;
		float farZ = 250.0f;
		Math::Vector2 pad0{ 0.0f, 0.0f };
	};

	struct CBClusterAssign
	{
		Math::Matrix viewMatrix;
		uint32_t lightCount = 0;
		uint32_t maxIndices = 0;
		Math::Vector2 pad0{ 0.0f, 0.0f };
	};

	struct CBCullData
	{
		Math::Vector4 frustumPlanes[6];
		Math::Vector4 cascadePlanes[24];
		uint32_t instanceCount = 0;
		uint32_t cascadeCount = 0;
		uint32_t batchCount = 0;
		uint32_t pad0 = 0;
	};

	struct CBTAA
	{
		Math::Vector4 jitterAndBlend; // xy = jitter UV, z = blend factor (0.92), w = sharpness (0.25)
		Math::Vector4 resolution;     // xy = resolution, zw = 1.0 / resolution
	};

	struct GpuMesh
	{
		std::shared_ptr<RHI::IGpuBuffer> vertexBuffer;
		std::shared_ptr<RHI::IGpuBuffer> indexBuffer;
		uint32_t indexCount = 0;
		std::vector<Submesh> submeshes;
		std::vector<MeshLodLevel> lodLevels;
		Math::BoundingBox localBounds{};
	};

	struct DF_API SceneRendererConfig
	{
		bool enableBloom = true;
		bool enableFXAA = true;
		bool enableTonemapping = true;
		float exposure = 1.0f;
		float bloomThreshold = 1.0f;
		float bloomIntensity = 0.5f;
		bool enableMotionBlur = false;
		float motionBlurShutterAngle = 0.5f;
		float motionBlurMaxRadius = 32.0f;
		uint32_t shadowAtlasResolution = 4096;
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
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getPrevHDRTexture() const noexcept { return m_prevHdrTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getDepthTexture() const noexcept { return m_depthTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getEntityIdTexture() const noexcept { return m_entityIdTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getFinalLDRTexture() const noexcept { return m_finalLdrTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getShadowMapTexture() const noexcept { return m_shadowMapTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getNormalRoughnessTexture() const noexcept { return m_normalRoughnessTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getAlbedoMetallicTexture() const noexcept { return m_albedoMetallicTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getGBufferEmissiveAOTexture() const noexcept { return m_gbufferEmissiveAOTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getProceduralEnvIBLTexture() const noexcept { return m_proceduralEnvIBLTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getSpecularIBLTexture() const noexcept { return m_specularIBLTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getIrradianceIBLTexture() const noexcept { return m_irradianceIBLTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getColorLUTTexture() const noexcept { return m_colorLutTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getRefractionHDRCopyTexture() const noexcept { return m_refractionHdrCopyTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getSSRTexture() const noexcept { return m_ssrTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getHiZTexture() const noexcept { return m_hiZTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getDDGIRadianceAtlas() const noexcept { return m_ddgiRadianceAtlas; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getDDGIDepthAtlas() const noexcept { return m_ddgiDepthAtlas; }
		[[nodiscard]] const GpuMesh& getGrassBladeMesh() const noexcept { return m_grassBladeMesh; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getVolumetricFogTexture() const noexcept { return m_volumetricFogIntegrated; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getMotionBlurTexture() const noexcept { return m_motionBlurTexture; }
		[[nodiscard]] const ShadowAtlasAllocator& getShadowAtlasAllocator() const noexcept { return m_shadowAtlasAllocator; }
		[[nodiscard]] RenderPipeline& getPipeline(RenderPipelineType type) noexcept;
		[[nodiscard]] const RenderPipeline& getPipeline(RenderPipelineType type) const noexcept;

		// Mesh, Material & Texture Resources
		GpuMesh getOrCreateMesh(const std::string& meshPath);
		GpuMesh getOrCreateTerrainMesh(const Core::TerrainComponent& tc);
		GpuMesh getOrCreateWaterMesh(const Core::WaterComponent& wc);
		std::shared_ptr<Material> getOrCreateMaterial(const std::string& materialPath);
		std::shared_ptr<RHI::IGpuTexture> getOrCreateTexture(const std::string& texturePath, bool sRGB = false);
		std::shared_ptr<RHI::IGpuTexture> getOrCreateORMTexture(const std::string& roughnessPath, const std::string& metallicPath);
		bool saveScreenshot(const std::string& filePath, RHI::ISwapChain* swapChain = nullptr) const;

		// Stats
		[[nodiscard]] uint32_t getDrawCallCount() const noexcept { return m_drawCallCount; }
		[[nodiscard]] uint32_t getBatchCount() const noexcept { return m_batchCount; }
		[[nodiscard]] uint32_t getInstanceCount() const noexcept { return m_instanceCount; }
		[[nodiscard]] const RHI::UploadRing& getUploadRing() const noexcept { return m_uploadRing; }

		static constexpr uint32_t kShadowMapResolution = 4096;

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
		std::shared_ptr<RHI::IGpuTexture> m_prevHdrTexture;   // R16G16B16A16_FLOAT previous frame copy for SSR
		std::shared_ptr<RHI::IGpuTexture> m_depthTexture;     // D32_FLOAT / R32_TYPELESS
		std::shared_ptr<RHI::IGpuTexture> m_entityIdTexture;  // R32_UINT
		std::shared_ptr<RHI::IGpuTexture> m_finalLdrTexture;  // R8G8B8A8_UNORM
		std::shared_ptr<RHI::IGpuTexture> m_shadowMapTexture; // 2048x2048 D32_FLOAT shadow depth map

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
		std::shared_ptr<RHI::IVertexShader> m_vsShadow;
		std::shared_ptr<RHI::IInputLayout> m_inputLayoutShadow;
		std::shared_ptr<RHI::IPixelShader> m_psSky;
		std::shared_ptr<RHI::IPixelShader> m_psSSAO;
		std::shared_ptr<RHI::IPixelShader> m_psSSAOBlur;
		std::shared_ptr<RHI::IPixelShader> m_psBloomPrefilter;
		std::shared_ptr<RHI::IPixelShader> m_psBloomDownsample;
		std::shared_ptr<RHI::IPixelShader> m_psBloomUpsample;
		std::shared_ptr<RHI::IPixelShader> m_psTonemap;
		std::shared_ptr<RHI::IPixelShader> m_psFXAA;
		std::shared_ptr<RHI::IPixelShader> m_psDepthCutout;
		std::shared_ptr<RHI::IPixelShader> m_psShadowCutout;

		std::shared_ptr<RHI::IRasterizerState> m_rsSolid;
		std::shared_ptr<RHI::IRasterizerState> m_rsCullNone;
		std::shared_ptr<RHI::IDepthStencilState> m_dssShadow;
		std::shared_ptr<RHI::IDepthStencilState> m_dssDepthPrepass;
		std::shared_ptr<RHI::IDepthStencilState> m_dssOpaque;
		std::shared_ptr<RHI::IDepthStencilState> m_dssSky;
		std::shared_ptr<RHI::IDepthStencilState> m_dssTransparent;
		std::shared_ptr<RHI::IBlendState> m_bsAlphaBlend;
		std::shared_ptr<RHI::IBlendState> m_bsAdditive;
		std::shared_ptr<RHI::IBlendState> m_bsPremultipliedAlpha;

		std::shared_ptr<RHI::ISamplerState> m_samplerLinearWrap;
		std::shared_ptr<RHI::ISamplerState> m_samplerLinearClamp;
		std::shared_ptr<RHI::ISamplerState> m_samplerPointClamp;
		std::shared_ptr<RHI::ISamplerState> m_samplerAnisoWrap;
		std::shared_ptr<RHI::ISamplerState> m_samplerShadow;

		// Default Textures
		std::shared_ptr<RHI::IGpuTexture> m_whiteTexture;
		std::shared_ptr<RHI::IGpuTexture> m_flatNormalTexture;
		std::shared_ptr<RHI::IGpuTexture> m_defaultMRTexture;
		std::shared_ptr<RHI::IGpuTexture> m_blackTexture;
		std::shared_ptr<RHI::IGpuTexture> m_brdfLutTexture;
		GpuMesh m_billboardQuadMesh;

		// Render Targets for Post-Processing & SSAO
		std::shared_ptr<RHI::IGpuTexture> m_ssaoRawTexture;
		std::shared_ptr<RHI::IGpuTexture> m_ssaoBlurredTexture;
		std::shared_ptr<RHI::IGpuTexture> m_ldrIntermediateTexture;
		std::array<std::shared_ptr<RHI::IGpuTexture>, 5> m_bloomDown;
		std::array<std::shared_ptr<RHI::IGpuTexture>, 5> m_bloomUp;

		// G-Buffer 4-MRT & Depth Copy / IBL Textures
		std::shared_ptr<RHI::IGpuTexture> m_normalRoughnessTexture;   // R16G16B16A16_FLOAT full-res
		std::shared_ptr<RHI::IGpuTexture> m_albedoMetallicTexture;    // R8G8B8A8_UNORM full-res
		std::shared_ptr<RHI::IGpuTexture> m_gbufferEmissiveAOTexture; // R16G16B16A16_FLOAT full-res
		std::shared_ptr<RHI::IGpuTexture> m_depthCopyTexture;         // D32_FLOAT copy for Forward+ Contact Shadows
		std::shared_ptr<RHI::IGpuTexture> m_proceduralEnvIBLTexture;  // 6-mip R16G16B16A16_FLOAT pre-convolved sky/sun IBL
		std::shared_ptr<RHI::IGpuTexture> m_specularIBLTexture;        // Slot 17: Local prefiltered reflection probe
		std::shared_ptr<RHI::IGpuTexture> m_irradianceIBLTexture;      // Slot 18: Diffuse irradiance probe
		std::shared_ptr<RHI::IGpuTexture> m_colorLutTexture;           // Slot 20: 3D Color grading LUT (32x32x32)
		Math::Vector4 m_cachedEnvSkyZenith{ -1.0f, -1.0f, -1.0f, -1.0f };
		Math::Vector4 m_cachedEnvSkyHorizon{ -1.0f, -1.0f, -1.0f, -1.0f };
		Math::Vector4 m_cachedEnvSkyGround{ -1.0f, -1.0f, -1.0f, -1.0f };
		Math::Vector4 m_cachedEnvSunDir{ 0.0f, 0.0f, 0.0f, -1.0f };
		Math::Vector4 m_cachedEnvSunColor{ -1.0f, -1.0f, -1.0f, -1.0f };
		std::string   m_cachedBakedProbePath;

		// Dedicated CBs
		std::shared_ptr<RHI::IGpuBuffer> m_cbMaterial;
		std::shared_ptr<RHI::IGpuBuffer> m_cbShadow;
		std::shared_ptr<RHI::IGpuBuffer> m_cbPostProcess;
		std::shared_ptr<RHI::IGpuBuffer> m_cbEnvironment;
		std::shared_ptr<RHI::IGpuBuffer> m_cbSSAO;
		std::shared_ptr<RHI::IGpuBuffer> m_cbSSAOBlur;
		std::shared_ptr<RHI::IGpuBuffer> m_cbBloom;
		std::shared_ptr<RHI::IGpuBuffer> m_cbFXAA;
		std::shared_ptr<RHI::IGpuBuffer> m_cbTerrain;
		std::shared_ptr<RHI::IGpuBuffer> m_cbFoliage;
		std::shared_ptr<RHI::IGpuBuffer> m_cbWater;
		std::shared_ptr<RHI::IGpuBuffer> m_cbSSR;
		std::shared_ptr<RHI::IGpuBuffer> m_cbDDGI;
		std::shared_ptr<RHI::IGpuBuffer> m_cbSSS;

		// Deferred, Decal & Tessellation Shaders
		std::shared_ptr<RHI::IPixelShader> m_psGBuffer;
		std::shared_ptr<RHI::IPixelShader> m_psDeferredLighting;
		std::shared_ptr<RHI::IVertexShader> m_vsDecal;
		std::shared_ptr<RHI::IPixelShader> m_psDecal;
		std::shared_ptr<RHI::IHullShader> m_hsStandard;
		std::shared_ptr<RHI::IDomainShader> m_dsStandard;

		// Buildable Render Pipelines
		RenderPipeline m_forwardPipeline{ RenderPipelineType::ForwardPlus };
		RenderPipeline m_deferredPipeline{ RenderPipelineType::Deferred };
		RenderPipeline m_customPipeline{ RenderPipelineType::Custom };

		// Nature Shaders (Phases 5-7)
		std::shared_ptr<RHI::IVertexShader> m_vsTerrain;
		std::shared_ptr<RHI::IPixelShader> m_psTerrain;
		std::shared_ptr<RHI::IVertexShader> m_vsGrass;
		std::shared_ptr<RHI::IPixelShader> m_psGrass;
		std::shared_ptr<RHI::IVertexShader> m_vsWater;
		std::shared_ptr<RHI::IPixelShader> m_psWater;

		// Viewport Debug Line Shaders & Dynamic Buffer (Phase 8)
		std::shared_ptr<RHI::IVertexShader> m_vsDebugLine;
		std::shared_ptr<RHI::IPixelShader> m_psDebugLine;
		std::shared_ptr<RHI::IInputLayout> m_inputLayoutDebugLine;
		std::shared_ptr<RHI::IGpuBuffer> m_vbDebugLines;
		uint32_t m_vbDebugLinesCapacity = 0;

		// Velocity, TAA & Halton Jitter (Tier 1)
		std::shared_ptr<RHI::IGpuTexture> m_velocityTexture;
		std::shared_ptr<RHI::IGpuTexture> m_taaHistory[2];
		std::shared_ptr<RHI::IGpuTexture> m_taaResolvedTexture;
		uint32_t m_taaHistoryIndex = 0;
		std::shared_ptr<RHI::IGpuBuffer> m_cbTAA;
		std::shared_ptr<RHI::IPixelShader> m_psTAA;
		bool m_enableTAA = true;
		Math::Vector2 m_jitterOffset{ 0.0f, 0.0f };
		Math::Vector2 m_prevJitterOffset{ 0.0f, 0.0f };
		Math::Matrix m_prevViewProjMatrix = Math::Matrix::Identity;
		Math::Matrix m_prevCameraViewMatrix = Math::Matrix::Identity;
		bool m_isCameraMoving = false;

		// Clustered Dynamic Lighting (Tier 1: 16x16x24 Clusters)
		ComputeKernel m_csClusterBuild;
		ComputeKernel m_csClusterAssign;
		std::shared_ptr<RHI::IGpuBuffer> m_bufClusterAABBs;
		std::shared_ptr<RHI::IGpuBuffer> m_bufClusterGrid;
		std::shared_ptr<RHI::IGpuBuffer> m_bufClusterLightIndices;
		std::shared_ptr<RHI::IGpuBuffer> m_bufClusterCounter;
		std::shared_ptr<RHI::IGpuBuffer> m_cbClusterBuild;
		std::shared_ptr<RHI::IGpuBuffer> m_cbClusterAssign;
		bool m_clustersBuilt = false;
		float m_prevClusterNearZ = 0.0f;
		float m_prevClusterFarZ = 0.0f;
		uint32_t m_prevClusterWidth = 0;
		uint32_t m_prevClusterHeight = 0;

		// GPU-Driven Culling & Indirect Drawing (Tier 1)
		ComputeKernel m_csCullInstances;
		std::shared_ptr<RHI::IGpuBuffer> m_bufGpuInstances;
		std::shared_ptr<RHI::IGpuBuffer> m_bufCameraIndirectArgs;
		std::shared_ptr<RHI::IGpuBuffer> m_bufCameraVisibleInstances;
		std::shared_ptr<RHI::IGpuBuffer> m_bufCascadeIndirectArgs;
		std::shared_ptr<RHI::IGpuBuffer> m_bufCascadeVisibleInstances;
		std::shared_ptr<RHI::IGpuBuffer> m_cbCull;
		uint32_t m_gpuInstanceCapacity = 0;
		uint32_t m_indirectBatchCapacity = 0;

		// Nature Meshes & Caches
		GpuMesh m_grassBladeMesh;
		std::unordered_map<uint64_t, GpuMesh> m_terrainMeshCache;
		std::unordered_map<uint64_t, GpuMesh> m_waterMeshCache;
		std::unordered_map<uint32_t, std::shared_ptr<RHI::IGpuBuffer>> m_foliageBufferCache;
		std::unordered_map<uint32_t, uint32_t> m_foliageCountCache;
		std::shared_ptr<RHI::IGpuTexture> m_refractionHdrCopyTexture;

		// SSAO Hemisphere Kernel
		std::array<Math::Vector4, 16> m_ssaoKernel;

		// Milestone 1 (Tier 2/3): Hi-Z SSR, DDGI & Subsurface Scattering
		std::shared_ptr<RHI::IGpuTexture> m_ssrTexture;
		std::shared_ptr<RHI::IGpuTexture> m_hiZTexture;
		std::shared_ptr<RHI::IGpuTexture> m_ddgiRadianceAtlas;
		std::shared_ptr<RHI::IGpuTexture> m_ddgiDepthAtlas;
		std::shared_ptr<RHI::IGpuTexture> m_sssIntermediateTexture;

		ComputeKernel m_csHiZBuild;
		ComputeKernel m_csDDGIRadianceGather;
		ComputeKernel m_csDDGIDepthGather;
		std::shared_ptr<RHI::IPixelShader> m_psSSR;
		std::shared_ptr<RHI::IPixelShader> m_psSubsurface;

		// Milestone 2 (Atmospheric Lighting, Volumetrics & Camera VFX)
		std::shared_ptr<RHI::IGpuTexture> m_tileMaxTexture;
		std::shared_ptr<RHI::IGpuTexture> m_neighborMaxTexture;
		std::shared_ptr<RHI::IGpuTexture> m_motionBlurTexture;

		std::shared_ptr<RHI::IGpuTexture> m_volumetricFogRaw;
		std::shared_ptr<RHI::IGpuTexture> m_volumetricFogHistory[2];
		std::shared_ptr<RHI::IGpuTexture> m_volumetricFogIntegrated;
		uint32_t m_volumetricFogHistoryIndex = 0;

		std::shared_ptr<RHI::IGpuBuffer> m_bufLuminanceHistogram;
		std::shared_ptr<RHI::IGpuBuffer> m_bufAdaptedLuminance;

		std::shared_ptr<RHI::IGpuBuffer> m_cbMotionBlur;
		std::shared_ptr<RHI::IGpuBuffer> m_cbVolumetricFog;
		std::shared_ptr<RHI::IGpuBuffer> m_cbVolumetricFogIntegrate;
		std::shared_ptr<RHI::IGpuBuffer> m_cbHistogram;
		std::shared_ptr<RHI::IGpuBuffer> m_cbAverage;
		std::shared_ptr<RHI::IGpuBuffer> m_cbParaboloidShadow;

		ComputeKernel m_csTileMax;
		ComputeKernel m_csNeighborMax;
		std::shared_ptr<RHI::IPixelShader> m_psMotionBlur;

		ComputeKernel m_csVolumetricFog;
		ComputeKernel m_csVolumetricFogIntegrate;
		ComputeKernel m_csLuminanceHistogram;
		ComputeKernel m_csLuminanceAverage;

		std::shared_ptr<RHI::IVertexShader> m_vsShadowParaboloid;
		std::shared_ptr<RHI::IInputLayout>  m_inputLayoutShadowParaboloid;

		ShadowAtlasAllocator m_shadowAtlasAllocator;

		// Staging texture for readEntityId
		mutable void* m_stagingEntityId = nullptr;

		// Performance Stats
		uint32_t m_drawCallCount = 0;
		uint32_t m_batchCount = 0;
		uint32_t m_instanceCount = 0;

		// Frame & Animation state
		float m_totalTime = 0.0f;
		uint32_t m_frameIndex = 0;
		float m_adaptedExposure = 1.0f;

		void initializeResources();
		void buildDefaultPipelines();
		void generateSSAOKernel();
		void generateBRDFLUT();
		void generateColorLUT();
		void updateProceduralEnvIBL(const RenderScene& scene);
		const std::vector<Submesh>& selectLodSubmeshes(const GpuMesh& gpuMesh, float distanceToCamera, int32_t lodBias, uint32_t& outTotalIndexCount) const;
		void executePasses(const RenderScene& scene, const RenderView& view);
		void passShadows(const RenderScene& scene, const RenderView& view);
		void passDepthPrepass(const RenderScene& scene);
		void passSSAO(const RenderScene& scene, const RenderView& view);
		void passOpaque(const RenderScene& scene);
		void passGBuffer(const RenderScene& scene);
		void passDecals(const RenderScene& scene, bool isDeferred);
		void passDeferredLighting(const RenderScene& scene);
		void passSkybox(const RenderScene& scene);
		void passTerrain(const RenderScene& scene);
		void passFoliage(const RenderScene& scene);
		void passWater(const RenderScene& scene);
		void passTransparent(const RenderScene& scene);
		void passDebugLines(const RenderScene& scene, const RenderView& view);
		void passHiZBuild();
		void passSSR(const RenderScene& scene, const RenderView& view);
		void passDDGI(const RenderScene& scene);
		void passSubsurface(const RenderScene& scene);
		void passVolumetricFog(const RenderScene& scene, const RenderView& view);
		void passMotionBlur(const RenderScene& scene, const RenderView& view);
		void passAutoExposure(const RenderScene& scene, const RenderView& view);
		void passBloom(const RenderScene& scene);
		void passTonemap(const RenderScene& scene, const RenderView& view, const std::shared_ptr<RHI::IGpuTexture>& destination);
		void passFXAA(const RenderScene& scene, const std::shared_ptr<RHI::IGpuTexture>& source, const std::shared_ptr<RHI::IGpuTexture>& destination);
		void passTAA(const RenderScene& scene, const RenderView& view);
		void passPostProcess(const RenderScene& scene, const RenderView& view);
		void updateClusters(const RenderScene& scene, const RenderView& view);

	public:
		static float halton(uint32_t index, uint32_t base);
		void setTAAEnabled(bool enabled) noexcept { m_enableTAA = enabled; }
		[[nodiscard]] bool isTAAEnabled() const noexcept { return m_enableTAA; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuTexture> getVelocityTexture() const noexcept { return m_velocityTexture; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuBuffer> getClusterGridBuffer() const noexcept { return m_bufClusterGrid; }
		[[nodiscard]] std::shared_ptr<RHI::IGpuBuffer> getClusterLightIndicesBuffer() const noexcept { return m_bufClusterLightIndices; }

	private:
		GpuMesh createGpuMesh(const MeshData& meshData);
	};
}
