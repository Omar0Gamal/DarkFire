#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Entity.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Scene/SceneRendererBridge.h"
#include "Engine/Core/Serialization/SceneSerializer.h"
#include "Engine/RHI/Null/NullRHI.h"
#include "Engine/Renderer/RenderScene.h"
#include "Engine/Renderer/SceneRenderer.h"
#include "Engine/Renderer/ShaderLibrary.h"

#ifdef _MSC_VER
#pragma warning(disable: 5285)
#endif
#include "doctest/doctest.h"

using namespace Core;
using namespace Core::RHI;
using namespace Core::Renderer;

TEST_SUITE("Phase 9: GraphicsTest3D Dual Showcase Benchmark Scenes")
{
	TEST_CASE("Phase 9.1: SponzaAtrium.scene Deserialization & NullRHI Pipeline Render")
	{
		Scene scene;
		SceneSerializer serializer(&scene);
		bool deserialized = serializer.deserializeFromFile("Templates/GraphicsTest3D/Scenes/SponzaAtrium.scene");
		REQUIRE(deserialized);

		// Verify key authored entities
		auto envEntity = scene.findEntityByName("World Environment");
		REQUIRE(envEntity.isValid());
		auto* envComp = envEntity.tryGetComponent<EnvironmentComponent>();
		REQUIRE(envComp != nullptr);
		CHECK(envComp->enableFog == false);
		CHECK(envComp->fogDensity == doctest::Approx(0.0015f));
		CHECK(envComp->skyType == Core::SkyType::SkyboxTexture);
		CHECK(envComp->skyboxTexturePath == "Assets/Textures/05-18_Day_D.hdr");

		auto* ppComp = envEntity.tryGetComponent<PostProcessingComponent>();
		REQUIRE(ppComp != nullptr);
		CHECK(ppComp->enableShadows == true);
		CHECK(ppComp->enableTonemapping == true);
		CHECK(ppComp->enableFXAA == false);
		CHECK(ppComp->enableTAA == true);
		CHECK(ppComp->enableSSAO == true);
		CHECK(ppComp->enableBloom == true);
		CHECK(ppComp->enablePCSS == true);
		CHECK(ppComp->enableContactShadows == true);
		CHECK(ppComp->enableBoxProjectedIBL == true);
		CHECK(ppComp->exposure == doctest::Approx(0.98f));

		auto* hudComp = envEntity.tryGetComponent<HUDTelemetryComponent>();
		REQUIRE(hudComp != nullptr);
		CHECK(hudComp->showOverlay == true);

		auto camEntity = scene.findEntityByName("Main Camera");
		REQUIRE(camEntity.isValid());
		auto* camComp = camEntity.tryGetComponent<CameraComponent>();
		REQUIRE(camComp != nullptr);
		CHECK(camComp->isPrimary == true);
		CHECK(camComp->fov == doctest::Approx(40.0f));

		auto probeEntity = scene.findEntityByName("Sponza Central Reflection Probe");
		if (!probeEntity.isValid()) {
			probeEntity = scene.findEntityByName("Atrium Reflection Probe");
		}
		REQUIRE(probeEntity.isValid());
		auto* probeComp = probeEntity.tryGetComponent<ReflectionProbeComponent>();
		REQUIRE(probeComp != nullptr);
		CHECK(probeComp->boxProjection == true);
		CHECK(probeComp->intensity == doctest::Approx(1.0f));

		auto sunEntity = scene.findEntityByName("Daylight Sun (5500K)");
		REQUIRE(sunEntity.isValid());
		auto* sunLight = sunEntity.tryGetComponent<LightComponent>();
		REQUIRE(sunLight != nullptr);
		CHECK(sunLight->intensity == doctest::Approx(4.2f));

		auto sponzaEntity = scene.findEntityByName("Sponza Atrium Architecture");
		REQUIRE(sponzaEntity.isValid());
		auto* meshComp = sponzaEntity.tryGetComponent<MeshRendererComponent>();
		REQUIRE(meshComp != nullptr);
		CHECK(meshComp->meshPath == "Assets/Models/Sponza/Sponza.fbx");

		auto basinEntity = scene.findEntityByName("Central Reflecting Marble Basin");
		REQUIRE(basinEntity.isValid());
		auto* basinMesh = basinEntity.tryGetComponent<MeshRendererComponent>();
		REQUIRE(basinMesh != nullptr);
		CHECK(basinMesh->materialPath == "Assets/Materials/PolishedMarble.mat");

		auto astrolabeEntity = scene.findEntityByName("Kinetic Astrolabe Outer Ring");
		REQUIRE(astrolabeEntity.isValid());
		auto* scriptsComp = astrolabeEntity.tryGetComponent<ScriptsComponent>();
		REQUIRE(scriptsComp != nullptr);
		CHECK(!scriptsComp->scripts.empty());
		CHECK(scriptsComp->scripts[0].scriptName == "AstrolabeRotator");

		scene.updateTransforms();

		// Extract to RenderScene and verify multi-light & mesh extraction
		RenderScene renderScene;
		ExtractRenderScene(scene, renderScene);

		CHECK(!renderScene.meshes.empty());
		CHECK(renderScene.lights.size() >= 1); // 1 directional sun (fake arcade lights purged to match Unity reference)
		CHECK(renderScene.environment.enableShadows == true);
		CHECK(renderScene.environment.enableTonemapping == true);
		CHECK(renderScene.environment.enableFXAA == false);
		CHECK(renderScene.environment.enableTAA == true);
		CHECK(renderScene.environment.enableFog == false);
		CHECK(renderScene.environment.enableSSAO == true);
		CHECK(renderScene.environment.enableBloom == true);
		CHECK(renderScene.environment.enableBoxProjectedIBL == true);

		// Execute full render pass suite with NullRHI
		NullRHIDevice device;
		SceneRenderer renderer(device);
		RenderView view{};
		view.width = 1600;
		view.height = 900;
		renderer.render(renderScene, view);

		CHECK(renderer.getDrawCallCount() > 0);
	}

	TEST_CASE("Phase 9.2: TropicalIsland.scene Deserialization & NullRHI Pipeline Render")
	{
		Scene scene;
		SceneSerializer serializer(&scene);
		bool deserialized = serializer.deserializeFromFile("Templates/GraphicsTest3D/Scenes/TropicalIsland.scene");
		REQUIRE(deserialized);

		// Verify World Environment
		auto envEntity = scene.findEntityByName("World Environment");
		REQUIRE(envEntity.isValid());
		auto* envComp = envEntity.tryGetComponent<EnvironmentComponent>();
		REQUIRE(envComp != nullptr);
		CHECK(envComp->enableFog == false);
		CHECK(envComp->fogDensity == doctest::Approx(0.0045f));
		CHECK(envComp->fogHeightFalloff == doctest::Approx(0.06f));

		auto* ppComp = envEntity.tryGetComponent<PostProcessingComponent>();
		REQUIRE(ppComp != nullptr);
		CHECK(ppComp->enableShadows == true);
		CHECK(ppComp->enableTonemapping == true);
		CHECK(ppComp->enableFXAA == true);
		CHECK(ppComp->enableSSAO == false);
		CHECK(ppComp->enableBloom == false);

		// Verify Procedural Volcanic Island
		auto terrainEntity = scene.findEntityByName("Procedural Volcanic Island");
		REQUIRE(terrainEntity.isValid());
		auto* tc = terrainEntity.tryGetComponent<TerrainComponent>();
		REQUIRE(tc != nullptr);
		CHECK(tc->resolution == 160);
		CHECK(tc->worldSize == doctest::Approx(220.0f));
		CHECK(tc->maxHeight == doctest::Approx(34.0f));
		CHECK(tc->isIsland == true);
		CHECK(tc->islandRadius == doctest::Approx(90.0f));

		// Verify Foliage Field (45,000 grass blades)
		auto foliageEntity = scene.findEntityByName("Island Meadow Grass Field");
		REQUIRE(foliageEntity.isValid());
		auto* fc = foliageEntity.tryGetComponent<FoliageComponent>();
		REQUIRE(fc != nullptr);
		CHECK(fc->bladeCount == 45000);
		CHECK(fc->snapToTerrain == true);
		CHECK(fc->windSpeed == doctest::Approx(2.4f));

		// Verify Tropical Gerstner Ocean
		auto oceanEntity = scene.findEntityByName("Tropical Gerstner Ocean");
		REQUIRE(oceanEntity.isValid());
		auto* wc = oceanEntity.tryGetComponent<WaterComponent>();
		REQUIRE(wc != nullptr);
		CHECK(wc->gridResolution == 160);
		CHECK(wc->waveAmplitude == doctest::Approx(0.32f));
		CHECK(wc->waveSteepness == doctest::Approx(0.62f));
		CHECK(wc->buoyancyDensity == doctest::Approx(2.6f));

		// Verify Audio Sources
		auto surfAudio = scene.findEntityByName("Ocean Surf Audio");
		REQUIRE(surfAudio.isValid());
		auto* acSurf = surfAudio.tryGetComponent<AudioSourceComponent>();
		REQUIRE(acSurf != nullptr);
		CHECK(acSurf->clipPath == "builtin:ocean_waves");
		CHECK(acSurf->spatialize3D == true);

		auto windAudio = scene.findEntityByName("Island Wind Audio");
		REQUIRE(windAudio.isValid());
		auto* acWind = windAudio.tryGetComponent<AudioSourceComponent>();
		REQUIRE(acWind != nullptr);
		CHECK(acWind->clipPath == "builtin:wind");

		// Verify dynamic buoyant crates & rolling spheres
		auto crateAlpha = scene.findEntityByName("Buoyant Crate Alpha");
		REQUIRE(crateAlpha.isValid());
		CHECK(crateAlpha.tryGetComponent<RigidbodyComponent>() != nullptr);
		CHECK(crateAlpha.tryGetComponent<ColliderComponent>() != nullptr);

		auto sphereWest = scene.findEntityByName("Rolling Chrome Sphere West");
		REQUIRE(sphereWest.isValid());
		CHECK(sphereWest.tryGetComponent<RigidbodyComponent>() != nullptr);

		scene.updateTransforms();

		// Extract to RenderScene
		RenderScene renderScene;
		ExtractRenderScene(scene, renderScene);

		CHECK(renderScene.terrains.size() == 1);
		CHECK(renderScene.foliage.size() == 1);
		CHECK(renderScene.waters.size() == 1);
		CHECK(!renderScene.meshes.empty());
		CHECK(renderScene.environment.enableShadows == true);
		CHECK(renderScene.environment.enableTonemapping == true);
		CHECK(renderScene.environment.enableFXAA == true);

		// Execute full render pass suite with NullRHI
		NullRHIDevice device;
		SceneRenderer renderer(device);
		RenderView view{};
		view.width = 1600;
		view.height = 900;
		renderer.render(renderScene, view);

		CHECK(renderer.getDrawCallCount() > 0);
	}

	TEST_CASE("Phase 9.3: Pipeline Shaders Verification (GBufferPS, DeferredLightingPS, StandardHS, StandardDS)")
	{
		NullRHIDevice device;
		ShaderLibrary shaderLib(device);

		auto gbufferPS = shaderLib.getOrCreatePixelShader("GBufferPS.hlsl");
		CHECK(gbufferPS.isOk());
		CHECK(*gbufferPS != nullptr);

		auto defLightingPS = shaderLib.getOrCreatePixelShader("DeferredLightingPS.hlsl");
		CHECK(defLightingPS.isOk());
		CHECK(*defLightingPS != nullptr);

		auto standardHS = shaderLib.getOrCreateHullShader("StandardHS.hlsl");
		CHECK(standardHS.isOk());
		CHECK(*standardHS != nullptr);

		auto standardDS = shaderLib.getOrCreateDomainShader("StandardDS.hlsl");
		CHECK(standardDS.isOk());
		CHECK(*standardDS != nullptr);
	}
}
