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
	void SceneRuntimeLayer::onImGuiRender()
	{
		if (!m_scene || !ImGui::GetCurrentContext()) return;

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

				if (ImGui::Begin("Graphics & Lighting Effects", &m_showEffectsMenu, ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings))
				{
					auto* m_renderer = &Application::Get().getRenderer();
					ImGui::TextColored(ImVec4(0.35f, 0.85f, 1.0f, 1.0f), "Draw Calls: %u | Batches: %u | Instances: %u",
						m_renderer->getDrawCallCount(), m_renderer->getBatchCount(), m_renderer->getInstanceCount());
					ImGui::Separator();

					// Prominent Top Screenshot & Lighting Buttons
					if (ImGui::Button("Move Camera to Screenshot Pose [R]", ImVec2(195.0f, 28.0f)))
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
					ImGui::SameLine();
					if (ImGui::Button("Apply Classic Sponza Settings", ImVec2(195.0f, 28.0f)))
					{
						if (pp)
						{
							pp->enableShadows = true;
							pp->enablePCSS = true;
							pp->enableContactShadows = true;
							pp->enableSSAO = true;
							pp->ssaoRadius = 0.55f;
							pp->ssaoBias = 0.015f;
							pp->ssaoIntensity = 0.65f;
							pp->ssaoDirectLightInfluence = 0.25f;

							pp->enableBloom = true;
							pp->bloomThreshold = 1.1f;
							pp->bloomIntensity = 0.08f;

							pp->enableTonemapping = true;
							pp->tonemapper = Core::TonemapperType::ACES;
							pp->exposure = 1.10f;
							pp->enableAutoExposure = false;
							pp->contrast = 1.10f;
							pp->saturation = 1.15f;
							pp->colorTemperature = 0.14f;
							pp->vignetteIntensity = 0.12f;

							pp->enableFXAA = false;
							pp->enableTAA = true;
							pp->enableClusteredLighting = true;
							pp->enableGpuCulling = true;
							pp->fxaaSubpix = 0.85f;
							pp->fxaaEdgeThreshold = 0.125f;
							pp->fxaaEdgeThresholdMin = 0.04f;
							pp->casSharpening = 0.40f;

							pp->enableBoxProjectedIBL = true;
							pp->iblBoxExtents = Core::Vector3(14.5f, 7.5f, 6.5f);
							pp->iblBoxCenter = Core::Vector3(0.0f, 6.5f, -0.35f);
						}
						auto probeView = m_scene->view<Core::ReflectionProbeComponent>();
						for (auto pe : probeView)
						{
							auto& pr = probeView.get<Core::ReflectionProbeComponent>(pe);
							pr.bakedCubemapPath = "Assets/Textures/SponzaAtrium_BakedProbe.dds";
							pr.boxExtents = Core::Vector3(14.5f, 7.5f, 6.5f);
							pr.boxProjection = true;
						}
						if (env)
						{
							env->skyType = Core::SkyType::SkyboxTexture;
							env->skyboxTexturePath = "Assets/Textures/05-18_Day_D.hdr";
							env->skyColor = Core::Color(0.24f, 0.44f, 0.78f, 1.0f);
							env->horizonColor = Core::Color(0.76f, 0.82f, 0.88f, 1.0f);
							env->groundColor = Core::Color(0.42f, 0.35f, 0.26f, 1.0f);
							env->ambientColor = Core::Color(0.18f, 0.22f, 0.28f, 1.0f);
							env->ambientEnergy = 0.20f;
							env->enableFog = false;
							env->fogColor = Core::Color(0.78f, 0.82f, 0.86f, 1.0f);
							env->fogDensity = 0.0015f;
							env->fogHeightFalloff = 0.05f;
							env->fogSunScattering = 0.65f;
						}
						auto lightView = m_scene->view<Core::LightComponent, Core::TransformComponent>();
						for (auto le : lightView)
						{
							auto& lc = lightView.get<Core::LightComponent>(le);
							if (lc.type == Core::LightType::Directional)
							{
								auto& tc = lightView.get<Core::TransformComponent>(le);
								tc.position = Core::Vector3(0.0f, 3.0f, 0.0f);
								tc.setEulerDegrees(Core::Vector3(66.0f, 52.5f, 0.0f));
								lc.color = Core::Color(1.0f, 0.943137f, 0.90f, 1.0f);
								lc.intensity = 4.2f;
								lc.castShadows = true;
								lc.shadowSoftness = 1.35f;
								lc.shadowDistance = 60.0f;
								break;
							}
						}
					}

					ImGui::Separator();

					if (pp)
					{
						// Quick Preset Buttons
						if (ImGui::Button("Enable All Effects", ImVec2(195.0f, 24.0f)))
						{
							pp->enableShadows = true;
							pp->enablePCSS = true;
							pp->enableContactShadows = true;
							pp->enableSSAO = true;
							if (env) env->enableFog = true;
							pp->enableBloom = true;
							pp->enableTonemapping = true;
							pp->enableFXAA = true;
							pp->enableTAA = true;
							pp->enableClusteredLighting = true;
							pp->enableGpuCulling = true;
							pp->enableBoxProjectedIBL = true;
							pp->enableAutoExposure = true;
						}
						ImGui::SameLine();
						if (ImGui::Button("Disable All (Clean PBR)", ImVec2(195.0f, 24.0f)))
						{
							pp->enableShadows = false;
							pp->enablePCSS = false;
							pp->enableContactShadows = false;
							pp->enableSSAO = false;
							if (env) env->enableFog = false;
							pp->enableBloom = false;
							pp->enableTonemapping = false;
							pp->enableFXAA = false;
							pp->enableTAA = false;
							pp->enableTessellation = false;
							pp->enableBoxProjectedIBL = false;
							pp->enableAutoExposure = false;
						}
						if (ImGui::Button("Baseline (Shadows + AA)", ImVec2(-1.0f, 26.0f)))
						{
							pp->enableShadows = true;
							pp->enableFXAA = true;
							pp->enableTonemapping = true;
							pp->enableSSAO = false;
							if (env) env->enableFog = false;
							pp->enableBloom = false;
							pp->enablePCSS = false;
							pp->enableContactShadows = false;
							pp->enableBoxProjectedIBL = false;
							pp->enableAutoExposure = false;
						}

						ImGui::Separator();
						ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "Render Pipeline & Geometry:");
						int curPipeline = static_cast<int>(pp->renderPipeline);
						if (ImGui::Combo("Render Pipeline", &curPipeline, "Forward+ (2-MRT Optimized)\0Deferred PBR (G-Buffer)\0\0"))
						{
							pp->renderPipeline = static_cast<uint8_t>(curPipeline);
						}
						ImGui::Combo("Mesh LOD Mode", &pp->meshLodBias, "Auto (Distance LOD0-LOD2)\0Force LOD0 (Full Detail)\0Force LOD1 (Medium ~50%)\0Force LOD2 (Coarse ~25%)\0\0");
						ImGui::Checkbox("Hardware Tessellation (HS/DS PN-Triangles)", &pp->enableTessellation);
						if (pp->enableTessellation)
						{
							ImGui::Indent();
							ImGui::SliderFloat("Tessellation Factor", &pp->tessellationFactor, 1.0f, 12.0f, "%.1f");
							ImGui::Unindent();
						}

						ImGui::Separator();
						ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "Effect Toggles (Click Button or Checkbox):");

						auto drawToggleButton = [](const char* label, bool& state, const char* hotkey) {
							ImGui::PushID(label);
							ImVec4 btnCol = state ? ImVec4(0.16f, 0.52f, 0.25f, 1.0f) : ImVec4(0.45f, 0.18f, 0.18f, 1.0f);
							ImVec4 hoverCol = state ? ImVec4(0.22f, 0.65f, 0.32f, 1.0f) : ImVec4(0.58f, 0.24f, 0.24f, 1.0f);
							ImGui::PushStyleColor(ImGuiCol_Button, btnCol);
							ImGui::PushStyleColor(ImGuiCol_ButtonHovered, hoverCol);
							char btnText[128];
							std::snprintf(btnText, sizeof(btnText), "%s: %s (%s)", label, state ? "ON" : "OFF", hotkey);
							if (ImGui::Button(btnText, ImVec2(-1.0f, 24.0f)))
							{
								state = !state;
							}
							ImGui::PopStyleColor(2);
							ImGui::PopID();
						};

						drawToggleButton("Shadows (4-Cascade CSM)", pp->enableShadows, "F3");
						if (pp->enableShadows)
						{
							ImGui::Indent();
							ImGui::Checkbox("PCSS Contact-Hardening Soft Shadows", &pp->enablePCSS);
							ImGui::Checkbox("Micro Contact Shadows (Screen-Space)", &pp->enableContactShadows);
							ImGui::Unindent();
						}
						drawToggleButton("SSAO (GTAO Occlusion)", pp->enableSSAO, "F4");
						drawToggleButton("Bloom (HDR Pyramid)", pp->enableBloom, "F8");
						drawToggleButton("SSR (Screen-Space Reflections)", pp->enableSSR, "F9");
						if (pp->enableSSR)
						{
							ImGui::Indent();
							ImGui::SliderFloat("SSR Max Distance", &pp->ssrMaxDistance, 5.0f, 150.0f, "%.1f m");
							ImGui::SliderFloat("SSR Thickness", &pp->ssrThickness, 0.05f, 2.0f, "%.2f");
							ImGui::SliderFloat("SSR Roughness Cutoff", &pp->ssrRoughnessCutoff, 0.1f, 1.0f, "%.2f");
							ImGui::SliderFloat("SSR Intensity", &pp->ssrIntensity, 0.0f, 2.0f, "%.2f");
							ImGui::Unindent();
						}

						auto ddgiView = m_scene->view<Core::DDGIProbeVolumeComponent>();
						auto ddgiIt = ddgiView.begin();
						if (ddgiIt != ddgiView.end())
						{
							auto& ddgi = ddgiView.get<Core::DDGIProbeVolumeComponent>(*ddgiIt);
							drawToggleButton("DDGI Dynamic Probe Volume", ddgi.enabled, "P / I");
							if (ddgi.enabled)
							{
								ImGui::Indent();
								ImGui::SliderFloat("DDGI Intensity", &ddgi.intensity, 0.0f, 3.0f, "%.2f");
								ImGui::SliderFloat("DDGI Normal Bias", &ddgi.normalBias, 0.01f, 0.5f, "%.2f");
								ImGui::Unindent();
							}
						}

						if (env)
						{
							drawToggleButton("Atmospheric Fog", env->enableFog, "Fog");
							int skyMode = (env->skyType == Core::SkyType::SkyboxTexture) ? 1 : 0;
							if (ImGui::Combo("Sky Mode", &skyMode, "Procedural Physical Sky\0HDRI Skybox (05-18_Day_D.hdr)\0\0"))
							{
								if (skyMode == 1)
								{
									env->skyType = Core::SkyType::SkyboxTexture;
									env->skyboxTexturePath = "Assets/Textures/05-18_Day_D.hdr";
								}
								else
								{
									env->skyType = Core::SkyType::ProceduralGradient;
								}
							}
						}
						ImGui::Checkbox("Box-Projected Reflection Probe (Indoor Parallax IBL)", &pp->enableBoxProjectedIBL);
						if (pp->enableBoxProjectedIBL)
						{
							ImGui::Indent();
							ImGui::DragFloat3("Probe Box Extents", &pp->iblBoxExtents.x, 0.5f, 1.0f, 200.0f);
							ImGui::DragFloat3("Probe Center", &pp->iblBoxCenter.x, 0.25f, -100.0f, 100.0f);

							auto probeView = m_scene->view<Core::ReflectionProbeComponent>();
							for (auto pe : probeView)
							{
								auto& pr = probeView.get<Core::ReflectionProbeComponent>(pe);
								ImGui::SliderFloat("Probe Intensity", &pr.intensity, 0.0f, 5.0f, "%.2f");
								bool hasBaked = !pr.bakedCubemapPath.empty();
								if (hasBaked)
								{
									ImGui::TextColored(ImVec4(0.3f, 1.0f, 0.5f, 1.0f), "Baked Probe: %s", pr.bakedCubemapPath.c_str());
									if (ImGui::Button("Switch to Procedural Sky IBL", ImVec2(-1.0f, 22.0f)))
									{
										pr.bakedCubemapPath = "";
									}
								}
								else
								{
									ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Mode: Procedural Gradient Sky IBL");
									if (ImGui::Button("Load Baked Probe (SponzaAtrium_BakedProbe.dds) [B]", ImVec2(-1.0f, 22.0f)))
									{
										pr.bakedCubemapPath = "Assets/Textures/SponzaAtrium_BakedProbe.dds";
									}
								}
							}
							ImGui::Unindent();
						}
						drawToggleButton("Tonemapping (HDR -> SDR)", pp->enableTonemapping, "F10");
						if (pp->enableTonemapping)
						{
							ImGui::Indent();
							ImGui::Checkbox("Auto-Exposure (Eye Adaptation)", &pp->enableAutoExposure);
							ImGui::SliderFloat("Contrast", &pp->contrast, 0.5f, 1.8f, "%.2f");
							ImGui::SliderFloat("Saturation", &pp->saturation, 0.0f, 2.0f, "%.2f");
							ImGui::SliderFloat("Color Temperature", &pp->colorTemperature, -1.0f, 1.0f, "%.2f");
							ImGui::SliderFloat("Optical Vignette", &pp->vignetteIntensity, 0.0f, 1.0f, "%.2f");
							ImGui::Unindent();
						}
						drawToggleButton("FXAA 3.11 (Anti-Aliasing)", pp->enableFXAA, "F11");
						ImGui::Indent();
						ImGui::SliderFloat("CAS Texture Sharpening", &pp->casSharpening, 0.0f, 1.0f, "%.2f");
						ImGui::Unindent();

						drawToggleButton("TAA (Temporal Anti-Aliasing)", pp->enableTAA, "F12");
						drawToggleButton("Clustered Light Culling (16x16x24)", pp->enableClusteredLighting, "Auto");
						drawToggleButton("GPU-Driven Frustum Culling", pp->enableGpuCulling, "Auto");

						ImGui::Separator();
						ImGui::TextColored(ImVec4(0.4f, 0.9f, 1.0f, 1.0f), "Debug Buffer Views [Keys 0 - 7]:");
						int curDbg = static_cast<int>(pp->debugView);
						if (curDbg < 0 || curDbg > 7) curDbg = 0;
						if (ImGui::Combo("Debug View", &curDbg, "Lit (Final)\0SSAO\0Bloom\0Shadow Map\0World Normals\0Albedo\0Metallic / Roughness\0Velocity (Motion Vectors)\0\0"))
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
						if (ImGui::CollapsingHeader("Fine-Tune Effect Parameters"))
						{
							const char* tonemappers[] = { "ACES Filmic", "Reinhard", "Linear", "AgX", "Khronos Neutral" };
							int curTm = static_cast<int>(pp->tonemapper);
							if (ImGui::Combo("Tonemapper", &curTm, tonemappers, 5))
							{
								pp->tonemapper = static_cast<Core::TonemapperType>(curTm);
							}
							ImGui::SliderFloat("Exposure", &pp->exposure, 0.1f, 4.0f, "%.2f");
							ImGui::Checkbox("Auto-Exposure (Eye Adaptation)", &pp->enableAutoExposure);
							ImGui::SliderFloat("Contrast", &pp->contrast, 0.5f, 1.8f, "%.2f");
							ImGui::SliderFloat("Saturation", &pp->saturation, 0.0f, 2.0f, "%.2f");
							ImGui::SliderFloat("Color Temperature", &pp->colorTemperature, -1.0f, 1.0f, "%.2f");
							ImGui::SliderFloat("Optical Vignette", &pp->vignetteIntensity, 0.0f, 1.0f, "%.2f");
							ImGui::SliderFloat("SSAO Intensity", &pp->ssaoIntensity, 0.0f, 3.0f, "%.2f");
							ImGui::SliderFloat("SSAO Radius", &pp->ssaoRadius, 0.1f, 2.0f, "%.2f m");
							ImGui::SliderFloat("SSAO Direct Influence", &pp->ssaoDirectLightInfluence, 0.0f, 1.0f, "%.2f");
							ImGui::SliderFloat("Bloom Intensity", &pp->bloomIntensity, 0.0f, 0.5f, "%.3f");
							ImGui::SliderFloat("Bloom Threshold", &pp->bloomThreshold, 0.1f, 4.0f, "%.2f");
							if (env)
							{
								ImGui::SliderFloat("Ambient Energy", &env->ambientEnergy, 0.0f, 3.0f, "%.2f");
								ImGui::SliderFloat("Fog Density", &env->fogDensity, 0.0f, 0.05f, "%.4f");
								ImGui::SliderFloat("Fog Sun Scatter", &env->fogSunScattering, 0.0f, 1.5f, "%.2f");
							}
						}
					}
				}
				ImGui::End();
			}
		}
	}
}
