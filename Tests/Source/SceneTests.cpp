#ifdef _MSC_VER
#pragma warning(disable: 5285)
#endif
#include "doctest/doctest.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Entity.h"
#include "Engine/Core/Scene/UUID.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Reflection/Reflection.h"
#include "Engine/Core/Serialization/SceneSerializer.h"
#include <unordered_set>

using namespace Core;

TEST_CASE("Scene::UUID - Generation and Hashing")
{
	SUBCASE("UUID Validity and Uniqueness")
	{
		std::unordered_set<uint64_t> uniqueUuids;
		for (int i = 0; i < 10000; ++i)
		{
			Core::UUID id;
			CHECK(id.isValid());
			CHECK(id.value() != 0);
			CHECK(uniqueUuids.insert(id.value()).second);
		}
	}

	SUBCASE("UUID Null and Equality")
	{
		Core::UUID nullId = Core::UUID::Null;
		CHECK_FALSE(nullId.isValid());
		CHECK(nullId.value() == 0);

		Core::UUID id1(12345);
		Core::UUID id2(12345);
		Core::UUID id3(67890);

		CHECK(id1 == id2);
		CHECK(id1 != id3);
		CHECK(id1.toString() == "0000000000003039");
	}
}

TEST_CASE("Scene::Entity - Creation and Component Management")
{
	Core::Scene scene;

	Entity e1 = scene.createEntity("Player");
	CHECK(e1.isValid());
	CHECK(e1.getName() == "Player");
	CHECK(e1.getUUID().isValid());

	CHECK(e1.hasComponent<Core::IdentityComponent>());
	CHECK(e1.hasComponent<Core::TransformComponent>());
	CHECK(e1.hasComponent<Core::HierarchyComponent>());
	CHECK(e1.hasComponent<Core::WorldTransform>());

	SUBCASE("Add and Remove Custom Components")
	{
		CHECK_FALSE(e1.hasComponent<Core::CameraComponent>());
		auto& cam = e1.addComponent<Core::CameraComponent>();
		cam.fov = 75.0f;

		CHECK(e1.hasComponent<Core::CameraComponent>());
		CHECK(e1.getComponent<Core::CameraComponent>().fov == 75.0f);

		e1.removeComponent<Core::CameraComponent>();
		CHECK_FALSE(e1.hasComponent<Core::CameraComponent>());
	}

	SUBCASE("Find Entity by UUID and Name")
	{
		Core::UUID uuid = e1.getUUID();
		Entity foundByUuid = scene.findEntityByUUID(uuid);
		CHECK(foundByUuid == e1);

		Entity foundByName = scene.findEntityByName("Player");
		CHECK(foundByName == e1);

		Entity notFound = scene.findEntityByName("NonExistent");
		CHECK_FALSE(notFound.isValid());
	}
}

TEST_CASE("Scene::Hierarchy - Parenting and Transform Propagation")
{
	Core::Scene scene;

	Entity parent = scene.createEntity("Parent");
	Entity child = scene.createEntity("Child");
	Entity grandChild = scene.createEntity("GrandChild");

	parent.getTransform().position = Core::Vector3(10.0f, 0.0f, 0.0f);
	child.getTransform().position = Core::Vector3(0.0f, 5.0f, 0.0f);
	grandChild.getTransform().position = Core::Vector3(0.0f, 0.0f, 2.0f);

	child.setParent(parent, false);
	grandChild.setParent(child, false);

	scene.updateTransforms();

	SUBCASE("Hierarchy Navigation")
	{
		CHECK(child.getParent() == parent);
		CHECK(grandChild.getParent() == child);
		CHECK_FALSE(parent.getParent().isValid());

		auto parentChildren = parent.getChildren();
		CHECK(parentChildren.size() == 1);
		CHECK(parentChildren[0] == child);

		auto childChildren = child.getChildren();
		CHECK(childChildren.size() == 1);
		CHECK(childChildren[0] == grandChild);
	}

	SUBCASE("World Transform Propagation")
	{
		const auto& pWorld = parent.getComponent<Core::WorldTransform>().worldMatrix;
		const auto& cWorld = child.getComponent<Core::WorldTransform>().worldMatrix;
		const auto& gcWorld = grandChild.getComponent<Core::WorldTransform>().worldMatrix;

		Core::Vector3 pPos = Core::Math::getTranslation(pWorld);
		Core::Vector3 cPos = Core::Math::getTranslation(cWorld);
		Core::Vector3 gcPos = Core::Math::getTranslation(gcWorld);

		CHECK(pPos.x == doctest::Approx(10.0f).epsilon(1e-4f));
		CHECK(pPos.y == doctest::Approx(0.0f).epsilon(1e-4f));
		CHECK(pPos.z == doctest::Approx(0.0f).epsilon(1e-4f));

		CHECK(cPos.x == doctest::Approx(10.0f).epsilon(1e-4f));
		CHECK(cPos.y == doctest::Approx(5.0f).epsilon(1e-4f));
		CHECK(cPos.z == doctest::Approx(0.0f).epsilon(1e-4f));

		CHECK(gcPos.x == doctest::Approx(10.0f).epsilon(1e-4f));
		CHECK(gcPos.y == doctest::Approx(5.0f).epsilon(1e-4f));
		CHECK(gcPos.z == doctest::Approx(2.0f).epsilon(1e-4f));
	}

	SUBCASE("Keep World Transform on Reparent")
	{
		// Unparent grandChild but preserve world position (10, 5, 2)
		grandChild.setParent(Core::Entity{}, true);
		scene.updateTransforms();

		const auto& gcWorld = grandChild.getComponent<Core::WorldTransform>().worldMatrix;
		Core::Vector3 gcPos = Core::Math::getTranslation(gcWorld);

		CHECK(gcPos.x == doctest::Approx(10.0f).epsilon(1e-4f));
		CHECK(gcPos.y == doctest::Approx(5.0f).epsilon(1e-4f));
		CHECK(gcPos.z == doctest::Approx(2.0f).epsilon(1e-4f));
	}
}

TEST_CASE("Scene::Serialization - Byte-Identical Round-Trip Test")
{
	Core::Scene scene1;

	// Build complex entity hierarchy with every registered authored component
	Entity parent = scene1.createEntity("RootParent");
	parent.getTransform().position = Core::Vector3(1.0f, 2.0f, 3.0f);
	parent.getTransform().setEulerDegrees(Core::Vector3(15.0f, 30.0f, 45.0f));
	parent.getTransform().scale = Core::Vector3(1.5f, 1.5f, 1.5f);

	auto& cam = parent.addComponent<Core::CameraComponent>();
	cam.fov = 90.0f;
	cam.nearZ = 0.05f;
	cam.farZ = 2000.0f;
	cam.isPrimary = true;

	auto& light = parent.addComponent<Core::LightComponent>();
	light.type = Core::LightType::Point;
	light.color = Core::Color(1.0f, 0.8f, 0.6f, 1.0f);
	light.intensity = 5.0f;
	light.range = 25.0f;

	Entity child = scene1.createEntity("ChildMesh");
	child.getTransform().position = Core::Vector3(0.0f, 10.0f, 0.0f);
	child.setParent(parent, false);

	auto& mesh = child.addComponent<Core::MeshRendererComponent>();
	mesh.meshPath = "Assets/Models/Kart.obj";
	mesh.materialPath = "Assets/Materials/KartRed.dfmat";
	mesh.castShadows = true;
	mesh.receiveShadows = true;

	auto& rb = child.addComponent<Core::RigidbodyComponent>();
	rb.mass = 250.0f;
	rb.drag = 0.1f;
	rb.useGravity = true;

	auto& col = child.addComponent<Core::ColliderComponent>();
	col.shape = Core::ColliderShape::Box;
	col.size = Core::Vector3(2.0f, 1.0f, 4.0f);

	auto& emitter = child.addComponent<Core::ParticleEmitterComponent>();
	emitter.maxParticles = 500;
	emitter.emissionRate = 30.0f;
	emitter.startColor = Core::Color(1.0f, 0.5f, 0.0f, 1.0f);

	// Add runtime-only states that should NOT be serialized
	child.addComponent<Core::RigidbodyRuntimeState>();
	child.getComponent<Core::RigidbodyRuntimeState>().linearVelocity = Core::Vector3(100.0f, 0.0f, 50.0f);
	child.addComponent<Core::ParticleRuntimeState>();
	child.getComponent<Core::ParticleRuntimeState>().activeCount = 42;

	scene1.updateTransforms();

	// 1. Serialize scene1 to JSON string
	Core::SceneSerializer serializer1(&scene1);
	std::string json1 = serializer1.serializeToString();
	CHECK_FALSE(json1.empty());

	// Verify runtime state is NOT present in serialized JSON
	CHECK(json1.find("RigidbodyRuntimeState") == std::string::npos);
	CHECK(json1.find("ParticleRuntimeState") == std::string::npos);

	// 2. Deserialize into scene2
	Core::Scene scene2;
	Core::SceneSerializer serializer2(&scene2);
	bool success = serializer2.deserializeFromString(json1);
	CHECK(success);
	CHECK(scene2.getEntityCount() == 2);

	// 3. Serialize scene2 back to JSON string
	std::string json2 = serializer2.serializeToString();

	// Exit Criteria 2: Byte-identical round-trip
	CHECK(json1 == json2);

	// Verify scene2 hierarchy and components
	Entity deserializedParent = scene2.findEntityByName("RootParent");
	Entity deserializedChild = scene2.findEntityByName("ChildMesh");
	CHECK(deserializedParent.isValid());
	CHECK(deserializedChild.isValid());
	CHECK(deserializedChild.getParent() == deserializedParent);
	CHECK(deserializedChild.getComponent<Core::RigidbodyComponent>().mass == 250.0f);
	CHECK(deserializedChild.getComponent<Core::MeshRendererComponent>().meshPath == "Assets/Models/Kart.obj");
}

TEST_CASE("Scene::DeepCopy - Byte-Identical and Zero Runtime State Leakage")
{
	Core::Scene scene1;

	Entity parent = scene1.createEntity("Root");
	parent.getTransform().position = Core::Vector3(5.0f, 10.0f, 15.0f);

	Entity child = scene1.createEntity("SubChild");
	child.getTransform().position = Core::Vector3(0.0f, 2.0f, 0.0f);
	child.setParent(parent, false);

	auto& light = child.addComponent<Core::LightComponent>();
	light.intensity = 3.5f;

	auto& rb = child.addComponent<Core::RigidbodyComponent>();
	rb.mass = 50.0f;

	// Add runtime state to scene1
	child.addComponent<Core::RigidbodyRuntimeState>();
	child.getComponent<Core::RigidbodyRuntimeState>().linearVelocity = Core::Vector3(999.0f, 0.0f, 0.0f);

	scene1.updateTransforms();

	// Deep copy scene1 into copiedScene
	std::unique_ptr<Core::Scene> copiedScene = Core::Scene::copy(scene1);
	CHECK(copiedScene != nullptr);
	CHECK(copiedScene->getEntityCount() == scene1.getEntityCount());

	// Verify serialized representation of copied scene matches source scene exactly
	Core::SceneSerializer serializer1(&scene1);
	std::string json1 = serializer1.serializeToString();

	Core::SceneSerializer serializerCopy(copiedScene.get());
	std::string jsonCopy = serializerCopy.serializeToString();

	// Exit Criteria 3: Byte-identical serialized JSON
	CHECK(json1 == jsonCopy);

	// Verify zero runtime state leakage
	Entity copiedChild = copiedScene->findEntityByName("SubChild");
	CHECK(copiedChild.isValid());
	CHECK_FALSE(copiedChild.hasComponent<Core::RigidbodyRuntimeState>());

	// Verify independence: mutating copied scene does not mutate source scene
	copiedChild.getComponent<Core::RigidbodyComponent>().mass = 1234.0f;
	CHECK(child.getComponent<Core::RigidbodyComponent>().mass == 50.0f);
	CHECK(copiedChild.getComponent<Core::RigidbodyComponent>().mass == 1234.0f);
}
