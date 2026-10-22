#include "SceneRendererBridge.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Math/Math.h"

namespace Core
{
	void ExtractRenderScene(const Scene& scene, Renderer::RenderScene& outRenderScene, float targetAspect)
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
				outRenderScene.camera.position = Math::getTranslation(worldTransform.worldMatrix);
				outRenderScene.camera.forward = Math::getForward(worldTransform.worldMatrix);
				outRenderScene.camera.nearZ = cam.nearZ;
				outRenderScene.camera.farZ = cam.farZ;
				outRenderScene.camera.fov = Math::toRadians(cam.fov);
				outRenderScene.camera.aspect = targetAspect > 0.001f ? targetAspect : (16.0f / 9.0f);
				outRenderScene.camera.isOrthographic = cam.isOrthographic;

				// View Matrix is inverse of camera world transform
				outRenderScene.camera.viewMatrix = Math::affineInverse(worldTransform.worldMatrix);
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
			const auto& meshComp = meshView.get<MeshRendererComponent>(entity);
			const auto& worldTransform = meshView.get<WorldTransform>(entity);

			Renderer::RenderMeshInstance meshInst{};
			meshInst.world = worldTransform.worldMatrix;
			meshInst.entityId = static_cast<uint32_t>(entity);
			meshInst.meshPath = meshComp.meshPath;
			meshInst.materialPath = meshComp.materialPath;
			meshInst.layer = Renderer::RenderLayer::Opaque;
			meshInst.indexCount = 36; // Default unit primitive count if unassigned
			meshInst.startIndex = 0;
			meshInst.baseVertex = 0;

			const Vector3 pos = Math::getTranslation(worldTransform.worldMatrix);
			meshInst.distanceToCamera = Vector3::Distance(pos, outRenderScene.camera.position);

			outRenderScene.meshes.push_back(meshInst);
		}

		// 3. Extract Lights
		auto lightView = registry.view<LightComponent, WorldTransform>();
		for (auto entity : lightView)
		{
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
		for (auto entity : envView)
		{
			const auto& env = envView.get<EnvironmentComponent>(entity);
			outRenderScene.environment.skyType = static_cast<Renderer::SkyType>(env.skyType);
			outRenderScene.environment.skyColor = Math::Vector4(env.skyColor.R(), env.skyColor.G(), env.skyColor.B(), env.skyColor.A());
			outRenderScene.environment.horizonColor = Math::Vector4(env.horizonColor.R(), env.horizonColor.G(), env.horizonColor.B(), env.horizonColor.A());
			outRenderScene.environment.groundColor = Math::Vector4(env.groundColor.R(), env.groundColor.G(), env.groundColor.B(), env.groundColor.A());
			outRenderScene.environment.ambientColor = Math::Vector4(env.ambientColor.R(), env.ambientColor.G(), env.ambientColor.B(), env.ambientColor.A());
			outRenderScene.environment.ambientIntensity = env.ambientEnergy;
			outRenderScene.environment.skyboxTexturePath = env.skyboxTexturePath;
			break;
		}

		// 5. Extract Post-Processing
		auto ppView = registry.view<PostProcessingComponent>();
		for (auto entity : ppView)
		{
			const auto& pp = ppView.get<PostProcessingComponent>(entity);
			outRenderScene.environment.enableTonemapping = pp.enableTonemapping;
			outRenderScene.environment.tonemapper = static_cast<Renderer::TonemapperType>(pp.tonemapper);
			outRenderScene.environment.exposure = pp.exposure;
			outRenderScene.environment.gamma = pp.gamma;
			outRenderScene.environment.enableFXAA = pp.enableFXAA;
			outRenderScene.environment.fxaaSubpix = pp.fxaaSubpix;
			outRenderScene.environment.fxaaEdgeThreshold = pp.fxaaEdgeThreshold;
			outRenderScene.environment.fxaaEdgeThresholdMin = pp.fxaaEdgeThresholdMin;
			outRenderScene.environment.enableSSAO = pp.enableSSAO;
			outRenderScene.environment.ssaoRadius = pp.ssaoRadius;
			outRenderScene.environment.ssaoBias = pp.ssaoBias;
			outRenderScene.environment.ssaoIntensity = pp.ssaoIntensity;
			outRenderScene.environment.enableBloom = pp.enableBloom;
			outRenderScene.environment.bloomThreshold = pp.bloomThreshold;
			outRenderScene.environment.bloomIntensity = pp.bloomIntensity;
			break;
		}

		// 6. Extract Particles
		auto particleView = registry.view<ParticleEmitterComponent, ParticleRuntimeState>();
		for (auto entity : particleView)
		{
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
	}
}
