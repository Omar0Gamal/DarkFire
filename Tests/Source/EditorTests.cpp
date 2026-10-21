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
#include <filesystem>

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
}
