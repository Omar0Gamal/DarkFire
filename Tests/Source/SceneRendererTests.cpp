#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Entity.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Scene/SceneRendererBridge.h"
#include "Engine/RHI/Null/NullRHI.h"
#include "Engine/Renderer/RenderScene.h"
#include "Engine/Renderer/SceneRenderer.h"
#include "Engine/Core/Scene/Systems/ParticleSystem.h"

#include <ostream>
#include <fstream>
#include <filesystem>
#ifdef _MSC_VER
#pragma warning(disable: 5285)
#endif
#include "doctest/doctest.h"

using namespace Core;
using namespace Core::RHI;
using namespace Core::Renderer;

TEST_SUITE("Milestone P4: Frame Pipeline, Color Space & SceneRenderer")
{
	TEST_CASE("P4.1: RenderScene Snapshot Extraction & ECS Decoupling")
	{
		Scene scene;

		// 1. Create Camera Entity
		auto camEntity = scene.createEntity("MainCamera");
		auto& camComp = camEntity.addComponent<CameraComponent>();
		camComp.isPrimary = true;
		camComp.fov = 60.0f;
		camComp.nearZ = 0.5f;
		camComp.farZ = 500.0f;

		auto& camTrans = camEntity.getComponent<TransformComponent>();
		camTrans.position = Vector3(0.0f, 2.0f, -10.0f);

		// 2. Create Mesh Entities
		auto mesh1 = scene.createEntity("Cube1");
		mesh1.addComponent<MeshRendererComponent>();
		mesh1.getComponent<TransformComponent>().position = Vector3(-2.0f, 0.0f, 0.0f);

		auto mesh2 = scene.createEntity("Cube2");
		mesh2.addComponent<MeshRendererComponent>();
		mesh2.getComponent<TransformComponent>().position = Vector3(2.0f, 0.0f, 5.0f);

		// 3. Create Light Entity
		auto lightEntity = scene.createEntity("SunLight");
		auto& lightComp = lightEntity.addComponent<LightComponent>();
		lightComp.type = LightType::Directional;
		lightComp.intensity = 2.5f;
		lightEntity.getComponent<TransformComponent>().position = Vector3(0.0f, 10.0f, 0.0f);

		// Update scene hierarchy transforms
		scene.updateTransforms();

		// Extract flat snapshot
		RenderScene renderScene;
		ExtractRenderScene(scene, renderScene);

		// Verify extracted snapshot data
		CHECK(renderScene.camera.nearZ == doctest::Approx(0.5f));
		CHECK(renderScene.camera.farZ == doctest::Approx(500.0f));
		CHECK(renderScene.camera.position.x == doctest::Approx(0.0f));
		CHECK(renderScene.camera.position.y == doctest::Approx(2.0f));
		CHECK(renderScene.camera.position.z == doctest::Approx(-10.0f));

		CHECK(renderScene.meshes.size() == 2);
		CHECK(renderScene.lights.size() == 1);
		CHECK(renderScene.lights[0].intensity == doctest::Approx(2.5f));
	}

	TEST_CASE("P4.2: Reversed-Z Ray Unprojection & Entity Picking Math")
	{
		const float width = 1920.0f;
		const float height = 1080.0f;
		const float aspect = width / height;
		const float nearZ = 0.1f;
		const float farZ = 1000.0f;

		const Vector3 eye(0.0f, 0.0f, -10.0f);
		const Vector3 target(0.0f, 0.0f, 0.0f);
		const Vector3 up(0.0f, 1.0f, 0.0f);

		const Matrix view = Math::lookAtLH(eye, target, up);
		const Matrix proj = Math::perspectiveFovLH_ReversedZ(Math::toRadians(60.0f), aspect, nearZ, farZ);
		const Matrix viewProj = view * proj;
		const Matrix invViewProj = viewProj.Invert();

		// Center of screen: (960, 540)
		auto rayCenter = SceneRenderer::unprojectRayReversedZ(
			width * 0.5f,
			height * 0.5f,
			width,
			height,
			invViewProj);

		// In Left-Handed space looking forward (+Z), the ray must point along +Z
		CHECK(rayCenter.direction.x == doctest::Approx(0.0f).epsilon(0.01f));
		CHECK(rayCenter.direction.y == doctest::Approx(0.0f).epsilon(0.01f));
		CHECK(rayCenter.direction.z == doctest::Approx(1.0f).epsilon(0.01f));

		// Ray origin must be near eye position at nearZ
		CHECK(rayCenter.position.x == doctest::Approx(eye.x).epsilon(0.01f));
		CHECK(rayCenter.position.y == doctest::Approx(eye.y).epsilon(0.01f));
		CHECK(rayCenter.position.z == doctest::Approx(eye.z + nearZ).epsilon(0.02f));

		// Intersection with target at (0, 0, 0)
		BoundingSphere sphere(Vector3(0.0f, 0.0f, 0.0f), 1.0f);
		float dist = 0.0f;
		CHECK(rayCenter.Intersects(sphere, dist));
		CHECK(dist == doctest::Approx(10.0f - nearZ - 1.0f).epsilon(0.05f));
	}

	TEST_CASE("P4.3: SceneRenderer Dual Output (Backbuffer vs Offscreen)")
	{
		NullRHIDevice device;
		SceneRenderer renderer(device);

		RenderScene scene;
		scene.camera.position = Vector3(0.0f, 0.0f, -5.0f);
		scene.camera.viewMatrix = Math::lookAtLH(scene.camera.position, Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f));
		scene.camera.projMatrix = Math::perspectiveFovLH_ReversedZ(Math::toRadians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
		scene.camera.viewProjMatrix = scene.camera.viewMatrix * scene.camera.projMatrix;

		// Add 3 test meshes
		for (int i = 0; i < 3; ++i)
		{
			RenderMeshInstance mesh{};
			mesh.world = Matrix::CreateTranslation(static_cast<float>(i * 2), 0.0f, 0.0f);
			mesh.entityId = static_cast<uint32_t>(i + 1);
			mesh.layer = RenderLayer::Opaque;
			mesh.indexCount = 36;
			mesh.startIndex = 0;
			mesh.baseVertex = 0;
			scene.meshes.push_back(mesh);
		}

		// 1. Render to Offscreen Texture (e.g. Editor Viewport)
		GpuTextureDesc offscreenDesc{};
		offscreenDesc.width = 1280;
		offscreenDesc.height = 720;
		offscreenDesc.format = PixelFormat::R8G8B8A8_UNORM;
		offscreenDesc.bindFlags = BufferBindFlags::RenderTarget | BufferBindFlags::ShaderResource;
		auto offscreenRes = device.createTexture(offscreenDesc);
		REQUIRE(offscreenRes.isOk());

		RenderView editorView{};
		editorView.width = 1280;
		editorView.height = 720;
		editorView.targetTexture = *offscreenRes;
		editorView.isEditorView = true;

		renderer.render(scene, editorView);
		const uint32_t editorDrawCalls = renderer.getDrawCallCount();
		const uint32_t editorBatches = renderer.getBatchCount();
		CHECK(editorDrawCalls > 0);
		CHECK(editorBatches > 0);

		auto* nullOffscreen = dynamic_cast<NullTexture*>(offscreenRes->get());
		REQUIRE(nullOffscreen != nullptr);
		CHECK(nullOffscreen->isCleared());

		// 2. Render to SwapChain (e.g. Runtime Player)
		auto swapChainRes = device.createSwapChain(reinterpret_cast<void*>(0x5678), 1280, 720);
		REQUIRE(swapChainRes.isOk());
		auto swapChain = std::move(*swapChainRes);

		RenderView playerView{};
		playerView.width = 1280;
		playerView.height = 720;
		playerView.swapChain = std::move(swapChain);
		playerView.isEditorView = false;

		renderer.render(scene, playerView);
		const uint32_t playerDrawCalls = renderer.getDrawCallCount();
		const uint32_t playerBatches = renderer.getBatchCount();

		// Both paths must execute identically
		CHECK(playerDrawCalls == editorDrawCalls);
		CHECK(playerBatches == editorBatches);
	}

	TEST_CASE("P4.4: Dynamic Camera Aspect Ratio & Material Properties Parsing")
	{
		Scene scene;
		auto camEntity = scene.createEntity("DynamicCam");
		auto& camComp = camEntity.addComponent<CameraComponent>();
		camComp.isPrimary = true;
		camComp.fov = 45.0f;
		scene.updateTransforms();

		// Extract with ultrawide 21:9 aspect (~2.333)
		RenderScene renderScene;
		ExtractRenderScene(scene, renderScene, 21.0f / 9.0f);
		CHECK(renderScene.camera.aspect == doctest::Approx(21.0f / 9.0f));

		// Test Material JSON parsing with render states
		NullRHIDevice device;
		SceneRenderer renderer(device);

		std::filesystem::create_directories("Temp_MatTest");
		std::string matPath = "Temp_MatTest/CustomMaterial.mat";
		{
			std::ofstream f(matPath);
			f << "{\n"
			  << "  \"name\": \"TestMat\",\n"
			  << "  \"baseColor\": [0.2, 0.4, 0.6, 1.0],\n"
			  << "  \"roughness\": 0.25,\n"
			  << "  \"metallic\": 0.75,\n"
			  << "  \"emissive\": [0.1, 0.2, 0.3, 0.0],\n"
			  << "  \"tiling\": [2.0, 3.0],\n"
			  << "  \"blendMode\": 0,\n"
			  << "  \"depthMode\": 0,\n"
			  << "  \"cullMode\": 2\n"
			  << "}\n";
		}

		auto mat = renderer.getOrCreateMaterial(matPath);
		REQUIRE(mat != nullptr);
		CHECK(mat->getName() == "TestMat");
		CHECK(mat->getBaseColor().x == doctest::Approx(0.2f));
		CHECK(mat->getBaseColor().y == doctest::Approx(0.4f));
		CHECK(mat->getBaseColor().z == doctest::Approx(0.6f));
		CHECK(mat->getRoughness() == doctest::Approx(0.25f));
		CHECK(mat->getMetallic() == doctest::Approx(0.75f));
		CHECK(mat->getTilingOffset().x == doctest::Approx(2.0f));
		CHECK(mat->getTilingOffset().y == doctest::Approx(3.0f));
		CHECK(mat->getBlendMode() == BlendMode::Opaque);
		CHECK(mat->getCullMode() == RHI::CullMode::Back);

		std::filesystem::remove_all("Temp_MatTest");
	}

	TEST_CASE("P4.5: OBJ Vertex Welding & Lengyel Tangent Calculation")
	{
		std::filesystem::create_directories("Temp_ObjTest");
		std::string objPath = "Temp_ObjTest/WeldedCube.obj";
		{
			std::ofstream f(objPath);
			// 8 positions
			f << "v -1.0 -1.0  1.0\n"
			  << "v  1.0 -1.0  1.0\n"
			  << "v  1.0  1.0  1.0\n"
			  << "v -1.0  1.0  1.0\n"
			  << "v  1.0 -1.0 -1.0\n"
			  << "v -1.0 -1.0 -1.0\n"
			  << "v -1.0  1.0 -1.0\n"
			  << "v  1.0  1.0 -1.0\n"
			  // 6 normals
			  << "vn  0.0  0.0  1.0\n"
			  << "vn  0.0  0.0 -1.0\n"
			  << "vn  0.0  1.0  0.0\n"
			  << "vn  0.0 -1.0  0.0\n"
			  << "vn  1.0  0.0  0.0\n"
			  << "vn -1.0  0.0  0.0\n"
			  // 4 UVs
			  << "vt 0.0 0.0\n"
			  << "vt 1.0 0.0\n"
			  << "vt 1.0 1.0\n"
			  << "vt 0.0 1.0\n"
			  // 6 faces (12 triangles, 36 indices)
			  << "f 1/1/1 2/2/1 3/3/1\n f 1/1/1 3/3/1 4/4/1\n"
			  << "f 5/1/2 6/2/2 7/3/2\n f 5/1/2 7/3/2 8/4/2\n"
			  << "f 4/1/3 3/2/3 8/3/3\n f 4/1/3 8/3/3 7/4/3\n"
			  << "f 6/1/4 5/2/4 2/3/4\n f 6/1/4 2/3/4 1/4/4\n"
			  << "f 2/1/5 5/2/5 8/3/5\n f 2/1/5 8/3/5 3/4/5\n"
			  << "f 6/1/6 1/2/6 4/3/6\n f 6/1/6 4/3/6 7/4/6\n";
		}

		auto mesh = Core::Renderer::LoadObjMesh(objPath);
		// Cube has 6 faces * 4 vertices = 24 welded vertices (down from 36 without welding)
		CHECK(mesh.vertices.size() == 24);
		CHECK(mesh.indices.size() == 36);

		for (const auto& v : mesh.vertices)
		{
			// Normal must be unit length
			CHECK(v.normal.Length() == doctest::Approx(1.0f).epsilon(0.01f));

			// Tangent XYZ must be unit length
			Vector3 tan(v.tangent.x, v.tangent.y, v.tangent.z);
			CHECK(tan.Length() == doctest::Approx(1.0f).epsilon(0.01f));

			// Tangent must be orthonormal to Normal (|dot| < 1e-4)
			CHECK(std::abs(v.normal.Dot(tan)) < 0.001f);

			// Handedness must be +1.0 or -1.0
			CHECK(std::abs(v.tangent.w) == doctest::Approx(1.0f));
		}

		std::filesystem::remove_all("Temp_ObjTest");
	}

	TEST_CASE("P4.6: CPU Particle System Simulation & Recycling")
	{
		Scene scene;
		auto entity = scene.createEntity("ParticleFX");
		auto& emitter = entity.addComponent<ParticleEmitterComponent>();
		emitter.maxParticles = 100;
		emitter.emissionRate = 50.0f;
		emitter.lifetime = 0.5f;
		emitter.startSize = 1.0f;
		emitter.endSize = 0.1f;
		emitter.startVelocity = Vector3(0.0f, 10.0f, 0.0f);
		emitter.gravityScale = 1.0f;

		scene.updateTransforms();

		// Tick 0.1s -> should spawn 5 particles (50 * 0.1) at age 0
		Core::Systems::ParticleSystem::Tick(scene, 0.1f);
		auto& state = entity.getComponent<ParticleRuntimeState>();
		CHECK(state.activeCount == 5);

		for (int i = 0; i < state.activeCount; ++i)
		{
			const auto& p = state.particles[i];
			CHECK(p.age == doctest::Approx(0.0f));
			CHECK(p.size == doctest::Approx(1.0f));
		}

		// Tick another 0.1s -> existing particles advance age by 0.1s, 5 more spawn (total 10)
		Core::Systems::ParticleSystem::Tick(scene, 0.1f);
		CHECK(state.activeCount == 10);

		for (int i = 0; i < 5; ++i)
		{
			const auto& p = state.particles[i];
			CHECK(p.age == doctest::Approx(0.1f));
			CHECK(p.size < 1.0f);
			CHECK(p.size >= 0.1f);
			CHECK(p.velocity.y < 10.0f);
			CHECK(p.velocity.y > 5.0f);
		}

		// Tick 0.6s -> original particles should expire and new ones spawn
		Core::Systems::ParticleSystem::Tick(scene, 0.6f);
		CHECK(state.activeCount > 0);
		CHECK(state.activeCount <= emitter.maxParticles);

		// Reset -> should clear all active particles
		Core::Systems::ParticleSystem::Reset(scene);
		CHECK(state.activeCount == 0);
	}
}
