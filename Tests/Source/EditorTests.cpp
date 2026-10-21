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
#include <filesystem>
#include <random>

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
}
