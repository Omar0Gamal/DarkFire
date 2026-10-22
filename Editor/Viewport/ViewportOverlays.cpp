#include "Editor/Viewport/ViewportOverlays.h"
#include "Engine/Core/Scene/Components.h"
#include <ImGuizmo.h>
#include <cmath>
#include <algorithm>

namespace Editor
{
	namespace ViewportOverlays
	{
		bool ProjectClippedSegment(
			const Core::Vector3& p0, const Core::Vector3& p1,
			const Core::Matrix& viewProj,
			float startX, float startY, float vpW, float vpH,
			ImVec2& out0, ImVec2& out1)
		{
			Core::Vector4 c0 = Core::Vector4::Transform(Core::Vector4(p0.x, p0.y, p0.z, 1.0f), viewProj);
			Core::Vector4 c1 = Core::Vector4::Transform(Core::Vector4(p1.x, p1.y, p1.z, 1.0f), viewProj);

			const float nearW = 0.05f;
			const float farW  = 1000.0f;

			// Discard if completely outside near or far plane
			if ((c0.w < nearW && c1.w < nearW) || (c0.w > farW && c1.w > farW))
				return false;

			// Clip against near plane
			if (c0.w < nearW)
			{
				float t = (nearW - c0.w) / (c1.w - c0.w);
				c0 = c0 + (c1 - c0) * t;
			}
			else if (c1.w < nearW)
			{
				float t = (nearW - c1.w) / (c0.w - c1.w);
				c1 = c1 + (c0 - c1) * t;
			}

			// Clip against far plane
			if (c0.w > farW)
			{
				float t = (farW - c0.w) / (c1.w - c0.w);
				c0 = c0 + (c1 - c0) * t;
			}
			else if (c1.w > farW)
			{
				float t = (farW - c1.w) / (c0.w - c1.w);
				c1 = c1 + (c0 - c1) * t;
			}

			float ndc0X = c0.x / c0.w;
			float ndc0Y = c0.y / c0.w;
			float ndc1X = c1.x / c1.w;
			float ndc1Y = c1.y / c1.w;

			out0.x = startX + (ndc0X * 0.5f + 0.5f) * vpW;
			out0.y = startY + (0.5f - ndc0Y * 0.5f) * vpH;
			out1.x = startX + (ndc1X * 0.5f + 0.5f) * vpW;
			out1.y = startY + (0.5f - ndc1Y * 0.5f) * vpH;

			return true;
		}

		void DrawGrid3D(
			const Core::Matrix& viewProj,
			const Core::Vector3& camPos,
			float startX, float startY, float vpW, float vpH)
		{
			ImDrawList* drawList = ImGui::GetWindowDrawList();

			float centerSnapX = std::floor(camPos.x);
			float centerSnapZ = std::floor(camPos.z);

			const int halfLines = 150;
			const float range = static_cast<float>(halfLines);

			ImVec2 p0, p1;

			// Lines parallel to Z (varying X)
			for (int i = -halfLines; i <= halfLines; ++i)
			{
				float x = centerSnapX + static_cast<float>(i);
				float zStart = centerSnapZ - range;
				float zEnd   = centerSnapZ + range;

				if (ProjectClippedSegment(
					Core::Vector3(x, 0.0f, zStart),
					Core::Vector3(x, 0.0f, zEnd),
					viewProj, startX, startY, vpW, vpH, p0, p1))
				{
					bool isOrigin = (std::abs(x) < 0.001f);
					bool isMajor = (static_cast<int>(std::round(x)) % 10 == 0);

					float distRatio = static_cast<float>(std::abs(i)) / range;
					float fade = std::clamp((1.0f - distRatio) * (1.0f - distRatio), 0.0f, 1.0f);

					ImU32 col;
					float thickness = 1.0f;
					if (isOrigin)
					{
						col = IM_COL32(60, 140, 240, static_cast<int>(220.0f * fade)); // Blue for Z-axis
						thickness = 2.0f;
					}
					else if (isMajor)
					{
						col = IM_COL32(110, 115, 135, static_cast<int>(130.0f * fade));
						thickness = 1.2f;
					}
					else
					{
						col = IM_COL32(75, 80, 95, static_cast<int>(60.0f * fade));
					}

					if (fade > 0.01f)
					{
						drawList->AddLine(p0, p1, col, thickness);
					}
				}
			}

			// Lines parallel to X (varying Z)
			for (int i = -halfLines; i <= halfLines; ++i)
			{
				float z = centerSnapZ + static_cast<float>(i);
				float xStart = centerSnapX - range;
				float xEnd   = centerSnapX + range;

				if (ProjectClippedSegment(
					Core::Vector3(xStart, 0.0f, z),
					Core::Vector3(xEnd, 0.0f, z),
					viewProj, startX, startY, vpW, vpH, p0, p1))
				{
					bool isOrigin = (std::abs(z) < 0.001f);
					bool isMajor = (static_cast<int>(std::round(z)) % 10 == 0);

					float distRatio = static_cast<float>(std::abs(i)) / range;
					float fade = std::clamp((1.0f - distRatio) * (1.0f - distRatio), 0.0f, 1.0f);

					ImU32 col;
					float thickness = 1.0f;
					if (isOrigin)
					{
						col = IM_COL32(240, 70, 70, static_cast<int>(220.0f * fade)); // Red for X-axis
						thickness = 2.0f;
					}
					else if (isMajor)
					{
						col = IM_COL32(110, 115, 135, static_cast<int>(130.0f * fade));
						thickness = 1.2f;
					}
					else
					{
						col = IM_COL32(75, 80, 95, static_cast<int>(60.0f * fade));
					}

					if (fade > 0.01f)
					{
						drawList->AddLine(p0, p1, col, thickness);
					}
				}
			}
		}

		void DrawViewportIcons(
			Core::Scene* scene,
			SelectionContext& selection,
			const Core::Matrix& viewProj,
			float startX, float startY, float vpW, float vpH,
			bool viewportHovered)
		{
			if (!scene) return;

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 mousePos = ImGui::GetMousePos();
			bool mouseClicked = ImGui::IsMouseClicked(ImGuiMouseButton_Left) &&
				viewportHovered &&
				!ImGuizmo::IsOver() &&
				!ImGuizmo::IsUsing();

			auto projectPoint = [&](const Core::Vector3& v, ImVec2& outPt) -> bool {
				Core::Vector4 pCS = Core::Vector4::Transform(Core::Vector4(v.x, v.y, v.z, 1.0f), viewProj);
				if (pCS.w < 0.05f) return false;
				float ndcX = pCS.x / pCS.w;
				float ndcY = pCS.y / pCS.w;
				outPt.x = startX + (ndcX * 0.5f + 0.5f) * vpW;
				outPt.y = startY + (0.5f - ndcY * 0.5f) * vpH;
				return true;
			};

			// 1. Directional & Point Lights
			auto lightView = scene->view<Core::LightComponent, Core::TransformComponent>();
			for (auto handle : lightView)
			{
				Core::Entity entity{ handle, scene };
				const auto& tc = lightView.get<Core::TransformComponent>(handle);
				const auto& light = lightView.get<Core::LightComponent>(handle);

				bool isSelected = selection.isSelected(entity);
				ImVec2 screenPos;
				if (!projectPoint(tc.position, screenPos)) continue;

				if (screenPos.x < startX || screenPos.x > startX + vpW ||
					screenPos.y < startY || screenPos.y > startY + vpH)
					continue;

				if (light.type == Core::LightType::Directional)
				{
					float radius = 10.0f;
					ImU32 sunCoreCol = isSelected ? IM_COL32(255, 240, 100, 255) : IM_COL32(255, 205, 50, 220);
					ImU32 sunRayCol  = isSelected ? IM_COL32(255, 255, 180, 255) : IM_COL32(255, 180, 30, 180);

					drawList->AddCircleFilled(screenPos, radius, sunCoreCol, 16);
					drawList->AddCircle(screenPos, radius + 1.5f, IM_COL32(40, 30, 0, 150), 16, 1.0f);

					for (int r = 0; r < 8; ++r)
					{
						float angle = r * (3.14159265f / 4.0f);
						ImVec2 rStart(screenPos.x + std::cos(angle) * (radius + 2.0f), screenPos.y + std::sin(angle) * (radius + 2.0f));
						ImVec2 rEnd(screenPos.x + std::cos(angle) * (radius + 6.0f), screenPos.y + std::sin(angle) * (radius + 6.0f));
						drawList->AddLine(rStart, rEnd, sunRayCol, 1.5f);
					}

					Core::Vector3 fwd = tc.getForward();
					ImVec2 arr0, arr1;
					if (ProjectClippedSegment(tc.position, tc.position + fwd * 2.0f, viewProj, startX, startY, vpW, vpH, arr0, arr1))
					{
						drawList->AddLine(arr0, arr1, IM_COL32(255, 220, 80, 220), 2.0f);
						drawList->AddCircleFilled(arr1, 3.5f, IM_COL32(255, 230, 120, 240));
					}

					float distSq = (mousePos.x - screenPos.x) * (mousePos.x - screenPos.x) + (mousePos.y - screenPos.y) * (mousePos.y - screenPos.y);
					if (mouseClicked && distSq < 16.0f * 16.0f)
					{
						selection.select(entity, false);
					}
				}
				else if (light.type == Core::LightType::Point)
				{
					float radius = 9.0f;
					ImU32 bulbCol = isSelected ? IM_COL32(255, 240, 140, 255) : IM_COL32(240, 180, 70, 220);
					drawList->AddCircleFilled(screenPos, radius, bulbCol, 16);
					drawList->AddCircle(screenPos, radius + 1.0f, IM_COL32(50, 40, 10, 150), 16, 1.0f);

					drawList->AddLine(ImVec2(screenPos.x - 3, screenPos.y - 2), ImVec2(screenPos.x + 3, screenPos.y - 2), IM_COL32(255, 255, 255, 200), 1.0f);

					if (isSelected && light.range > 0.1f)
					{
						const int segs = 32;
						for (int s = 0; s < segs; ++s)
						{
							float a0 = s * (6.2831853f / segs);
							float a1 = (s + 1) * (6.2831853f / segs);
							Core::Vector3 cp0 = tc.position + Core::Vector3(std::cos(a0) * light.range, 0.0f, std::sin(a0) * light.range);
							Core::Vector3 cp1 = tc.position + Core::Vector3(std::cos(a1) * light.range, 0.0f, std::sin(a1) * light.range);
							ImVec2 sp0, sp1;
							if (ProjectClippedSegment(cp0, cp1, viewProj, startX, startY, vpW, vpH, sp0, sp1))
							{
								drawList->AddLine(sp0, sp1, IM_COL32(255, 210, 70, 140), 1.2f);
							}
						}
					}

					float distSq = (mousePos.x - screenPos.x) * (mousePos.x - screenPos.x) + (mousePos.y - screenPos.y) * (mousePos.y - screenPos.y);
					if (mouseClicked && distSq < 14.0f * 14.0f)
					{
						selection.select(entity, false);
					}
				}
			}

			// 2. Cameras
			auto camView = scene->view<Core::CameraComponent, Core::TransformComponent>();
			for (auto handle : camView)
			{
				Core::Entity entity{ handle, scene };
				const auto& tc = camView.get<Core::TransformComponent>(handle);
				const auto& cam = camView.get<Core::CameraComponent>(handle);

				bool isSelected = selection.isSelected(entity);
				ImVec2 screenPos;
				if (!projectPoint(tc.position, screenPos)) continue;

				if (screenPos.x >= startX && screenPos.x <= startX + vpW &&
					screenPos.y >= startY && screenPos.y <= startY + vpH)
				{
					ImU32 camCol = isSelected ? IM_COL32(100, 220, 255, 255) : IM_COL32(60, 160, 220, 220);
					ImVec2 bMin(screenPos.x - 9.0f, screenPos.y - 6.0f);
					ImVec2 bMax(screenPos.x + 5.0f, screenPos.y + 6.0f);
					drawList->AddRectFilled(bMin, bMax, camCol, 2.0f);
					drawList->AddRect(bMin, bMax, IM_COL32(10, 30, 50, 180), 2.0f, 0, 1.0f);

					ImVec2 lensPts[3] = {
						ImVec2(screenPos.x + 5.0f, screenPos.y - 3.5f),
						ImVec2(screenPos.x + 10.0f, screenPos.y - 6.5f),
						ImVec2(screenPos.x + 10.0f, screenPos.y + 6.5f)
					};
					drawList->AddTriangleFilled(lensPts[0], lensPts[1], lensPts[2], camCol);

					float distSq = (mousePos.x - screenPos.x) * (mousePos.x - screenPos.x) + (mousePos.y - screenPos.y) * (mousePos.y - screenPos.y);
					if (mouseClicked && distSq < 15.0f * 15.0f)
					{
						selection.select(entity, false);
					}
				}

				if (isSelected)
				{
					float dist = 3.5f;
					float fovRads = Core::Math::toRadians(cam.fov);
					float h = 2.0f * dist * std::tan(fovRads * 0.5f);
					float w = h * (16.0f / 9.0f);

					Core::Vector3 fwd = tc.getForward();
					Core::Vector3 right = tc.getRight();
					Core::Vector3 up = tc.getUp();

					Core::Vector3 center = tc.position + fwd * dist;
					Core::Vector3 halfR = right * (w * 0.5f);
					Core::Vector3 halfU = up * (h * 0.5f);

					Core::Vector3 c0 = center - halfR - halfU;
					Core::Vector3 c1 = center + halfR - halfU;
					Core::Vector3 c2 = center + halfR + halfU;
					Core::Vector3 c3 = center - halfR + halfU;

					ImVec2 p0, p1;
					ImU32 frustumCol = IM_COL32(80, 200, 255, 200);

					if (ProjectClippedSegment(c0, c1, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.5f);
					if (ProjectClippedSegment(c1, c2, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.5f);
					if (ProjectClippedSegment(c2, c3, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.5f);
					if (ProjectClippedSegment(c3, c0, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.5f);

					if (ProjectClippedSegment(tc.position, c0, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.2f);
					if (ProjectClippedSegment(tc.position, c1, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.2f);
					if (ProjectClippedSegment(tc.position, c2, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.2f);
					if (ProjectClippedSegment(tc.position, c3, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.2f);

					Core::Vector3 topMid = (c2 + c3) * 0.5f;
					Core::Vector3 topApex = topMid + up * (h * 0.25f);
					if (ProjectClippedSegment(c3, topApex, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.2f);
					if (ProjectClippedSegment(c2, topApex, viewProj, startX, startY, vpW, vpH, p0, p1)) drawList->AddLine(p0, p1, frustumCol, 1.2f);
				}
			}
		}

		void DrawSelectionWireframes(
			const SelectionContext& selection,
			const Core::Matrix& viewProj,
			float startX, float startY, float vpW, float vpH)
		{
			if (!selection.hasSelection()) return;

			ImDrawList* drawList = ImGui::GetWindowDrawList();

			for (auto entity : selection.getEntities())
			{
				if (!entity.isValid() || !entity.hasComponent<Core::TransformComponent>()) continue;

				const auto& tc = entity.getComponent<Core::TransformComponent>();
				Core::Vector3 p = tc.position;
				Core::Vector3 s = tc.scale * 0.5f;

				Core::Vector3 corners[8] = {
					p + Core::Vector3(-s.x, -s.y, -s.z),
					p + Core::Vector3( s.x, -s.y, -s.z),
					p + Core::Vector3( s.x,  s.y, -s.z),
					p + Core::Vector3(-s.x,  s.y, -s.z),
					p + Core::Vector3(-s.x, -s.y,  s.z),
					p + Core::Vector3( s.x, -s.y,  s.z),
					p + Core::Vector3( s.x,  s.y,  s.z),
					p + Core::Vector3(-s.x,  s.y,  s.z)
				};

				static const int edges[12][2] = {
					{0, 1}, {1, 2}, {2, 3}, {3, 0},
					{4, 5}, {5, 6}, {6, 7}, {7, 4},
					{0, 4}, {1, 5}, {2, 6}, {3, 7}
				};

				ImU32 colCoral = IM_COL32(252, 127, 127, 240); // Godot Coral Red
				for (int e = 0; e < 12; ++e)
				{
					ImVec2 ptA, ptB;
					if (ProjectClippedSegment(corners[edges[e][0]], corners[edges[e][1]], viewProj, startX, startY, vpW, vpH, ptA, ptB))
					{
						drawList->AddLine(ptA, ptB, colCoral, 1.5f);
					}
				}
			}
		}

		void DrawOrientationGizmo(
			const Core::Matrix& viewMat,
			const Core::Vector2& viewportPos,
			const Core::Vector2& viewportSize)
		{
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			float radius = 32.0f;
			ImVec2 center(viewportPos.x + viewportSize.x - radius - 16.0f, viewportPos.y + radius + 16.0f);

			auto projectAxis = [&](const Core::Vector3& axis) -> ImVec2 {
				Core::Vector3 transformed = Core::Vector3::TransformNormal(axis, viewMat);
				return ImVec2(center.x + transformed.x * radius, center.y - transformed.y * radius);
			};

			ImVec2 ptX = projectAxis(Core::Vector3(1, 0, 0));
			ImVec2 ptY = projectAxis(Core::Vector3(0, 1, 0));
			ImVec2 ptZ = projectAxis(Core::Vector3(0, 0, 1));

			// Base circle
			drawList->AddCircleFilled(center, radius + 4.0f, IM_COL32(30, 34, 40, 180));
			drawList->AddCircle(center, radius + 4.0f, IM_COL32(50, 55, 65, 200), 24, 1.0f);

			// X Axis (Red)
			drawList->AddLine(center, ptX, IM_COL32(230, 60, 60, 255), 2.0f);
			drawList->AddCircleFilled(ptX, 6.0f, IM_COL32(230, 60, 60, 255));

			// Y Axis (Green)
			drawList->AddLine(center, ptY, IM_COL32(60, 210, 80, 255), 2.0f);
			drawList->AddCircleFilled(ptY, 6.0f, IM_COL32(60, 210, 80, 255));

			// Z Axis (Blue)
			drawList->AddLine(center, ptZ, IM_COL32(60, 120, 230, 255), 2.0f);
			drawList->AddCircleFilled(ptZ, 6.0f, IM_COL32(60, 120, 230, 255));
		}

		void DrawCameraPiP(
			std::shared_ptr<Core::RHI::IGpuTexture> gameViewportTexture,
			const Core::Vector2& viewportPos,
			const Core::Vector2& viewportSize)
		{
			if (!gameViewportTexture || !gameViewportTexture->getShaderResourceView()) return;

			float pipW = 240.0f;
			float pipH = 135.0f;
			ImVec2 pipPos(viewportPos.x + viewportSize.x - pipW - 16.0f, viewportPos.y + viewportSize.y - pipH - 16.0f);

			ImDrawList* drawList = ImGui::GetWindowDrawList();
			drawList->AddRectFilled(ImVec2(pipPos.x - 2, pipPos.y - 20), ImVec2(pipPos.x + pipW + 2, pipPos.y + pipH + 2), IM_COL32(24, 28, 34, 230), 4.0f);
			drawList->AddRect(ImVec2(pipPos.x - 2, pipPos.y - 20), ImVec2(pipPos.x + pipW + 2, pipPos.y + pipH + 2), IM_COL32(71, 140, 191, 200), 4.0f, 0, 1.5f);
			drawList->AddText(ImVec2(pipPos.x + 6, pipPos.y - 18), IM_COL32(200, 210, 225, 255), "Camera Preview (PiP)");

			drawList->AddImage(
				(ImTextureID)gameViewportTexture->getShaderResourceView(),
				pipPos,
				ImVec2(pipPos.x + pipW, pipPos.y + pipH)
			);
		}
	}
}
