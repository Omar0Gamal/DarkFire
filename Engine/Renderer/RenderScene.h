#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/RHI/IRHI.h"
#include "Engine/Renderer/DrawItem.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Core/Scene/Components.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace Core::Renderer
{
	struct DF_API RenderMeshInstance
	{
		Math::Matrix world{};
		uint32_t entityId = 0;
		std::string meshPath;
		std::string materialPath;
		std::shared_ptr<Material> material;
		std::shared_ptr<RHI::IGpuBuffer> vertexBuffer;
		std::shared_ptr<RHI::IGpuBuffer> indexBuffer;
		uint32_t vertexStride = 0;
		uint32_t indexCount = 0;
		uint32_t startIndex = 0;
		int32_t baseVertex = 0;
		RenderLayer layer = RenderLayer::Opaque;
		float distanceToCamera = 0.0f;
		bool castShadows = true;
		bool receiveShadows = true;
	};

	struct DF_API RenderLight
	{
		Math::Vector3 position = Math::Vector3(0.0f, 0.0f, 0.0f);
		Math::Vector3 direction = Math::Vector3(0.0f, -1.0f, 0.0f);
		Math::Vector3 color = Math::Vector3(1.0f, 1.0f, 1.0f);
		float intensity = 1.0f;
		float range = 10.0f;
		float innerConeAngle = 0.785f; // ~45 deg
		float outerConeAngle = 0.872f; // ~50 deg
		uint32_t type = 0;             // 0 = Directional, 1 = Point, 2 = Spot
		bool castShadows = true;
		float shadowBias = 0.0015f;
		float shadowNormalBias = 0.02f;
		float shadowSoftness = 1.0f;
		float shadowDistance = 60.0f;
	};

	struct DF_API RenderCamera
	{
		Math::Matrix viewMatrix{};
		Math::Matrix projMatrix{};
		Math::Matrix viewProjMatrix{};
		Math::Matrix invViewMatrix{};
		Math::Matrix invProjMatrix{};
		Math::Matrix invViewProjMatrix{};
		Math::Vector3 position = Math::Vector3(0.0f, 0.0f, 0.0f);
		Math::Vector3 forward = Math::Vector3(0.0f, 0.0f, 1.0f);
		float nearZ = 0.1f;
		float farZ = 1000.0f;
		float fov = 1.047f; // ~60 deg
		float aspect = 1.777f;
		bool isOrthographic = false;
	};

	enum class SkyType : uint8_t
	{
		ProceduralGradient = 0,
		SkyboxTexture,
		CustomColor
	};

	enum class TonemapperType : uint8_t
	{
		ACES = 0,
		Reinhard,
		Linear,
		AgX,
		Neutral,
		ReinhardJodie
	};

	struct DF_API RenderEnvironment
	{
		SkyType skyType = SkyType::ProceduralGradient;
		Math::Vector4 skyColor = Math::Vector4(0.12f, 0.13f, 0.16f, 1.0f);
		Math::Vector4 horizonColor = Math::Vector4(0.35f, 0.38f, 0.44f, 1.0f);
		Math::Vector4 groundColor = Math::Vector4(0.08f, 0.08f, 0.09f, 1.0f);
		Math::Vector4 ambientColor = Math::Vector4(0.2f, 0.22f, 0.28f, 1.0f);
		float ambientIntensity = 0.5f;
		std::shared_ptr<RHI::IGpuTexture> skyboxTexture;
		std::string skyboxTexturePath;

		// Pipeline, Geometry, Shadows & Post Processing settings from scene
		uint8_t renderPipeline = 0; // 0 = Forward+, 1 = Deferred PBR
		bool enableShadows = true;
		bool enablePCSS = false;
		bool enableContactShadows = false;
		bool enableTessellation = false;
		float tessellationFactor = 3.0f;
		int32_t meshLodBias = 0; // -1=Force LOD0, 0=Auto Distance LOD, 1=Force LOD1, 2=Force LOD2

		bool enableTonemapping = true;
		TonemapperType tonemapper = TonemapperType::ACES;
		float exposure = 1.0f;
		float gamma = 2.2f;
		bool enableAutoExposure = false;
		float autoExposureMinEV = -1.5f;
		float autoExposureMaxEV = 1.5f;
		float autoExposureSpeed = 2.5f;
		float autoExposureSpeedUp = 3.0f;
		float autoExposureSpeedDown = 1.0f;
		bool enableMotionBlur = false;
		float motionBlurShutterAngle = 0.5f;
		float motionBlurMaxRadius = 32.0f;
		float volumetricFogHistoryBlend = 0.88f;
		float contrast = 1.0f;
		float saturation = 1.0f;
		float vignetteIntensity = 0.0f;
		float colorTemperature = 0.0f;

		bool enableBoxProjectedIBL = false;
		Math::Vector3 iblBoxCenter = Math::Vector3(0.0f, 6.0f, 0.0f);
		Math::Vector3 iblBoxExtents = Math::Vector3(30.0f, 16.0f, 18.0f);
		float iblBoxIntensity = 1.0f;
		bool captureProbeSceneGeometry = true;
		std::string bakedProbeCubemapPath;

		// Temporal Anti-Aliasing (Tier 1)
		bool enableTAA = true;

		// High-Performance GPU Compute (Tier 1)
		bool enableClusteredLighting = true;
		bool enableGpuCulling = true;

		bool enableFXAA = true;
		float fxaaSubpix = 0.75f;
		float fxaaEdgeThreshold = 0.166f;
		float fxaaEdgeThresholdMin = 0.0833f;
		float casSharpening = 0.35f;

		bool enableSSAO = false;
		float ssaoRadius = 0.5f;
		float ssaoBias = 0.025f;
		float ssaoIntensity = 1.0f;

		bool enableBloom = false;
		float bloomThreshold = 1.0f;
		float bloomIntensity = 0.04f;

		float ssaoDirectLightInfluence = 0.35f;

		// Atmospheric Fog
		bool enableFog = false;
		Math::Vector4 fogColor = Math::Vector4(0.45f, 0.52f, 0.65f, 1.0f);
		float fogDensity = 0.008f;
		float fogHeightFalloff = 0.08f;
		float fogSunScattering = 0.35f;

		// Screen-Space Reflections (Hi-Z SSR)
		bool enableSSR = false;
		float ssrMaxDistance = 25.0f;
		float ssrThickness = 0.5f;
		float ssrRoughnessCutoff = 0.65f;
		float ssrIntensity = 1.0f;
		uint32_t ssrStepCount = 32;

		// Separable Subsurface Scattering (SSS)
		bool enableSSS = false;
		float sssRadius = 1.5f;
		float sssMaxDepthDiff = 0.2f;

		// DDGI Dynamic Probe Volume
		bool enableDDGI = false;
		Math::Vector3 ddgiOrigin = Math::Vector3(0.0f, 0.0f, 0.0f);
		Math::Vector3 ddgiProbeCounts = Math::Vector3(16.0f, 8.0f, 16.0f);
		float ddgiProbeSpacing = 2.0f;
		float ddgiNormalBias = 0.15f;
		float ddgiHysteresis = 0.95f;
		float ddgiIntensity = 1.0f;
		float ddgiEnergyPreservation = 0.85f;

		uint32_t debugView = 0; // 0=Lit, 1=SSAO, 2=Bloom, 3=Shadows, 4=Normals, 5=Albedo, 6=MetallicRoughness
	};

	struct DF_API RenderParticleInstance
	{
		Math::Vector3 position;
		float size = 0.1f;
		Math::Vector4 color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		float distanceToCamera = 0.0f;
		std::string texturePath;
		BlendMode blendMode = BlendMode::Translucent;
	};

	struct DF_API RenderTerrainInstance
	{
		Math::Matrix world{};
		uint32_t entityId = 0;
		Core::TerrainComponent component{};
	};

	struct DF_API RenderFoliageInstance
	{
		Math::Matrix world{};
		uint32_t entityId = 0;
		Core::FoliageComponent component{};
	};

	struct DF_API RenderWaterInstance
	{
		Math::Matrix world{};
		uint32_t entityId = 0;
		Core::WaterComponent component{};
	};

	struct DF_API RenderDecalInstance
	{
		Math::Matrix world{};
		Math::Matrix invWorld{};
		uint32_t entityId = 0;
		Math::Vector4 color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		std::string albedoTexturePath;
		std::string normalTexturePath;
		float normalStrength = 1.0f;
		float roughness = 0.5f;
		float fadeDistance = 50.0f;
	};

	struct DF_API RenderScene
	{
		RenderCamera camera;
		RenderEnvironment environment;
		std::vector<RenderMeshInstance> meshes;
		std::vector<RenderLight> lights;
		std::vector<RenderParticleInstance> particles;
		std::vector<RenderTerrainInstance> terrains;
		std::vector<RenderFoliageInstance> foliage;
		std::vector<RenderWaterInstance> waters;
		std::vector<RenderDecalInstance> decals;

		void clear()
		{
			meshes.clear();
			lights.clear();
			particles.clear();
			terrains.clear();
			foliage.clear();
			waters.clear();
			decals.clear();
		}
	};

	struct DF_API RenderViewOverrides
	{
		bool overrideFXAA = false;
		bool enableFXAA = true;
		bool overrideSSAO = false;
		bool enableSSAO = true;
		bool overrideBloom = false;
		bool enableBloom = true;
		bool overrideShadows = false;
		bool enableShadows = true;
		uint32_t debugView = 0; // 0 = Lit, 1 = SSAO, 2 = Bloom, 3 = Shadows, 4 = Normals, 5 = Albedo, 6 = MetallicRoughness, 7 = SSR, 8 = SSGI
		bool overrideWireframe = false;
		bool enableWireframe = false;
	};

	struct DF_API RenderView
	{
		uint32_t width = 1280;
		uint32_t height = 720;
		std::shared_ptr<RHI::IGpuTexture> targetTexture; // Offscreen target or null for backbuffer
		std::shared_ptr<RHI::ISwapChain> swapChain;      // Optional swapchain
		bool isEditorView = false;
		RenderViewOverrides overrides{};
	};
}
