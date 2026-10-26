#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Entity.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Serialization/SceneSerializer.h"
#include "Engine/Core/Reflection/Reflection.h"
#include "Engine/Audio/AudioSystem.h"
#include "Engine/Renderer/DebugRenderer.h"
#include "Engine/Renderer/SceneRenderer.h"
#include "Engine/Renderer/RenderScene.h"
#include "Engine/RHI/Null/NullRHI.h"

#ifdef _MSC_VER
#pragma warning(disable: 5285)
#endif
#include "doctest/doctest.h"

using namespace Core;
using namespace Core::RHI;
using namespace Core::Renderer;

TEST_SUITE("Phase 8: Audio, Debug Renderer & HUD Telemetry")
{
	TEST_CASE("Phase 8.1: AudioSystem Lifecycle & 3D Spatial Audio Update")
	{
		Audio::AudioSystem audioSystem;
		bool initialized = audioSystem.init();
		CHECK(initialized == true);
		CHECK(audioSystem.isInitialized() == true);

		Scene scene;

		// 1. Audio Listener
		auto listenerEntity = scene.createEntity("AudioListener");
		auto& listener = listenerEntity.addComponent<AudioListenerComponent>();
		listener.active = true;
		listener.masterVolume = 0.9f;
		listenerEntity.getComponent<TransformComponent>().position = Vector3(0.0f, 1.8f, 0.0f);

		// 2. Procedural Ocean Waves Audio Source
		auto oceanAudioEntity = scene.createEntity("OceanAudioSource");
		auto& oceanSrc = oceanAudioEntity.addComponent<AudioSourceComponent>();
		oceanSrc.clipPath = "builtin:ocean_waves";
		oceanSrc.volume = 0.85f;
		oceanSrc.pitch = 1.0f;
		oceanSrc.spatialize3D = true;
		oceanSrc.minDistance = 3.0f;
		oceanSrc.maxDistance = 100.0f;
		oceanSrc.playOnStart = true;
		oceanAudioEntity.getComponent<TransformComponent>().position = Vector3(0.0f, 0.0f, 50.0f);

		// 3. Procedural Wind Audio Source
		auto windAudioEntity = scene.createEntity("WindAudioSource");
		auto& windSrc = windAudioEntity.addComponent<AudioSourceComponent>();
		windSrc.clipPath = "builtin:wind";
		windSrc.volume = 0.7f;
		windSrc.pitch = 0.95f;
		windSrc.spatialize3D = true;
		windSrc.minDistance = 5.0f;
		windSrc.maxDistance = 150.0f;
		windSrc.playOnStart = true;
		windAudioEntity.getComponent<TransformComponent>().position = Vector3(0.0f, 25.0f, 0.0f);

		scene.updateTransforms();

		// Update audio system multiple ticks to simulate playback & parameter modulation
		for (int frame = 0; frame < 5; ++frame)
		{
			audioSystem.update(scene, 1.0f / 60.0f);
		}

		// Move audio source and update
		oceanAudioEntity.getComponent<TransformComponent>().position = Vector3(10.0f, 0.0f, 20.0f);
		scene.updateTransforms();
		audioSystem.update(scene, 1.0f / 60.0f);

		// Clean shutdown
		audioSystem.shutdown();
		CHECK(audioSystem.isInitialized() == false);
	}

	TEST_CASE("Phase 8.2: DebugRenderer Primitives & SceneRenderer Batched Execution")
	{
		DebugRenderer::clear();
		CHECK(DebugRenderer::getVertices().empty());

		// 1. Draw single line
		DebugRenderer::drawLine(Vector3(0.0f, 0.0f, 0.0f), Vector3(1.0f, 1.0f, 1.0f), Vector4(1.0f, 0.0f, 0.0f, 1.0f));
		CHECK(DebugRenderer::getVertices().size() == 2);

		// 2. Draw Box (12 line segments = 24 vertices)
		DebugRenderer::drawBox(Vector3(0.0f, 0.0f, 0.0f), Vector3(2.0f, 2.0f, 2.0f), Quaternion(0.0f, 0.0f, 0.0f, 1.0f), Vector4(0.0f, 1.0f, 0.0f, 1.0f));
		CHECK(DebugRenderer::getVertices().size() == 26);

		// 3. Draw Sphere (3 rings x 16 segments x 2 vertices = 96 vertices)
		DebugRenderer::drawSphere(Vector3(0.0f, 5.0f, 0.0f), 1.5f, Vector4(0.0f, 0.0f, 1.0f, 1.0f), 16);
		CHECK(DebugRenderer::getVertices().size() == 26 + 96);

		// 4. Test rendering with NullRHI
		NullRHIDevice device;
		SceneRenderer renderer(device);

		RenderScene renderScene;
		renderScene.camera.aspect = 16.0f / 9.0f;
		renderScene.camera.projMatrix = Math::perspectiveFovLH_ReversedZ(Math::toRadians(60.0f), renderScene.camera.aspect, 0.1f, 1000.0f);
		renderScene.camera.viewMatrix = DirectX::XMMatrixIdentity();
		renderScene.camera.viewProjMatrix = renderScene.camera.viewMatrix * renderScene.camera.projMatrix;

		RenderView view{};
		view.width = 1280;
		view.height = 720;

		renderer.render(renderScene, view);

		// Verify draw calls executed and vertices were cleared
		CHECK(renderer.getDrawCallCount() > 0);
		CHECK(DebugRenderer::getVertices().empty());
	}

	TEST_CASE("Phase 8.3: HUDTelemetryComponent Reflection & Serialization")
	{
		ComponentRegistry::Get().registerBuiltinTypes();

		Scene scene;
		auto entity = scene.createEntity("HUDOwner");
		auto& hud = entity.addComponent<HUDTelemetryComponent>();
		hud.showOverlay = true;
		hud.showHotkeys = false;
		hud.customBannerText = "Custom Engine Benchmark v2.0";

		auto& audioSrc = entity.addComponent<AudioSourceComponent>();
		audioSrc.clipPath = "builtin:wind";
		audioSrc.volume = 0.65f;
		audioSrc.spatialize3D = false;

		auto& audioListener = entity.addComponent<AudioListenerComponent>();
		audioListener.active = true;
		audioListener.masterVolume = 0.85f;

		SceneSerializer serializer(&scene);
		std::string jsonStr = serializer.serializeToString();

		Scene loadedScene;
		SceneSerializer deserializer(&loadedScene);
		bool ok = deserializer.deserializeFromString(jsonStr);
		REQUIRE(ok);

		auto loadedEntity = loadedScene.findEntityByName("HUDOwner");
		REQUIRE(loadedEntity.isValid());

		auto* loadedHud = loadedEntity.tryGetComponent<HUDTelemetryComponent>();
		REQUIRE(loadedHud != nullptr);
		CHECK(loadedHud->showOverlay == true);
		CHECK(loadedHud->showHotkeys == false);
		CHECK(loadedHud->customBannerText == "Custom Engine Benchmark v2.0");

		auto* loadedAudioSrc = loadedEntity.tryGetComponent<AudioSourceComponent>();
		REQUIRE(loadedAudioSrc != nullptr);
		CHECK(loadedAudioSrc->clipPath == "builtin:wind");
		CHECK(loadedAudioSrc->volume == doctest::Approx(0.65f));
		CHECK(loadedAudioSrc->spatialize3D == false);

		auto* loadedListener = loadedEntity.tryGetComponent<AudioListenerComponent>();
		REQUIRE(loadedListener != nullptr);
		CHECK(loadedListener->active == true);
		CHECK(loadedListener->masterVolume == doctest::Approx(0.85f));
	}
}
