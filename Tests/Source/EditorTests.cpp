#ifdef _MSC_VER
#pragma warning(disable: 5285)
#endif
#include "doctest/doctest.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Entity.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Serialization/SceneSerializer.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Physics/PhysicsWorld.h"
#include "Engine/Renderer/SceneRenderer.h"
#include "Editor/EditorCamera.cpp"
#include "Editor/Commands/ICommand.h"
#include "Editor/Commands/CommandHistory.cpp"
#include "Editor/Commands/EntityCommands.cpp"
#include "Editor/SelectionContext.h"
#include "Editor/Clipboard.cpp"
#include "Engine/Core/Assets/AssetGuid.h"
#include "Engine/Core/Assets/AssetTypes.h"
#include "Engine/Core/Assets/AssetManager.h"
#include "Engine/Core/Scripting/ScriptRegistry.h"
#include "Engine/Core/Scripting/ScriptableEntity.h"
#include <filesystem>
#include <random>
#include <fstream>

TEST_SUITE("Editor")
{
	TEST_CASE("EditorCamera - Reversed-Z & Orientation")
	{
		Editor::EditorCamera camera(60.0f, 16.0f / 9.0f, 0.1f, 1000.0f);
		camera.setViewportSize(1920, 1080);

		// Reversed-Z: near plane projects to z = 1.0f, far plane projects to z = 0.0f
		const auto& proj = camera.getProjectionMatrix();
		CHECK(proj._33 < 0.0f); // Negative slope for reversed-z
		CHECK(proj._34 == 1.0f); // LH perspective projection

		// Check camera focus
		Core::Vector3 target(10.0f, 5.0f, 20.0f);
		camera.focusOn(target, 4.0f);

		// Camera should be positioned looking at target
		Core::Vector3 forward = camera.getForward();
		Core::Vector3 camPos = camera.getPosition();
		Core::Vector3 toTarget = target - camPos;
		toTarget.Normalize();

		float dot = forward.Dot(toTarget);
		CHECK(dot > 0.99f); // Forward vector directly points at target
	}

	TEST_CASE("Editor - Raycast Unprojection Reversed-Z")
	{
		Editor::EditorCamera camera(60.0f, 1.0f, 0.1f, 1000.0f);
		camera.setViewportSize(800, 800);

		// Unproject ray at center of viewport (400, 400)
		auto ray = Core::Renderer::SceneRenderer::unprojectRayReversedZ(
			400.0f, 400.0f, 800.0f, 800.0f, camera.getInverseViewProjectionMatrix()
		);

		// Ray direction at viewport center should match camera forward
		Core::Vector3 camForward = camera.getForward();
		float dot = ray.direction.Dot(camForward);
		CHECK(dot > 0.999f);
	}

	TEST_CASE("Editor - Entity Hierarchy & Transform Manipulation")
	{
		Core::Scene scene;

		auto parent = scene.createEntity("ParentNode");
		auto child = scene.createEntity("ChildNode");

		parent.getTransform().position = Core::Vector3(5.0f, 0.0f, 0.0f);
		child.getTransform().position = Core::Vector3(0.0f, 2.0f, 0.0f);

		child.setParent(parent, false);

		scene.updateTransforms();

		// World matrix of child should be parent's position (5, 0, 0) + child's local (0, 2, 0)
		Core::Matrix childWorld = child.getWorldMatrix();
		Core::Vector3 worldPos(childWorld._41, childWorld._42, childWorld._43);

		CHECK(worldPos.x == doctest::Approx(5.0f));
		CHECK(worldPos.y == doctest::Approx(2.0f));
		CHECK(worldPos.z == doctest::Approx(0.0f));

		// Test ImGuizmo inverse transformation math:
		// When child is transformed in world space to (10, 5, 0), local position should become (5, 5, 0)
		Core::Matrix desiredWorld = Core::Math::createTransformTRS(
			Core::Vector3(10.0f, 5.0f, 0.0f),
			Core::Quaternion{},
			Core::Vector3(1.0f, 1.0f, 1.0f)
		);

		Core::Matrix parentWorldInv = parent.getWorldMatrix().Invert();
		Core::Matrix newLocal = desiredWorld * parentWorldInv;

		Core::Vector3 localPos, localScale;
		Core::Quaternion localRot;
		Core::Math::decomposeTRS(newLocal, localPos, localRot, localScale);

		CHECK(localPos.x == doctest::Approx(5.0f));
		CHECK(localPos.y == doctest::Approx(5.0f));
		CHECK(localPos.z == doctest::Approx(0.0f));
	}

	TEST_CASE("Editor - Entity Raycast Picking")
	{
		Core::Scene scene;
		Physics::PhysicsWorld physics;
		physics.init(Core::Vector3(0.0f, -9.81f, 0.0f), 1.0f / 60.0f);

		auto targetEntity = scene.createEntity("PickTarget");
		targetEntity.getTransform().position = Core::Vector3(0.0f, 0.0f, 5.0f);

		auto& col = targetEntity.addComponent<Core::ColliderComponent>();
		col.shape = Core::ColliderShape::Sphere;
		col.radius = 1.0f;

		Physics::RaycastHit hit;
		bool hasHit = physics.raycast(
			scene,
			Core::Vector3(0.0f, 0.0f, 0.0f),
			Core::Vector3(0.0f, 0.0f, 1.0f),
			100.0f,
			hit
		);

		CHECK(hasHit == true);
		CHECK(hit.entity == targetEntity);
		CHECK(hit.distance == doctest::Approx(4.0f)); // 5.0f - radius 1.0f = 4.0f
	}

	TEST_CASE("Editor - Full Scene Authoring & Serialization Round-Trip")
	{
		std::filesystem::path tempScenePath = "Temp_EditorAuthoring_Test.scene";

		// 1. Author scene in editor
		{
			Core::Scene editorScene;

			auto camera = editorScene.createEntity("Main Camera");
			camera.getTransform().position = Core::Vector3(0.0f, 2.0f, -5.0f);
			auto& camComp = camera.addComponent<Core::CameraComponent>();
			camComp.isPrimary = true;
			camComp.fov = 65.0f;

			auto light = editorScene.createEntity("Directional Light");
			light.getTransform().position = Core::Vector3(0.0f, 10.0f, 0.0f);
			auto& lightComp = light.addComponent<Core::LightComponent>();
			lightComp.type = Core::LightType::Directional;
			lightComp.intensity = 2.5f;

			auto cube = editorScene.createEntity("Physics Cube");
			cube.getTransform().position = Core::Vector3(1.0f, 4.0f, 3.0f);
			auto& mesh = cube.addComponent<Core::MeshRendererComponent>();
			mesh.meshPath = "/engine/Content/cube.obj";
			mesh.materialPath = "/engine/Content/Default.mat";
			auto& rb = cube.addComponent<Core::RigidbodyComponent>();
			rb.mass = 2.0f;
			rb.isKinematic = false;
			auto& col = cube.addComponent<Core::ColliderComponent>();
			col.shape = Core::ColliderShape::Box;
			col.size = Core::Vector3(1.0f, 1.0f, 1.0f);

			// Save to disk
			Core::SceneSerializer serializer(&editorScene);
			bool saved = serializer.serializeToFile(tempScenePath);
			CHECK(saved == true);
		}

		// 2. Load scene in runtime (simulating Player.exe)
		{
			Core::Scene playerScene;
			Core::SceneSerializer deserializer(&playerScene);
			bool loaded = deserializer.deserializeFromFile(tempScenePath);
			CHECK(loaded == true);

			CHECK(playerScene.getEntityCount() == 3);

			// Verify Primary Camera
			auto camEntity = playerScene.findEntityByName("Main Camera");
			CHECK(camEntity.isValid());
			CHECK(camEntity.hasComponent<Core::CameraComponent>());
			CHECK(camEntity.getComponent<Core::CameraComponent>().isPrimary == true);
			CHECK(camEntity.getComponent<Core::CameraComponent>().fov == doctest::Approx(65.0f));

			// Verify Directional Light
			auto lightEntity = playerScene.findEntityByName("Directional Light");
			CHECK(lightEntity.isValid());
			CHECK(lightEntity.hasComponent<Core::LightComponent>());
			CHECK(lightEntity.getComponent<Core::LightComponent>().type == Core::LightType::Directional);
			CHECK(lightEntity.getComponent<Core::LightComponent>().intensity == doctest::Approx(2.5f));

			// Verify Physics Cube
			auto cubeEntity = playerScene.findEntityByName("Physics Cube");
			CHECK(cubeEntity.isValid());
			CHECK(cubeEntity.hasComponent<Core::MeshRendererComponent>());
			CHECK(cubeEntity.getComponent<Core::MeshRendererComponent>().meshPath == "/engine/Content/cube.obj");
			CHECK(cubeEntity.hasComponent<Core::RigidbodyComponent>());
			CHECK(cubeEntity.getComponent<Core::RigidbodyComponent>().mass == doctest::Approx(2.0f));
			CHECK(cubeEntity.hasComponent<Core::ColliderComponent>());
			CHECK(cubeEntity.getComponent<Core::ColliderComponent>().shape == Core::ColliderShape::Box);
		}

		// Clean up temporary file
		if (std::filesystem::exists(tempScenePath))
		{
			std::filesystem::remove(tempScenePath);
		}
	}

	TEST_CASE("Editor - SelectionContext Tracking")
	{
		Core::Scene scene;
		auto e1 = scene.createEntity("Entity1");
		auto e2 = scene.createEntity("Entity2");
		auto e3 = scene.createEntity("Entity3");

		Editor::SelectionContext sel;
		CHECK(sel.isEmpty());
		CHECK(sel.getCount() == 0);
		CHECK(!sel.getPrimary().isValid());

		// Single select e1
		sel.select(e1, false);
		CHECK(sel.getCount() == 1);
		CHECK(sel.isSelected(e1));
		CHECK(sel.getPrimary() == e1);

		// Additive select e2
		sel.select(e2, true);
		CHECK(sel.getCount() == 2);
		CHECK(sel.isSelected(e1));
		CHECK(sel.isSelected(e2));
		CHECK(sel.getPrimary() == e2);

		// Additive toggle off e1
		sel.select(e1, true);
		CHECK(sel.getCount() == 1);
		CHECK(!sel.isSelected(e1));
		CHECK(sel.isSelected(e2));
		CHECK(sel.getPrimary() == e2);

		// Deselect
		sel.deselect(e2);
		CHECK(sel.isEmpty());

		// Multi-select then prune dead entities
		sel.select(e1, true);
		sel.select(e3, true);
		CHECK(sel.getCount() == 2);

		e1.destroy();
		sel.removeInvalidEntities();
		CHECK(sel.getCount() == 1);
		CHECK(sel.isSelected(e3));
		CHECK(sel.getPrimary() == e3);

		sel.clear();
		CHECK(sel.isEmpty());
	}

	class DummySizedCommand : public Editor::ICommand
	{
	public:
		DummySizedCommand(size_t size) : m_size(size) {}
		void execute() override { m_executed = true; }
		void undo() override { m_executed = false; }
		size_t getMemorySize() const override { return m_size; }
		std::string_view getName() const override { return "Dummy"; }
		bool isExecuted() const { return m_executed; }

	private:
		size_t m_size = 100;
		bool m_executed = false;
	};

	TEST_CASE("Editor - CommandHistory Bounded Capacity Eviction")
	{
		Editor::CommandHistory history(250); // 250 bytes max

		CHECK(history.getMemoryUsage() == 0);
		CHECK(history.getMaxMemory() == 250);

		// Execute cmd1 (100b)
		history.execute(std::make_unique<DummySizedCommand>(100));
		CHECK(history.getUndoCount() == 1);
		CHECK(history.getMemoryUsage() == 100);

		// Execute cmd2 (100b)
		history.execute(std::make_unique<DummySizedCommand>(100));
		CHECK(history.getUndoCount() == 2);
		CHECK(history.getMemoryUsage() == 200);

		// Execute cmd3 (100b) -> total 300b > 250b -> cmd1 evicted!
		history.execute(std::make_unique<DummySizedCommand>(100));
		CHECK(history.getUndoCount() == 2);
		CHECK(history.getMemoryUsage() == 200);

		// Test undo & redo
		CHECK(history.canUndo());
		CHECK(history.undo());
		CHECK(history.getUndoCount() == 1);
		CHECK(history.getRedoCount() == 1);

		CHECK(history.canRedo());
		CHECK(history.redo());
		CHECK(history.getUndoCount() == 2);
		CHECK(history.getRedoCount() == 0);

		history.clear();
		CHECK(history.getUndoCount() == 0);
		CHECK(history.getRedoCount() == 0);
		CHECK(history.getMemoryUsage() == 0);
	}

	TEST_CASE("Editor - Clipboard Copy, Cut, Paste, Duplicate")
	{
		Core::Scene scene;
		Editor::CommandHistory history(64 * 1024 * 1024);

		auto origEnt = scene.createEntity("OriginalEntity");
		origEnt.getTransform().position = Core::Vector3(5.0f, 10.0f, 15.0f);
		auto& cam = origEnt.addComponent<Core::CameraComponent>();
		cam.fov = 75.0f;

		// 1. Copy & Paste
		Editor::Clipboard::Copy({ origEnt }, &scene);
		CHECK(Editor::Clipboard::HasClipboardData());

		auto pasted = Editor::Clipboard::Paste(&scene, history);
		REQUIRE(pasted.size() == 1);
		auto pastedEnt = pasted[0];
		CHECK(pastedEnt.isValid());
		CHECK(pastedEnt.getUUID() != origEnt.getUUID());
		CHECK(pastedEnt.getName() == "OriginalEntity");
		CHECK(pastedEnt.hasComponent<Core::CameraComponent>());
		CHECK(pastedEnt.getComponent<Core::CameraComponent>().fov == doctest::Approx(75.0f));
		// Position offset by (+1, 0, +1)
		CHECK(pastedEnt.getTransform().position.x == doctest::Approx(6.0f));
		CHECK(pastedEnt.getTransform().position.y == doctest::Approx(10.0f));
		CHECK(pastedEnt.getTransform().position.z == doctest::Approx(16.0f));

		// Undo paste
		CHECK(history.undo());
		CHECK(!pastedEnt.isValid());

		// Redo paste
		CHECK(history.redo());
		auto restoredPasted = scene.findEntityByName("OriginalEntity");
		CHECK(restoredPasted.isValid());

		// 2. Duplicate
		auto dups = Editor::Clipboard::Duplicate({ origEnt }, &scene, history);
		REQUIRE(dups.size() == 1);
		auto dupEnt = dups[0];
		CHECK(dupEnt.isValid());
		CHECK(dupEnt.getUUID() != origEnt.getUUID());
		CHECK(dupEnt.getTransform().position.x == doctest::Approx(6.0f));

		// Undo duplicate
		CHECK(history.undo());
		CHECK(!dupEnt.isValid());

		// 3. Cut
		auto cutEntities = Editor::Clipboard::Cut({ origEnt }, &scene, history);
		CHECK(!origEnt.isValid()); // origEnt was deleted
		CHECK(Editor::Clipboard::HasClipboardData());

		// Undo cut restores origEnt
		CHECK(history.undo());
		auto restoredOrig = scene.findEntityByName("OriginalEntity");
		CHECK(restoredOrig.isValid());
		CHECK(restoredOrig.getTransform().position.x == doctest::Approx(5.0f));
	}

	static bool IsEntityDescendantOf(Core::Entity parent, Core::Entity candidate)
	{
		if (!parent.isValid() || !candidate.isValid()) return false;
		Core::Entity curr = candidate.getParent();
		while (curr.isValid())
		{
			if (curr == parent) return true;
			curr = curr.getParent();
		}
		return false;
	}

	static void CollectSubtreeUuids(Core::Entity root, std::vector<Core::UUID>& outUuids)
	{
		if (!root.isValid()) return;
		outUuids.push_back(root.getUUID());
		for (auto child : root.getChildren())
		{
			CollectSubtreeUuids(child, outUuids);
		}
	}

	TEST_CASE("Editor - 500 Commands Undo/Redo Fuzz Test")
	{
		Core::Scene scene;
		Core::SceneSerializer serializer(&scene);

		// Seed initial entities
		std::vector<Core::UUID> activeUuids;
		for (int i = 0; i < 10; ++i)
		{
			auto ent = scene.createEntity("SeedEntity_" + std::to_string(i));
			ent.getTransform().position = Core::Vector3(static_cast<float>(i * 2), static_cast<float>(i), 0.0f);
			ent.getTransform().setEulerDegrees(Core::Vector3(0.0f, static_cast<float>(i * 10), 0.0f));
			ent.getTransform().scale = Core::Vector3(1.0f, 1.0f, 1.0f);
			if (i % 3 == 0)
			{
				auto& cam = ent.addComponent<Core::CameraComponent>();
				cam.isPrimary = (i == 0);
				cam.fov = 60.0f;
			}
			if (i % 2 == 0)
			{
				auto& mesh = ent.addComponent<Core::MeshRendererComponent>();
				mesh.meshPath = "/engine/Content/cube.obj";
				mesh.materialPath = "/engine/Content/Default.mat";
			}
			activeUuids.push_back(ent.getUUID());
		}

		// Hierarchy between some seeds
		auto e1 = scene.findEntityByUUID(activeUuids[1]);
		auto e0 = scene.findEntityByUUID(activeUuids[0]);
		e1.setParent(e0, false);

		scene.updateTransforms();
		std::string initialSceneJson = serializer.serializeToString();

		Editor::CommandHistory history(128 * 1024 * 1024); // 128 MB capacity

		std::mt19937 rng(1337);
		std::uniform_int_distribution<int> opDist(0, 3);
		std::uniform_real_distribution<float> floatDist(-50.0f, 50.0f);
		std::uniform_real_distribution<float> scaleDist(0.1f, 5.0f);

		for (int step = 0; step < 500; ++step)
		{
			int op = opDist(rng);

			// op 0: Create Entity
			if (op == 0)
			{
				auto ent = scene.createEntity("FuzzEntity_" + std::to_string(step));
				ent.getTransform().position = Core::Vector3(floatDist(rng), floatDist(rng), floatDist(rng));
				ent.getTransform().setEulerDegrees(Core::Vector3(floatDist(rng), floatDist(rng), floatDist(rng)));
				ent.getTransform().scale = Core::Vector3(scaleDist(rng), scaleDist(rng), scaleDist(rng));
				activeUuids.push_back(ent.getUUID());
				history.execute<Editor::CreateEntityCommand>(&scene, ent);
			}
			// op 1: Modify Transform
			else if (op == 1 && !activeUuids.empty())
			{
				std::uniform_int_distribution<size_t> idxDist(0, activeUuids.size() - 1);
				Core::UUID targetUuid = activeUuids[idxDist(rng)];
				auto ent = scene.findEntityByUUID(targetUuid);
				if (ent.isValid())
				{
					Core::TransformComponent oldTc = ent.getTransform();
					Core::TransformComponent newTc = oldTc;
					newTc.position = Core::Vector3(floatDist(rng), floatDist(rng), floatDist(rng));
					newTc.setEulerDegrees(Core::Vector3(floatDist(rng), floatDist(rng), floatDist(rng)));
					newTc.scale = Core::Vector3(scaleDist(rng), scaleDist(rng), scaleDist(rng));
					history.execute<Editor::ModifyTransformCommand>(&scene, targetUuid, oldTc, newTc, false);
				}
				else
				{
					auto newEnt = scene.createEntity("FuzzEntityFallback_" + std::to_string(step));
					activeUuids.push_back(newEnt.getUUID());
					history.execute<Editor::CreateEntityCommand>(&scene, newEnt);
				}
			}
			// op 2: Reparent
			else if (op == 2 && activeUuids.size() >= 2)
			{
				std::uniform_int_distribution<size_t> idxDist(0, activeUuids.size() - 1);
				Core::UUID childUuid = activeUuids[idxDist(rng)];
				Core::UUID parentUuid = activeUuids[idxDist(rng)];
				auto childEnt = scene.findEntityByUUID(childUuid);
				auto parentEnt = scene.findEntityByUUID(parentUuid);

				if (childEnt.isValid() && parentEnt.isValid() && childUuid != parentUuid && !IsEntityDescendantOf(childEnt, parentEnt))
				{
					Core::UUID oldParentUuid = childEnt.getParent().isValid() ? childEnt.getParent().getUUID() : Core::UUID(0);
					if (oldParentUuid != parentUuid)
					{
						history.execute<Editor::ReparentCommand>(&scene, childUuid, oldParentUuid, parentUuid);
					}
					else
					{
						auto newEnt = scene.createEntity("FuzzEntityFallback_" + std::to_string(step));
						activeUuids.push_back(newEnt.getUUID());
						history.execute<Editor::CreateEntityCommand>(&scene, newEnt);
					}
				}
				else
				{
					auto newEnt = scene.createEntity("FuzzEntityFallback_" + std::to_string(step));
					activeUuids.push_back(newEnt.getUUID());
					history.execute<Editor::CreateEntityCommand>(&scene, newEnt);
				}
			}
			// op 3: Delete Entity
			else if (op == 3 && activeUuids.size() > 5)
			{
				std::uniform_int_distribution<size_t> idxDist(0, activeUuids.size() - 1);
				size_t idx = idxDist(rng);
				Core::UUID targetUuid = activeUuids[idx];
				auto ent = scene.findEntityByUUID(targetUuid);
				if (ent.isValid())
				{
					// Collect all descendant UUIDs that will also be destroyed
					std::vector<Core::UUID> destroyedUuids;
					CollectSubtreeUuids(ent, destroyedUuids);
					for (auto du : destroyedUuids)
					{
						auto it = std::find(activeUuids.begin(), activeUuids.end(), du);
						if (it != activeUuids.end())
						{
							activeUuids.erase(it);
						}
					}
					history.execute<Editor::DeleteEntityCommand>(&scene, ent);
				}
				else
				{
					auto newEnt = scene.createEntity("FuzzEntityFallback_" + std::to_string(step));
					activeUuids.push_back(newEnt.getUUID());
					history.execute<Editor::CreateEntityCommand>(&scene, newEnt);
				}
			}
			else
			{
				auto newEnt = scene.createEntity("FuzzEntityFallback_" + std::to_string(step));
				activeUuids.push_back(newEnt.getUUID());
				history.execute<Editor::CreateEntityCommand>(&scene, newEnt);
			}
		}

		scene.updateTransforms();
		std::string finalSceneJson = serializer.serializeToString();

		// Exit Criterion 1a: 500 undo calls produces byte-identical initial scene
		REQUIRE(history.getUndoCount() == 500);
		for (int i = 0; i < 500; ++i)
		{
			CHECK(history.canUndo());
			bool undone = history.undo();
			CHECK(undone == true);
		}
		CHECK(history.canUndo() == false);
		CHECK(history.getRedoCount() == 500);

		scene.updateTransforms();
		std::string restoredInitialJson = serializer.serializeToString();
		CHECK(restoredInitialJson == initialSceneJson);

		// Exit Criterion 1b: 500 redo calls restores the final state
		for (int i = 0; i < 500; ++i)
		{
			CHECK(history.canRedo());
			bool redone = history.redo();
			CHECK(redone == true);
		}
		CHECK(history.canRedo() == false);
		CHECK(history.getUndoCount() == 500);

		scene.updateTransforms();
		std::string restoredFinalJson = serializer.serializeToString();
		CHECK(restoredFinalJson == finalSceneJson);
	}

	class TestMoverScript : public Core::ScriptableEntity
	{
	public:
		void onUpdate(float dt) override
		{
			auto& tc = getComponent<Core::TransformComponent>();
			tc.position.x += 10.0f * dt;
		}
	};

	TEST_CASE("Editor - Play Mode Snapshot & Byte-Identical Restoration")
	{
		Core::ScriptRegistry::Get().registerScript<TestMoverScript>("TestMoverScript");

		Core::Scene editScene;
		Core::SceneSerializer serializer(&editScene);

		// Author scene with camera, light, physics box, and scripted entity
		auto cam = editScene.createEntity("Main Camera");
		cam.getTransform().position = Core::Vector3(0.0f, 2.0f, -5.0f);
		auto& camComp = cam.addComponent<Core::CameraComponent>();
		camComp.isPrimary = true;
		camComp.fov = 60.0f;

		auto box = editScene.createEntity("DynamicBox");
		box.getTransform().position = Core::Vector3(0.0f, 10.0f, 0.0f);
		auto& rb = box.addComponent<Core::RigidbodyComponent>();
		rb.mass = 1.0f;
		rb.isKinematic = false;
		auto& col = box.addComponent<Core::ColliderComponent>();
		col.shape = Core::ColliderShape::Box;
		col.size = Core::Vector3(1.0f, 1.0f, 1.0f);

		auto mover = editScene.createEntity("ScriptedEntity");
		mover.getTransform().position = Core::Vector3(0.0f, 0.0f, 0.0f);
		auto& sc = mover.addComponent<Core::ScriptsComponent>();
		sc.addScript("TestMoverScript");

		editScene.updateTransforms();
		std::string initialSceneJson = serializer.serializeToString();

		// 1. Enter Play Mode: snapshot scene
		auto activePlayScene = Core::Scene::copy(editScene);

		// Initialize physics and start scripts in play scene
		Physics::PhysicsWorld physics;
		physics.init(Core::Vector3(0.0f, -9.81f, 0.0f), 1.0f / 60.0f);

		auto view = activePlayScene->view<Core::ScriptsComponent>();
		for (auto handle : view)
		{
			Core::Entity ent(handle, activePlayScene.get());
			auto& comp = view.get<Core::ScriptsComponent>(handle);
			for (auto& s : comp.scripts)
			{
				s.instance = Core::ScriptRegistry::Get().instantiate(s.scriptName);
				if (s.instance)
				{
					s.instance->bind(ent, activePlayScene.get());
					s.instance->onCreate();
					s.instance->onStart();
					s.isStarted = true;
				}
			}
		}

		// 2. Simulate 120 ticks (2 seconds of live gameplay at 60Hz)
		float dt = 1.0f / 60.0f;
		for (int tick = 0; tick < 120; ++tick)
		{
			physics.step(*activePlayScene, dt);

			for (auto handle : view)
			{
				auto& comp = view.get<Core::ScriptsComponent>(handle);
				for (auto& s : comp.scripts)
				{
					if (s.isEnabled && s.isStarted && s.instance)
					{
						s.instance->onFixedUpdate(dt);
						s.instance->onUpdate(dt);
						s.instance->onLateUpdate(dt);
					}
				}
			}
		}

		activePlayScene->updateTransforms();

		// Verify that play scene mutated significantly
		Core::SceneSerializer playSerializer(activePlayScene.get());
		std::string playSceneJson = playSerializer.serializeToString();
		CHECK(playSceneJson != initialSceneJson);

		auto playBox = activePlayScene->findEntityByName("DynamicBox");
		CHECK(playBox.getTransform().position.y < 10.0f); // Fell due to gravity

		auto playMover = activePlayScene->findEntityByName("ScriptedEntity");
		CHECK(playMover.getTransform().position.x > 0.0f); // Moved by script

		// 3. Stop Play Mode: teardown scripts and restore pristine edit scene
		for (auto handle : view)
		{
			auto& comp = view.get<Core::ScriptsComponent>(handle);
			for (auto& s : comp.scripts)
			{
				if (s.isStarted && s.instance)
				{
					s.instance->onDestroy();
					s.isStarted = false;
				}
			}
		}

		activePlayScene.reset();

		// Verify edit scene is 100% byte-identical to before Play Mode
		editScene.updateTransforms();
		std::string restoredEditJson = serializer.serializeToString();
		CHECK(restoredEditJson == initialSceneJson);
	}

	TEST_CASE("Asset Pipeline - GUID Preservation on Rename & Move")
	{
		std::filesystem::path testDir = "Temp_AssetTest";
		std::filesystem::create_directories(testDir);

		std::filesystem::path texFile = testDir / "test_texture.png";
		std::filesystem::path meshFile = testDir / "test_mesh.obj";

		{
			std::ofstream f(texFile);
			f << "FAKE_PNG_DATA";
		}
		{
			std::ofstream f(meshFile);
			f << "v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";
		}

		auto& assetMgr = Core::AssetManager::Get();
		assetMgr.init(testDir);

		CHECK(assetMgr.hasAsset(texFile));
		CHECK(assetMgr.hasAsset(meshFile));

		Core::AssetGuid texGuid = assetMgr.getGuidFromPath(texFile);
		Core::AssetGuid meshGuid = assetMgr.getGuidFromPath(meshFile);
		CHECK(texGuid.isValid());
		CHECK(meshGuid.isValid());
		CHECK(texGuid != meshGuid);

		// Check .meta files exist on disk
		CHECK(std::filesystem::exists(texFile.string() + ".meta"));
		CHECK(std::filesystem::exists(meshFile.string() + ".meta"));

		// 1. Rename asset: test_texture.png -> test_texture_renamed.png
		std::filesystem::path renamedTexFile = testDir / "test_texture_renamed.png";
		bool renameOk = assetMgr.renameAsset(texFile, renamedTexFile);
		CHECK(renameOk == true);

		// Verify on disk
		CHECK(!std::filesystem::exists(texFile));
		CHECK(!std::filesystem::exists(texFile.string() + ".meta"));
		CHECK(std::filesystem::exists(renamedTexFile));
		CHECK(std::filesystem::exists(renamedTexFile.string() + ".meta"));

		// Verify GUID lookup preserves reference
		CHECK(assetMgr.getGuidFromPath(renamedTexFile) == texGuid);
		CHECK(assetMgr.getPathFromGuid(texGuid) == renamedTexFile);

		// 2. Move asset: test_mesh.obj -> SubDir/test_mesh.obj
		std::filesystem::path subDir = testDir / "SubDir";
		std::filesystem::create_directories(subDir);

		bool moveOk = assetMgr.moveAsset(meshFile, subDir);
		CHECK(moveOk == true);

		std::filesystem::path movedMeshFile = subDir / "test_mesh.obj";
		CHECK(!std::filesystem::exists(meshFile));
		CHECK(!std::filesystem::exists(meshFile.string() + ".meta"));
		CHECK(std::filesystem::exists(movedMeshFile));
		CHECK(std::filesystem::exists(movedMeshFile.string() + ".meta"));

		// Verify GUID lookup preserves reference after move
		CHECK(assetMgr.getGuidFromPath(movedMeshFile) == meshGuid);
		CHECK(assetMgr.getPathFromGuid(meshGuid) == movedMeshFile);

		// Clean up
		assetMgr.deleteAsset(renamedTexFile);
		assetMgr.deleteAsset(movedMeshFile);
		assetMgr.shutdown();
		std::filesystem::remove_all(testDir);
	}

	TEST_CASE("Asset Pipeline - 50 Concurrent Async Asset Loads")
	{
		std::filesystem::path testDir = "Temp_ConcurrentAssets";
		std::filesystem::create_directories(testDir);

		auto& assetMgr = Core::AssetManager::Get();
		assetMgr.init(testDir);

		std::vector<Core::AssetGuid> guids;
		guids.reserve(50);

		// Generate 50 dummy asset files
		for (int i = 0; i < 50; ++i)
		{
			std::filesystem::path path = testDir / ("asset_" + std::to_string(i) + ".bin");
			{
				std::ofstream f(path, std::ios::binary);
				std::vector<char> dummyData(1024 * 8, static_cast<char>(i % 256));
				f.write(dummyData.data(), dummyData.size());
			}

			assetMgr.registerAsset(path);
			guids.push_back(assetMgr.getGuidFromPath(path));
		}

		REQUIRE(guids.size() == 50);

		// Dispatch 50 async loading tasks concurrently
		std::vector<std::future<bool>> futures;
		futures.reserve(50);

		for (int i = 0; i < 50; ++i)
		{
			futures.push_back(assetMgr.loadAssetAsync(guids[i]));
		}

		// Wait for all 50 concurrent loads to complete
		for (int i = 0; i < 50; ++i)
		{
			bool success = futures[i].get();
			CHECK(success == true);
		}

		// Verify all 50 assets are marked isLoaded
		for (int i = 0; i < 50; ++i)
		{
			const auto* meta = assetMgr.getMetadata(guids[i]);
			REQUIRE(meta != nullptr);
			CHECK(meta->isLoaded == true);
		}

		// Clean up
		assetMgr.shutdown();
		std::filesystem::remove_all(testDir);
	}

	TEST_CASE("Editor - Prefab Serialization and Instantiation")
	{
		std::filesystem::path prefabPath = "Temp_Entity.prefab";

		// 1. Author source entity hierarchy
		Core::Scene authorScene;
		auto root = authorScene.createEntity("PrefabRoot");
		root.getTransform().position = Core::Vector3(1.0f, 2.0f, 3.0f);
		auto& mesh = root.addComponent<Core::MeshRendererComponent>();
		mesh.meshPath = "/engine/Content/cube.obj";

		auto child = authorScene.createEntity("PrefabChild");
		child.getTransform().position = Core::Vector3(0.0f, 5.0f, 0.0f);
		auto& cam = child.addComponent<Core::CameraComponent>();
		cam.fov = 80.0f;
		child.setParent(root, false);

		authorScene.updateTransforms();

		// Save as Prefab
		{
			Core::SceneSerializer serializer(&authorScene);
			std::vector<Core::Entity> subtree = { root, child };
			std::string serialized = serializer.serializeEntities(subtree);

			std::ofstream pf(prefabPath);
			pf << serialized;
		}

		CHECK(std::filesystem::exists(prefabPath));

		// 2. Instantiate Prefab in a new scene
		Core::Scene targetScene;
		Core::SceneSerializer targetSerializer(&targetScene);

		std::string content;
		{
			std::ifstream pf(prefabPath);
			content = std::string((std::istreambuf_iterator<char>(pf)), std::istreambuf_iterator<char>());
		}

		Core::AssetGuid prefabGuid(987654321);
		auto instantiated = targetSerializer.deserializeEntities(content, true, Core::Vector3(10.0f, 0.0f, 0.0f));
		REQUIRE(instantiated.size() == 2);

		for (auto e : instantiated)
		{
			auto& pComp = e.addComponent<Core::PrefabInstanceComponent>();
			pComp.prefabGuid = prefabGuid.value();
			pComp.prefabPath = prefabPath.generic_string();
		}

		targetScene.updateTransforms();

		// Verify instantiated entities have remapped UUIDs distinct from authorScene
		auto instRoot = targetScene.findEntityByName("PrefabRoot");
		auto instChild = targetScene.findEntityByName("PrefabChild");

		CHECK(instRoot.isValid());
		CHECK(instChild.isValid());
		CHECK(instRoot.getUUID() != root.getUUID());
		CHECK(instChild.getUUID() != child.getUUID());

		// Verify components preserved
		CHECK(instRoot.hasComponent<Core::MeshRendererComponent>());
		CHECK(instRoot.getComponent<Core::MeshRendererComponent>().meshPath == "/engine/Content/cube.obj");
		CHECK(instChild.hasComponent<Core::CameraComponent>());
		CHECK(instChild.getComponent<Core::CameraComponent>().fov == doctest::Approx(80.0f));

		// Verify hierarchy preserved
		CHECK(instChild.getParent() == instRoot);

		// Verify PrefabInstanceComponent
		CHECK(instRoot.hasComponent<Core::PrefabInstanceComponent>());
		CHECK(instRoot.getComponent<Core::PrefabInstanceComponent>().prefabGuid == prefabGuid.value());

		// Clean up
		std::filesystem::remove(prefabPath);
	}
}
