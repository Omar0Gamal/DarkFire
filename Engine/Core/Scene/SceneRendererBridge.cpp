#include "SceneRendererBridge.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Math/Math.h"

namespace Core
{
	void ExtractRenderScene(const Scene& scene, Renderer::RenderScene& outRenderScene, float targetAspect, float alpha)
	{
		outRenderScene.clear();

		const auto& registry = scene.getRegistry();

		// 1. Extract Primary Camera
		bool cameraFound = false;
		auto cameraView = registry.view<CameraComponent, WorldTransform>();
		for (auto entity : cameraView)
		{
			const auto& cam = cameraView.get<CameraComponent>(entity);
			const auto& worldTransform = cameraView.get<WorldTransform>(entity);

			if (cam.isPrimary || !cameraFound)
			{
				Matrix camWorld = worldTransform.worldMatrix;
				if (alpha < 0.999f)
				{
					if (const auto* tc = registry.try_get<TransformComponent>(entity))
					{
						Vector3 p = Vector3::Lerp(tc->prevPosition, tc->position, alpha);
						Quaternion q = Quaternion::Slerp(tc->prevRotation, tc->rotation, alpha);
						camWorld = Math::createTransformTRS(p, q, tc->scale);
					}
				}
				outRenderScene.camera.position = Math::getTranslation(camWorld);
				outRenderScene.camera.forward = Math::getForward(camWorld);
				outRenderScene.camera.nearZ = cam.nearZ;
				outRenderScene.camera.farZ = cam.farZ;
				outRenderScene.camera.fov = Math::toRadians(cam.fov);
				outRenderScene.camera.aspect = targetAspect > 0.001f ? targetAspect : (16.0f / 9.0f);
				outRenderScene.camera.isOrthographic = cam.isOrthographic;

				// View Matrix is inverse of camera world transform
				outRenderScene.camera.viewMatrix = Math::affineInverse(camWorld);
				outRenderScene.camera.projMatrix = cam.getProjection(outRenderScene.camera.aspect);
				outRenderScene.camera.viewProjMatrix = outRenderScene.camera.viewMatrix * outRenderScene.camera.projMatrix;
				outRenderScene.camera.invViewMatrix = outRenderScene.camera.viewMatrix.Invert();
				outRenderScene.camera.invProjMatrix = outRenderScene.camera.projMatrix.Invert();
				outRenderScene.camera.invViewProjMatrix = outRenderScene.camera.viewProjMatrix.Invert();

				cameraFound = true;
				if (cam.isPrimary)
				{
					break;
				}
			}
		}

		// 2. Extract Mesh Renderers
		auto meshView = registry.view<MeshRendererComponent, WorldTransform>();
		for (auto entity : meshView)
		{
			if (const auto* idComp = registry.try_get<IdentityComponent>(entity))
			{
				if (!idComp->isActive) continue;
			}

			const auto& meshComp = meshView.get<MeshRendererComponent>(entity);
			const auto& worldTransform = meshView.get<WorldTransform>(entity);

			Matrix meshWorld = worldTransform.worldMatrix;
			if (alpha < 0.999f)
			{
				if (const auto* tc = registry.try_get<TransformComponent>(entity))
				{
					meshWorld = tc->getInterpolatedMatrix(alpha);
				}
			}

			Renderer::RenderMeshInstance meshInst{};
			meshInst.world = meshWorld;
			meshInst.entityId = static_cast<uint32_t>(entity);
			meshInst.meshPath = meshComp.meshPath;
			meshInst.materialPath = meshComp.materialPath;
			meshInst.layer = Renderer::RenderLayer::Opaque;
			meshInst.indexCount = 36; // Default unit primitive count if unassigned
			meshInst.startIndex = 0;
			meshInst.baseVertex = 0;

			const Vector3 pos = Math::getTranslation(worldTransform.worldMatrix);
			meshInst.distanceToCamera = Vector3::Distance(pos, outRenderScene.camera.position);
			meshInst.castShadows = meshComp.castShadows;
			meshInst.receiveShadows = meshComp.receiveShadows;

			outRenderScene.meshes.push_back(meshInst);
		}

		// 3. Extract Lights
		auto lightView = registry.view<LightComponent, WorldTransform>();
		for (auto entity : lightView)
		{
			if (const auto* idComp = registry.try_get<IdentityComponent>(entity))
			{
				if (!idComp->isActive) continue;
			}

			const auto& lightComp = lightView.get<LightComponent>(entity);
			const auto& worldTransform = lightView.get<WorldTransform>(entity);

			Renderer::RenderLight light{};
			light.position = Math::getTranslation(worldTransform.worldMatrix);
			light.direction = Math::getForward(worldTransform.worldMatrix);
			light.color = Vector3(lightComp.color.x, lightComp.color.y, lightComp.color.z);
			light.intensity = lightComp.intensity;
			light.range = lightComp.range;
			light.innerConeAngle = Math::toRadians(lightComp.spotInnerAngle);
			light.outerConeAngle = Math::toRadians(lightComp.spotOuterAngle);
			light.type = static_cast<uint32_t>(lightComp.type);
			light.castShadows = lightComp.castShadows;
			light.shadowBias = lightComp.shadowBias;
			light.shadowNormalBias = lightComp.shadowNormalBias;
			light.shadowSoftness = lightComp.shadowSoftness;
			light.shadowDistance = lightComp.shadowDistance;

			outRenderScene.lights.push_back(light);
		}

		// Editor fallback light if scene has no light sources
		if (outRenderScene.lights.empty())
		{
			Renderer::RenderLight defSun{};
			defSun.position = Vector3(0.0f, 20.0f, 0.0f);
			defSun.direction = Vector3(0.577f, -0.577f, 0.577f);
			defSun.color = Vector3(1.0f, 0.98f, 0.92f);
			defSun.intensity = 1.0f;
			defSun.range = 1000.0f;
			defSun.type = 0; // Directional
			outRenderScene.lights.push_back(defSun);
		}

		// 4. Extract Environment
		auto envView = registry.view<EnvironmentComponent>();
		if (auto it = envView.begin(); it != envView.end())
		{
			const auto& env = envView.get<EnvironmentComponent>(*it);
			outRenderScene.environment.skyType = static_cast<Renderer::SkyType>(env.skyType);
			outRenderScene.environment.skyColor = Math::Vector4(env.skyColor.R(), env.skyColor.G(), env.skyColor.B(), env.skyColor.A());
			outRenderScene.environment.horizonColor = Math::Vector4(env.horizonColor.R(), env.horizonColor.G(), env.horizonColor.B(), env.horizonColor.A());
			outRenderScene.environment.groundColor = Math::Vector4(env.groundColor.R(), env.groundColor.G(), env.groundColor.B(), env.groundColor.A());
			outRenderScene.environment.ambientColor = Math::Vector4(env.ambientColor.R(), env.ambientColor.G(), env.ambientColor.B(), env.ambientColor.A());
			outRenderScene.environment.ambientIntensity = env.ambientEnergy;
			outRenderScene.environment.skyboxTexturePath = env.skyboxTexturePath;
			outRenderScene.environment.enableFog = env.enableFog;
			outRenderScene.environment.fogColor = Math::Vector4(env.fogColor.R(), env.fogColor.G(), env.fogColor.B(), env.fogColor.A());
			outRenderScene.environment.fogDensity = env.fogDensity;
			outRenderScene.environment.fogHeightFalloff = env.fogHeightFalloff;
			outRenderScene.environment.fogSunScattering = env.fogSunScattering;
		}

		// 5. Extract Post-Processing
		auto ppView = registry.view<PostProcessingComponent>();
		if (auto it = ppView.begin(); it != ppView.end())
		{
			const auto& pp = ppView.get<PostProcessingComponent>(*it);
			outRenderScene.environment.renderPipeline = pp.renderPipeline;
			outRenderScene.environment.enableShadows = pp.enableShadows;
			outRenderScene.environment.enablePCSS = pp.enablePCSS;
			outRenderScene.environment.enableContactShadows = pp.enableContactShadows;
			outRenderScene.environment.enableTessellation = pp.enableTessellation;
			outRenderScene.environment.tessellationFactor = pp.tessellationFactor;
			outRenderScene.environment.meshLodBias = pp.meshLodBias;
			outRenderScene.environment.enableTonemapping = pp.enableTonemapping;
			outRenderScene.environment.tonemapper = static_cast<Renderer::TonemapperType>(pp.tonemapper);
			outRenderScene.environment.exposure = pp.exposure;
			outRenderScene.environment.gamma = pp.gamma;
			outRenderScene.environment.enableAutoExposure = pp.enableAutoExposure;
			outRenderScene.environment.autoExposureMinEV = pp.autoExposureMinEV;
			outRenderScene.environment.autoExposureMaxEV = pp.autoExposureMaxEV;
			outRenderScene.environment.autoExposureSpeed = pp.autoExposureSpeed;
			outRenderScene.environment.autoExposureSpeedUp = pp.autoExposureSpeedUp;
			outRenderScene.environment.autoExposureSpeedDown = pp.autoExposureSpeedDown;
			outRenderScene.environment.enableMotionBlur = pp.enableMotionBlur;
			outRenderScene.environment.motionBlurShutterAngle = pp.motionBlurShutterAngle;
			outRenderScene.environment.motionBlurMaxRadius = pp.motionBlurMaxRadius;
			outRenderScene.environment.contrast = pp.contrast;
			outRenderScene.environment.saturation = pp.saturation;
			outRenderScene.environment.vignetteIntensity = pp.vignetteIntensity;
			outRenderScene.environment.colorTemperature = pp.colorTemperature;
			outRenderScene.environment.enableBoxProjectedIBL = pp.enableBoxProjectedIBL;
			outRenderScene.environment.iblBoxCenter = pp.iblBoxCenter;
			outRenderScene.environment.iblBoxExtents = pp.iblBoxExtents;
			outRenderScene.environment.enableTAA = pp.enableTAA;
			outRenderScene.environment.enableClusteredLighting = pp.enableClusteredLighting;
			outRenderScene.environment.enableGpuCulling = pp.enableGpuCulling;
			outRenderScene.environment.enableFXAA = pp.enableFXAA;
			outRenderScene.environment.fxaaSubpix = pp.fxaaSubpix;
			outRenderScene.environment.fxaaEdgeThreshold = pp.fxaaEdgeThreshold;
			outRenderScene.environment.fxaaEdgeThresholdMin = pp.fxaaEdgeThresholdMin;
			outRenderScene.environment.casSharpening = pp.casSharpening;
			outRenderScene.environment.enableSSAO = pp.enableSSAO;
			outRenderScene.environment.ssaoRadius = pp.ssaoRadius;
			outRenderScene.environment.ssaoBias = pp.ssaoBias;
			outRenderScene.environment.ssaoIntensity = pp.ssaoIntensity;
			outRenderScene.environment.ssaoDirectLightInfluence = pp.ssaoDirectLightInfluence;
			outRenderScene.environment.enableBloom = pp.enableBloom;
			outRenderScene.environment.bloomThreshold = pp.bloomThreshold;
			outRenderScene.environment.bloomIntensity = pp.bloomIntensity;
			outRenderScene.environment.enableSSR = pp.enableSSR;
			outRenderScene.environment.ssrMaxDistance = pp.ssrMaxDistance;
			outRenderScene.environment.ssrThickness = pp.ssrThickness;
			outRenderScene.environment.ssrRoughnessCutoff = pp.ssrRoughnessCutoff;
			outRenderScene.environment.ssrIntensity = pp.ssrIntensity;
			outRenderScene.environment.ssrStepCount = pp.ssrStepCount;
			outRenderScene.environment.enableSSS = pp.enableSSS;
			outRenderScene.environment.sssRadius = pp.sssRadius;
			outRenderScene.environment.sssMaxDepthDiff = pp.sssMaxDepthDiff;
			outRenderScene.environment.debugView = pp.debugView;
		}

		// Check for DDGIProbeVolumeComponent
		auto ddgiView = registry.view<DDGIProbeVolumeComponent>();
		for (auto entity : ddgiView)
		{
			if (const auto* idComp = registry.try_get<IdentityComponent>(entity))
			{
				if (!idComp->isActive) continue;
			}

			const auto& ddgi = ddgiView.get<DDGIProbeVolumeComponent>(entity);
			outRenderScene.environment.enableDDGI = ddgi.enabled;
			if (const auto* wt = registry.try_get<WorldTransform>(entity))
			{
				outRenderScene.environment.ddgiOrigin = Math::getTranslation(wt->worldMatrix);
			}
			else
			{
				outRenderScene.environment.ddgiOrigin = ddgi.origin;
			}
			outRenderScene.environment.ddgiProbeCounts = ddgi.probeCounts;
			outRenderScene.environment.ddgiProbeSpacing = ddgi.probeSpacing;
			outRenderScene.environment.ddgiNormalBias = ddgi.normalBias;
			outRenderScene.environment.ddgiHysteresis = ddgi.hysteresis;
			outRenderScene.environment.ddgiIntensity = ddgi.intensity;
			outRenderScene.environment.ddgiEnergyPreservation = ddgi.energyPreservation;
			break;
		}

		// Check for ReflectionProbeComponent override for Box-Projected IBL
		auto probeView = registry.view<ReflectionProbeComponent>();
		for (auto entity : probeView)
		{
			if (const auto* idComp = registry.try_get<IdentityComponent>(entity))
			{
				if (!idComp->isActive) continue;
			}

			const auto& probe = probeView.get<ReflectionProbeComponent>(entity);
			outRenderScene.environment.enableBoxProjectedIBL = probe.boxProjection;
			if (const auto* wt = registry.try_get<WorldTransform>(entity))
			{
				outRenderScene.environment.iblBoxCenter = Math::getTranslation(wt->worldMatrix);
			}
			else if (const auto* tc = registry.try_get<TransformComponent>(entity))
			{
				outRenderScene.environment.iblBoxCenter = Math::getTranslation(tc->getLocalMatrix());
			}
			outRenderScene.environment.iblBoxExtents = probe.boxExtents;
			outRenderScene.environment.iblBoxIntensity = probe.intensity;
			outRenderScene.environment.captureProbeSceneGeometry = probe.captureSceneGeometry;
			outRenderScene.environment.bakedProbeCubemapPath = probe.bakedCubemapPath;
			break;
		}

		// 6. Extract Particles
		auto particleView = registry.view<ParticleEmitterComponent, ParticleRuntimeState>();
		for (auto entity : particleView)
		{
			if (const auto* idComp = registry.try_get<IdentityComponent>(entity))
			{
				if (!idComp->isActive) continue;
			}

			const auto& emitter = particleView.get<ParticleEmitterComponent>(entity);
			const auto& state = particleView.get<ParticleRuntimeState>(entity);

			for (int i = 0; i < state.activeCount; ++i)
			{
				const auto& p = state.particles[i];
				Renderer::RenderParticleInstance pi{};
				pi.position = p.position;
				pi.size = p.size;
				pi.color = Math::Vector4(p.color.R(), p.color.G(), p.color.B(), p.color.A());
				pi.distanceToCamera = Vector3::Distance(p.position, outRenderScene.camera.position);
				pi.texturePath = emitter.texturePath;
				pi.blendMode = (emitter.blendMode == ParticleBlendMode::Additive) ? Renderer::BlendMode::Additive : Renderer::BlendMode::Translucent;
				outRenderScene.particles.push_back(pi);
			}
		}

		// 7. Extract Terrain
		auto terrainView = registry.view<TerrainComponent, WorldTransform>();
		for (auto entity : terrainView)
		{
			if (const auto* idComp = registry.try_get<IdentityComponent>(entity))
			{
				if (!idComp->isActive) continue;
			}

			const auto& comp = terrainView.get<TerrainComponent>(entity);
			const auto& wt = terrainView.get<WorldTransform>(entity);
			Renderer::RenderTerrainInstance ti{};
			ti.world = wt.worldMatrix;
			ti.entityId = static_cast<uint32_t>(entity);
			ti.component = comp;
			outRenderScene.terrains.push_back(ti);
		}

		// 8. Extract Foliage
		auto foliageView = registry.view<FoliageComponent, WorldTransform>();
		for (auto entity : foliageView)
		{
			if (const auto* idComp = registry.try_get<IdentityComponent>(entity))
			{
				if (!idComp->isActive) continue;
			}

			const auto& comp = foliageView.get<FoliageComponent>(entity);
			const auto& wt = foliageView.get<WorldTransform>(entity);
			Renderer::RenderFoliageInstance fi{};
			fi.world = wt.worldMatrix;
			fi.entityId = static_cast<uint32_t>(entity);
			fi.component = comp;
			outRenderScene.foliage.push_back(fi);
		}

		// 9. Extract Water
		auto waterView = registry.view<WaterComponent, WorldTransform>();
		for (auto entity : waterView)
		{
			if (const auto* idComp = registry.try_get<IdentityComponent>(entity))
			{
				if (!idComp->isActive) continue;
			}

			const auto& comp = waterView.get<WaterComponent>(entity);
			const auto& wt = waterView.get<WorldTransform>(entity);
			Renderer::RenderWaterInstance wi{};
			wi.world = wt.worldMatrix;
			wi.entityId = static_cast<uint32_t>(entity);
			wi.component = comp;
			outRenderScene.waters.push_back(wi);
		}

		// 10. Extract Decals
		auto decalView = registry.view<DecalComponent>();
		for (auto entity : decalView)
		{
			if (const auto* idComp = registry.try_get<IdentityComponent>(entity))
			{
				if (!idComp->isActive) continue;
			}

			const auto& decal = decalView.get<DecalComponent>(entity);

			Matrix baseWorld = Matrix::Identity;
			if (const auto* wt = registry.try_get<WorldTransform>(entity))
			{
				baseWorld = wt->worldMatrix;
			}
			else if (const auto* tc = registry.try_get<TransformComponent>(entity))
			{
				baseWorld = tc->getLocalMatrix();
			}

			Matrix world = Matrix::CreateScale(decal.extents) * baseWorld;

			Renderer::RenderDecalInstance di{};
			di.world = world;
			di.invWorld = world.Invert();
			di.entityId = static_cast<uint32_t>(entity);
			di.color = Math::Vector4(decal.color.R(), decal.color.G(), decal.color.B(), decal.color.A());
			di.albedoTexturePath = decal.albedoTexturePath;
			di.normalTexturePath = decal.normalTexturePath;
			di.normalStrength = decal.normalStrength;
			di.roughness = decal.roughness;
			di.fadeDistance = decal.fadeDistance;

			outRenderScene.decals.push_back(di);
		}
	}
}
