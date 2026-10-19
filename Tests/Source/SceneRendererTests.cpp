#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Entity.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Scene/SceneRendererBridge.h"
#include "Engine/RHI/Null/NullRHI.h"
#include "Engine/Renderer/RenderScene.h"
#include "Engine/Renderer/SceneRenderer.h"

#include <ostream>
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
}
