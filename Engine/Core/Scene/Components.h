#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Scene/UUID.h"
#include <entt/entt.hpp>
#include <string>
#include <vector>

namespace Physics { class BVH; }

namespace Core
{
	// ==========================================
	// Core Identity & Hierarchy Components
	// ==========================================

	struct DF_API IdentityComponent
	{
		UUID id;
		std::string name = "Entity";
		std::string tag = "Default";
		bool isActive = true;

		IdentityComponent() = default;
		explicit IdentityComponent(UUID uuid, std::string n = "Entity", std::string t = "Default", bool active = true)
			: id(uuid), name(std::move(n)), tag(std::move(t)), isActive(active) {}
	};

	struct DF_API HierarchyComponent
	{
		entt::entity parent = entt::null;
		std::vector<entt::entity> children;

		HierarchyComponent() = default;
	};

	// Authored local transform
	struct DF_API TransformComponent
	{
		Vector3 position = Vector3(0.0f, 0.0f, 0.0f);
		Quaternion rotation = Quaternion(0.0f, 0.0f, 0.0f, 1.0f);
		Vector3 scale = Vector3(1.0f, 1.0f, 1.0f);
		Vector3 eulerDegrees = Vector3(0.0f, 0.0f, 0.0f); // Authoring & Inspector presentation

		Vector3 prevPosition = Vector3(0.0f, 0.0f, 0.0f);
		Quaternion prevRotation = Quaternion(0.0f, 0.0f, 0.0f, 1.0f);

		TransformComponent() = default;
		TransformComponent(const Vector3& pos, const Quaternion& rot, const Vector3& scl)
			: position(pos), rotation(rot), scale(scl), prevPosition(pos), prevRotation(rot)
		{
			Vector3 rads = rot.ToEuler();
			eulerDegrees = Vector3(Math::toDegrees(rads.x), Math::toDegrees(rads.y), Math::toDegrees(rads.z));
		}

		void syncPreviousTransform() noexcept
		{
			prevPosition = position;
			prevRotation = rotation;
		}

		Matrix getInterpolatedMatrix(float alpha) const noexcept
		{
			Vector3 p = Vector3::Lerp(prevPosition, position, alpha);
			Quaternion q = Quaternion::Slerp(prevRotation, rotation, alpha);
			return Math::createTransformTRS(p, q, scale);
		}

		Matrix getLocalMatrix() const noexcept
		{
			return Math::createTransformTRS(position, rotation, scale);
		}

		void setEulerDegrees(const Vector3& degs) noexcept
		{
			eulerDegrees = degs;
			rotation = Math::eulerToQuaternion(Math::toRadians(degs.x), Math::toRadians(degs.y), Math::toRadians(degs.z));
		}

		Vector3 getForward() const noexcept
		{
			return Vector3::Transform(Vector3(0.0f, 0.0f, 1.0f), rotation);
		}

		Vector3 getUp() const noexcept
		{
			return Vector3::Transform(Vector3(0.0f, 1.0f, 0.0f), rotation);
		}

		Vector3 getRight() const noexcept
		{
			return Vector3::Transform(Vector3(1.0f, 0.0f, 0.0f), rotation);
		}
	};

	// ==========================================
	// Runtime-Only Components (Ignored by Serializer & Scene Copy)
	// ==========================================

	struct DF_API WorldTransform
	{
		Matrix worldMatrix = DirectX::XMMatrixIdentity();
		bool isDirty = true;
	};

	struct DF_API RigidbodyRuntimeState
	{
		Vector3 linearVelocity = Vector3(0.0f, 0.0f, 0.0f);
		Vector3 angularVelocity = Vector3(0.0f, 0.0f, 0.0f);
	};

	struct DF_API Particle
	{
		Vector3 position = Vector3(0.0f, 0.0f, 0.0f);
		Vector3 velocity = Vector3(0.0f, 0.0f, 0.0f);
		Color color = Color(1.0f, 1.0f, 1.0f, 1.0f);
		float size = 0.1f;
		float age = 0.0f;
		float lifetime = 1.0f;
	};

	struct DF_API ParticleRuntimeState
	{
		int activeCount = 0;
		float emitAccumulator = 0.0f;
		uint32_t rngSeed = 1337;
		std::vector<Particle> particles;
	};

	// ==========================================
	// Authored Gameplay & Engine Components
	// ==========================================

	struct DF_API CameraComponent
	{
		float fov = 60.0f; // vertical FOV in degrees
		float nearZ = 0.1f;
		float farZ = 1000.0f;
		bool isPrimary = true;
		bool isOrthographic = false;
		float orthoWidth = 10.0f;
		float orthoHeight = 10.0f;

		Matrix getProjection(float aspectRatio) const noexcept
		{
			if (isOrthographic)
			{
				return Math::orthographicLH_ReversedZ(orthoWidth, orthoHeight, nearZ, farZ);
			}
			return Math::perspectiveFovLH_ReversedZ(Math::toRadians(fov), aspectRatio, nearZ, farZ);
		}
	};

	struct DF_API MeshRendererComponent
	{
		std::string meshPath;
		std::string materialPath;
		bool castShadows = true;
		bool receiveShadows = true;
	};

	enum class LightType : uint8_t
	{
		Directional = 0,
		Point,
		Spot
	};

	struct DF_API LightComponent
	{
		LightType type = LightType::Directional;
		Color color = Color(1.0f, 1.0f, 1.0f, 1.0f);
		float intensity = 1.0f;
		float range = 10.0f;
		float spotInnerAngle = 30.0f; // degrees
		float spotOuterAngle = 45.0f; // degrees
		bool castShadows = true;
		float shadowBias = 0.0015f;
		float shadowNormalBias = 0.02f;
		float shadowSoftness = 1.0f;
		float shadowDistance = 60.0f;
	};

	struct DF_API RigidbodyComponent
	{
		float mass = 1.0f;
		float drag = 0.05f;
		float angularDrag = 0.05f;
		bool isKinematic = false;
		bool useGravity = true;
		float restitution = 0.2f;
		float friction = 0.5f;
	};

		struct DF_API CollisionTriangle
	{
		Vector3 v0 = Vector3(0.0f, 0.0f, 0.0f);
		Vector3 v1 = Vector3(0.0f, 0.0f, 0.0f);
		Vector3 v2 = Vector3(0.0f, 0.0f, 0.0f);
		Vector3 normal = Vector3(0.0f, 1.0f, 0.0f);
	};

	struct DF_API CollisionMesh
	{
		std::vector<Vector3> vertices;
		std::vector<uint32_t> indices;
		std::vector<CollisionTriangle> triangles;
		std::shared_ptr<Physics::BVH> bvh;
		Vector3 boundsMin = Vector3(-0.5f, -0.5f, -0.5f);
		Vector3 boundsMax = Vector3(0.5f, 0.5f, 0.5f);
	};

	enum class ColliderShape : uint8_t
	{
		Box = 0,
		Sphere,
		Capsule,
		TriangleMesh,
		ConvexMesh
	};

	struct DF_API ColliderComponent
	{
		ColliderShape shape = ColliderShape::Box;
		Vector3 size = Vector3(1.0f, 1.0f, 1.0f);
		float radius = 0.5f;
		float height = 1.0f;
		bool isTrigger = false;
		uint8_t layer = 0; // Layer index 0..15 for collision filtering
		std::string meshPath;
		std::shared_ptr<CollisionMesh> collisionMesh;
	};

	enum class ParticleBlendMode : uint8_t
	{
		AlphaBlend = 0,
		Additive
	};

	enum class SimulationSpace : uint8_t
	{
		World = 0,
		Local
	};

	struct DF_API ParticleEmitterComponent
	{
		int maxParticles = 1000;
		float emissionRate = 50.0f;
		float lifetime = 2.0f;
		Vector3 startVelocity = Vector3(0.0f, 5.0f, 0.0f);
		float velocityRandomness = 1.0f;
		float gravityScale = 0.0f;
		float startSize = 0.2f;
		float endSize = 0.05f;
		Color startColor = Color(1.0f, 1.0f, 1.0f, 1.0f);
		Color endColor = Color(1.0f, 1.0f, 1.0f, 0.0f);
		ParticleBlendMode blendMode = ParticleBlendMode::AlphaBlend;
		SimulationSpace simulationSpace = SimulationSpace::World;
		bool looping = true;
		bool playOnStart = true;
		std::string texturePath;
	};

	// ==========================================
	// Scripting Component
	// ==========================================

	class ScriptableEntity;

	struct DF_API ScriptInstance
	{
		std::string scriptName;
		std::shared_ptr<ScriptableEntity> instance = nullptr;
		bool isEnabled = true;
		bool isStarted = false;
		std::string serializedFieldsJson = "{}";
	};

	struct DF_API MissingScriptData
	{
		std::string scriptName;
		std::string rawJson;
	};

	struct DF_API ScriptsComponent
	{
		std::vector<ScriptInstance> scripts;
		std::vector<MissingScriptData> missingScripts;

		ScriptsComponent() = default;

		template<typename T>
		T* getScript()
		{
			for (auto& s : scripts)
			{
				if (s.instance)
				{
					if (auto casted = dynamic_cast<T*>(s.instance.get()))
					{
						return casted;
					}
				}
			}
			return nullptr;
		}

		void addScript(const std::string& name, std::shared_ptr<ScriptableEntity> inst = nullptr)
		{
			scripts.push_back(ScriptInstance{ name, std::move(inst), true, false });
		}
	};

	struct DF_API PrefabInstanceComponent
	{
		uint64_t prefabGuid = 0;
		std::string prefabPath;
	};

	enum class SkyType : uint8_t
	{
		ProceduralGradient = 0,
		SkyboxTexture,
		CustomColor
	};

	struct DF_API EnvironmentComponent
	{
		SkyType skyType = SkyType::ProceduralGradient;
		Color skyColor = Color(0.12f, 0.13f, 0.16f, 1.0f);
		Color horizonColor = Color(0.35f, 0.38f, 0.44f, 1.0f);
		Color groundColor = Color(0.08f, 0.08f, 0.09f, 1.0f);
		Color ambientColor = Color(0.2f, 0.22f, 0.28f, 1.0f);
		float ambientEnergy = 0.5f;
		std::string skyboxTexturePath;

		// Atmospheric Fog
		bool enableFog = false;
		Color fogColor = Color(0.45f, 0.52f, 0.65f, 1.0f);
		float fogDensity = 0.008f;
		float fogHeightFalloff = 0.08f;
		float fogSunScattering = 0.35f;
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

	struct DF_API PostProcessingComponent
	{
		// Buildable Render Pipeline Selection (0 = Forward+, 1 = Deferred PBR)
		uint8_t renderPipeline = 0;

		// Directional & Spot Shadows + PCSS & Contact Shadows
		bool enableShadows = true;
		bool enablePCSS = false;
		bool enableContactShadows = false;

		// Hardware Tessellation (HS/DS) & Mesh LODs
		bool enableTessellation = false;
		float tessellationFactor = 3.0f;
		int32_t meshLodBias = 0; // -1=Force LOD0, 0=Auto Distance LOD, 1=Force LOD1, 2=Force LOD2

		// Tonemapping, Auto-Exposure (Eye Adaptation) & Color Grading
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
		float contrast = 1.0f;
		float saturation = 1.0f;
		float vignetteIntensity = 0.0f;
		float colorTemperature = 0.0f; // -1.0 (Cool) to +1.0 (Warm)

		// Box-Projected Local Reflection Probe IBL
		bool enableBoxProjectedIBL = false;
		Vector3 iblBoxCenter = Vector3(0.0f, 6.0f, 0.0f);
		Vector3 iblBoxExtents = Vector3(30.0f, 16.0f, 18.0f);

		// TAA (Temporal Anti-Aliasing with Halton Jitter & Velocity Reprojection)
		bool enableTAA = true;

		// High-Performance GPU Compute (Tier 1)
		bool enableClusteredLighting = true;
		bool enableGpuCulling = true;

		// FXAA 3.11 + Contrast Adaptive Sharpening (CAS)
		bool enableFXAA = true;
		float fxaaSubpix = 0.75f;
		float fxaaEdgeThreshold = 0.166f;
		float fxaaEdgeThresholdMin = 0.0833f;
		float casSharpening = 0.35f;

		// SSAO (Screen Space Ambient Occlusion)
		bool enableSSAO = false;
		float ssaoRadius = 0.5f;
		float ssaoBias = 0.025f;
		float ssaoIntensity = 1.2f;
		float ssaoDirectLightInfluence = 0.35f;

		// Bloom (UE subtle defaults)
		bool enableBloom = false;
		float bloomThreshold = 1.0f;
		float bloomIntensity = 0.04f;

		// SSR (Hierarchical-Z Screen-Space Reflections)
		bool enableSSR = false;
		float ssrMaxDistance = 25.0f;
		float ssrThickness = 0.5f;
		float ssrRoughnessCutoff = 0.65f;
		float ssrIntensity = 1.0f;
		uint32_t ssrStepCount = 32;

		// SSS (Separable Subsurface Scattering)
		bool enableSSS = false;
		float sssRadius = 1.5f;
		float sssMaxDepthDiff = 0.2f;

		uint32_t debugView = 0; // 0=Lit, 1=SSAO, 2=Bloom, 3=Shadows, 4=Normals, 5=Albedo, 6=MetallicRoughness
	};

	struct DF_API DDGIProbeVolumeComponent
	{
		bool enabled = true;
		Vector3 origin = Vector3(0.0f, 0.0f, 0.0f);
		Vector3 probeCounts = Vector3(16.0f, 8.0f, 16.0f);
		float probeSpacing = 2.0f;
		float normalBias = 0.15f;
		float hysteresis = 0.95f;
		float intensity = 1.0f;
		float energyPreservation = 0.85f;
	};

	struct DF_API ReflectionProbeComponent
	{
		Vector3 boxExtents = Vector3(30.0f, 16.0f, 18.0f);
		float intensity = 1.0f;
		bool boxProjection = true;
		bool captureSceneGeometry = true;
		uint32_t resolution = 128;
		std::string bakedCubemapPath;
		bool isDirty = true;
	};

	struct DF_API DecalComponent
	{
		std::string albedoTexturePath;
		std::string normalTexturePath;
		Color color = Color(1.0f, 1.0f, 1.0f, 1.0f);
		float normalStrength = 1.0f;
		float roughness = 0.5f;
		float fadeDistance = 50.0f;
		Vector3 extents = Vector3(2.0f, 1.0f, 2.0f);
	};

	struct DF_API SplineComponent
	{
		std::vector<Vector3> controlPoints = { Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 0.0f, 10.0f) };
		bool isClosedLoop = false;
	};

	// ==========================================
	// World, Nature & Environment Components (Phases 5-7)
	// ==========================================

	struct DF_API TerrainComponent
	{
		uint32_t resolution = 128;
		float worldSize = 200.0f;
		float maxHeight = 32.0f;
		uint32_t seed = 1337;
		uint32_t octaves = 6;
		float frequency = 0.015f;
		float persistence = 0.5f;
		float lacunarity = 2.0f;
		float ridgeWeight = 0.4f;
		bool isIsland = true;
		float islandRadius = 85.0f;
		float coastlineFalloff = 2.2f;
		float seaFloorDepth = -8.0f;
		float sandHeight = 2.5f;
		float cliffSlopeThreshold = 0.65f;
		float snowHeight = 22.0f;
	};

	struct DF_API FoliageComponent
	{
		uint32_t bladeCount = 32768;
		Vector2 patchDimensions = Vector2(120.0f, 120.0f);
		float bladeHeightMin = 0.4f;
		float bladeHeightMax = 0.85f;
		float bladeWidth = 0.05f;
		float windSpeed = 2.2f;
		float windStrength = 0.3f;
		float windFrequency = 0.7f;
		float maxDrawDistance = 85.0f;
		Color rootColor = Color(0.07f, 0.20f, 0.04f, 1.0f);
		Color tipColor = Color(0.34f, 0.60f, 0.15f, 1.0f);
		bool snapToTerrain = true;
		float minAltitude = 2.2f;
		float maxAltitude = 20.0f;
		float maxSlope = 0.32f;
	};

	struct DF_API WaterComponent
	{
		float waterLevel = 0.0f;
		float planeSize = 300.0f;
		uint32_t gridResolution = 128;
		float waveAmplitude = 0.35f;
		float waveFrequency = 0.18f;
		float waveSpeed = 1.4f;
		float waveSteepness = 0.6f;
		Color shallowColor = Color(0.08f, 0.58f, 0.62f, 1.0f);
		Color deepColor = Color(0.02f, 0.10f, 0.24f, 1.0f);
		Color foamColor = Color(0.92f, 0.96f, 1.0f, 1.0f);
		float depthAbsorption = 0.22f;
		float foamDepthThreshold = 1.2f;
		float refractionStrength = 0.03f;
		float causticsIntensity = 0.45f;
		float buoyancyDensity = 2.5f;
		float fluidDrag = 1.8f;
		float sssIntensity = 1.0f;
		float jacobianThreshold = 0.35f;
		Color sssColor = Color(0.05f, 0.65f, 0.55f, 1.0f);
		float windAngle = 0.785f;
	};

	// ==========================================
	// Audio & HUD Telemetry Components (Phase 8)
	// ==========================================

	enum class AudioBus : uint8_t
	{
		Master = 0,
		SFX,
		Ambient,
		Engine,
		Music,
		Count
	};

	struct DF_API AudioSourceComponent
	{
		std::string clipPath = "builtin:ocean_waves"; // "builtin:ocean_waves", "builtin:wind", or file path (.wav/.mp3/.flac)
		float volume = 0.8f;
		float pitch = 1.0f;
		bool loop = true;
		bool spatialize3D = true;
		float minDistance = 2.0f;
		float maxDistance = 80.0f;
		bool playOnStart = true;
		AudioBus bus = AudioBus::SFX;
		float dopplerFactor = 1.0f;

		// Parameter smoothing API
		float targetVolume = 0.8f;
		float volumeSlewRateHz = 10.0f;
		float targetPitch = 1.0f;
		float pitchSlewRateHz = 10.0f;

		AudioSourceComponent& setVolumeSmooth(float target, float slewRate = 10.0f) noexcept
		{
			targetVolume = target;
			volumeSlewRateHz = slewRate;
			return *this;
		}

		AudioSourceComponent& setPitchSmooth(float target, float slewRate = 10.0f) noexcept
		{
			targetPitch = target;
			pitchSlewRateHz = slewRate;
			return *this;
		}
	};

	struct DF_API AudioListenerComponent
	{
		bool active = true;
		float masterVolume = 1.0f;
	};

	struct DF_API HUDTelemetryComponent
	{
		bool showOverlay = true;
		bool showHotkeys = true;
		std::string customBannerText = "DarkFire Engine 2.0 - Graphics Benchmark";
	};
}

