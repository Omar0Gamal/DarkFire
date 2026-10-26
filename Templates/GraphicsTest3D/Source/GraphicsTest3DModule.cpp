#include "Engine/Runtime/Module.h"
#include "Engine/Runtime/Application.h"
#include "Engine/Runtime/SceneRuntimeLayer.h"
#include "Engine/Core/Scripting/ScriptableEntity.h"
#include "Engine/Core/Scripting/ScriptRegistry.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Entity.h"
#include "Engine/Core/Serialization/SceneSerializer.h"
#include "Engine/Core/VFS/AssetPathResolver.h"
#include "Engine/Platform/Input.h"
#include "Engine/Platform/KeyCodes.h"
#include "Engine/Core/Log/Log.h"

#include <cmath>
#include <vector>
#include <deque>
#include <algorithm>
#include <string>
#include <cstdio>
#include <filesystem>

namespace GraphicsTest3D
{
	using Core::Vector3;
	using Core::Vector2;
	using Core::Quaternion;
	using Core::Matrix;

	class AstrolabeRotator : public Core::ScriptableEntity
	{
	public:
		void onCreate() override
		{
			exposeVec3("Axis Speeds", m_axisSpeeds, 2.0f);
		}

		void onStart() override
		{
			std::string name = getEntity().getName();
			if (name.find("Outer") != std::string::npos)
			{
				m_axisSpeeds = Vector3(25.0f, 40.0f, 15.0f);
			}
			else if (name.find("Inner") != std::string::npos)
			{
				m_axisSpeeds = Vector3(-40.0f, 30.0f, 60.0f);
			}
			else if (name.find("Core") != std::string::npos || name.find("Sun") != std::string::npos)
			{
				m_axisSpeeds = Vector3(10.0f, 75.0f, 20.0f);
			}
		}

		void onUpdate(float deltaTime) override
		{
			auto& t = transform();
			Vector3 degs = t.eulerDegrees;
			degs += m_axisSpeeds * deltaTime;
			for (int i = 0; i < 3; ++i)
			{
				float* pVal = (i == 0) ? &degs.x : ((i == 1) ? &degs.y : &degs.z);
				while (*pVal >= 360.0f) *pVal -= 360.0f;
				while (*pVal < 0.0f) *pVal += 360.0f;
			}
			t.setEulerDegrees(degs);
		}

	private:
		Vector3 m_axisSpeeds = Vector3(20.0f, 35.0f, 15.0f);
	};

	class BenchmarkController : public Core::ScriptableEntity
	{
	public:
		void onCreate() override
		{
		}

		void onStart() override
		{
			Core::Log::Get().info("BenchmarkController", "BenchmarkController initialized on entity '{}'", getEntity().getName());
			if (isSponzaScene())
			{
				snapToScreenshotCamera();
			}
		}

		void snapToScreenshotCamera()
		{
			auto& t = transform();
			t.position = Vector3(-11.68f, 0.75f, 0.356f);
			t.setEulerDegrees(Vector3(-12.55f, 90.0f, 0.0f));
			if (getEntity().hasComponent<Core::CameraComponent>())
			{
				getEntity().getComponent<Core::CameraComponent>().fov = 40.0f;
			}
			Core::Log::Get().info("BenchmarkController", "Snapped camera to Classic Sponza screenshot pose.");
		}

		void onUpdate(float deltaTime) override
		{
			// 1. Hotkey Checks
			// [F1] / [F2] Scene switching
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F1))
			{
				loadSceneFile("Scenes/SponzaAtrium.scene");
				return;
			}
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F2))
			{
				loadSceneFile("Scenes/TropicalIsland.scene");
				return;
			}

			// [R] Snap to Classic Sponza Screenshot Camera pose
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::R))
			{
				snapToScreenshotCamera();
			}

			// [0] - [7] Post-processing debug view selection
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::Num0)) setDebugView(0);
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::Num1)) setDebugView(1);
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::Num2)) setDebugView(2);
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::Num3)) setDebugView(3);
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::Num4)) setDebugView(4);
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::Num5)) setDebugView(5);
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::Num6)) setDebugView(6);
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::Num7)) setDebugView(7);
			if (Platform::Input::isKeyJustPressed(Platform::KeyCode::Num8)) setDebugView(8);

			// [F3] - [F12] Individual effect toggles for live shader/lighting isolation
			if (m_scene)
			{
				auto ppView = m_scene->view<Core::PostProcessingComponent>();
				for (auto e : ppView)
				{
					auto& pp = ppView.get<Core::PostProcessingComponent>(e);
					if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F3)) pp.enableShadows = !pp.enableShadows;
					if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F4)) pp.enableSSAO = !pp.enableSSAO;
					if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F5)) pp.renderPipeline = (pp.renderPipeline == 0) ? 1 : 0;
					if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F6)) pp.enablePCSS = !pp.enablePCSS;
					if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F7)) pp.enableTessellation = !pp.enableTessellation;
					if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F8)) pp.enableBloom = !pp.enableBloom;
					if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F9)) pp.enableSSR = !pp.enableSSR;
					if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F10)) pp.enableTonemapping = !pp.enableTonemapping;
					if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F11)) pp.enableFXAA = !pp.enableFXAA;
					if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F12)) pp.enableTAA = !pp.enableTAA;

					// [T] Cycle Anti-Aliasing (TAA -> FXAA -> Off)
					if (Platform::Input::isKeyJustPressed(Platform::KeyCode::T))
					{
						if (pp.enableTAA)
						{
							pp.enableTAA = false;
							pp.enableFXAA = true;
						}
						else if (pp.enableFXAA)
						{
							pp.enableTAA = false;
							pp.enableFXAA = false;
						}
						else
						{
							pp.enableTAA = true;
							pp.enableFXAA = false;
						}
					}

					// [B] Toggle Baked Reflection Probe vs Procedural Sky IBL
					if (Platform::Input::isKeyJustPressed(Platform::KeyCode::B))
					{
						auto probeView = m_scene->view<Core::ReflectionProbeComponent>();
						for (auto pe : probeView)
						{
							auto& pr = probeView.get<Core::ReflectionProbeComponent>(pe);
							if (pr.bakedCubemapPath.empty())
							{
								pr.bakedCubemapPath = "Assets/Textures/SponzaAtrium_BakedProbe.dds";
								pr.boxProjection = true;
								Core::Log::Get().info("GraphicsTest3D", "Baked Reflection Probe enabled: {}", pr.bakedCubemapPath);
							}
							else
							{
								pr.bakedCubemapPath = "";
								Core::Log::Get().info("GraphicsTest3D", "Baked Reflection Probe disabled (reverted to procedural sky).");
							}
						}
					}

					// [C] Toggle GPU Culling
					if (Platform::Input::isKeyJustPressed(Platform::KeyCode::C))
					{
						pp.enableGpuCulling = !pp.enableGpuCulling;
					}

					// [P] Toggle DDGI Real-Time Probe Volume
					if (Platform::Input::isKeyJustPressed(Platform::KeyCode::P) || Platform::Input::isKeyJustPressed(Platform::KeyCode::I))
					{
						auto ddgiView = m_scene->view<Core::DDGIProbeVolumeComponent>();
						for (auto de : ddgiView)
						{
							auto& ddgi = ddgiView.get<Core::DDGIProbeVolumeComponent>(de);
							ddgi.enabled = !ddgi.enabled;
							Core::Log::Get().info("GraphicsTest3D", "DDGI Probe Volume: {}", ddgi.enabled ? "Enabled" : "Disabled");
						}
					}

					// [V] Toggle VSync
					if (Platform::Input::isKeyJustPressed(Platform::KeyCode::V))
					{
						bool curVsync = Runtime::Application::Get().isVSync();
						Runtime::Application::Get().setVSync(!curVsync);
						Core::Log::Get().info("BenchmarkController", "VSync set to {}", (!curVsync) ? "ON" : "OFF");
					}

					// [L] Toggle Dynamic Point Lights (for Clustered Lighting isolation)
					if (Platform::Input::isKeyJustPressed(Platform::KeyCode::L))
					{
						pp.enableClusteredLighting = !pp.enableClusteredLighting;
						auto lightView = m_scene->view<Core::LightComponent>();
						for (auto le : lightView)
						{
							auto& lc = lightView.get<Core::LightComponent>(le);
							if (lc.type == Core::LightType::Point || lc.type == Core::LightType::Spot)
							{
								lc.intensity = (lc.intensity > 0.01f) ? 0.0f : 4.5f;
							}
						}
					}

					m_currentDebugView = pp.debugView;
				}
				auto envView = m_scene->view<Core::EnvironmentComponent>();
				for (auto e : envView)
				{
					auto& env = envView.get<Core::EnvironmentComponent>(e);
					if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F9)) env.enableFog = !env.enableFog;
				}
			}

			// 2. Camera Update (Clean 6-DOF Flycam mode: RMB + WASD/QE, Shift sprint)
			updateFlycam(deltaTime);

			// 3. Rolling FPS & 1% Low FPS Tracking
			if (deltaTime > 0.0f && deltaTime < 0.5f)
			{
				m_frameTimes.push_back(deltaTime);
				if (m_frameTimes.size() > 180)
				{
					m_frameTimes.pop_front();
				}
			}

			m_uiUpdateTimer += deltaTime;
			if (m_uiUpdateTimer >= 0.15f && !m_frameTimes.empty())
			{
				m_uiUpdateTimer = 0.0f;

				float sum = 0.0f;
				std::vector<float> sorted(m_frameTimes.begin(), m_frameTimes.end());
				std::sort(sorted.begin(), sorted.end());

				for (float ft : sorted) sum += ft;
				float avgFt = sum / static_cast<float>(sorted.size());
				float rollingFps = (avgFt > 0.0f) ? (1.0f / avgFt) : 0.0f;

				size_t p99Idx = static_cast<size_t>(static_cast<float>(sorted.size()) * 0.99f);
				if (p99Idx >= sorted.size()) p99Idx = sorted.size() - 1;
				float p99Ft = sorted[p99Idx];
				float low1Fps = (p99Ft > 0.0f) ? (1.0f / p99Ft) : 0.0f;

				static const char* const s_debugNames[] = {
					"Lit", "SSAO", "Bloom", "Shadows", "Normals", "Albedo", "Metallic/Roughness", "Velocity", "SSR"
				};
				const char* debugStr = (m_currentDebugView < 9) ? s_debugNames[m_currentDebugView] : "Custom";

				std::string sceneStr = isSponzaScene() ? "Crytek Sponza Atrium" : "Tropical Island & Ocean";

				const char* aaMode = "Off";
				const char* clusterMode = "Clustered";
				bool ssrActive = false;
				if (m_scene)
				{
					auto ppView = m_scene->view<Core::PostProcessingComponent>();
					auto it = ppView.begin();
					if (it != ppView.end())
					{
						const auto& pp = ppView.get<Core::PostProcessingComponent>(*it);
						if (pp.enableTAA) aaMode = "TAA";
						else if (pp.enableFXAA) aaMode = "FXAA";
						else aaMode = "Off";

						clusterMode = pp.enableClusteredLighting ? "Clustered" : "Forward";
						ssrActive = pp.enableSSR;
					}
				}

				bool ddgiActive = false;
				if (m_scene)
				{
					auto ddgiView = m_scene->view<Core::DDGIProbeVolumeComponent>();
					auto it = ddgiView.begin();
					if (it != ddgiView.end())
					{
						const auto& ddgi = ddgiView.get<Core::DDGIProbeVolumeComponent>(*it);
						ddgiActive = ddgi.enabled;
					}
				}

				const char* vsyncStr = Runtime::Application::Get().isVSync() ? "VSync: ON" : "VSync: OFF";
				std::string gpuName = Runtime::Application::Get().getDevice().getAdapterName();

				char buf[384];
				std::snprintf(buf, sizeof(buf), "[%s] %s | %s | AA: %s | %s | SSR: %s | DDGI: %s | %s | FPS: %.0f (1%% Low: %.0f) | %.2f ms",
					sceneStr.c_str(), gpuName.c_str(), vsyncStr, aaMode, clusterMode,
					ssrActive ? "ON" : "OFF", ddgiActive ? "ON" : "OFF",
					debugStr, rollingFps, low1Fps, avgFt * 1000.0f);

				m_lastBannerText = buf;

				if (m_scene)
				{
					auto hudView = m_scene->view<Core::HUDTelemetryComponent>();
					for (auto e : hudView)
					{
						auto& hud = hudView.get<Core::HUDTelemetryComponent>(e);
						hud.customBannerText = m_lastBannerText;
					}
				}
			}
		}

	private:
		bool isSponzaScene() const
		{
			if (!m_scene) return false;
			return m_scene->findEntityByName("Sponza Atrium Architecture").isValid() ||
			       m_scene->findEntityByName("Central Reflecting Marble Basin").isValid();
		}

		void updateFlycam(float deltaTime)
		{
			auto& t = transform();

			if (Platform::Input::isMouseButtonPressed(Platform::MouseCode::Right))
			{
				Vector2 mouseDelta = Platform::Input::getMouseDelta();
				float sens = 0.15f;
				Vector3 degs = t.eulerDegrees;
				degs.x += mouseDelta.y * sens;
				degs.y += mouseDelta.x * sens;
				degs.x = std::clamp(degs.x, -88.0f, 88.0f);
				while (degs.y >= 360.0f) degs.y -= 360.0f;
				while (degs.y < 0.0f) degs.y += 360.0f;
				t.setEulerDegrees(degs);

				float speed = 12.0f;
				if (Platform::Input::isKeyPressed(Platform::KeyCode::LeftShift) || Platform::Input::isKeyPressed(Platform::KeyCode::RightShift))
				{
					speed = 36.0f;
				}

				Vector3 fwd = t.getForward();
				Vector3 right = t.getRight();
				Vector3 up = Vector3(0.0f, 1.0f, 0.0f);

				Vector3 move(0.0f, 0.0f, 0.0f);
				if (Platform::Input::isKeyPressed(Platform::KeyCode::W)) move += fwd;
				if (Platform::Input::isKeyPressed(Platform::KeyCode::S)) move -= fwd;
				if (Platform::Input::isKeyPressed(Platform::KeyCode::D)) move += right;
				if (Platform::Input::isKeyPressed(Platform::KeyCode::A)) move -= right;
				if (Platform::Input::isKeyPressed(Platform::KeyCode::E)) move += up;
				if (Platform::Input::isKeyPressed(Platform::KeyCode::Q)) move -= up;

				if (move.LengthSquared() > 0.0f)
				{
					move.Normalize();
					t.position += move * (speed * deltaTime);
				}
			}
		}

		void setDebugView(uint32_t viewIdx)
		{
			m_currentDebugView = viewIdx;
			if (!m_scene) return;
			auto ppView = m_scene->view<Core::PostProcessingComponent>();
			for (auto e : ppView)
			{
				auto& pp = ppView.get<Core::PostProcessingComponent>(e);
				pp.debugView = viewIdx;
			}
		}

		void loadSceneFile(const std::string& scenePath)
		{
			for (auto& layer : Runtime::Application::Get().getLayerStack())
			{
				auto sceneLayer = std::dynamic_pointer_cast<Runtime::SceneRuntimeLayer>(layer);
				if (sceneLayer)
				{
					if (sceneLayer->loadScene(scenePath))
					{
						Core::Log::Get().info("BenchmarkController", "Loaded scene via SceneRuntimeLayer: {}", scenePath);
						return;
					}
				}
			}

			if (m_scene)
			{
				auto textRes = Core::AssetPathResolver::readText(scenePath);
				if (textRes.isErr() || textRes.value().empty())
				{
					std::string filename = std::filesystem::path(scenePath).filename().string();
					textRes = Core::AssetPathResolver::readText("Scenes/" + filename);
					if (textRes.isErr() || textRes.value().empty())
					{
						textRes = Core::AssetPathResolver::readText("/Scenes/" + filename);
					}
				}

				if (textRes.isOk())
				{
					auto view = m_scene->view<Core::ScriptsComponent>();
					for (auto entity : view)
					{
						auto& sc = view.get<Core::ScriptsComponent>(entity);
						for (auto& s : sc.scripts)
						{
							if (s.isStarted && s.instance)
							{
								s.instance->onDestroy();
								s.isStarted = false;
							}
						}
					}

					std::vector<Core::Entity> roots;
					for (auto [handle] : m_scene->getRegistry().storage<entt::entity>().each())
					{
						Core::Entity e(handle, m_scene);
						if (!e.getParent().isValid())
						{
							roots.push_back(e);
						}
					}
					for (auto& e : roots)
					{
						if (e.isValid())
						{
							m_scene->destroyEntity(e);
						}
					}
					m_scene->getRegistry().clear();

					Core::SceneSerializer serializer(m_scene);
					serializer.deserializeFromString(textRes.value());
					Core::Log::Get().info("BenchmarkController", "Deserialized scene directly into active scene: {}", scenePath);
				}
			}
		}

	private:
		uint32_t m_currentDebugView = 0;
		std::deque<float> m_frameTimes;
		float m_uiUpdateTimer = 0.0f;
		std::string m_lastBannerText;
	};
}

extern "C"
{
	DF_MODULE_API Core::ModuleInfo DF_GetModuleInfo()
	{
		return Core::ModuleInfo{
			"GraphicsTest3D",
			"1.0.0",
			Core::DARKFIRE_API_VERSION
		};
	}

	DF_MODULE_API void DF_RegisterTypes(Core::EngineContext& ctx)
	{
		ctx.log.info("GraphicsTest3D", "Registering GraphicsTest3D benchmark scripts...");
		Core::ScriptRegistry::Get().registerScript<GraphicsTest3D::AstrolabeRotator>("AstrolabeRotator");
		Core::ScriptRegistry::Get().registerScript<GraphicsTest3D::BenchmarkController>("BenchmarkController");
	}

	DF_MODULE_API void DF_OnLoad(Core::EngineContext& ctx)
	{
		ctx.log.info("GraphicsTest3D", "GraphicsTest3D benchmark module loaded successfully!");
	}

	DF_MODULE_API void DF_OnUnload()
	{
		Core::Log::Get().info("GraphicsTest3D", "GraphicsTest3D module unloaded.");
	}

	DF_MODULE_API void DF_RegisterEditorTools([[maybe_unused]] Core::EditorContext& ctx)
	{
	}
}
