#include <doctest/doctest.h>
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Scene/SceneRendererBridge.h"
#include "Engine/Physics/BVH.h"
#include "Engine/Physics/PhysicsWorld.h"
#include "Engine/Physics/PhysicsCollisionNarrowphase.h"

using namespace Core;
using namespace Physics;

TEST_CASE("Milestone 3.1: Decoupled 60 Hz Physics & Sub-Frame Transform Interpolation")
{
	// 1. Verify TransformComponent Interpolation
	TransformComponent tc(Vector3(0.0f, 10.0f, 0.0f), Quaternion(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1.0f, 1.0f, 1.0f));
	tc.syncPreviousTransform(); // prevPosition = (0, 10, 0)

	// Simulate physics step advancing entity to y = 20
	tc.position = Vector3(0.0f, 20.0f, 0.0f);

	Matrix m0 = tc.getInterpolatedMatrix(0.0f);
	CHECK(Math::getTranslation(m0).y == doctest::Approx(10.0f));

	Matrix mHalf = tc.getInterpolatedMatrix(0.5f);
	CHECK(Math::getTranslation(mHalf).y == doctest::Approx(15.0f));

	Matrix m1 = tc.getInterpolatedMatrix(1.0f);
	CHECK(Math::getTranslation(m1).y == doctest::Approx(20.0f));

	// 2. Verify SceneRendererBridge with alpha and explicit interpolation policy
	Scene scene;
	auto camEntity = scene.createEntity("MainCamera");
	auto& cam = camEntity.addComponent<CameraComponent>();
	cam.isPrimary = true;
	auto& camTrans = camEntity.getComponent<TransformComponent>();
	camTrans.position = Vector3(0.0f, 5.0f, -10.0f);
	camTrans.syncPreviousTransform();
	camTrans.position = Vector3(0.0f, 5.0f, 0.0f); // Variable freelook moved camera forward
	scene.updateTransforms();

	auto meshEntity = scene.createEntity("MovingMesh");
	auto& meshComp = meshEntity.addComponent<MeshRendererComponent>();
	meshComp.meshPath = "Mesh.obj";
	auto& meshTrans = meshEntity.getComponent<TransformComponent>();
	meshTrans.interpolate = true; // Explicit simulation tick ownership
	meshTrans.position = Vector3(0.0f, 10.0f, 0.0f);
	meshTrans.syncPreviousTransform();
	meshTrans.position = Vector3(0.0f, 30.0f, 0.0f); // Physics moved it to 30
	scene.updateTransforms();

	Renderer::RenderScene renderScene;
	ExtractRenderScene(scene, renderScene, 16.0f / 9.0f, 0.5f);

	// By default, camera does NOT interpolate against fixed step (no rubber-banding)
	CHECK(renderScene.camera.position.z == doctest::Approx(0.0f));

	REQUIRE(renderScene.meshes.size() == 1);
	Vector3 extractedPos = Math::getTranslation(renderScene.meshes[0].world);
	CHECK(extractedPos.y == doctest::Approx(20.0f)); // Lerp(10, 30, 0.5) = 20

	// If explicit camera interpolation is enabled:
	cam.interpolate = true;
	ExtractRenderScene(scene, renderScene, 16.0f / 9.0f, 0.5f);
	CHECK(renderScene.camera.position.z == doctest::Approx(-5.0f)); // Lerp(-10, 0, 0.5) = -5
}

TEST_CASE("Milestone 3.2: Arbitrary TriangleMesh Collider & BVH Tree Construction")
{
	// Create a pyramid/ramp mesh: 4 vertices, 4 triangles
	std::vector<Vector3> vertices = {
		Vector3(-2.0f, 0.0f, -2.0f), // 0
		Vector3( 2.0f, 0.0f, -2.0f), // 1
		Vector3( 2.0f, 0.0f,  2.0f), // 2
		Vector3(-2.0f, 0.0f,  2.0f), // 3
		Vector3( 0.0f, 3.0f,  0.0f)  // 4 Apex
	};

	std::vector<uint32_t> indices = {
		// Base (2 triangles)
		0, 2, 1,
		0, 3, 2,
		// Sides (4 triangles)
		0, 1, 4,
		1, 2, 4,
		2, 3, 4,
		3, 0, 4
	};

	BVH bvh;
	bvh.build(vertices, indices);

	CHECK(bvh.getTriangles().size() == 6);
	CHECK(bvh.getNodes().size() >= 1);
	CHECK(bvh.getBoundsMin().y == doctest::Approx(0.0f));
	CHECK(bvh.getBoundsMax().y == doctest::Approx(3.0f));

	// Raycast straight down at apex
	float outT = 0.0f;
	Vector3 outNormal;
	uint32_t outTri = 0;
	bool hit = bvh.raycast(Vector3(0.0f, 10.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f), 20.0f, outT, outNormal, outTri);
	CHECK(hit == true);
	CHECK(outT == doctest::Approx(7.0f)); // 10.0 - 3.0 = 7.0

	// Raycast missing the mesh
	bool miss = bvh.raycast(Vector3(10.0f, 10.0f, 10.0f), Vector3(0.0f, 1.0f, 0.0f), 50.0f, outT, outNormal, outTri);
	CHECK(miss == false);

	// AABB query over apex
	std::vector<uint32_t> queryTris;
	bvh.queryAABB(Vector3(-0.5f, 2.5f, -0.5f), Vector3(0.5f, 3.5f, 0.5f), queryTris);
	CHECK(queryTris.size() >= 1);
}

TEST_CASE("Milestone 3.2: Sphere vs TriangleMesh Narrowphase Contact Resolution")
{
	// Create horizontal ground triangle at y = 0
	std::vector<Vector3> vertices = {
		Vector3(-5.0f, 0.0f, -5.0f),
		Vector3( 5.0f, 0.0f, -5.0f),
		Vector3( 0.0f, 0.0f,  5.0f)
	};
	std::vector<uint32_t> indices = { 0, 1, 2 };

	auto colMesh = std::make_shared<CollisionMesh>();
	colMesh->vertices = vertices;
	colMesh->indices = indices;
	colMesh->bvh = std::make_shared<BVH>();
	colMesh->bvh->build(vertices, indices);
	colMesh->boundsMin = colMesh->bvh->getBoundsMin();
	colMesh->boundsMax = colMesh->bvh->getBoundsMax();

	ColliderComponent meshCol;
	meshCol.shape = ColliderShape::TriangleMesh;
	meshCol.collisionMesh = colMesh;
	TransformComponent meshTrans(Vector3(0.0f, 0.0f, 0.0f), Quaternion(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1.0f, 1.0f, 1.0f));

	ColliderComponent sphereCol;
	sphereCol.shape = ColliderShape::Sphere;
	sphereCol.radius = 1.0f;
	// Position sphere at y = 0.6 (penetrating ground by 0.4)
	TransformComponent sphereTrans(Vector3(0.0f, 0.6f, 0.0f), Quaternion(0.0f, 0.0f, 0.0f, 1.0f), Vector3(1.0f, 1.0f, 1.0f));

	Vector3 outNormal;
	float outPenetration = 0.0f;
	bool collided = TestColliderPair(sphereCol, sphereTrans, meshCol, meshTrans, outNormal, outPenetration);

	CHECK(collided == true);
	CHECK(outPenetration == doctest::Approx(0.4f).epsilon(0.01f));
	CHECK(outNormal.y == doctest::Approx(1.0f));

	// Raycast against triangle mesh collider
	float rayT = 0.0f;
	Vector3 rayNorm;
	bool rayHit = RaycastCollider(meshCol, meshTrans, Vector3(0.0f, 5.0f, 0.0f), Vector3(0.0f, -1.0f, 0.0f), 10.0f, rayT, rayNorm);
	CHECK(rayHit == true);
	CHECK(rayT == doctest::Approx(5.0f));
}

TEST_CASE("Milestone 3.3: Directional Sweep Queries (sphereCast and boxCast)")
{
	Scene scene;
	PhysicsWorld physics;
	physics.init(Vector3(0.0f, -9.81f, 0.0f), 1.0f / 60.0f);

	// 1. Box obstacle at (0, 0, 10), size 2x2x2
	auto boxEntity = scene.createEntity("ObstacleBox");
	auto& boxCol = boxEntity.addComponent<ColliderComponent>();
	boxCol.shape = ColliderShape::Box;
	boxCol.size = Vector3(2.0f, 2.0f, 2.0f);
	auto& boxTrans = boxEntity.getComponent<TransformComponent>();
	boxTrans.position = Vector3(0.0f, 0.0f, 10.0f);

	// 2. Sphere obstacle at (10, 0, 0), radius 1.0
	auto sphereEntity = scene.createEntity("ObstacleSphere");
	auto& sphCol = sphereEntity.addComponent<ColliderComponent>();
	sphCol.shape = ColliderShape::Sphere;
	sphCol.radius = 1.0f;
	auto& sphTrans = sphereEntity.getComponent<TransformComponent>();
	sphTrans.position = Vector3(10.0f, 0.0f, 0.0f);

	scene.updateTransforms();

	// Test sphereCast against sphere obstacle
	// Swept sphere radius = 0.5, origin = (0, 0, 0), target sphere at (10, 0, 0) radius 1.0
	// Expected hit at distance = 10.0 - 1.0 - 0.5 = 8.5
	SweepHit hitSph;
	bool hasHitSph = physics.sphereCast(scene, Vector3(0.0f, 0.0f, 0.0f), 0.5f, Vector3(1.0f, 0.0f, 0.0f), 20.0f, hitSph);
	CHECK(hasHitSph == true);
	CHECK(hitSph.distance == doctest::Approx(8.5f));
	CHECK(hitSph.normal.x == doctest::Approx(-1.0f));

	// Test sphereCast against box obstacle
	// Swept sphere radius = 0.5, origin = (0, 0, 0), box at z = 10.0 with size 2 (extents 1.0)
	// Box front face is at z = 9.0. Swept sphere hits around z = 8.5
	SweepHit hitBox;
	bool hasHitBox = physics.sphereCast(scene, Vector3(0.0f, 0.0f, 0.0f), 0.5f, Vector3(0.0f, 0.0f, 1.0f), 20.0f, hitBox);
	CHECK(hasHitBox == true);
	CHECK(hitBox.distance > 8.0f);
	CHECK(hitBox.distance < 9.0f);
	CHECK(hitBox.normal.z < -0.9f);

	// Test boxCast against box obstacle
	SweepHit hitBoxCast;
	bool hasHitBoxCast = physics.boxCast(
		scene,
		Vector3(0.0f, 0.0f, 0.0f),
		Vector3(0.5f, 0.5f, 0.5f),
		Quaternion(0.0f, 0.0f, 0.0f, 1.0f),
		Vector3(0.0f, 0.0f, 1.0f),
		20.0f,
		hitBoxCast
	);
	CHECK(hasHitBoxCast == true);
	CHECK(hitBoxCast.distance > 8.0f);
	CHECK(hitBoxCast.distance < 9.0f);
	CHECK(hitBoxCast.normal.z < -0.9f);

	// Test sweep in empty direction
	SweepHit hitMiss;
	bool hasHitMiss = physics.sphereCast(scene, Vector3(0.0f, 0.0f, 0.0f), 0.5f, Vector3(0.0f, 1.0f, 0.0f), 20.0f, hitMiss);
	CHECK(hasHitMiss == false);
}
