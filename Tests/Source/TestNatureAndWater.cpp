#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Entity.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Scene/SceneRendererBridge.h"
#include "Engine/Core/Serialization/SceneSerializer.h"
#include "Engine/RHI/Null/NullRHI.h"
#include "Engine/Renderer/RenderScene.h"
#include "Engine/Renderer/SceneRenderer.h"
#include "Engine/Renderer/TerrainGenerator.h"
#include "Engine/Renderer/MeshPrimitives.h"
#include "Engine/Physics/PhysicsWorld.h"
#include "Engine/Shaders/Shared/Bindings.h"

#ifdef _MSC_VER
#pragma warning(disable: 5285)
#endif
#include "doctest/doctest.h"

using namespace Core;
using namespace Core::RHI;
using namespace Core::Renderer;
using namespace Physics;

TEST_SUITE("Phase 5, Phase 6 & Phase 7: Nature & Water System")
{
	TEST_CASE("Phase 5.1: TerrainGenerator Island Height & Normal Sampling")
	{
		TerrainComponent tc;
		tc.resolution = 64;
		tc.worldSize = 200.0f;
		tc.maxHeight = 30.0f;
		tc.seed = 1337;
		tc.isIsland = true;
		tc.islandRadius = 80.0f;
		tc.coastlineFalloff = 2.0f;
		tc.seaFloorDepth = -10.0f;

		// 1. Center of island (0, 0) must be elevated well above sea level (0.0)
		float centerHeight = TerrainGenerator::sampleHeight(tc, 0.0f, 0.0f);
		CHECK(centerHeight > 5.0f);

		// 2. Far perimeter beyond islandRadius (e.g. at radius 95.0f) must slope below 0 to seaFloorDepth (-10.0f)
		float perimeterHeight = TerrainGenerator::sampleHeight(tc, 95.0f, 0.0f);
		CHECK(perimeterHeight == doctest::Approx(-10.0f));

		float cornerHeight = TerrainGenerator::sampleHeight(tc, 85.0f, 85.0f);
		CHECK(cornerHeight == doctest::Approx(-10.0f));

		// 3. Normal at center should be primarily pointing Up (+Y)
		Vector3 centerNorm = TerrainGenerator::sampleNormal(tc, 0.0f, 0.0f);
		CHECK(centerNorm.y > 0.5f);
		CHECK(std::abs(centerNorm.Length() - 1.0f) < 0.01f);

		// 4. Generate mesh
		MeshData terrainMesh = TerrainGenerator::generateTerrainMesh(tc);
		CHECK(terrainMesh.vertices.size() == (tc.resolution + 1) * (tc.resolution + 1));
		CHECK(terrainMesh.indices.size() == tc.resolution * tc.resolution * 6);
		CHECK(!terrainMesh.submeshes.empty());
	}

	TEST_CASE("Phase 5.2: PhysicsWorld Heightfield Terrain Collision & Raycast")
	{
		PhysicsWorld physics;
		physics.init(Vector3(0.0f, -9.81f, 0.0f), 1.0f / 60.0f);

		Scene scene;
		// 1. Create Terrain Entity
		auto terrainEntity = scene.createEntity("IslandTerrain");
		auto& tc = terrainEntity.addComponent<TerrainComponent>();
		tc.resolution = 64;
		tc.worldSize = 200.0f;
		tc.maxHeight = 30.0f;
		tc.seed = 42;
		tc.isIsland = true;
		tc.islandRadius = 80.0f;
		tc.seaFloorDepth = -10.0f;
		terrainEntity.getComponent<TransformComponent>().position = Vector3(0.0f, 0.0f, 0.0f);

		float groundY = TerrainGenerator::sampleHeight(tc, 0.0f, 0.0f);

		// 2. Create Dynamic Dropped Ball
		auto sphereEntity = scene.createEntity("DroppedBall");
		auto& sphereTrans = sphereEntity.getComponent<TransformComponent>();
		sphereTrans.position = Vector3(0.0f, groundY + 5.0f, 0.0f);

		auto& rb = sphereEntity.addComponent<RigidbodyComponent>();
		rb.mass = 1.0f;
		rb.useGravity = true;
		rb.isKinematic = false;
		rb.restitution = 0.0f;
		rb.friction = 5.0f;
		rb.drag = 2.0f;

		auto& col = sphereEntity.addComponent<ColliderComponent>();
		col.shape = ColliderShape::Sphere;
		col.radius = 0.5f;

		scene.updateTransforms();

		// Step physics for 180 ticks (~3.0 seconds) so it fully lands and comes to rest
		for (int i = 0; i < 180; ++i)
		{
			physics.step(scene, 1.0f / 60.0f);
		}

		// Ball should have landed on the terrain surface (bottom point at groundY -> center at groundY + radius)
		CHECK(sphereTrans.position.y >= groundY);
		CHECK(sphereTrans.position.y == doctest::Approx(groundY + col.radius).epsilon(0.15f));

		// Velocity should be near zero (stopped falling, unlike free-fall at -9.8m/s)
		const auto& state = scene.getRegistry().get<RigidbodyRuntimeState>(sphereEntity.getHandle());
		CHECK(std::abs(state.linearVelocity.y) < 0.2f);

		// 3. Test Raycast against terrain (offset from dropped ball)
		float targetY = TerrainGenerator::sampleHeight(tc, 10.0f, 10.0f);
		RaycastHit hit{};
		Vector3 rayOrigin(10.0f, targetY + 20.0f, 10.0f);
		Vector3 rayDir(0.0f, -1.0f, 0.0f);
		bool didHit = physics.raycast(scene, rayOrigin, rayDir, 50.0f, hit);
		CHECK(didHit);
		CHECK(hit.hasHit);
		CHECK(hit.point.y == doctest::Approx(targetY).epsilon(0.1f));
		CHECK(hit.entity == terrainEntity);
	}

	TEST_CASE("Phase 7.1: PhysicsWorld Archimedes Water Buoyancy")
	{
		PhysicsWorld physics;
		physics.init(Vector3(0.0f, -9.81f, 0.0f), 1.0f / 60.0f);

		Scene scene;
		// 1. Create Water Entity at waterLevel = 0.0
		auto waterEntity = scene.createEntity("OceanWater");
		auto& water = waterEntity.addComponent<WaterComponent>();
		water.waterLevel = 0.0f;
		water.buoyancyDensity = 3.0f;
		water.fluidDrag = 2.0f;

		// 2. Create Submerged Dynamic Rigidbody
		auto subEntity = scene.createEntity("SubmergedBox");
		auto& subTrans = subEntity.getComponent<TransformComponent>();
		subTrans.position = Vector3(0.0f, -5.0f, 0.0f);

		auto& rb = subEntity.addComponent<RigidbodyComponent>();
		rb.mass = 1.0f;
		rb.useGravity = true;
		rb.isKinematic = false;

		auto& col = subEntity.addComponent<ColliderComponent>();
		col.shape = ColliderShape::Sphere;
		col.radius = 0.5f;

		scene.updateTransforms();

		// Step physics for 100 ticks
		for (int i = 0; i < 100; ++i)
		{
			physics.step(scene, 1.0f / 60.0f);
		}

		// Buoyancy must push the object upward toward the water surface
		CHECK(subTrans.position.y > -5.0f);
		const auto& state = scene.getRegistry().get<RigidbodyRuntimeState>(subEntity.getHandle());
		CHECK((subTrans.position.y > -2.0f || state.linearVelocity.y > 0.0f));
	}

	TEST_CASE("Phase 6.1: MeshPrimitives GrassBlade & SubdividedPlane")
	{
		MeshData blade = CreateGrassBladeMesh(5);
		CHECK(blade.vertices.size() == 12);
		CHECK(blade.indices.size() == 60);
		CHECK(!blade.submeshes.empty());

		CHECK(blade.vertices[0].position.y == doctest::Approx(0.0f));
		CHECK(blade.vertices.back().position.y == doctest::Approx(1.0f));

		MeshData plane = CreateSubdividedPlaneMesh(100.0f, 16);
		CHECK(plane.vertices.size() == 17 * 17);
		CHECK(plane.indices.size() == 16 * 16 * 6);
		CHECK(!plane.submeshes.empty());
	}

	TEST_CASE("Phase 5-7: SceneRenderer Full Frame with Terrain, Foliage & Water")
	{
		NullRHIDevice device;
		SceneRenderer renderer(device);

		Scene scene;
		// 1. Camera
		auto camEntity = scene.createEntity("MainCamera");
		auto& cam = camEntity.addComponent<CameraComponent>();
		cam.isPrimary = true;
		camEntity.getComponent<TransformComponent>().position = Vector3(0.0f, 15.0f, -30.0f);

		// 2. Directional Sun Light
		auto sunEntity = scene.createEntity("Sun");
		auto& sun = sunEntity.addComponent<LightComponent>();
		sun.type = LightType::Directional;
		sun.castShadows = true;
		sunEntity.getComponent<TransformComponent>().position = Vector3(10.0f, 50.0f, -10.0f);

		// 3. Terrain Component
		auto terrainEntity = scene.createEntity("Terrain");
		auto& tc = terrainEntity.addComponent<TerrainComponent>();
		tc.resolution = 32;
		tc.worldSize = 100.0f;
		tc.maxHeight = 20.0f;
		tc.isIsland = true;

		// 4. Foliage Component (Hardware-Instanced Grass)
		auto foliageEntity = scene.createEntity("GrassPatch");
		auto& fc = foliageEntity.addComponent<FoliageComponent>();
		fc.bladeCount = 256;
		fc.snapToTerrain = true;

		// 5. Water Component (Gerstner Ocean)
		auto waterEntity = scene.createEntity("Ocean");
		auto& wc = waterEntity.addComponent<WaterComponent>();
		wc.waterLevel = 0.0f;
		wc.planeSize = 150.0f;
		wc.gridResolution = 32;

		scene.updateTransforms();

		// Extract to RenderScene
		RenderScene renderScene;
		ExtractRenderScene(scene, renderScene);

		CHECK(renderScene.terrains.size() == 1);
		CHECK(renderScene.foliage.size() == 1);
		CHECK(renderScene.waters.size() == 1);

		// Render frame
		RenderView view{};
		view.width = 1280;
		view.height = 720;
		renderer.render(renderScene, view);

		CHECK(renderer.getDrawCallCount() > 0);
	}

	TEST_CASE("Phase 5-7: Component Serialization & Reflection")
	{
		Scene scene;
		auto e = scene.createEntity("NatureEntity");

		auto& tc = e.addComponent<TerrainComponent>();
		tc.resolution = 64;
		tc.worldSize = 150.0f;
		tc.maxHeight = 45.0f;
		tc.seed = 9999;
		tc.sandHeight = 3.5f;

		auto& fc = e.addComponent<FoliageComponent>();
		fc.bladeCount = 1024;
		fc.bladeWidth = 0.08f;
		fc.windStrength = 0.5f;

		auto& wc = e.addComponent<WaterComponent>();
		wc.waterLevel = 1.5f;
		wc.waveAmplitude = 0.6f;
		wc.buoyancyDensity = 3.2f;

		SceneSerializer serializer(&scene);
		std::string jsonStr = serializer.serializeToString();

		Scene loadedScene;
		SceneSerializer deserializer(&loadedScene);
		bool ok = deserializer.deserializeFromString(jsonStr);
		REQUIRE(ok);

		auto loadedEntity = loadedScene.findEntityByName("NatureEntity");
		REQUIRE(loadedEntity.isValid());

		auto* loadedTc = loadedEntity.tryGetComponent<TerrainComponent>();
		REQUIRE(loadedTc != nullptr);
		CHECK(loadedTc->resolution == 64);
		CHECK(loadedTc->worldSize == doctest::Approx(150.0f));
		CHECK(loadedTc->maxHeight == doctest::Approx(45.0f));
		CHECK(loadedTc->seed == 9999);
		CHECK(loadedTc->sandHeight == doctest::Approx(3.5f));

		auto* loadedFc = loadedEntity.tryGetComponent<FoliageComponent>();
		REQUIRE(loadedFc != nullptr);
		CHECK(loadedFc->bladeCount == 1024);
		CHECK(loadedFc->bladeWidth == doctest::Approx(0.08f));
		CHECK(loadedFc->windStrength == doctest::Approx(0.5f));

		auto* loadedWc = loadedEntity.tryGetComponent<WaterComponent>();
		REQUIRE(loadedWc != nullptr);
		CHECK(loadedWc->waterLevel == doctest::Approx(1.5f));
		CHECK(loadedWc->waveAmplitude == doctest::Approx(0.6f));
		CHECK(loadedWc->buoyancyDensity == doctest::Approx(3.2f));
	}
}
