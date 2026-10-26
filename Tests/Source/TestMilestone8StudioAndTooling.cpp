#include "Engine/Platform/CrashHandler.h"
#include <doctest/doctest.h>
#include "Engine/Core/Console/CVar.h"
#include "Tools/dfcook/CookCommon.h"
#include "Tools/dfcook/CookMeshes.h"
#include "Engine/Runtime/Application.h"
#include "Engine/Runtime/Config.h"
#include <filesystem>
#include <fstream>

TEST_CASE("Milestone 8.1: In-Engine CVar Registry & Terminal Autocomplete")
{
	auto& cvars = Core::CVarSystem::Get();

	// Test registration
	auto* testVar = cvars.registerCVar("test.latency", "Test simulated latency", 42.0f);
	REQUIRE(testVar != nullptr);
	CHECK(testVar->asFloat() == doctest::Approx(42.0f));

	// Test find
	auto* found = cvars.findCVar("test.latency");
	REQUIRE(found != nullptr);
	CHECK(found == testVar);

	// Test execute setting
	bool ok = cvars.execute("test.latency 105.5");
	CHECK(ok == true);
	CHECK(testVar->asFloat() == doctest::Approx(105.5f));

	// Test CVar callback
	bool callbackTriggered = false;
	testVar->onChange = [&](const Core::CVarValue&) {
		callbackTriggered = true;
	};
	cvars.execute("test.latency 50.0");
	CHECK(callbackTriggered == true);
	CHECK(testVar->asFloat() == doctest::Approx(50.0f));

	// Test Autocomplete
	auto completions = cvars.getCompletions("test.");
	CHECK(completions.size() >= 1);
	CHECK(std::find(completions.begin(), completions.end(), "test.latency") != completions.end());
}

TEST_CASE("Milestone 8.2: Incremental Content-Hash Cook Cache (.cookcache)")
{
	std::filesystem::path testCachePath = "Temp_TestCookCache/.cookcache";
	std::filesystem::path dummyAsset = "Temp_TestCookCache/dummy_asset.txt";

	std::filesystem::create_directories("Temp_TestCookCache");
	{
		std::ofstream f(dummyAsset);
		f << "DarkFire Shader Bytecode Dummy V1" << std::endl;
	}

	dfcook::CookCache cache;
	uint64_t hash1 = dfcook::CookCache::ComputeFileHash(dummyAsset);
	CHECK(hash1 != 0);

	cache.update(dummyAsset, "/Shaders/dummy.cso");
	CHECK(cache.isUpToDate(dummyAsset, "/Shaders/dummy.cso") == true);

	// Save and reload cache
	CHECK(cache.save(testCachePath) == true);

	dfcook::CookCache loadedCache;
	CHECK(loadedCache.load(testCachePath) == true);
	CHECK(loadedCache.isUpToDate(dummyAsset, "/Shaders/dummy.cso") == true);

	// Modify dummy asset -> verify cache detects modification
	{
		std::ofstream f(dummyAsset, std::ios::app);
		f << "Modified Content For Cache Invalidation" << std::endl;
	}
	CHECK(loadedCache.isUpToDate(dummyAsset, "/Shaders/dummy.cso") == false);

	// Cleanup
	std::error_code ec;
	std::filesystem::remove_all("Temp_TestCookCache", ec);
}

TEST_CASE("Milestone 8.4: Headless Dedicated Server Runtime Architecture")
{
	Runtime::ProjectConfig config;
	config.name = "TestDedicatedServer";
	config.headless = true; // Zero-GPU mode

	Platform::CrashHandler::Install();
	Runtime::Application app(config);
	bool initOk = app.init();
	CHECK(initOk == true);

	// In headless mode: device is NullRHI, swapchain is null, window is null
	CHECK(app.getDevice().getBackendName() == "Null");
	CHECK(app.getSwapChain() == nullptr);
	CHECK(app.getWindowPtr() == nullptr);
	CHECK(app.hasWindow() == false);

	// Run 5 bounded frames in headless mode
	app.run(5);

	// Verify Saved/Crashes directory creation
	CHECK(std::filesystem::exists("Saved/Crashes"));
}

TEST_CASE("Milestone 8.5: Quadric Error Metric (QEM) Mesh LOD Generation")
{
	// Create a simple box mesh with 8 vertices and 12 triangles (36 indices)
	std::vector<float> positions = {
		-1.0f, -1.0f, -1.0f, // 0
		 1.0f, -1.0f, -1.0f, // 1
		 1.0f,  1.0f, -1.0f, // 2
		-1.0f,  1.0f, -1.0f, // 3
		-1.0f, -1.0f,  1.0f, // 4
		 1.0f, -1.0f,  1.0f, // 5
		 1.0f,  1.0f,  1.0f, // 6
		-1.0f,  1.0f,  1.0f  // 7
	};

	std::vector<uint32_t> indicesLOD0 = {
		// Front
		0, 1, 2, 0, 2, 3,
		// Back
		5, 4, 7, 5, 7, 6,
		// Left
		4, 0, 3, 4, 3, 7,
		// Right
		1, 5, 6, 1, 6, 2,
		// Top
		3, 2, 6, 3, 6, 7,
		// Bottom
		4, 5, 1, 4, 1, 0
	};
	REQUIRE(indicesLOD0.size() == 36);

	// Simplify to 50% (LOD1)
	std::vector<uint32_t> indicesLOD1;
	bool ok50 = dfcook::SimplifyMeshQEM(positions, indicesLOD0, 0.5f, indicesLOD1);
	CHECK(ok50 == true);
	CHECK(indicesLOD1.size() < indicesLOD0.size());
	CHECK(indicesLOD1.size() % 3 == 0);

	// Simplify to 25% (LOD2)
	std::vector<uint32_t> indicesLOD2;
	bool ok25 = dfcook::SimplifyMeshQEM(positions, indicesLOD0, 0.25f, indicesLOD2);
	CHECK(ok25 == true);
	CHECK(indicesLOD2.size() <= indicesLOD1.size());
	CHECK(indicesLOD2.size() % 3 == 0);
}
