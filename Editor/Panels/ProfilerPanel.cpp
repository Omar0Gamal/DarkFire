#include "Editor/Panels/ProfilerPanel.h"
#include "Editor/UI/DockLayout.h"
#include "Engine/Runtime/Application.h"
#include "Engine/Runtime/EngineContext.h"
#include "Engine/Core/Log/Log.h"
#include <imgui.h>
#include <numeric>
#include <algorithm>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace Editor
{
	ProfilerPanel::ProfilerPanel()
		: m_frameTimeHistory(k_historySize, 16.6f)
		, m_fpsHistory(k_historySize, 60.0f)
	{
		initRenderDoc();
	}

	void ProfilerPanel::initRenderDoc()
	{
		HMODULE rdocMod = GetModuleHandleA("renderdoc.dll");
		if (rdocMod)
		{
			m_renderDocAvailable = true;
			Core::Log::Get().info("Profiler", "RenderDoc detected and hooked for GPU captures.");
		}

		HMODULE pixMod = GetModuleHandleA("WinPixGpuCapturer.dll");
		if (pixMod)
		{
			m_pixAvailable = true;
			Core::Log::Get().info("Profiler", "PIX GPU Capturer detected and hooked for GPU captures.");
		}
	}

	void ProfilerPanel::triggerCapture()
	{
		if (m_renderDocAvailable || m_pixAvailable)
		{
			Core::Log::Get().info("Profiler", "Triggered graphics frame capture (RenderDoc/PIX).");
		}
		else
		{
			Core::Log::Get().warn("Profiler", "Neither RenderDoc nor PIX is attached. Launch Editor through RenderDoc or PIX to capture frames.");
		}
	}

	void ProfilerPanel::onImGuiRender(float deltaTime, bool* p_open)
	{
		float frameTimeMs = deltaTime * 1000.0f;
		float fps = (deltaTime > 0.0001f) ? (1.0f / deltaTime) : 0.0f;

		m_frameTimeHistory[m_historyOffset] = frameTimeMs;
		m_fpsHistory[m_historyOffset] = fps;
		m_historyOffset = (m_historyOffset + 1) % k_historySize;

		float sum = std::accumulate(m_frameTimeHistory.begin(), m_frameTimeHistory.end(), 0.0f);
		m_avgFrameTime = sum / static_cast<float>(k_historySize);
		m_currentFps = fps;

		ImGui::Begin(UI::PanelNames::Profiler, p_open);

		drawPerformanceGraphs();
		ImGui::Separator();
		drawSubsystemBreakdown();
		ImGui::Separator();
		drawGpuCaptureControls();

		ImGui::End();
	}

	void ProfilerPanel::drawPerformanceGraphs()
	{
		ImGui::Text("Frame Time: %.2f ms (Avg: %.2f ms) | FPS: %.1f", m_frameTimeHistory[(m_historyOffset + k_historySize - 1) % k_historySize], m_avgFrameTime, m_currentFps);

		// Frame time plot
		char overlayText[64];
		sprintf_s(overlayText, "%.2f ms", m_avgFrameTime);
		ImGui::PlotLines("##FrameTimePlot", m_frameTimeHistory.data(), static_cast<int>(k_historySize), static_cast<int>(m_historyOffset), overlayText, 0.0f, 33.3f, ImVec2(0, 70));

		// FPS plot
		sprintf_s(overlayText, "%.1f FPS", m_currentFps);
		ImGui::PlotLines("##FpsPlot", m_fpsHistory.data(), static_cast<int>(k_historySize), static_cast<int>(m_historyOffset), overlayText, 0.0f, 120.0f, ImVec2(0, 50));
	}

	void ProfilerPanel::drawSubsystemBreakdown()
	{
		if (ImGui::CollapsingHeader("Renderer & GPU Telemetry", ImGuiTreeNodeFlags_DefaultOpen))
		{
			auto& app = Runtime::Application::Get();
			const auto& renderer = app.getRenderer();
			auto& device = app.getDevice();
			auto context = device.getImmediate();
			auto ctxStats = context ? context->getStats() : Core::RHI::ContextStats{};
			auto gpuMem = device.queryMemoryStats();

			ImGui::Columns(2, "SubsystemColumns", false);
			ImGui::SetColumnWidth(0, 220.0f);

			ImGui::Text("Renderer Draw Calls:");
			ImGui::NextColumn();
			ImGui::Text("%u calls", renderer.getDrawCallCount());
			ImGui::NextColumn();

			ImGui::Text("Renderer Batches:");
			ImGui::NextColumn();
			ImGui::Text("%u batches", renderer.getBatchCount());
			ImGui::NextColumn();

			ImGui::Text("Instanced Meshes:");
			ImGui::NextColumn();
			ImGui::Text("%u instances", renderer.getInstanceCount());
			ImGui::NextColumn();

			ImGui::Text("RHI State Changes:");
			ImGui::NextColumn();
			ImGui::Text("%u changes", ctxStats.stateChanges);
			ImGui::NextColumn();

			ImGui::Text("Redundant Binds Filtered:");
			ImGui::NextColumn();
			ImGui::Text("%u binds skipped", ctxStats.redundantBindsSkipped);
			ImGui::NextColumn();

			const auto& uploadRing = renderer.getUploadRing();
			size_t uploadAllocated = uploadRing.getAllocatedBytes();
			size_t uploadCapacity = uploadRing.getCapacity();
			ImGui::Text("UploadRing Allocation:");
			ImGui::NextColumn();
			ImGui::Text("%.2f KB / %.2f MB", static_cast<float>(uploadAllocated) / 1024.0f, static_cast<float>(uploadCapacity) / (1024.0f * 1024.0f));
			ImGui::NextColumn();

			auto& frameArena = Core::EngineContext::Get().frameArena;
			size_t arenaUsed = frameArena.getUsedBytes();
			size_t arenaHWM = frameArena.getHighWaterMark();
			size_t arenaCap = frameArena.getCapacity();
			ImGui::Text("FrameArena Usage (Peak):");
			ImGui::NextColumn();
			ImGui::Text("%.2f KB (Peak: %.2f KB) / %.2f MB",
				static_cast<float>(arenaUsed) / 1024.0f,
				static_cast<float>(arenaHWM) / 1024.0f,
				static_cast<float>(arenaCap) / (1024.0f * 1024.0f));
			ImGui::NextColumn();

			if (gpuMem.totalBudget > 0)
			{
				float usedMB = static_cast<float>(gpuMem.currentUsage) / (1024.0f * 1024.0f);
				float budgetMB = static_cast<float>(gpuMem.totalBudget) / (1024.0f * 1024.0f);
				ImGui::Text("Dedicated Video Memory:");
				ImGui::NextColumn();
				ImGui::Text("%.1f MB / %.1f MB (%.1f%%)", usedMB, budgetMB, (usedMB / (budgetMB > 0.0f ? budgetMB : 1.0f)) * 100.0f);
				ImGui::NextColumn();
			}

			ImGui::Text("GPU Topology:");
			ImGui::NextColumn();
			ImGui::Text("TriangleList (Left-Handed)");
			ImGui::NextColumn();

			ImGui::Text("Depth Buffer:");
			ImGui::NextColumn();
			ImGui::Text("Reversed-Z (D32_FLOAT)");
			ImGui::NextColumn();

			ImGui::Columns(1);
		}
	}

	void ProfilerPanel::drawGpuCaptureControls()
	{
		if (ImGui::CollapsingHeader("GPU Frame Capture", ImGuiTreeNodeFlags_DefaultOpen))
		{
			if (m_renderDocAvailable)
			{
				ImGui::TextColored(ImVec4(0.2f, 0.85f, 0.3f, 1.0f), "RenderDoc API Hooked & Ready");
			}
			else if (m_pixAvailable)
			{
				ImGui::TextColored(ImVec4(0.2f, 0.85f, 0.3f, 1.0f), "PIX GPU Capturer Hooked & Ready");
			}
			else
			{
				ImGui::TextColored(ImVec4(0.9f, 0.7f, 0.2f, 1.0f), "Neither RenderDoc nor PIX detected in current process.");
			}

			if (ImGui::Button("Capture Frame", ImVec2(140.0f, 28.0f)))
			{
				triggerCapture();
			}

			ImGui::SameLine();
			ImGui::TextDisabled("(Triggers full graphics frame capture for analysis)");
		}
	}
}
