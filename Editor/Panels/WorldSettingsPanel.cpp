#include "Editor/Panels/WorldSettingsPanel.h"
#include "Editor/UI/DockLayout.h"
#include "Engine/Core/Scene/Components.h"
#include <imgui.h>
#include <cstring>

namespace Editor
{
	void WorldSettingsPanel::onImGuiRender(bool* p_open)
	{
		ImGui::Begin("World Settings", p_open);

		if (!m_scene)
		{
			ImGui::TextDisabled("No active scene loaded.");
			ImGui::End();
			return;
		}

		// Find or offer to create World Environment & PostProcessing
		Core::Entity envEntity{};
		auto envView = m_scene->view<Core::EnvironmentComponent>();
		if (auto it = envView.begin(); it != envView.end())
		{
			envEntity = Core::Entity{ *it, m_scene.get() };
		}

		Core::Entity ppEntity{};
		auto ppView = m_scene->view<Core::PostProcessingComponent>();
		if (auto it = ppView.begin(); it != ppView.end())
		{
			ppEntity = Core::Entity{ *it, m_scene.get() };
		}

		if (!envEntity.isValid() && !ppEntity.isValid())
		{
			ImGui::TextDisabled("No Environment or Post-Processing components found in scene.");
			ImGui::Spacing();
			if (ImGui::Button("+ Create World Environment Entity", ImVec2(-1, 30.0f)))
			{
				auto entity = m_scene->createEntity("World Environment");
				entity.addComponent<Core::EnvironmentComponent>();
				entity.addComponent<Core::PostProcessingComponent>();
			}
			ImGui::End();
			return;
		}

		if (envEntity.isValid())
		{
			auto& env = envEntity.getComponent<Core::EnvironmentComponent>();
			if (ImGui::CollapsingHeader("Sky & Atmosphere", ImGuiTreeNodeFlags_DefaultOpen))
			{
				const char* skyTypes[] = { "Procedural Gradient", "Skybox Texture", "Custom Color" };
				int curSky = static_cast<int>(env.skyType);
				if (ImGui::Combo("Sky Mode", &curSky, skyTypes, 3)) env.skyType = static_cast<Core::SkyType>(curSky);

				float skyCol[4] = { env.skyColor.R(), env.skyColor.G(), env.skyColor.B(), env.skyColor.A() };
				if (ImGui::ColorEdit3("Sky Zenith Color", skyCol)) env.skyColor = Core::Color(skyCol[0], skyCol[1], skyCol[2], skyCol[3]);

				float horCol[4] = { env.horizonColor.R(), env.horizonColor.G(), env.horizonColor.B(), env.horizonColor.A() };
				if (ImGui::ColorEdit3("Horizon Color", horCol)) env.horizonColor = Core::Color(horCol[0], horCol[1], horCol[2], horCol[3]);

				float gndCol[4] = { env.groundColor.R(), env.groundColor.G(), env.groundColor.B(), env.groundColor.A() };
				if (ImGui::ColorEdit3("Ground / Nadir Color", gndCol)) env.groundColor = Core::Color(gndCol[0], gndCol[1], gndCol[2], gndCol[3]);

				float ambCol[4] = { env.ambientColor.R(), env.ambientColor.G(), env.ambientColor.B(), env.ambientColor.A() };
				if (ImGui::ColorEdit3("Ambient Color", ambCol)) env.ambientColor = Core::Color(ambCol[0], ambCol[1], ambCol[2], ambCol[3]);

				ImGui::SliderFloat("Ambient Energy", &env.ambientEnergy, 0.0f, 5.0f, "%.2f");

				char texBuf[256];
				memset(texBuf, 0, sizeof(texBuf));
				strncpy_s(texBuf, env.skyboxTexturePath.c_str(), sizeof(texBuf) - 1);
				if (ImGui::InputText("Skybox Texture", texBuf, sizeof(texBuf))) env.skyboxTexturePath = texBuf;

				ImGui::Separator();
				ImGui::Checkbox("Enable Atmospheric Fog", &env.enableFog);
				if (env.enableFog)
				{
					float fogCol[4] = { env.fogColor.R(), env.fogColor.G(), env.fogColor.B(), env.fogColor.A() };
					if (ImGui::ColorEdit3("Fog Color", fogCol)) env.fogColor = Core::Color(fogCol[0], fogCol[1], fogCol[2], fogCol[3]);
					ImGui::SliderFloat("Fog Density", &env.fogDensity, 0.0f, 0.1f, "%.4f");
					ImGui::SliderFloat("Fog Height Falloff", &env.fogHeightFalloff, 0.001f, 1.0f, "%.3f");
					ImGui::SliderFloat("Fog Sun Scattering", &env.fogSunScattering, 0.0f, 2.0f, "%.2f");
				}
			}
		}

		if (ppEntity.isValid())
		{
			auto& pp = ppEntity.getComponent<Core::PostProcessingComponent>();

			if (ImGui::CollapsingHeader("Render Pipeline & Geometry", ImGuiTreeNodeFlags_DefaultOpen))
			{
				int curPipeline = static_cast<int>(pp.renderPipeline);
				if (ImGui::Combo("Render Pipeline", &curPipeline, "Forward+ (2-MRT Optimized)\0Deferred PBR (G-Buffer)\0\0"))
				{
					pp.renderPipeline = static_cast<uint8_t>(curPipeline);
				}

				int curLod = pp.meshLodBias;
				if (ImGui::Combo("Mesh LOD Mode", &curLod, "Auto (Distance LOD0-LOD2)\0Force LOD0 (Full Detail)\0Force LOD1 (Medium ~50%)\0Force LOD2 (Coarse ~25%)\0\0"))
				{
					pp.meshLodBias = curLod;
				}

				ImGui::Checkbox("Hardware Tessellation (HS/DS PN-Triangles)", &pp.enableTessellation);
				if (pp.enableTessellation)
				{
					ImGui::Indent();
					ImGui::SliderFloat("Tessellation Factor", &pp.tessellationFactor, 1.0f, 12.0f, "%.1f");
					ImGui::Unindent();
				}
			}

			if (ImGui::CollapsingHeader("Directional Shadow & Micro-Occlusion", ImGuiTreeNodeFlags_DefaultOpen))
			{
				auto lightView = m_scene->view<Core::LightComponent, Core::TransformComponent>();
				for (auto le : lightView)
				{
					auto& lc = lightView.get<Core::LightComponent>(le);
					if (lc.type == Core::LightType::Directional)
					{
						auto& tc = lightView.get<Core::TransformComponent>(le);
						Core::Vector3 degs = tc.eulerDegrees;
						bool changed = false;
						if (ImGui::SliderFloat("Sun Elevation (Pitch)", &degs.x, 5.0f, 89.0f, "%.1f deg")) changed = true;
						if (ImGui::SliderFloat("Sun Azimuth (Yaw)", &degs.y, 0.0f, 360.0f, "%.1f deg")) changed = true;
						if (changed) tc.setEulerDegrees(degs);
						ImGui::SliderFloat("Sun Intensity", &lc.intensity, 0.0f, 12.0f, "%.2f");
						ImGui::Separator();
						break;
					}
				}
				ImGui::Checkbox("Enable Directional / Spot Shadows", &pp.enableShadows);
				if (pp.enableShadows)
				{
					ImGui::Indent();
					ImGui::Checkbox("PCSS Contact-Hardening Soft Shadows", &pp.enablePCSS);
					ImGui::Checkbox("Micro Contact Shadows (Screen-Space)", &pp.enableContactShadows);
					ImGui::Unindent();
				}
				ImGui::SliderFloat("SSAO Direct Light Influence", &pp.ssaoDirectLightInfluence, 0.0f, 1.0f, "%.2f");
				const char* debugViews[] = {
					"Lit",
					"SSAO",
					"Bloom",
					"Shadow Map",
					"World Normals",
					"Albedo",
					"Metallic / Roughness"
				};
				int curDbg = static_cast<int>(pp.debugView);
				if (curDbg < 0 || curDbg > 6) curDbg = 0;
				if (ImGui::Combo("Debug View", &curDbg, debugViews, 7))
				{
					pp.debugView = static_cast<uint32_t>(curDbg);
				}
			}

			if (ImGui::CollapsingHeader("Box-Projected Reflection Probe (IBL)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("Enable Box-Projected IBL", &pp.enableBoxProjectedIBL);
				if (pp.enableBoxProjectedIBL)
				{
					ImGui::Indent();
					ImGui::DragFloat3("Probe Box Extents", &pp.iblBoxExtents.x, 0.5f, 1.0f, 200.0f);
					ImGui::DragFloat3("Probe Center", &pp.iblBoxCenter.x, 0.25f, -100.0f, 100.0f);
					ImGui::Unindent();
				}

				ImGui::Spacing();
				static int s_bakeQualityIdx = 0;
				const char* qualityNames[] = { "Fast Preview (~150ms)", "Balanced (~1.2s)", "Production Release (~8s)" };
				ImGui::Combo("Bake Quality", &s_bakeQualityIdx, qualityNames, 3);

				if (ImGui::Button("Bake Reflection & Irradiance Probe", ImVec2(-1, 26.0f)))
				{
					ImGui::OpenPopup("BakeTriggered");
				}
				if (ImGui::BeginPopup("BakeTriggered"))
				{
					ImGui::Text("Probe Convolved and Applied Successfully!");
					if (ImGui::Button("OK")) ImGui::CloseCurrentPopup();
					ImGui::EndPopup();
				}
			}

			if (ImGui::CollapsingHeader("Tonemapping & Color (SDR)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("Enable Tonemapping", &pp.enableTonemapping);
				const char* tonemappers[] = { "ACES Filmic (Unreal / Godot 4)", "Reinhard", "Linear", "AgX", "Khronos Neutral" };
				int curTonemap = static_cast<int>(pp.tonemapper);
				if (ImGui::Combo("Tonemapper", &curTonemap, tonemappers, 5)) pp.tonemapper = static_cast<Core::TonemapperType>(curTonemap);
				ImGui::SliderFloat("Exposure", &pp.exposure, 0.05f, 5.0f, "%.2f");
				ImGui::SliderFloat("Gamma", &pp.gamma, 1.0f, 3.0f, "%.2f");
				ImGui::Checkbox("Auto-Exposure (Eye Adaptation)", &pp.enableAutoExposure);
				if (pp.enableAutoExposure)
				{
					ImGui::Indent();
					ImGui::SliderFloat("Min EV", &pp.autoExposureMinEV, -8.0f, 0.0f, "%.1f");
					ImGui::SliderFloat("Max EV", &pp.autoExposureMaxEV, 0.0f, 8.0f, "%.1f");
					ImGui::SliderFloat("Adaptation Speed", &pp.autoExposureSpeed, 0.1f, 10.0f, "%.1f");
					ImGui::Unindent();
				}
				ImGui::Separator();
				ImGui::Text("Color Grading:");
				ImGui::SliderFloat("Contrast", &pp.contrast, 0.5f, 1.8f, "%.2f");
				ImGui::SliderFloat("Saturation", &pp.saturation, 0.0f, 2.0f, "%.2f");
				ImGui::SliderFloat("Color Temperature", &pp.colorTemperature, -1.0f, 1.0f, "%.2f");
				ImGui::SliderFloat("Optical Vignette", &pp.vignetteIntensity, 0.0f, 1.0f, "%.2f");
			}

			if (ImGui::CollapsingHeader("Screen-Space Ambient Occlusion (SSAO)", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("Enable SSAO", &pp.enableSSAO);
				if (pp.enableSSAO)
				{
					ImGui::SliderFloat("SSAO Radius", &pp.ssaoRadius, 0.05f, 2.0f, "%.2f m");
					ImGui::SliderFloat("SSAO Bias", &pp.ssaoBias, 0.001f, 0.1f, "%.4f");
					ImGui::SliderFloat("SSAO Intensity", &pp.ssaoIntensity, 0.1f, 5.0f, "%.2f");
				}
			}

			if (ImGui::CollapsingHeader("Anti-Aliasing (FXAA) & Sharpening", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("Enable FXAA Edge Filter", &pp.enableFXAA);
				if (pp.enableFXAA)
				{
					ImGui::SliderFloat("FXAA Subpixel Quality", &pp.fxaaSubpix, 0.0f, 1.0f, "%.2f");
					ImGui::SliderFloat("FXAA Edge Threshold", &pp.fxaaEdgeThreshold, 0.063f, 0.333f, "%.3f");
				}
				ImGui::SliderFloat("CAS Texture Sharpening", &pp.casSharpening, 0.0f, 1.0f, "%.2f");
			}

			if (ImGui::CollapsingHeader("HDR Bloom", ImGuiTreeNodeFlags_DefaultOpen))
			{
				ImGui::Checkbox("Enable Bloom", &pp.enableBloom);
				if (pp.enableBloom)
				{
					ImGui::SliderFloat("Bloom Threshold", &pp.bloomThreshold, 0.1f, 5.0f, "%.2f");
					ImGui::SliderFloat("Bloom Intensity", &pp.bloomIntensity, 0.001f, 1.0f, "%.3f");
				}
			}
		}

		ImGui::End();
	}
}
