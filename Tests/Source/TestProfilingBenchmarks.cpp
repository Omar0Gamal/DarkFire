#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Entity.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Scene/SceneRendererBridge.h"
#include "Engine/Core/Serialization/SceneSerializer.h"
#include "Engine/RHI/D3D11/D3D11RHI.h"
#include "Engine/RHI/Null/NullRHI.h"
#include "Engine/RHI/RHIFactory.h"
#include "Engine/Renderer/RenderScene.h"
#include "Engine/Renderer/SceneRenderer.h"
#include "Engine/Renderer/RenderPipeline.h"

#include <chrono>
#include <vector>
#include <numeric>
#include <algorithm>
#include <iostream>
#include <iomanip>
#include <map>
#include <d3d11.h>

#ifdef _MSC_VER
#pragma warning(disable: 5285)
#endif
#include "doctest/doctest.h"

using namespace Core;
using namespace Core::RHI;
using namespace Core::Renderer;

struct BenchmarkResults
{
	std::string sceneName;
	std::string pipelineName;
	std::string deviceBackend;
	uint32_t drawCalls = 0;
	uint32_t batchCount = 0;
	uint32_t instanceCount = 0;
	uint32_t stateChanges = 0;
	uint32_t redundantBindsSkipped = 0;
	size_t uploadRingBytes = 0;
	double avgFrameTimeMs = 0.0;
	double minFrameTimeMs = 0.0;
	double maxFrameTimeMs = 0.0;
	double p99FrameTimeMs = 0.0;
	double avgFps = 0.0;
	double low1Fps = 0.0;
	double extractionTimeMs = 0.0;
	std::map<std::string, double> passTimings;
};

static void PrintBenchmarkReport(const std::vector<BenchmarkResults>& reports)
{
	std::cout << "\n=========================================================================================\n";
	std::cout << "                 DARKFIRE ENGINE FULL PROFILING & BOTTLENECK REPORT                      \n";
	std::cout << "=========================================================================================\n";

	for (const auto& r : reports)
	{
		std::cout << "\n-----------------------------------------------------------------------------------------" << std::endl;
		std::cout << " Scene:    " << r.sceneName << " [" << r.pipelineName << "] on " << r.deviceBackend << std::endl;
		std::cout << "-----------------------------------------------------------------------------------------" << std::endl;
		std::cout << "  Frame Performance:" << std::endl;
		std::cout << "    Average FPS:       " << std::fixed << std::setprecision(1) << r.avgFps << " FPS (" << std::setprecision(2) << r.avgFrameTimeMs << " ms)" << std::endl;
		std::cout << "    1% Low FPS:        " << std::fixed << std::setprecision(1) << r.low1Fps << " FPS (P99: " << std::setprecision(2) << r.p99FrameTimeMs << " ms)" << std::endl;
		std::cout << "    Min / Max Time:    " << std::fixed << std::setprecision(2) << r.minFrameTimeMs << " ms / " << r.maxFrameTimeMs << " ms" << std::endl;
		std::cout << "    Scene Extraction:  " << std::fixed << std::setprecision(3) << r.extractionTimeMs << " ms" << std::endl;
		std::cout << "  Renderer Telemetry:" << std::endl;
		std::cout << "    Draw Calls:        " << r.drawCalls << std::endl;
		std::cout << "    Batches:           " << r.batchCount << std::endl;
		std::cout << "    Instances:         " << r.instanceCount << std::endl;
		std::cout << "    RHI State Changes: " << r.stateChanges << std::endl;
		std::cout << "    Redundant Binds:   " << r.redundantBindsSkipped << std::endl;
		std::cout << "    UploadRing Alloc:  " << std::fixed << std::setprecision(2) << (static_cast<double>(r.uploadRingBytes) / 1024.0) << " KB" << std::endl;
	}
	std::cout << "=========================================================================================\n" << std::endl;
}

TEST_SUITE("Full Engine Profiling & Bottleneck Analysis")
{
	TEST_CASE("Profile: Playground3D and GraphicsTest3D Sponza Atrium under Real Workloads")
	{
		auto device = CreateRHIDevice(BackendType::D3D11, false);
		if (!device)
		{
			DF_LOG_WARN("Profiler", "Live D3D11 device not available; falling back to NullRHIDevice.");
			device = CreateRHIDevice(BackendType::Null, false);
		}

		REQUIRE(device != nullptr);

		SceneRenderer renderer(*device);
		const uint32_t kWidth = 1920;
		const uint32_t kHeight = 1080;
		renderer.resize(kWidth, kHeight);

		GpuTextureDesc targetDesc{};
		targetDesc.width = kWidth;
		targetDesc.height = kHeight;
		targetDesc.format = PixelFormat::R8G8B8A8_UNORM;
		targetDesc.bindFlags = BufferBindFlags::RenderTarget | BufferBindFlags::ShaderResource;
		auto targetRes = device->createTexture(targetDesc);
		REQUIRE(targetRes.isOk());

		RenderView view{};
		view.width = kWidth;
		view.height = kHeight;
		view.targetTexture = *targetRes;
		view.isEditorView = false;

		std::vector<BenchmarkResults> allReports;

		auto runSceneBenchmark = [&](const std::string& scenePath, const std::string& sceneLabel, uint8_t pipelineMode) {
			Scene scene;
			SceneSerializer serializer(&scene);
			bool loaded = serializer.deserializeFromFile(scenePath);
			if (!loaded)
			{
				DF_LOG_ERROR("Profiler", "Failed to load scene: {}", scenePath);
				return;
			}
			scene.updateTransforms();

			// Measure extraction time
			auto tExtStart = std::chrono::high_resolution_clock::now();
			RenderScene renderScene;
			ExtractRenderScene(scene, renderScene, static_cast<float>(kWidth) / static_cast<float>(kHeight));
			auto tExtEnd = std::chrono::high_resolution_clock::now();
			double extTimeMs = std::chrono::duration<double, std::milli>(tExtEnd - tExtStart).count();

			renderScene.environment.renderPipeline = pipelineMode;

			// Warmup (3 frames)
			for (int w = 0; w < 3; ++w)
			{
				renderer.render(renderScene, view);
			}

			// Profile full benchmark frames
			const int kFrames = 30;
			std::vector<double> frameTimesMs(kFrames);
			auto ctx = device->getImmediate();

			for (int f = 0; f < kFrames; ++f)
			{
				auto tStart = std::chrono::high_resolution_clock::now();
				renderer.render(renderScene, view);
				auto tEnd = std::chrono::high_resolution_clock::now();
				std::chrono::duration<double, std::milli> diff = tEnd - tStart;
				frameTimesMs[f] = diff.count();
			}

			std::vector<double> sorted = frameTimesMs;
			std::sort(sorted.begin(), sorted.end());

			double sum = std::accumulate(sorted.begin(), sorted.end(), 0.0);
			double avg = sum / static_cast<double>(sorted.size());
			size_t p99Idx = static_cast<size_t>(static_cast<double>(sorted.size()) * 0.99);
			if (p99Idx >= sorted.size()) p99Idx = sorted.size() - 1;

			auto ctxStats = ctx ? ctx->getStats() : ContextStats{};

			BenchmarkResults res{};
			res.sceneName = sceneLabel;
			res.pipelineName = (pipelineMode == 1) ? "Deferred PBR" : "Forward+";
			res.deviceBackend = device->getBackendName();
			res.drawCalls = renderer.getDrawCallCount();
			res.batchCount = renderer.getBatchCount();
			res.instanceCount = renderer.getInstanceCount();
			res.stateChanges = ctxStats.stateChanges;
			res.redundantBindsSkipped = ctxStats.redundantBindsSkipped;
			res.uploadRingBytes = renderer.getUploadRing().getAllocatedBytes();
			res.avgFrameTimeMs = avg;
			res.minFrameTimeMs = sorted.front();
			res.maxFrameTimeMs = sorted.back();
			res.p99FrameTimeMs = sorted[p99Idx];
			res.avgFps = (avg > 0.0) ? (1000.0 / avg) : 0.0;
			res.low1Fps = (res.p99FrameTimeMs > 0.0) ? (1000.0 / res.p99FrameTimeMs) : 0.0;
			res.extractionTimeMs = extTimeMs;

			allReports.push_back(res);
		};

		// 1. Benchmark Playground3D (Forward+ and Deferred)
		runSceneBenchmark("Templates/Playground3D/Scenes/Playground.scene", "Playground3D", 0);
		runSceneBenchmark("Templates/Playground3D/Scenes/Playground.scene", "Playground3D", 1);

		// 2. Benchmark GraphicsTest3D Sponza Atrium (Forward+ and Deferred)
		runSceneBenchmark("Templates/GraphicsTest3D/Scenes/SponzaAtrium.scene", "Crytek Sponza Atrium", 0);
		runSceneBenchmark("Templates/GraphicsTest3D/Scenes/SponzaAtrium.scene", "Crytek Sponza Atrium", 1);

		// 3. Benchmark GraphicsTest3D Tropical Island (Nature & Ocean)
		runSceneBenchmark("Templates/GraphicsTest3D/Scenes/TropicalIsland.scene", "Tropical Island & Ocean", 0);

		PrintBenchmarkReport(allReports);

		CHECK(!allReports.empty());
	}
}
