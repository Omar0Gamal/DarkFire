#include <doctest/doctest.h>
#include "Engine/Platform/Input.h"
#include "Engine/Platform/KeyCodes.h"
#include "Engine/Runtime/LayerStack.h"
#include "Engine/Runtime/Config.h"
#include "Engine/Core/Scripting/ScriptableEntity.h"
#include "Engine/Core/Scripting/ScriptRegistry.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Serialization/SceneSerializer.h"
#include "Engine/Physics/PhysicsWorld.h"
#include "Engine/Core/Math/Math.h"
#include <memory>
#include <cmath>

using Core::Vector3;

namespace
{
	class TestScriptA : public Core::ScriptableEntity
	{
	public:
		int createCount = 0;
		int startCount = 0;
		int updateCount = 0;
		int fixedUpdateCount = 0;
		int lateUpdateCount = 0;
		int destroyCount = 0;
		int collisionEnterCount = 0;
		int collisionExitCount = 0;
		int triggerEnterCount = 0;
		int triggerExitCount = 0;

		void onCreate() override { ++createCount; }
		void onStart() override { ++startCount; }
		void onUpdate(float dt) override { (void)dt; ++updateCount; }
		void onFixedUpdate(float fixedDt) override { (void)fixedDt; ++fixedUpdateCount; }
		void onLateUpdate(float dt) override { (void)dt; ++lateUpdateCount; }
		void onDestroy() override { ++destroyCount; }
		void onCollisionEnter(Core::Entity other) override { (void)other; ++collisionEnterCount; }
		void onCollisionExit(Core::Entity other) override { (void)other; ++collisionExitCount; }
		void onTriggerEnter(Core::Entity other) override { (void)other; ++triggerEnterCount; }
		void onTriggerExit(Core::Entity other) override { (void)other; ++triggerExitCount; }
	};
}

TEST_SUITE("Milestone P5: Runtime, Scripting & Physics")
{
	TEST_CASE("P5.1 - Input subsystem queries and frame deltas")
	{
		Platform::Input::init();

		CHECK_FALSE(Platform::Input::isKeyPressed(Platform::KeyCode::W));
		CHECK_FALSE(Platform::Input::isKeyJustPressed(Platform::KeyCode::W));

		Platform::Input::onKeyDown(Platform::KeyCode::W);
		CHECK(Platform::Input::isKeyPressed(Platform::KeyCode::W));
		CHECK(Platform::Input::isKeyJustPressed(Platform::KeyCode::W));

		// Advance frame
		Platform::Input::newFrame();
		CHECK(Platform::Input::isKeyPressed(Platform::KeyCode::W));
		CHECK_FALSE(Platform::Input::isKeyJustPressed(Platform::KeyCode::W));

		Platform::Input::onKeyUp(Platform::KeyCode::W);
		CHECK_FALSE(Platform::Input::isKeyPressed(Platform::KeyCode::W));
		CHECK(Platform::Input::isKeyJustReleased(Platform::KeyCode::W));

		// Mouse testing
		Platform::Input::onMouseMove(100.0f, 200.0f);
		Platform::Input::onMouseMove(120.0f, 210.0f);
		CHECK(Platform::Input::getMousePosition().x == doctest::Approx(120.0f));
		CHECK(Platform::Input::getMousePosition().y == doctest::Approx(210.0f));
		CHECK(Platform::Input::getMouseDelta().x == doctest::Approx(20.0f));
		CHECK(Platform::Input::getMouseDelta().y == doctest::Approx(10.0f));

		Platform::Input::newFrame();
		CHECK(Platform::Input::getMouseDelta().x == doctest::Approx(0.0f));
		CHECK(Platform::Input::getMouseDelta().y == doctest::Approx(0.0f));
	}

	TEST_CASE("P5.1 - LayerStack ordering and lifecycle")
	{
		struct DummyLayer : public Runtime::Layer
		{
			bool attached = false;
			int updateCount = 0;
			int fixedCount = 0;

			explicit DummyLayer(std::string name) : Layer(std::move(name)) {}

			void onAttach() override { attached = true; }
			void onDetach() override { attached = false; }
			void onUpdate(float dt) override { (void)dt; ++updateCount; }
			void onFixedUpdate(float fixedDt) override { (void)fixedDt; ++fixedCount; }
		};

		Runtime::LayerStack stack;
		auto layer1 = std::make_shared<DummyLayer>("L1");
		auto layer2 = std::make_shared<DummyLayer>("L2");
		auto overlay = std::make_shared<DummyLayer>("Overlay");

		stack.pushLayer(layer1);
		stack.pushOverlay(overlay);
		stack.pushLayer(layer2);

		CHECK(layer1->attached);
		CHECK(layer2->attached);
		CHECK(overlay->attached);

		// Order should be layer1, layer2, overlay
		std::vector<std::string> names;
		for (auto& l : stack)
		{
			names.push_back(l->getName());
		}
		REQUIRE(names.size() == 3);
		CHECK(names[0] == "L1");
		CHECK(names[1] == "L2");
		CHECK(names[2] == "Overlay");

		stack.clear();
		CHECK_FALSE(layer1->attached);
		CHECK_FALSE(layer2->attached);
		CHECK_FALSE(overlay->attached);
	}

	TEST_CASE("P5.1 - ProjectConfig serialization round-trip")
	{
		Runtime::ProjectConfig cfg;
		cfg.name = "TestProject";
		cfg.version = "2.1.0";
		cfg.windowTitle = "My Game";
		cfg.windowWidth = 1920;
		cfg.windowHeight = 1080;
		cfg.gravity = Vector3(0.0f, -15.0f, 0.0f);
		cfg.fixedTimeStep = 1.0f / 120.0f;
		cfg.collisionMatrix[0] = 0x0001;

		std::filesystem::path tempPath = "bin/TestProject.dfproj";
		auto saveRes = cfg.Save(tempPath);
		REQUIRE(saveRes.isOk());

		auto loadRes = Runtime::ProjectConfig::Load(tempPath);
		REQUIRE(loadRes.isOk());

		const auto& loaded = loadRes.value();
		CHECK(loaded.name == "TestProject");
		CHECK(loaded.version == "2.1.0");
		CHECK(loaded.windowTitle == "My Game");
		CHECK(loaded.windowWidth == 1920);
		CHECK(loaded.windowHeight == 1080);
		CHECK(loaded.gravity.y == doctest::Approx(-15.0f));
		CHECK(loaded.fixedTimeStep == doctest::Approx(1.0f / 120.0f));
		CHECK(loaded.collisionMatrix[0] == 0x0001);

		std::filesystem::remove(tempPath);
	}

	TEST_CASE("P5.2 - Native Scripting & ScriptRegistry lifecycle")
	{
		Core::ScriptRegistry::Get().clear();
		Core::ScriptRegistry::Get().registerScript<TestScriptA>("TestScriptA");

		CHECK(Core::ScriptRegistry::Get().isRegistered("TestScriptA"));
		CHECK_FALSE(Core::ScriptRegistry::Get().isRegistered("NonExistentScript"));

		auto scriptInst = Core::ScriptRegistry::Get().instantiate("TestScriptA");
		REQUIRE(scriptInst.get() != nullptr);

		Core::Scene scene;
		auto ent = scene.createEntity("TestActor");
		scriptInst->bind(ent, &scene);

		auto* casted = dynamic_cast<TestScriptA*>(scriptInst.get());
		REQUIRE(casted != nullptr);

		casted->onCreate();
		casted->onStart();
		casted->onFixedUpdate(1.0f / 60.0f);
		casted->onUpdate(0.016f);
		casted->onLateUpdate(0.016f);
		casted->onDestroy();

		CHECK(casted->createCount == 1);
		CHECK(casted->startCount == 1);
		CHECK(casted->fixedUpdateCount == 1);
		CHECK(casted->updateCount == 1);
		CHECK(casted->lateUpdateCount == 1);
		CHECK(casted->destroyCount == 1);
	}

	TEST_CASE("P5.2 - ScriptsComponent preserves unrecognized scripts as MissingScriptData")
	{
		Core::Scene scene;
		auto ent = scene.createEntity("CustomActor");
		auto& sc = ent.addComponent<Core::ScriptsComponent>();
		sc.addScript("KnownScript");
		sc.missingScripts.push_back(Core::MissingScriptData{ "FutureModuleScript", "{\"speed\": 10.5}" });

		Core::SceneSerializer serializer(&scene);
		std::string jsonStr = serializer.serializeToString();

		Core::Scene deserializedScene;
		Core::SceneSerializer deserializer(&deserializedScene);
		bool success = deserializer.deserializeFromString(jsonStr);
		REQUIRE(success);

		auto loadedEnt = deserializedScene.findEntityByName("CustomActor");
		REQUIRE(loadedEnt.isValid());
		REQUIRE(loadedEnt.hasComponent<Core::ScriptsComponent>());

		const auto& loadedSC = loadedEnt.getComponent<Core::ScriptsComponent>();
		REQUIRE(loadedSC.scripts.size() == 1);
		CHECK(loadedSC.scripts[0].scriptName == "KnownScript");
		REQUIRE(loadedSC.missingScripts.size() == 1);
		CHECK(loadedSC.missingScripts[0].scriptName == "FutureModuleScript");
		CHECK(loadedSC.missingScripts[0].rawJson == "{\"speed\": 10.5}");
	}

	TEST_CASE("P5.3 - PhysicsWorld gravity integration over 60 steps")
	{
		Physics::PhysicsWorld physics;
		Vector3 gravity(0.0f, -9.81f, 0.0f);
		float dt = 1.0f / 60.0f;
		physics.init(gravity, dt);

		Core::Scene scene;
		auto ball = scene.createEntity("FallingBall");
		auto& trans = ball.getComponent<Core::TransformComponent>();
		trans.position = Vector3(0.0f, 10.0f, 0.0f);

		auto& rb = ball.addComponent<Core::RigidbodyComponent>();
		rb.useGravity = true;
		rb.drag = 0.0f; // No drag for analytical verification

		for (int step = 0; step < 60; ++step)
		{
			physics.step(scene, dt);
		}

		// Analytical: t = 1.0s, v = gt = -9.81 m/s, y = y0 + 0.5 * g * t^2
		auto* state = ball.tryGetComponent<Core::RigidbodyRuntimeState>();
		REQUIRE(state != nullptr);
		CHECK(state->linearVelocity.y == doctest::Approx(-9.81f).epsilon(0.05f));

		// Position after 60 steps with Euler integration
		CHECK(trans.position.y < 10.0f);
		CHECK(trans.position.y == doctest::Approx(5.095f).epsilon(0.1f));
	}

	TEST_CASE("P5.3 - PhysicsWorld collision resolution and bounce")
	{
		Physics::PhysicsWorld physics;
		physics.init(Vector3(0.0f, 0.0f, 0.0f), 1.0f / 60.0f); // Zero gravity

		Core::Scene scene;

		// Static wall at x = 2
		auto wall = scene.createEntity("Wall");
		auto& wallTrans = wall.getComponent<Core::TransformComponent>();
		wallTrans.position = Vector3(2.0f, 0.0f, 0.0f);
		auto& wallCol = wall.addComponent<Core::ColliderComponent>();
		wallCol.shape = Core::ColliderShape::Box;
		wallCol.size = Vector3(1.0f, 10.0f, 10.0f);

		// Dynamic sphere at x = 1.2 moving right at +10 m/s
		auto sphere = scene.createEntity("Sphere");
		auto& sphereTrans = sphere.getComponent<Core::TransformComponent>();
		sphereTrans.position = Vector3(1.2f, 0.0f, 0.0f);
		auto& sphereCol = sphere.addComponent<Core::ColliderComponent>();
		sphereCol.shape = Core::ColliderShape::Sphere;
		sphereCol.radius = 0.5f;

		auto& sphereRb = sphere.addComponent<Core::RigidbodyComponent>();
		sphereRb.mass = 1.0f;
		sphereRb.restitution = 1.0f; // Perfect elastic collision
		sphereRb.drag = 0.0f;
		sphereRb.useGravity = false;

		auto& state = scene.getRegistry().emplace<Core::RigidbodyRuntimeState>(sphere.getHandle());
		state.linearVelocity = Vector3(10.0f, 0.0f, 0.0f);

		// Step physics
		physics.step(scene, 1.0f / 60.0f);

		// Sphere should have bounced back with negative X velocity
		CHECK(state.linearVelocity.x < 0.0f);
		CHECK(state.linearVelocity.x == doctest::Approx(-10.0f).epsilon(0.01f));
	}

	TEST_CASE("P5.3 - Box-Sphere deep penetration resolution prevents ground clipping")
	{
		Physics::PhysicsWorld physics;
		physics.init(Vector3(0.0f, 0.0f, 0.0f), 1.0f / 60.0f);

		Core::Scene scene;

		// Static ground box at y = -0.5, size = (50, 1, 50) -> top face is y = 0.0
		auto ground = scene.createEntity("Ground");
		auto& gTrans = ground.getComponent<Core::TransformComponent>();
		gTrans.position = Vector3(0.0f, -0.5f, 0.0f);
		auto& gCol = ground.addComponent<Core::ColliderComponent>();
		gCol.shape = Core::ColliderShape::Box;
		gCol.size = Vector3(50.0f, 1.0f, 50.0f);

		// Dynamic sphere with radius = 0.5 placed with center at y = -0.05 (inside the box)
		auto sphere = scene.createEntity("Sphere");
		auto& sTrans = sphere.getComponent<Core::TransformComponent>();
		sTrans.position = Vector3(0.0f, -0.05f, 0.0f);
		auto& sCol = sphere.addComponent<Core::ColliderComponent>();
		sCol.shape = Core::ColliderShape::Sphere;
		sCol.radius = 0.5f;

		auto& sRb = sphere.addComponent<Core::RigidbodyComponent>();
		sRb.mass = 1.0f;
		sRb.restitution = 0.0f;
		sRb.useGravity = false;

		physics.step(scene, 1.0f / 60.0f);

		// Positional correction must push the sphere center completely out to y >= 0.5f
		CHECK(sTrans.position.y >= 0.5f);
		CHECK(sTrans.position.y == doctest::Approx(0.5f).epsilon(0.001f));
	}

	TEST_CASE("P5.3 - OBB vs OBB SAT collision detects rotated box intersection")
	{
		Physics::PhysicsWorld physics;
		physics.init(Vector3(0.0f, 0.0f, 0.0f), 1.0f / 60.0f);

		Core::Scene scene;

		// Box A at (0, 0, 0), size (1, 1, 1), unrotated
		auto boxA = scene.createEntity("BoxA");
		auto& aTrans = boxA.getComponent<Core::TransformComponent>();
		aTrans.position = Vector3(0.0f, 0.0f, 0.0f);
		auto& aCol = boxA.addComponent<Core::ColliderComponent>();
		aCol.shape = Core::ColliderShape::Box;
		aCol.size = Vector3(1.0f, 1.0f, 1.0f);

		// Box B at (1.1, 0, 0), size (1, 1, 1), rotated 45 degrees around Y
		// Unrotated: right edge of A is 0.5, left edge of B is 1.1 - 0.5 = 0.6 (no AABB collision).
		// Rotated 45 deg: corner of B extends by 0.5 * sqrt(2) = 0.707, reaching 1.1 - 0.707 = 0.393 (< 0.5).
		// OBB SAT MUST detect this collision and resolve it!
		auto boxB = scene.createEntity("BoxB");
		auto& bTrans = boxB.getComponent<Core::TransformComponent>();
		bTrans.position = Vector3(1.1f, 0.0f, 0.0f);
		bTrans.setEulerDegrees(Vector3(0.0f, 45.0f, 0.0f));
		auto& bCol = boxB.addComponent<Core::ColliderComponent>();
		bCol.shape = Core::ColliderShape::Box;
		bCol.size = Vector3(1.0f, 1.0f, 1.0f);

		auto& bRb = boxB.addComponent<Core::RigidbodyComponent>();
		bRb.mass = 1.0f;
		bRb.isKinematic = false;
		bRb.useGravity = false;

		physics.step(scene, 1.0f / 60.0f);

		// Positional correction must push Box B rightward away from Box A
		CHECK(bTrans.position.x > 1.1f);
	}

	TEST_CASE("P5.3 - Trigger detection dispatches enter/exit without physical response")
	{
		Core::ScriptRegistry::Get().clear();
		Core::ScriptRegistry::Get().registerScript<TestScriptA>("TestScriptA");

		Physics::PhysicsWorld physics;
		physics.init(Vector3(0.0f, 0.0f, 0.0f), 1.0f / 60.0f);

		Core::Scene scene;

		// Trigger zone at (0, 0, 0)
		auto triggerEnt = scene.createEntity("TriggerZone");
		auto& tCol = triggerEnt.addComponent<Core::ColliderComponent>();
		tCol.shape = Core::ColliderShape::Box;
		tCol.size = Vector3(2.0f, 2.0f, 2.0f);
		tCol.isTrigger = true;

		auto& tSc = triggerEnt.addComponent<Core::ScriptsComponent>();
		tSc.addScript("TestScriptA");
		auto tScriptInst = Core::ScriptRegistry::Get().instantiate("TestScriptA");
		tScriptInst->bind(triggerEnt, &scene);
		tSc.scripts[0].instance = tScriptInst;
		tSc.scripts[0].isStarted = true;

		// Moving entity entering trigger
		auto actor = scene.createEntity("Actor");
		actor.getComponent<Core::TransformComponent>().position = Vector3(0.0f, 0.0f, 0.0f);
		auto& aCol = actor.addComponent<Core::ColliderComponent>();
		aCol.shape = Core::ColliderShape::Sphere;
		aCol.radius = 0.5f;

		// Step 1: Actor is inside trigger -> trigger enter
		physics.step(scene, 1.0f / 60.0f);

		auto* script = dynamic_cast<TestScriptA*>(tSc.scripts[0].instance.get());
		REQUIRE(script != nullptr);
		CHECK(script->triggerEnterCount == 1);
		CHECK(script->triggerExitCount == 0);

		// Move actor far outside trigger
		actor.getComponent<Core::TransformComponent>().position = Vector3(100.0f, 0.0f, 0.0f);

		// Step 2: Actor moved away -> trigger exit
		physics.step(scene, 1.0f / 60.0f);
		CHECK(script->triggerEnterCount == 1);
		CHECK(script->triggerExitCount == 1);
	}

	TEST_CASE("P5.3 - 16x16 Collision matrix layer filtering")
	{
		Physics::PhysicsWorld physics;
		physics.init(Vector3(0.0f, 0.0f, 0.0f), 1.0f / 60.0f);

		// Layer 1 and Layer 2 do NOT collide
		std::array<uint16_t, 16> matrix{};
		matrix.fill(0xFFFF);
		matrix[1] &= ~(1 << 2); // Layer 1 does not collide with Layer 2
		matrix[2] &= ~(1 << 1); // Layer 2 does not collide with Layer 1
		physics.setCollisionMatrix(matrix);

		Core::Scene scene;

		auto entA = scene.createEntity("EntityA");
		entA.getComponent<Core::TransformComponent>().position = Vector3(0.0f, 0.0f, 0.0f);
		auto& colA = entA.addComponent<Core::ColliderComponent>();
		colA.shape = Core::ColliderShape::Sphere;
		colA.radius = 1.0f;
		colA.layer = 1;

		auto entB = scene.createEntity("EntityB");
		entB.getComponent<Core::TransformComponent>().position = Vector3(0.5f, 0.0f, 0.0f);
		auto& colB = entB.addComponent<Core::ColliderComponent>();
		colB.shape = Core::ColliderShape::Sphere;
		colB.radius = 1.0f;
		colB.layer = 2;

		auto& rbB = entB.addComponent<Core::RigidbodyComponent>();
		rbB.mass = 1.0f;
		rbB.drag = 0.0f;
		rbB.useGravity = false;
		auto& stateB = scene.getRegistry().emplace<Core::RigidbodyRuntimeState>(entB.getHandle());
		stateB.linearVelocity = Vector3(5.0f, 0.0f, 0.0f);

		physics.step(scene, 1.0f / 60.0f);

		// Because layer 1 and 2 don't collide, entB should pass right through without velocity change
		CHECK(stateB.linearVelocity.x == doctest::Approx(5.0f));
	}

	TEST_CASE("P5.3 - Physics raycasting")
	{
		Physics::PhysicsWorld physics;
		physics.init(Vector3(0.0f, 0.0f, 0.0f), 1.0f / 60.0f);

		Core::Scene scene;

		// Sphere at (0, 0, 10)
		auto sphere = scene.createEntity("TargetSphere");
		sphere.getComponent<Core::TransformComponent>().position = Vector3(0.0f, 0.0f, 10.0f);
		auto& col = sphere.addComponent<Core::ColliderComponent>();
		col.shape = Core::ColliderShape::Sphere;
		col.radius = 1.0f;

		Physics::RaycastHit hit;
		bool hasHit = physics.raycast(
			scene,
			Vector3(0.0f, 0.0f, 0.0f),
			Vector3(0.0f, 0.0f, 1.0f),
			50.0f,
			hit
		);

		REQUIRE(hasHit);
		CHECK(hit.hasHit);
		CHECK(hit.distance == doctest::Approx(9.0f)); // 10.0 - radius 1.0
		CHECK(hit.point.z == doctest::Approx(9.0f));
		CHECK(hit.entity == sphere);
	}
}
