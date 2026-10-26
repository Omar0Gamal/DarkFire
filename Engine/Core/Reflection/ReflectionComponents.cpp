#include "Reflection.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Components.h"

namespace Core
{
	using TypeRegistry = ComponentRegistry;

	template<> struct FieldTypeTraits<std::vector<Vector3>> { static constexpr FieldType type = FieldType::Vec3; };

	void registerComponentTypes(TypeRegistry& registry)
	{
		// 1. IdentityComponent
		ComponentBuilder<IdentityComponent>("IdentityComponent")
			.field("id", &IdentityComponent::id)
			.field("name", &IdentityComponent::name)
			.field("tag", &IdentityComponent::tag)
			.registerSelf();

		// 2. TransformComponent
		ComponentBuilder<TransformComponent>("TransformComponent")
			.field("position", &TransformComponent::position)
			.field("rotation", &TransformComponent::rotation)
			.field("scale", &TransformComponent::scale)
			.field("eulerDegrees", &TransformComponent::eulerDegrees)
			.registerSelf();

		// 3. CameraComponent
		ComponentBuilder<CameraComponent>("CameraComponent")
			.field("fov", &CameraComponent::fov, { true, 1.0f, 179.0f, "Field of view in degrees" })
			.field("nearZ", &CameraComponent::nearZ, { true, 0.001f, 100.0f, "Near clipping plane" })
			.field("farZ", &CameraComponent::farZ, { true, 1.0f, 10000.0f, "Far clipping plane" })
			.field("isPrimary", &CameraComponent::isPrimary)
			.field("isOrthographic", &CameraComponent::isOrthographic)
			.field("orthoWidth", &CameraComponent::orthoWidth)
			.field("orthoHeight", &CameraComponent::orthoHeight)
			.registerSelf();

		// 4. MeshRendererComponent
		ComponentBuilder<MeshRendererComponent>("MeshRendererComponent")
			.field("meshPath", &MeshRendererComponent::meshPath)
			.field("materialPath", &MeshRendererComponent::materialPath)
			.field("castShadows", &MeshRendererComponent::castShadows)
			.field("receiveShadows", &MeshRendererComponent::receiveShadows)
			.registerSelf();

		// 5. LightComponent
		ComponentBuilder<LightComponent>("LightComponent")
			.enumField("type", &LightComponent::type, { "Directional", "Point", "Spot" })
			.field("color", &LightComponent::color, { false, 0, 0, "", true })
			.field("intensity", &LightComponent::intensity, { true, 0.0f, 100.0f })
			.field("range", &LightComponent::range, { true, 0.1f, 1000.0f })
			.field("spotInnerAngle", &LightComponent::spotInnerAngle)
			.field("spotOuterAngle", &LightComponent::spotOuterAngle)
			.field("castShadows", &LightComponent::castShadows)
			.field("shadowBias", &LightComponent::shadowBias, { true, 0.0001f, 0.05f })
			.field("shadowNormalBias", &LightComponent::shadowNormalBias, { true, 0.001f, 0.2f })
			.field("shadowSoftness", &LightComponent::shadowSoftness, { true, 0.0f, 5.0f })
			.field("shadowDistance", &LightComponent::shadowDistance, { true, 5.0f, 500.0f })
			.registerSelf();

		// 6. RigidbodyComponent
		ComponentBuilder<RigidbodyComponent>("RigidbodyComponent")
			.field("mass", &RigidbodyComponent::mass, { true, 0.001f, 10000.0f })
			.field("drag", &RigidbodyComponent::drag, { true, 0.0f, 100.0f })
			.field("angularDrag", &RigidbodyComponent::angularDrag, { true, 0.0f, 100.0f })
			.field("isKinematic", &RigidbodyComponent::isKinematic)
			.field("useGravity", &RigidbodyComponent::useGravity)
			.field("restitution", &RigidbodyComponent::restitution, { true, 0.0f, 1.0f })
			.field("friction", &RigidbodyComponent::friction, { true, 0.0f, 10.0f })
			.registerSelf();

		// 7. ColliderComponent
		ComponentBuilder<ColliderComponent>("ColliderComponent")
			.enumField("shape", &ColliderComponent::shape, { "Box", "Sphere", "Capsule", "Mesh" })
			.field("size", &ColliderComponent::size)
			.field("radius", &ColliderComponent::radius)
			.field("height", &ColliderComponent::height)
			.field("isTrigger", &ColliderComponent::isTrigger)
			.field("layer", &ColliderComponent::layer)
			.registerSelf();

		// 8. ParticleEmitterComponent
		ComponentBuilder<ParticleEmitterComponent>("ParticleEmitterComponent")
			.field("maxParticles", &ParticleEmitterComponent::maxParticles)
			.field("emissionRate", &ParticleEmitterComponent::emissionRate)
			.field("lifetime", &ParticleEmitterComponent::lifetime)
			.field("startVelocity", &ParticleEmitterComponent::startVelocity)
			.field("velocityRandomness", &ParticleEmitterComponent::velocityRandomness)
			.field("gravityScale", &ParticleEmitterComponent::gravityScale)
			.field("startSize", &ParticleEmitterComponent::startSize)
			.field("endSize", &ParticleEmitterComponent::endSize)
			.field("startColor", &ParticleEmitterComponent::startColor, { false, 0, 0, "", true })
			.field("endColor", &ParticleEmitterComponent::endColor, { false, 0, 0, "", true })
			.enumField("blendMode", &ParticleEmitterComponent::blendMode, { "AlphaBlend", "Additive" })
			.enumField("simulationSpace", &ParticleEmitterComponent::simulationSpace, { "World", "Local" })
			.field("looping", &ParticleEmitterComponent::looping)
			.field("playOnStart", &ParticleEmitterComponent::playOnStart)
			.field("texturePath", &ParticleEmitterComponent::texturePath)
			.registerSelf();

		// 9. ScriptsComponent
		{
			ComponentDesc desc;
			desc.name = "ScriptsComponent";
			desc.typeId = entt::type_hash<ScriptsComponent>::value();
			desc.isAuthored = true;
			desc.hasComponent = [](const Scene& scene, entt::entity e) {
				return scene.getRegistry().all_of<ScriptsComponent>(e);
			};
			desc.addDefaultComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().emplace_or_replace<ScriptsComponent>(e);
			};
			desc.removeComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().remove<ScriptsComponent>(e);
			};
			desc.getComponentRaw = [](Scene& scene, entt::entity e) -> void* {
				return scene.getRegistry().try_get<ScriptsComponent>(e);
			};
			desc.getComponentRawConst = [](const Scene& scene, entt::entity e) -> const void* {
				return scene.getRegistry().try_get<ScriptsComponent>(e);
			};
			desc.copyComponent = [](const Scene& src, entt::entity s, Scene& dst, entt::entity d) {
				const auto& srcComp = src.getRegistry().get<ScriptsComponent>(s);
				auto& dstComp = dst.getRegistry().emplace_or_replace<ScriptsComponent>(d);
				dstComp.missingScripts = srcComp.missingScripts;
				for (const auto& scr : srcComp.scripts)
				{
					ScriptInstance copyInst;
					copyInst.scriptName = scr.scriptName;
					copyInst.isEnabled = scr.isEnabled;
					copyInst.isStarted = false;
					copyInst.serializedFieldsJson = scr.serializedFieldsJson;
					dstComp.scripts.push_back(std::move(copyInst));
				}
			};
			registry.registerComponent(std::move(desc));
		}

		// 10. PrefabInstanceComponent
		ComponentBuilder<PrefabInstanceComponent>("PrefabInstanceComponent")
			.field("prefabGuid", &PrefabInstanceComponent::prefabGuid)
			.field("prefabPath", &PrefabInstanceComponent::prefabPath)
			.registerSelf();

		// 11. RigidbodyRuntimeState (Runtime only)
		ComponentBuilder<RigidbodyRuntimeState>("RigidbodyRuntimeState", false).registerSelf();

		// 12. ParticleRuntimeState (Runtime only)
		ComponentBuilder<ParticleRuntimeState>("ParticleRuntimeState", false).registerSelf();

		// 13. EnvironmentComponent
		ComponentBuilder<EnvironmentComponent>("EnvironmentComponent")
			.enumField("skyType", &EnvironmentComponent::skyType, { "Procedural Gradient", "Skybox Texture", "Custom Color" })
			.field("skyColor", &EnvironmentComponent::skyColor, { false, 0, 0, "", true })
			.field("horizonColor", &EnvironmentComponent::horizonColor, { false, 0, 0, "", true })
			.field("groundColor", &EnvironmentComponent::groundColor, { false, 0, 0, "", true })
			.field("ambientColor", &EnvironmentComponent::ambientColor, { false, 0, 0, "", true })
			.field("ambientEnergy", &EnvironmentComponent::ambientEnergy, { true, 0.0f, 5.0f })
			.field("skyboxTexturePath", &EnvironmentComponent::skyboxTexturePath)
			.field("enableFog", &EnvironmentComponent::enableFog)
			.field("fogColor", &EnvironmentComponent::fogColor, { false, 0, 0, "", true })
			.field("fogDensity", &EnvironmentComponent::fogDensity, { true, 0.0f, 0.1f })
			.field("fogHeightFalloff", &EnvironmentComponent::fogHeightFalloff, { true, 0.001f, 1.0f })
			.field("fogSunScattering", &EnvironmentComponent::fogSunScattering, { true, 0.0f, 2.0f })
			.registerSelf();

		// 14. PostProcessingComponent
		ComponentBuilder<PostProcessingComponent>("PostProcessingComponent")
			.field("renderPipeline", &PostProcessingComponent::renderPipeline)
			.field("enableShadows", &PostProcessingComponent::enableShadows)
			.field("enablePCSS", &PostProcessingComponent::enablePCSS)
			.field("enableContactShadows", &PostProcessingComponent::enableContactShadows)
			.field("enableTessellation", &PostProcessingComponent::enableTessellation)
			.field("tessellationFactor", &PostProcessingComponent::tessellationFactor, { true, 1.0f, 8.0f })
			.field("meshLodBias", &PostProcessingComponent::meshLodBias)
			.field("enableTonemapping", &PostProcessingComponent::enableTonemapping)
			.enumField("tonemapper", &PostProcessingComponent::tonemapper, { "ACES Filmic", "Reinhard", "Linear", "AgX", "Khronos Neutral", "Reinhard-Jodie" })
			.field("enableMotionBlur", &PostProcessingComponent::enableMotionBlur)
			.field("motionBlurShutterAngle", &PostProcessingComponent::motionBlurShutterAngle, { true, 0.0f, 1.0f })
			.field("motionBlurMaxRadius", &PostProcessingComponent::motionBlurMaxRadius, { true, 1.0f, 100.0f })
			.field("exposure", &PostProcessingComponent::exposure, { true, 0.01f, 10.0f })
			.field("gamma", &PostProcessingComponent::gamma, { true, 1.0f, 3.0f })
			.field("enableAutoExposure", &PostProcessingComponent::enableAutoExposure)
			.field("autoExposureMinEV", &PostProcessingComponent::autoExposureMinEV, { true, -8.0f, 0.0f })
			.field("autoExposureMaxEV", &PostProcessingComponent::autoExposureMaxEV, { true, 0.0f, 8.0f })
			.field("autoExposureSpeed", &PostProcessingComponent::autoExposureSpeed, { true, 0.1f, 10.0f })
			.field("contrast", &PostProcessingComponent::contrast, { true, 0.5f, 1.8f })
			.field("saturation", &PostProcessingComponent::saturation, { true, 0.0f, 2.0f })
			.field("vignetteIntensity", &PostProcessingComponent::vignetteIntensity, { true, 0.0f, 1.0f })
			.field("colorTemperature", &PostProcessingComponent::colorTemperature, { true, -1.0f, 1.0f })
			.field("enableBoxProjectedIBL", &PostProcessingComponent::enableBoxProjectedIBL)
			.field("iblBoxCenter", &PostProcessingComponent::iblBoxCenter)
			.field("iblBoxExtents", &PostProcessingComponent::iblBoxExtents)
			.field("enableTAA", &PostProcessingComponent::enableTAA)
			.field("enableClusteredLighting", &PostProcessingComponent::enableClusteredLighting)
			.field("enableGpuCulling", &PostProcessingComponent::enableGpuCulling)
			.field("enableFXAA", &PostProcessingComponent::enableFXAA)
			.field("fxaaSubpix", &PostProcessingComponent::fxaaSubpix, { true, 0.0f, 1.0f })
			.field("fxaaEdgeThreshold", &PostProcessingComponent::fxaaEdgeThreshold, { true, 0.063f, 0.333f })
			.field("fxaaEdgeThresholdMin", &PostProcessingComponent::fxaaEdgeThresholdMin, { true, 0.0312f, 0.0833f })
			.field("casSharpening", &PostProcessingComponent::casSharpening, { true, 0.0f, 1.0f })
			.field("enableSSAO", &PostProcessingComponent::enableSSAO)
			.field("ssaoRadius", &PostProcessingComponent::ssaoRadius, { true, 0.1f, 3.0f })
			.field("ssaoBias", &PostProcessingComponent::ssaoBias, { true, 0.001f, 0.1f })
			.field("ssaoIntensity", &PostProcessingComponent::ssaoIntensity, { true, 0.0f, 5.0f })
			.field("ssaoDirectLightInfluence", &PostProcessingComponent::ssaoDirectLightInfluence, { true, 0.0f, 1.0f })
			.field("enableBloom", &PostProcessingComponent::enableBloom)
			.field("bloomThreshold", &PostProcessingComponent::bloomThreshold, { true, 0.1f, 5.0f })
			.field("bloomIntensity", &PostProcessingComponent::bloomIntensity, { true, 0.0f, 1.0f })
			.registerSelf();

		// 15. TerrainComponent
		ComponentBuilder<TerrainComponent>("TerrainComponent")
			.field("resolution", &TerrainComponent::resolution)
			.field("worldSize", &TerrainComponent::worldSize, { true, 10.0f, 2000.0f })
			.field("maxHeight", &TerrainComponent::maxHeight, { true, 1.0f, 500.0f })
			.field("seed", &TerrainComponent::seed)
			.field("octaves", &TerrainComponent::octaves)
			.field("frequency", &TerrainComponent::frequency, { true, 0.001f, 0.1f })
			.field("persistence", &TerrainComponent::persistence, { true, 0.0f, 1.0f })
			.field("lacunarity", &TerrainComponent::lacunarity, { true, 1.0f, 4.0f })
			.field("ridgeWeight", &TerrainComponent::ridgeWeight, { true, 0.0f, 1.0f })
			.field("isIsland", &TerrainComponent::isIsland)
			.field("islandRadius", &TerrainComponent::islandRadius, { true, 5.0f, 1000.0f })
			.field("coastlineFalloff", &TerrainComponent::coastlineFalloff, { true, 0.5f, 10.0f })
			.field("seaFloorDepth", &TerrainComponent::seaFloorDepth, { true, -100.0f, 0.0f })
			.field("sandHeight", &TerrainComponent::sandHeight, { true, -50.0f, 50.0f })
			.field("cliffSlopeThreshold", &TerrainComponent::cliffSlopeThreshold, { true, 0.0f, 1.0f })
			.field("snowHeight", &TerrainComponent::snowHeight, { true, 0.0f, 500.0f })
			.registerSelf();

		// 16. FoliageComponent
		ComponentBuilder<FoliageComponent>("FoliageComponent")
			.field("bladeCount", &FoliageComponent::bladeCount)
			.field("patchDimensions", &FoliageComponent::patchDimensions)
			.field("bladeHeightMin", &FoliageComponent::bladeHeightMin, { true, 0.05f, 5.0f })
			.field("bladeHeightMax", &FoliageComponent::bladeHeightMax, { true, 0.05f, 5.0f })
			.field("bladeWidth", &FoliageComponent::bladeWidth, { true, 0.01f, 1.0f })
			.field("windSpeed", &FoliageComponent::windSpeed, { true, 0.0f, 10.0f })
			.field("windStrength", &FoliageComponent::windStrength, { true, 0.0f, 2.0f })
			.field("windFrequency", &FoliageComponent::windFrequency, { true, 0.0f, 10.0f })
			.field("maxDrawDistance", &FoliageComponent::maxDrawDistance, { true, 5.0f, 500.0f })
			.field("rootColor", &FoliageComponent::rootColor)
			.field("tipColor", &FoliageComponent::tipColor)
			.field("snapToTerrain", &FoliageComponent::snapToTerrain)
			.field("minAltitude", &FoliageComponent::minAltitude, { true, -50.0f, 100.0f })
			.field("maxAltitude", &FoliageComponent::maxAltitude, { true, -50.0f, 100.0f })
			.field("maxSlope", &FoliageComponent::maxSlope, { true, 0.0f, 1.0f })
			.registerSelf();

		// 17. WaterComponent
		ComponentBuilder<WaterComponent>("WaterComponent")
			.field("waterLevel", &WaterComponent::waterLevel, { true, -100.0f, 100.0f })
			.field("planeSize", &WaterComponent::planeSize, { true, 10.0f, 2000.0f })
			.field("gridResolution", &WaterComponent::gridResolution)
			.field("waveAmplitude", &WaterComponent::waveAmplitude, { true, 0.0f, 5.0f })
			.field("waveFrequency", &WaterComponent::waveFrequency, { true, 0.01f, 2.0f })
			.field("waveSpeed", &WaterComponent::waveSpeed, { true, 0.0f, 10.0f })
			.field("waveSteepness", &WaterComponent::waveSteepness, { true, 0.0f, 1.0f })
			.field("shallowColor", &WaterComponent::shallowColor)
			.field("deepColor", &WaterComponent::deepColor)
			.field("foamColor", &WaterComponent::foamColor)
			.field("depthAbsorption", &WaterComponent::depthAbsorption, { true, 0.001f, 2.0f })
			.field("foamDepthThreshold", &WaterComponent::foamDepthThreshold, { true, 0.01f, 10.0f })
			.field("refractionStrength", &WaterComponent::refractionStrength, { true, 0.0f, 0.2f })
			.field("causticsIntensity", &WaterComponent::causticsIntensity, { true, 0.0f, 2.0f })
			.field("buoyancyDensity", &WaterComponent::buoyancyDensity, { true, 0.1f, 10.0f })
			.field("fluidDrag", &WaterComponent::fluidDrag, { true, 0.0f, 10.0f })
			.field("sssIntensity", &WaterComponent::sssIntensity, { true, 0.0f, 5.0f })
			.field("jacobianThreshold", &WaterComponent::jacobianThreshold, { true, 0.05f, 1.0f })
			.registerSelf();

		// 18. AudioSourceComponent
		ComponentBuilder<AudioSourceComponent>("AudioSourceComponent")
			.field("clipPath", &AudioSourceComponent::clipPath)
			.field("volume", &AudioSourceComponent::volume, { true, 0.0f, 2.0f })
			.field("pitch", &AudioSourceComponent::pitch, { true, 0.1f, 3.0f })
			.field("loop", &AudioSourceComponent::loop)
			.field("spatialize3D", &AudioSourceComponent::spatialize3D)
			.field("minDistance", &AudioSourceComponent::minDistance, { true, 0.1f, 100.0f })
			.field("maxDistance", &AudioSourceComponent::maxDistance, { true, 1.0f, 500.0f })
			.field("playOnStart", &AudioSourceComponent::playOnStart)
			.registerSelf();

		// 19. AudioListenerComponent
		ComponentBuilder<AudioListenerComponent>("AudioListenerComponent")
			.field("active", &AudioListenerComponent::active)
			.field("masterVolume", &AudioListenerComponent::masterVolume, { true, 0.0f, 2.0f })
			.registerSelf();

		// 20. HUDTelemetryComponent
		ComponentBuilder<HUDTelemetryComponent>("HUDTelemetryComponent")
			.field("showOverlay", &HUDTelemetryComponent::showOverlay)
			.field("showHotkeys", &HUDTelemetryComponent::showHotkeys)
			.field("customBannerText", &HUDTelemetryComponent::customBannerText)
			.registerSelf();

		// 21. ReflectionProbeComponent
		ComponentBuilder<ReflectionProbeComponent>("ReflectionProbeComponent")
			.field("boxExtents", &ReflectionProbeComponent::boxExtents)
			.field("intensity", &ReflectionProbeComponent::intensity, { true, 0.0f, 10.0f })
			.field("boxProjection", &ReflectionProbeComponent::boxProjection)
			.field("captureSceneGeometry", &ReflectionProbeComponent::captureSceneGeometry)
			.field("resolution", &ReflectionProbeComponent::resolution)
			.field("bakedCubemapPath", &ReflectionProbeComponent::bakedCubemapPath)
			.registerSelf();

		// 22. DDGIProbeVolumeComponent
		ComponentBuilder<DDGIProbeVolumeComponent>("DDGIProbeVolumeComponent")
			.field("enabled", &DDGIProbeVolumeComponent::enabled)
			.field("origin", &DDGIProbeVolumeComponent::origin)
			.field("probeCounts", &DDGIProbeVolumeComponent::probeCounts)
			.field("probeSpacing", &DDGIProbeVolumeComponent::probeSpacing, { true, 0.5f, 10.0f })
			.field("normalBias", &DDGIProbeVolumeComponent::normalBias, { true, 0.01f, 1.0f })
			.field("hysteresis", &DDGIProbeVolumeComponent::hysteresis, { true, 0.5f, 0.99f })
			.field("intensity", &DDGIProbeVolumeComponent::intensity, { true, 0.0f, 10.0f })
			.field("energyPreservation", &DDGIProbeVolumeComponent::energyPreservation, { true, 0.0f, 1.0f })
			.registerSelf();

		// 22. DecalComponent
		ComponentBuilder<DecalComponent>("DecalComponent")
			.field("albedoTexturePath", &DecalComponent::albedoTexturePath)
			.field("normalTexturePath", &DecalComponent::normalTexturePath)
			.field("color", &DecalComponent::color, { false, 0, 0, "", true })
			.field("normalStrength", &DecalComponent::normalStrength, { true, 0.0f, 2.0f })
			.field("roughness", &DecalComponent::roughness, { true, 0.0f, 1.0f })
			.field("fadeDistance", &DecalComponent::fadeDistance, { true, 0.1f, 500.0f })
			.field("extents", &DecalComponent::extents)
			.registerSelf();

		// 23. SplineComponent
		ComponentBuilder<SplineComponent>("SplineComponent")
			.field("controlPoints", &SplineComponent::controlPoints)
			.field("isClosedLoop", &SplineComponent::isClosedLoop)
			.registerSelf();
	}
}
