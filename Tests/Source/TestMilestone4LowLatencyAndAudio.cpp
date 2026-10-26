#include <doctest/doctest.h>
#include "Engine/RHI/IRHI.h"
#include "Engine/RHI/Null/NullRHI.h"
#include "Engine/Audio/AudioSystem.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Components.h"

TEST_CASE("Milestone 4.1: DXGI Waitable SwapChain & Latency Interface")
{
	Core::RHI::NullSwapChain swapChain(1920, 1080);

	// Verify defaults
	CHECK(swapChain.getWidth() == 1920);
	CHECK(swapChain.getHeight() == 1080);
	CHECK(swapChain.getBackBufferTexture() != nullptr);

	// Verify latency & waitable interfaces
	CHECK(swapChain.setMaximumFrameLatency(1) == true);
	CHECK(swapChain.waitForNextFrame(50) == true);
	CHECK(swapChain.isWaitable() == false); // Null swapchain is not hardware waitable
}

TEST_CASE("Milestone 4.2 & 4.3: Audio Mix Buses, Doppler & Parameter Smoothing")
{
	auto& audio = Audio::AudioSystem::Get();
	bool initialized = audio.init();
	CHECK(initialized == true);
	CHECK(audio.isInitialized() == true);

	// Test bus volume controls
	audio.setBusVolume(Core::AudioBus::Master, 0.9f);
	CHECK(audio.getBusVolume(Core::AudioBus::Master) == doctest::Approx(0.9f));

	audio.setBusVolume(Core::AudioBus::SFX, 0.75f);
	CHECK(audio.getBusVolume(Core::AudioBus::SFX) == doctest::Approx(0.75f));

	audio.setBusVolume(Core::AudioBus::Engine, 0.85f);
	CHECK(audio.getBusVolume(Core::AudioBus::Engine) == doctest::Approx(0.85f));

	audio.setBusVolume(Core::AudioBus::Music, 0.6f);
	CHECK(audio.getBusVolume(Core::AudioBus::Music) == doctest::Approx(0.6f));

	// Test audio scene update, velocity tracking & smoothing
	Core::Scene scene;

	// Listener entity
	auto listenerEnt = scene.createEntity("Listener");
	auto& lTc = listenerEnt.getComponent<Core::TransformComponent>();
	lTc.position = Core::Vector3(0.0f, 0.0f, 0.0f);
	auto& listener = listenerEnt.addComponent<Core::AudioListenerComponent>();
	listener.active = true;
	listener.masterVolume = 1.0f;

	// Sound source entity
	auto soundEnt = scene.createEntity("VehicleEngine");
	auto& sTc = soundEnt.getComponent<Core::TransformComponent>();
	sTc.position = Core::Vector3(0.0f, 0.0f, 10.0f);
	auto& audioComp = soundEnt.addComponent<Core::AudioSourceComponent>();
	audioComp.clipPath = "builtin:ocean_waves";
	audioComp.bus = Core::AudioBus::Engine;
	audioComp.volume = 0.5f;
	audioComp.pitch = 1.0f;
	audioComp.dopplerFactor = 1.5f;
	audioComp.spatialize3D = true;
	audioComp.playOnStart = true;

	// Initial tick
	audio.update(scene, 0.016f);

	// Slew volume and pitch smoothly
	audioComp.setVolumeSmooth(1.0f, 10.0f);
	audioComp.setPitchSmooth(2.0f, 10.0f);
	CHECK(audioComp.targetVolume == doctest::Approx(1.0f));
	CHECK(audioComp.targetPitch == doctest::Approx(2.0f));

	// Move listener and sound to simulate high-speed pass-by (Doppler)
	lTc.position = Core::Vector3(0.0f, 0.0f, 2.0f);
	sTc.position = Core::Vector3(0.0f, 0.0f, 5.0f);
	audio.update(scene, 0.016f);

	// Slew progress
	audio.update(scene, 0.1f);

	audio.shutdown();
	CHECK(audio.isInitialized() == false);
}
