#include "Engine/Runtime/SceneRuntimeLayer.h"
#include "Engine/Runtime/Application.h"
#include "Engine/Renderer/SceneRenderer.h"
#include "Engine/Platform/Input.h"
#include "Engine/Platform/KeyCodes.h"
#include <imgui.h>
#include <algorithm>
#include <cstdio>

namespace Runtime
{
static int s_screenshotCountdown = 0;
static std::string s_screenshotDestination = "Docs/images/engine.png";
static float s_screenshotToastTimer = 0.0f;
static std::string s_screenshotToastText;

	void SceneRuntimeLayer::onImGuiRender()
	{
		if (!m_scene || !ImGui::GetCurrentContext()) return;

		// [F12] Take Screenshot
		if (Platform::Input::isKeyJustPressed(Platform::KeyCode::F12))
		{
			s_screenshotCountdown = 2;
			s_screenshotDestination = "Docs/images/engine.png";
		}

		// Brief ImGui deactivation: skip UI rendering for the capture frame so only the 3D scene is captured
		if (s_screenshotCountdown > 0)
		{
			--s_screenshotCountdown;
			if (s_screenshotCountdown == 0)
			{
				auto& app = Application::Get();
				bool ok = app.getRenderer().saveScreenshot(s_screenshotDestination, app.getSwapChain());
				if (!ok)
				{
					ok = app.getRenderer().saveScreenshot(s_screenshotDestination, nullptr);
				}
				if (ok)
				{
					s_screenshotToastText = "Screenshot saved to " + s_screenshotDestination;
				}
				else
				{
					s_screenshotToastText = "Failed to save screenshot!";
				}
				s_screenshotToastTimer = 3.5f;
			}
			return; // Briefly deactivate ImGui for pristine capture
		}

		if (Platform::Input::isKeyJustPressed(Platform::KeyCode::G) ||
		    Platform::Input::isKeyJustPressed(Platform::KeyCode::Tab))
		{
			m_showEffectsMenu = !m_showEffectsMenu;
		}

		if (Platform::Input::isKeyJustPressed(Platform::KeyCode::R))
		{
			auto camView = m_scene->view<Core::CameraComponent, Core::TransformComponent>();
			for (auto ce : camView)
			{
				auto& cc = camView.get<Core::CameraComponent>(ce);
				if (cc.isPrimary)
				{
					auto& tc = camView.get<Core::TransformComponent>(ce);
					tc.position = Core::Vector3(-11.68f, 0.75f, 0.356f);
					tc.setEulerDegrees(Core::Vector3(-12.55f, 90.0f, 0.0f));
					cc.fov = 40.0f;
					break;
				}
			}
		}

		auto view = m_scene->view<Core::HUDTelemetryComponent>();
		for (auto entity : view)
		{
			const auto& hud = view.get<Core::HUDTelemetryComponent>(entity);
			if (!hud.showOverlay) continue;

			ImGuiWindowFlags flags = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
				ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav;
			ImGui::SetNextWindowPos(ImVec2(16.0f, 16.0f), ImGuiCond_Always);
			ImGui::SetNextWindowBgAlpha(0.75f);
			if (ImGui::Begin("##InGameHUDTelemetry", nullptr, flags))
			{
				ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "%s", hud.customBannerText.c_str());
				ImGui::Separator();
				float fps = ImGui::GetIO().Framerate;
				float ms = 1000.0f / (fps > 0.0f ? fps : 60.0f);
				auto& app = Application::Get();
				ImGui::TextColored(ImVec4(0.4f, 0.9f, 0.4f, 1.0f), "GPU: %s (%u MB VRAM)",
					app.getDevice().getAdapterName().c_str(),
					app.getDevice().getDedicatedVramMB());
				ImGui::Text("FPS: %.1f (%.2f ms) | %s", fps, ms, app.isVSync() ? "VSync: ON" : "VSync: OFF");
				const auto& r = Application::Get().getRenderer();
				ImGui::Text("Draw Calls: %u | Batches: %u | Instances: %u", r.getDrawCallCount(), r.getBatchCount(), r.getInstanceCount());

				ImGui::Spacing();
				ImGui::Spacing();
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.6f, 0.35f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.25f, 0.72f, 0.42f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.5f, 0.28f, 1.0f));
				if (ImGui::Button("Take Screenshot [F12]", ImVec2(-1.0f, 28.0f)))
				{
					s_screenshotCountdown = 2;
					s_screenshotDestination = "Docs/images/engine.png";
				}
				ImGui::PopStyleColor(3);

				ImGui::Spacing();
				if (ImGui::Button(m_showEffectsMenu ? "Hide Effects & Shaders Menu [G / Tab]" : "Open Effects & Shaders Menu [G / Tab]", ImVec2(-1.0f, 26.0f)))
				{
					m_showEffectsMenu = !m_showEffectsMenu;
				}

				if (hud.showHotkeys)
				{
					ImGui::Spacing();
					ImGui::TextDisabled("Controls / Hotkeys:");
					ImGui::BulletText("WASD + RMB: Flycam Movement / Look");
					ImGui::BulletText("Shift: Sprint (Flycam)");
					ImGui::BulletText("[R]: Reset Camera to Screenshot Pose");
					ImGui::BulletText("[F1 / F2]: Switch Showcase Scene");
					ImGui::BulletText("[G / Tab]: Toggle Effects Menu | [F3 - F12]: Toggle Effects");
					ImGui::BulletText("[F9]: Toggle Hi-Z SSR | [P]: Toggle DDGI Probes | [B]: Toggle Baked IBL");
					ImGui::BulletText("[V]: Toggle VSync | [C]: Toggle GPU Culling | [L]: Toggle Dynamic Lights");
					ImGui::BulletText("[T]: Cycle AA (TAA / FXAA / Off) | [0 - 8]: Debug Views (8 = SSR)");
				}
			}
			ImGui::End();
			break;
		}

		if (m_showEffectsMenu)
		{
			Core::PostProcessingComponent* pp = nullptr;
			auto ppView = m_scene->view<Core::PostProcessingComponent>();
			if (auto it = ppView.begin(); it != ppView.end())
			{
				pp = &ppView.get<Core::PostProcessingComponent>(*it);
			}

			Core::EnvironmentComponent* env = nullptr;
			auto envView = m_scene->view<Core::EnvironmentComponent>();
			if (auto it = envView.begin(); it != envView.end())
			{
				env = &envView.get<Core::EnvironmentComponent>(*it);
			}

			if (pp || env)
			{
				const ImGuiIO& io = ImGui::GetIO();
				ImGui::SetNextWindowPos(ImVec2(io.DisplaySize.x - 16.0f, 16.0f), ImGuiCond_FirstUseEver, ImVec2(1.0f, 0.0f));
				ImGui::SetNextWindowSize(ImVec2(410.0f, 0.0f), ImGuiCond_FirstUseEver);
				ImGui::SetNextWindowBgAlpha(0.85f);

				if (ImGui::Begin("Graphics & Lighting Options", &m_showEffectsMenu, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
				{
					auto* m_renderer = &Application::Get().getRenderer();
					ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "Draw Calls: %u | Batches: %u | Instances: %u",
						m_renderer->getDrawCallCount(), m_renderer->getBatchCount(), m_renderer->getInstanceCount());
					ImGui::Separator();

					if (ImGui::BeginTabBar("GraphicsOptionsTabBar", ImGuiTabBarFlags_None))
					{
						// =========================================================================
						// TAB 1: Simplified Game Settings (Clean, curated, non-conflicting)
						// =========================================================================
						if (ImGui::BeginTabItem("Game Settings"))
						{
							ImGui::Spacing();
							ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "Quality Presets:");
							if (ImGui::Button("Low", ImVec2(90.0f, 26.0f)))
							{
								if (pp)
								{
									pp->enableShadows = true;
									pp->enablePCSS = false;
									pp->enableContactShadows = false;
									pp->enableSSAO = false;
									pp->enableSSR = false;
									pp->enableBloom = false;
									pp->enableMotionBlur = false;
									pp->enableFXAA = true;
									pp->enableTAA = false;
									pp->enableTessellation = false;
								}
								if (env) env->enableFog = false;
								auto ddgiView = m_scene->view<Core::DDGIProbeVolumeComponent>();
								for (auto de : ddgiView) ddgiView.get<Core::DDGIProbeVolumeComponent>(de).enabled = false;
							}
							ImGui::SameLine();
							if (ImGui::Button("Medium", ImVec2(90.0f, 26.0f)))
							{
								if (pp)
								{
									pp->enableShadows = true;
									pp->enablePCSS = false;
									pp->enableContactShadows = false;
									pp->enableSSAO = true;
									pp->ssaoIntensity = 0.5f;
									pp->enableSSR = false;
									pp->enableBloom = true;
									pp->enableMotionBlur = true;
									pp->enableFXAA = false;
									pp->enableTAA = true;
									pp->enableTessellation = false;
								}
								if (env) env->enableFog = false;
								auto ddgiView = m_scene->view<Core::DDGIProbeVolumeComponent>();
								for (auto de : ddgiView) ddgiView.get<Core::DDGIProbeVolumeComponent>(de).enabled = false;
							}
							ImGui::SameLine();
							if (ImGui::Button("High", ImVec2(90.0f, 26.0f)))
							{
								if (pp)
								{
									pp->enableShadows = true;
									pp->enablePCSS = true;
									pp->enableContactShadows = false;
									pp->enableSSAO = true;
									pp->ssaoIntensity = 0.65f;
									pp->enableSSR = true;
									pp->enableBloom = true;
									pp->enableMotionBlur = true;
									pp->enableFXAA = false;
									pp->enableTAA = true;
									pp->enableTessellation = false;
								}
								if (env) { env->enableFog = true; env->fogDensity = 0.0015f; }
								auto ddgiView = m_scene->view<Core::DDGIProbeVolumeComponent>();
								for (auto de : ddgiView) { auto& d = ddgiView.get<Core::DDGIProbeVolumeComponent>(de); d.enabled = true; d.intensity = 1.0f; }
							}
							ImGui::SameLine();
							if (ImGui::Button("Ultra", ImVec2(90.0f, 26.0f)))
							{
								if (pp)
								{
									pp->enableShadows = true;
									pp->enablePCSS = true;
									pp->enableContactShadows = true;
									pp->enableSSAO = true;
									pp->ssaoIntensity = 0.85f;
									pp->enableSSR = true;
									pp->enableBloom = true;
									pp->enableMotionBlur = true;
									pp->enableFXAA = false;
									pp->enableTAA = true;
									pp->enableTessellation = false;
								}
								if (env) { env->enableFog = true; env->fogDensity = 0.0020f; }
								auto ddgiView = m_scene->view<Core::DDGIProbeVolumeComponent>();
								for (auto de : ddgiView) { auto& d = ddgiView.get<Core::DDGIProbeVolumeComponent>(de); d.enabled = true; d.intensity = 1.25f; }
							}

							ImGui::Separator();
							if (pp)
							{
								ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "Display & Pipeline:");
								int curPipeline = static_cast<int>(pp->renderPipeline);
								if (ImGui::Combo("Render Pipeline", &curPipeline, "Forward+ (High Framerate)\0Deferred PBR (G-Buffer Fidelity)\0\0"))
								{
									pp->renderPipeline = static_cast<uint8_t>(curPipeline);
								}
								bool vsync = Application::Get().isVSync();
								if (ImGui::Checkbox("VSync (Vertical Sync)", &vsync))
								{
									Application::Get().setVSync(vsync);
								}

								ImGui::Separator();
								ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "Anti-Aliasing:");
								const char* aaModes[] = { "Disabled", "FXAA (Fast)", "TAA (Filmic Temporal)" };
								int curAA = pp->enableTAA ? 2 : (pp->enableFXAA ? 1 : 0);
								if (ImGui::Combo("Anti-Aliasing Mode", &curAA, aaModes, 3))
								{
									pp->enableFXAA = (curAA == 1);
									pp->enableTAA = (curAA == 2);
								}
								if (pp->enableTAA || pp->enableFXAA)
								{
									ImGui::SliderFloat("Texture Sharpness (CAS)", &pp->casSharpening, 0.0f, 1.0f, "%.2f");
								}

								ImGui::Separator();
								ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "Lighting & Visuals:");
								const char* shadowModes[] = { "Disabled", "Standard (4-CSM)", "PCSS Soft Shadows", "Ultra (PCSS + Contact)" };
								int curShadow = !pp->enableShadows ? 0 : (pp->enableContactShadows ? 3 : (pp->enablePCSS ? 2 : 1));
								if (ImGui::Combo("Shadow Quality", &curShadow, shadowModes, 4))
								{
									pp->enableShadows = (curShadow > 0);
									pp->enablePCSS = (curShadow >= 2);
									pp->enableContactShadows = (curShadow == 3);
								}

								ImGui::Checkbox("Ambient Occlusion (SSAO)", &pp->enableSSAO);
								ImGui::Checkbox("Screen-Space Reflections (SSR)", &pp->enableSSR);

								auto probeView = m_scene->view<Core::ReflectionProbeComponent>();
								for (auto pe : probeView)
								{
									auto& pr = probeView.get<Core::ReflectionProbeComponent>(pe);
									bool hasProbe = !pr.bakedCubemapPath.empty();
									if (ImGui::Checkbox("Baked Reflection Probe [B]", &hasProbe))
									{
										pr.bakedCubemapPath = hasProbe ? "Assets/Textures/SponzaAtrium_BakedProbe.dds" : "";
										pr.boxProjection = hasProbe;
									}
									if (hasProbe)
									{
										ImGui::Indent();
										ImGui::SliderFloat("Probe Specular Intensity", &pr.intensity, 0.0f, 2.0f, "%.2f");
										ImGui::Unindent();
									}
								}

								auto ddgiView = m_scene->view<Core::DDGIProbeVolumeComponent>();
								for (auto de : ddgiView)
								{
									auto& ddgi = ddgiView.get<Core::DDGIProbeVolumeComponent>(de);
									ImGui::Checkbox("Dynamic Global Illumination (DDGI)", &ddgi.enabled);
									if (ddgi.enabled)
									{
										ImGui::Indent();
										ImGui::SliderFloat("GI Bounce Intensity", &ddgi.intensity, 0.0f, 3.0f, "%.2f");
										ImGui::Unindent();
									}
								}

								if (env)
								{
									ImGui::Checkbox("Volumetric Atmospheric Fog", &env->enableFog);
									if (env->enableFog)
									{
										ImGui::Indent();
										ImGui::SliderFloat("Fog Density", &env->fogDensity, 0.0f, 0.01f, "%.4f");
										ImGui::Unindent();
									}
								}

								ImGui::Checkbox("Motion Blur", &pp->enableMotionBlur);
								ImGui::Checkbox("HDR Bloom", &pp->enableBloom);

								ImGui::Separator();
								ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "Camera Exposure & Tonemapping:");
								ImGui::Checkbox("Auto-Exposure (Eye Adaptation)", &pp->enableAutoExposure);
								if (!pp->enableAutoExposure)
								{
									ImGui::SliderFloat("Manual Exposure", &pp->exposure, 0.1f, 3.0f, "%.2f");
								}
								const char* tonemappers[] = { "ACES Filmic (Vibrant)", "Reinhard", "Linear", "AgX", "Khronos Neutral", "Reinhard-Jodie" };
								int curTm = static_cast<int>(pp->tonemapper);
								if (ImGui::Combo("Tonemapper Mode", &curTm, tonemappers, 6))
								{
									pp->tonemapper = static_cast<Core::TonemapperType>(curTm);
								}
							}

							ImGui::EndTabItem();
						}

						// =========================================================================
						// TAB 2: Advanced Developer Tools (Full engine knobs, debug views)
						// =========================================================================
						if (ImGui::BeginTabItem("Advanced"))
						{
							ImGui::Spacing();
							if (ImGui::Button("Reset Camera to Screenshot Pose [R]", ImVec2(-1.0f, 26.0f)))
							{
								auto camView = m_scene->view<Core::CameraComponent, Core::TransformComponent>();
								for (auto ce : camView)
								{
									auto& cc = camView.get<Core::CameraComponent>(ce);
									if (cc.isPrimary)
									{
										auto& tc = camView.get<Core::TransformComponent>(ce);
										tc.position = Core::Vector3(-11.68f, 0.75f, 0.356f);
										tc.setEulerDegrees(Core::Vector3(-12.55f, 90.0f, 0.0f));
										cc.fov = 40.0f;
										break;
									}
								}
							}

							if (pp)
							{
								ImGui::Separator();
								ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "Debug Buffer Views [Keys 0 - 7]:");
								int curDbg = static_cast<int>(pp->debugView);
								if (curDbg < 0 || curDbg > 7) curDbg = 0;
								if (ImGui::Combo("Debug Buffer", &curDbg, "Lit (Final)\0SSAO\0Bloom\0Shadow Map\0World Normals\0Albedo\0Metallic / Roughness\0Velocity (Motion Vectors)\0\0"))
								{
									pp->debugView = static_cast<uint32_t>(curDbg);
								}

								static const char* const s_dbgLabels[] = {
									"0: Lit", "1: SSAO", "2: Bloom", "3: Shadows", "4: Normals",
									"5: Albedo", "6: Metal/Rough", "7: Velocity"
								};
								for (uint32_t i = 0; i < 8; ++i)
								{
									if (i % 4 != 0) ImGui::SameLine();
									bool active = (pp->debugView == i);
									if (active) ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.55f, 0.85f, 1.0f));
									if (ImGui::Button(s_dbgLabels[i], ImVec2(92.0f, 22.0f)))
									{
										pp->debugView = i;
									}
									if (active) ImGui::PopStyleColor();
								}

								ImGui::Separator();
								ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "Geometry & Culling:");
								ImGui::Combo("Mesh LOD Mode", &pp->meshLodBias, "Auto (Distance LOD0-LOD2)\0Force LOD0 (Full Detail)\0Force LOD1 (Medium ~50%)\0Force LOD2 (Coarse ~25%)\0\0");
								ImGui::Checkbox("Hardware Tessellation (PN-Triangles)", &pp->enableTessellation);
								if (pp->enableTessellation)
								{
									ImGui::Indent();
									ImGui::SliderFloat("Tessellation Factor", &pp->tessellationFactor, 1.0f, 12.0f, "%.1f");
									ImGui::Unindent();
								}
								ImGui::Checkbox("GPU-Driven Frustum Culling", &pp->enableGpuCulling);
								ImGui::Checkbox("Clustered Light Culling (16x16x24)", &pp->enableClusteredLighting);

								ImGui::Separator();
								ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "Directional Sun:");
								auto lightView = m_scene->view<Core::LightComponent, Core::TransformComponent>();
								for (auto le : lightView)
								{
									auto& lc = lightView.get<Core::LightComponent>(le);
									if (lc.type == Core::LightType::Directional)
									{
										auto& tc = lightView.get<Core::TransformComponent>(le);
										Core::Vector3 degs = tc.eulerDegrees;
										bool changed = false;
										if (ImGui::SliderFloat("Elevation (Pitch)", &degs.x, 5.0f, 89.0f, "%.1f deg")) changed = true;
										if (ImGui::SliderFloat("Azimuth (Yaw)", &degs.y, 0.0f, 360.0f, "%.1f deg")) changed = true;
										if (changed) tc.setEulerDegrees(degs);
										ImGui::SliderFloat("Sun Radiance Intensity", &lc.intensity, 0.0f, 12.0f, "%.2f");
										break;
									}
								}

								ImGui::Separator();
								ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "SSAO & Micro-Occlusion:");
								ImGui::SliderFloat("SSAO Radius", &pp->ssaoRadius, 0.1f, 2.0f, "%.2f m");
								ImGui::SliderFloat("SSAO Bias", &pp->ssaoBias, 0.001f, 0.1f, "%.4f");
								ImGui::SliderFloat("SSAO Direct Influence", &pp->ssaoDirectLightInfluence, 0.0f, 1.0f, "%.2f");

								ImGui::Separator();
								ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "Screen-Space Reflections (SSR):");
								ImGui::SliderFloat("SSR Max Distance", &pp->ssrMaxDistance, 5.0f, 150.0f, "%.1f m");
								ImGui::SliderFloat("SSR Thickness", &pp->ssrThickness, 0.05f, 2.0f, "%.2f");
								ImGui::SliderFloat("SSR Roughness Cutoff", &pp->ssrRoughnessCutoff, 0.1f, 1.0f, "%.2f");
								ImGui::SliderFloat("SSR Intensity", &pp->ssrIntensity, 0.0f, 2.0f, "%.2f");

								ImGui::Separator();
								ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "DDGI Advanced Parameters:");
								auto ddgiView = m_scene->view<Core::DDGIProbeVolumeComponent>();
								for (auto de : ddgiView)
								{
									auto& ddgi = ddgiView.get<Core::DDGIProbeVolumeComponent>(de);
									ImGui::SliderFloat("Normal Bias", &ddgi.normalBias, 0.01f, 0.5f, "%.2f m");
									ImGui::SliderFloat("Temporal Hysteresis", &ddgi.hysteresis, 0.5f, 0.99f, "%.2f");
									ImGui::SliderFloat("Energy Preservation", &ddgi.energyPreservation, 0.1f, 1.0f, "%.2f");
								}

								ImGui::Separator();
								ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "Motion Blur & Bloom:");
								ImGui::SliderFloat("Shutter Angle", &pp->motionBlurShutterAngle, 0.0f, 1.0f, "%.2f");
								ImGui::SliderFloat("Max Blur Radius", &pp->motionBlurMaxRadius, 1.0f, 64.0f, "%.0f px");
								ImGui::SliderFloat("Bloom Threshold", &pp->bloomThreshold, 0.1f, 4.0f, "%.2f");
								ImGui::SliderFloat("Bloom Intensity", &pp->bloomIntensity, 0.0f, 0.5f, "%.3f");

								ImGui::Separator();
								ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "Color Grading:");
								ImGui::SliderFloat("Contrast", &pp->contrast, 0.5f, 1.8f, "%.2f");
								ImGui::SliderFloat("Saturation", &pp->saturation, 0.0f, 2.0f, "%.2f");
								ImGui::SliderFloat("Color Temperature", &pp->colorTemperature, -1.0f, 1.0f, "%.2f");
								ImGui::SliderFloat("Optical Vignette", &pp->vignetteIntensity, 0.0f, 1.0f, "%.2f");
							}

							if (env)
							{
								ImGui::Separator();
								ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "Environment & Atmosphere:");
								int skyMode = (env->skyType == Core::SkyType::SkyboxTexture) ? 1 : 0;
								if (ImGui::Combo("Sky Type", &skyMode, "Procedural Physical Sky\0HDRI Skybox (05-18_Day_D.hdr)\0\0"))
								{
									env->skyType = (skyMode == 1) ? Core::SkyType::SkyboxTexture : Core::SkyType::ProceduralGradient;
									if (skyMode == 1) env->skyboxTexturePath = "Assets/Textures/05-18_Day_D.hdr";
								}
								ImGui::SliderFloat("Ambient Energy", &env->ambientEnergy, 0.0f, 3.0f, "%.2f");
								ImGui::SliderFloat("Fog Sun Scattering", &env->fogSunScattering, 0.0f, 2.0f, "%.2f");
								ImGui::SliderFloat("Fog Height Falloff", &env->fogHeightFalloff, 0.001f, 0.5f, "%.3f");
							}

							ImGui::EndTabItem();
						}

						ImGui::EndTabBar();
					}
				}
				ImGui::End();
			}
		}

		if (s_screenshotToastTimer > 0.0f)
		{
			s_screenshotToastTimer -= ImGui::GetIO().DeltaTime;
			ImGui::SetNextWindowPos(ImVec2(ImGui::GetIO().DisplaySize.x * 0.5f, 24.0f), ImGuiCond_Always, ImVec2(0.5f, 0.0f));
			ImGui::SetNextWindowBgAlpha(0.9f);
			if (ImGui::Begin("##ScreenshotToast", nullptr,
				ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize |
				ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoFocusOnAppearing | ImGuiWindowFlags_NoNav))
			{
				ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), " %s ", s_screenshotToastText.c_str());
			}
			ImGui::End();
		}
	}
}