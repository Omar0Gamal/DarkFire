#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Entity.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Scene/SceneRendererBridge.h"
#include "Engine/RHI/Null/NullRHI.h"
#include "Engine/Renderer/RenderScene.h"
#include "Engine/Renderer/SceneRenderer.h"
#include "Engine/Renderer/RenderPipeline.h"
#include "Engine/Shaders/Shared/Bindings.h"

#ifdef _MSC_VER
#pragma warning(disable: 5285)
#endif
#include "doctest/doctest.h"

using namespace Core;
using namespace Core::RHI;
using namespace Core::Renderer;

TEST_SUITE("Phase 3 & Phase 4: G-Buffer, Deferred PBR, Environment IBL, Forward+, Tessellation & Fog")
{
	TEST_CASE("Phase 3: G-Buffer Targets, Pre-Convolved Environment IBL Texture & Resize")
	{
		NullRHIDevice device;
		SceneRenderer renderer(device);

		// 1. Verify G-Buffer Render Targets (initial default 1280x720)
		auto nrTex = renderer.getNormalRoughnessTexture();
		REQUIRE(nrTex != nullptr);
		CHECK(nrTex->getDesc().format == PixelFormat::R16G16B16A16_FLOAT);
		CHECK(nrTex->getDesc().width == 1280);
		CHECK(nrTex->getDesc().height == 720);

		auto amTex = renderer.getAlbedoMetallicTexture();
		REQUIRE(amTex != nullptr);
		CHECK(amTex->getDesc().format == PixelFormat::R8G8B8A8_UNORM);
		CHECK(amTex->getDesc().width == 1280);
		CHECK(amTex->getDesc().height == 720);

		auto emAoTex = renderer.getGBufferEmissiveAOTexture();
		REQUIRE(emAoTex != nullptr);
		CHECK(emAoTex->getDesc().format == PixelFormat::R16G16B16A16_FLOAT);
		CHECK(emAoTex->getDesc().width == 1280);
		CHECK(emAoTex->getDesc().height == 720);

		// 2. Verify Resize updates all G-Buffer Render Targets
		renderer.resize(1920, 1080);
		nrTex = renderer.getNormalRoughnessTexture();
		REQUIRE(nrTex != nullptr);
		CHECK(nrTex->getDesc().width == 1920);
		CHECK(nrTex->getDesc().height == 1080);

		amTex = renderer.getAlbedoMetallicTexture();
		REQUIRE(amTex != nullptr);
		CHECK(amTex->getDesc().width == 1920);
		CHECK(amTex->getDesc().height == 1080);

		emAoTex = renderer.getGBufferEmissiveAOTexture();
		REQUIRE(emAoTex != nullptr);
		CHECK(emAoTex->getDesc().width == 1920);
		CHECK(emAoTex->getDesc().height == 1080);

		// Restore baseline resolution
		renderer.resize(1280, 720);

		// 3. Render a scene to generate and verify Pre-Convolved Environment IBL Texture
		RenderScene scene;
		scene.camera.position = Math::Vector3(0.0f, 2.0f, -5.0f);
		scene.camera.viewMatrix = Math::lookAtLH(scene.camera.position, Math::Vector3(0.0f, 0.0f, 0.0f), Math::Vector3(0.0f, 1.0f, 0.0f));
		scene.camera.projMatrix = Math::perspectiveFovLH_ReversedZ(Math::toRadians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
		scene.camera.viewProjMatrix = scene.camera.viewMatrix * scene.camera.projMatrix;

		scene.environment.skyColor = Math::Vector4(0.2f, 0.4f, 0.8f, 1.0f);
		scene.environment.horizonColor = Math::Vector4(0.5f, 0.7f, 0.9f, 1.0f);
		scene.environment.groundColor = Math::Vector4(0.1f, 0.1f, 0.1f, 1.0f);

		RenderLight dirLight{};
		dirLight.type = 0; // Directional
		dirLight.direction = Math::Vector3(0.577f, -0.577f, 0.577f);
		dirLight.color = Math::Vector3(1.0f, 0.95f, 0.85f);
		dirLight.intensity = 1.0f;
		scene.lights.push_back(dirLight);

		GpuTextureDesc offscreenDesc{};
		offscreenDesc.width = 1280;
		offscreenDesc.height = 720;
		offscreenDesc.format = PixelFormat::R8G8B8A8_UNORM;
		offscreenDesc.bindFlags = BufferBindFlags::RenderTarget | BufferBindFlags::ShaderResource;
		auto offscreenRes = device.createTexture(offscreenDesc);
		REQUIRE(offscreenRes.isOk());

		RenderView view{};
		view.width = 1280;
		view.height = 720;
		view.targetTexture = *offscreenRes;

		renderer.render(scene, view);

		auto iblTex = renderer.getProceduralEnvIBLTexture();
		REQUIRE(iblTex != nullptr);
		CHECK(iblTex->getDesc().format == PixelFormat::R16G16B16A16_FLOAT);
		CHECK(iblTex->getDesc().width == 128);
		CHECK(iblTex->getDesc().height == 64);
		CHECK(iblTex->getDesc().mipLevels == 6);
	}

	TEST_CASE("Phase 3: Buildable RenderPipeline ForwardPlus and Deferred Pass Verification")
	{
		NullRHIDevice device;
		SceneRenderer renderer(device);

		// 1. Verify ForwardPlus Pipeline Passes
		const auto& forwardPasses = renderer.getPipeline(RenderPipelineType::ForwardPlus).getPasses();
		CHECK(!forwardPasses.empty());
		bool hasShadow = false, hasForwardOpaque = false, hasSkybox = false, hasPostProcess = false;
		for (const auto& pass : forwardPasses)
		{
			if (pass->getName() == "ShadowPass") hasShadow = true;
			if (pass->getName() == "ForwardOpaquePass") hasForwardOpaque = true;
			if (pass->getName() == "SkyboxPass") hasSkybox = true;
			if (pass->getName() == "PostProcessPass") hasPostProcess = true;
		}
		CHECK(hasShadow);
		CHECK(hasForwardOpaque);
		CHECK(hasSkybox);
		CHECK(hasPostProcess);

		// 2. Verify Deferred Pipeline Passes
		const auto& deferredPasses = renderer.getPipeline(RenderPipelineType::Deferred).getPasses();
		CHECK(!deferredPasses.empty());
		bool hasGBuffer = false, hasDeferredLighting = false;
		for (const auto& pass : deferredPasses)
		{
			if (pass->getName() == "GBufferPass") hasGBuffer = true;
			if (pass->getName() == "DeferredLightingPass") hasDeferredLighting = true;
		}
		CHECK(hasGBuffer);
		CHECK(hasDeferredLighting);
	}

	TEST_CASE("Phase 3 & 4: Frame Rendering in ForwardPlus and Deferred Modes with PCSS, Contact Shadows and Fog")
	{
		NullRHIDevice device;
		SceneRenderer renderer(device);

		RenderScene scene;
		scene.camera.position = Math::Vector3(0.0f, 2.0f, -5.0f);
		scene.camera.viewMatrix = Math::lookAtLH(scene.camera.position, Math::Vector3(0.0f, 0.0f, 0.0f), Math::Vector3(0.0f, 1.0f, 0.0f));
		scene.camera.projMatrix = Math::perspectiveFovLH_ReversedZ(Math::toRadians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
		scene.camera.viewProjMatrix = scene.camera.viewMatrix * scene.camera.projMatrix;

		// Configure Fog (Phase 4)
		scene.environment.enableFog = true;
		scene.environment.fogDensity = 0.015f;
		scene.environment.fogHeightFalloff = 0.08f;
		scene.environment.fogSunScattering = 0.4f;
		scene.environment.fogColor = Math::Vector4(0.45f, 0.52f, 0.65f, 1.0f);

		// Configure PCSS, Contact Shadows, CAS Sharpening, and Hardware Tessellation
		scene.environment.enableShadows = true;
		scene.environment.enablePCSS = true;
		scene.environment.enableContactShadows = true;
		scene.environment.casSharpening = 0.35f;
		scene.environment.enableTessellation = true;
		scene.environment.tessellationFactor = 3.0f;

		// Add Spot Light as primary shadow caster
		RenderLight spotLight{};
		spotLight.type = 2; // Spot
		spotLight.position = Math::Vector3(0.0f, 10.0f, 0.0f);
		spotLight.direction = Math::Vector3(0.0f, -1.0f, 0.0f);
		spotLight.color = Math::Vector3(1.0f, 0.95f, 0.85f);
		spotLight.intensity = 2.0f;
		spotLight.range = 30.0f;
		spotLight.innerConeAngle = Math::toRadians(25.0f);
		spotLight.outerConeAngle = Math::toRadians(40.0f);
		spotLight.castShadows = true;
		scene.lights.push_back(spotLight);

		// Add Directional fill light (non-caster)
		RenderLight dirLight{};
		dirLight.type = 0; // Directional
		dirLight.direction = Math::Vector3(0.577f, -0.577f, 0.577f);
		dirLight.color = Math::Vector3(0.3f, 0.35f, 0.5f);
		dirLight.intensity = 0.5f;
		dirLight.castShadows = false;
		scene.lights.push_back(dirLight);

		// Add test meshes (Opaque Cube and Plane)
		RenderMeshInstance cube{};
		cube.world = Math::Matrix::CreateTranslation(0.0f, 0.5f, 0.0f);
		cube.entityId = 1;
		cube.meshPath = "builtin:cube";
		cube.layer = RenderLayer::Opaque;
		cube.indexCount = 36;
		scene.meshes.push_back(cube);

		RenderMeshInstance plane{};
		plane.world = Math::Matrix::CreateScale(10.0f, 1.0f, 10.0f);
		plane.entityId = 2;
		plane.meshPath = "builtin:plane";
		plane.layer = RenderLayer::Opaque;
		plane.indexCount = 6;
		scene.meshes.push_back(plane);

		// Render target view
		GpuTextureDesc offscreenDesc{};
		offscreenDesc.width = 1280;
		offscreenDesc.height = 720;
		offscreenDesc.format = PixelFormat::R8G8B8A8_UNORM;
		offscreenDesc.bindFlags = BufferBindFlags::RenderTarget | BufferBindFlags::ShaderResource;
		auto offscreenRes = device.createTexture(offscreenDesc);
		REQUIRE(offscreenRes.isOk());

		RenderView view{};
		view.width = 1280;
		view.height = 720;
		view.targetTexture = *offscreenRes;

		// 1. Render in ForwardPlus Mode (renderPipeline = 0)
		scene.environment.renderPipeline = 0;
		renderer.render(scene, view);
		CHECK(renderer.getDrawCallCount() > 0);
		CHECK(renderer.getBatchCount() > 0);

		// 2. Render in Deferred Mode (renderPipeline = 1)
		scene.environment.renderPipeline = 1;
		renderer.render(scene, view);
		CHECK(renderer.getDrawCallCount() > 0);
		CHECK(renderer.getBatchCount() > 0);

		// 3. Verify All Active Debug Views (0..6: Lit, SSAO, Bloom, Shadows, Normals, Albedo, MetallicRoughness)
		for (uint32_t dv = 0; dv <= 6; ++dv)
		{
			scene.environment.debugView = dv;
			renderer.render(scene, view);
			CHECK(renderer.getDrawCallCount() > 0);
		}
	}

	TEST_CASE("Phase 3: Hardware Tessellation NullRHI Shaders Support")
	{
		NullRHIDevice device;

		const uint8_t dummyBytecode[] = { 0x01, 0x02, 0x03, 0x04 };
		auto hsRes = device.createHullShader(dummyBytecode);
		REQUIRE(hsRes.isOk());
		CHECK(*hsRes != nullptr);

		auto dsRes = device.createDomainShader(dummyBytecode);
		REQUIRE(dsRes.isOk());
		CHECK(*dsRes != nullptr);
	}

	TEST_CASE("Phase 4: Equirectangular HDR Skybox & SkyType Selection")
	{
		NullRHIDevice device;
		SceneRenderer renderer(device);

		RenderScene scene;
		scene.camera.position = Math::Vector3(0.0f, 0.0f, -5.0f);
		scene.camera.viewMatrix = Math::lookAtLH(scene.camera.position, Math::Vector3(0.0f, 0.0f, 0.0f), Math::Vector3(0.0f, 1.0f, 0.0f));
		scene.camera.projMatrix = Math::perspectiveFovLH_ReversedZ(Math::toRadians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
		scene.camera.viewProjMatrix = scene.camera.viewMatrix * scene.camera.projMatrix;

		GpuTextureDesc offscreenDesc{};
		offscreenDesc.width = 1280;
		offscreenDesc.height = 720;
		offscreenDesc.format = PixelFormat::R8G8B8A8_UNORM;
		offscreenDesc.bindFlags = BufferBindFlags::RenderTarget | BufferBindFlags::ShaderResource;
		auto offscreenRes = device.createTexture(offscreenDesc);
		REQUIRE(offscreenRes.isOk());

		RenderView view{};
		view.width = 1280;
		view.height = 720;
		view.targetTexture = *offscreenRes;

		// 1. Procedural Gradient
		scene.environment.skyType = Core::Renderer::SkyType::ProceduralGradient;
		renderer.render(scene, view);
		CHECK(renderer.getDrawCallCount() > 0);

		// 2. Custom Color
		scene.environment.skyType = Core::Renderer::SkyType::CustomColor;
		scene.environment.skyColor = Math::Vector4(0.2f, 0.4f, 0.8f, 1.0f);
		renderer.render(scene, view);
		CHECK(renderer.getDrawCallCount() > 0);

		// 3. Skybox Texture (Equirectangular)
		scene.environment.skyType = Core::Renderer::SkyType::SkyboxTexture;
		GpuTextureDesc skyTexDesc{};
		skyTexDesc.width = 512;
		skyTexDesc.height = 256;
		skyTexDesc.format = PixelFormat::R16G16B16A16_FLOAT;
		skyTexDesc.bindFlags = BufferBindFlags::ShaderResource;
		auto skyTexRes = device.createTexture(skyTexDesc);
		REQUIRE(skyTexRes.isOk());
		scene.environment.skyboxTexture = *skyTexRes;

		renderer.render(scene, view);
		CHECK(renderer.getDrawCallCount() > 0);
	}

	TEST_CASE("Phase 3 & 4: ECS Bridge Component Extraction Verification")
	{
		Scene scene;
		auto envEntity = scene.createEntity("Environment");
		auto& envComp = envEntity.addComponent<EnvironmentComponent>();
		envComp.enableFog = true;
		envComp.fogDensity = 0.02f;
		envComp.fogHeightFalloff = 0.12f;
		envComp.fogSunScattering = 0.6f;
		envComp.fogColor = Color(0.4f, 0.5f, 0.6f, 1.0f);
		envComp.skyType = Core::SkyType::SkyboxTexture;
		envComp.skyboxTexturePath = "Assets/Skyboxes/DefaultSky.hdr";

		auto ppEntity = scene.createEntity("PostProcessing");
		auto& ppComp = ppEntity.addComponent<PostProcessingComponent>();
		ppComp.renderPipeline = 1; // Deferred
		ppComp.enableShadows = true;
		ppComp.enablePCSS = true;
		ppComp.enableContactShadows = true;
		ppComp.enableTessellation = true;
		ppComp.tessellationFactor = 3.0f;
		ppComp.meshLodBias = 1;
		ppComp.casSharpening = 0.45f;
		ppComp.enableTonemapping = true;
		ppComp.tonemapper = Core::TonemapperType::ACES;
		ppComp.exposure = 1.1f;
		ppComp.gamma = 2.2f;
		ppComp.enableFXAA = true;
		ppComp.enableSSAO = true;
		ppComp.ssaoRadius = 0.6f;
		ppComp.enableBloom = true;
		ppComp.bloomThreshold = 1.2f;
		ppComp.debugView = 4; // Normals

		RenderScene renderScene;
		ExtractRenderScene(scene, renderScene);

		CHECK(renderScene.environment.enableFog == true);
		CHECK(renderScene.environment.fogDensity == doctest::Approx(0.02f));
		CHECK(renderScene.environment.fogHeightFalloff == doctest::Approx(0.12f));
		CHECK(renderScene.environment.fogSunScattering == doctest::Approx(0.6f));
		CHECK(static_cast<uint32_t>(renderScene.environment.skyType) == static_cast<uint32_t>(Core::Renderer::SkyType::SkyboxTexture));
		CHECK(renderScene.environment.skyboxTexturePath == "Assets/Skyboxes/DefaultSky.hdr");

		CHECK(renderScene.environment.renderPipeline == 1);
		CHECK(renderScene.environment.enableShadows == true);
		CHECK(renderScene.environment.enablePCSS == true);
		CHECK(renderScene.environment.enableContactShadows == true);
		CHECK(renderScene.environment.enableTessellation == true);
		CHECK(renderScene.environment.tessellationFactor == doctest::Approx(3.0f));
		CHECK(renderScene.environment.meshLodBias == 1);
		CHECK(renderScene.environment.casSharpening == doctest::Approx(0.45f));
		CHECK(renderScene.environment.enableTonemapping == true);
		CHECK(static_cast<uint32_t>(renderScene.environment.tonemapper) == static_cast<uint32_t>(Core::Renderer::TonemapperType::ACES));
		CHECK(renderScene.environment.exposure == doctest::Approx(1.1f));
		CHECK(renderScene.environment.gamma == doctest::Approx(2.2f));
		CHECK(renderScene.environment.enableFXAA == true);
		CHECK(renderScene.environment.enableSSAO == true);
		CHECK(renderScene.environment.ssaoRadius == doctest::Approx(0.6f));
		CHECK(renderScene.environment.enableBloom == true);
		CHECK(renderScene.environment.bloomThreshold == doctest::Approx(1.2f));
		CHECK(renderScene.environment.debugView == 4);
	}
}
