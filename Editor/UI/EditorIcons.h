#pragma once

#include "Engine/Core/Defines.h"
#include <imgui.h>
#include <cmath>

namespace Editor::Icons
{
	inline void DrawPlayIcon(ImDrawList* drawList, ImVec2 pos, float size, ImU32 color = IM_COL32(75, 181, 67, 255))
	{
		ImVec2 p1 = ImVec2(pos.x, pos.y);
		ImVec2 p2 = ImVec2(pos.x, pos.y + size);
		ImVec2 p3 = ImVec2(pos.x + size * 0.9f, pos.y + size * 0.5f);
		drawList->AddTriangleFilled(p1, p2, p3, color);
	}

	inline void DrawPauseIcon(ImDrawList* drawList, ImVec2 pos, float size, ImU32 color = IM_COL32(255, 184, 48, 255))
	{
		float w = size * 0.3f;
		drawList->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + size), color, 1.0f);
		drawList->AddRectFilled(ImVec2(pos.x + size - w, pos.y), ImVec2(pos.x + size, pos.y + size), color, 1.0f);
	}

	inline void DrawStepIcon(ImDrawList* drawList, ImVec2 pos, float size, ImU32 color = IM_COL32(56, 182, 255, 255))
	{
		float w = size * 0.2f;
		drawList->AddRectFilled(pos, ImVec2(pos.x + w, pos.y + size), color, 1.0f);
		ImVec2 p1 = ImVec2(pos.x + w + 2.0f, pos.y);
		ImVec2 p2 = ImVec2(pos.x + w + 2.0f, pos.y + size);
		ImVec2 p3 = ImVec2(pos.x + size, pos.y + size * 0.5f);
		drawList->AddTriangleFilled(p1, p2, p3, color);
	}

	inline void DrawStopIcon(ImDrawList* drawList, ImVec2 pos, float size, ImU32 color = IM_COL32(230, 57, 70, 255))
	{
		drawList->AddRectFilled(pos, ImVec2(pos.x + size, pos.y + size), color, 2.0f);
	}

	inline void DrawSimulateIcon(ImDrawList* drawList, ImVec2 pos, float size, ImU32 color = IM_COL32(255, 146, 51, 255))
	{
		// Lightning bolt shape
		ImVec2 pts[6] = {
			ImVec2(pos.x + size * 0.55f, pos.y),
			ImVec2(pos.x + size * 0.15f, pos.y + size * 0.55f),
			ImVec2(pos.x + size * 0.45f, pos.y + size * 0.55f),
			ImVec2(pos.x + size * 0.35f, pos.y + size),
			ImVec2(pos.x + size * 0.85f, pos.y + size * 0.40f),
			ImVec2(pos.x + size * 0.55f, pos.y + size * 0.40f)
		};
		drawList->AddConvexPolyFilled(pts, 6, color);
	}

	inline void DrawStandaloneIcon(ImDrawList* drawList, ImVec2 pos, float size, ImU32 color = IM_COL32(71, 140, 191, 255))
	{
		// Rocket shape
		ImVec2 tip(pos.x + size * 0.5f, pos.y);
		ImVec2 bLeft(pos.x + size * 0.25f, pos.y + size * 0.75f);
		ImVec2 bRight(pos.x + size * 0.75f, pos.y + size * 0.75f);
		drawList->AddTriangleFilled(tip, bLeft, bRight, color);
		drawList->AddTriangleFilled(bLeft, ImVec2(pos.x + size * 0.1f, pos.y + size), ImVec2(pos.x + size * 0.35f, pos.y + size * 0.75f), color);
		drawList->AddTriangleFilled(bRight, ImVec2(pos.x + size * 0.65f, pos.y + size * 0.75f), ImVec2(pos.x + size * 0.9f, pos.y + size), color);
	}

	inline void DrawNode3DIcon(ImDrawList* drawList, ImVec2 pos, float size, ImU32 color = IM_COL32(252, 127, 127, 255))
	{
		// Godot 3D Node red cube isometric
		float cx = pos.x + size * 0.5f;
		float cy = pos.y + size * 0.5f;
		float r = size * 0.45f;
		float h = r * 0.55f;

		ImVec2 top(cx, cy - r);
		ImVec2 topR(cx + r * 0.866f, cy - h);
		ImVec2 topL(cx - r * 0.866f, cy - h);
		ImVec2 mid(cx, cy);
		ImVec2 botR(cx + r * 0.866f, cy + h);
		ImVec2 botL(cx - r * 0.866f, cy + h);
		ImVec2 bot(cx, cy + r);

		drawList->AddLine(top, topR, color, 1.5f);
		drawList->AddLine(topR, botR, color, 1.5f);
		drawList->AddLine(botR, bot, color, 1.5f);
		drawList->AddLine(bot, botL, color, 1.5f);
		drawList->AddLine(botL, topL, color, 1.5f);
		drawList->AddLine(topL, top, color, 1.5f);
		drawList->AddLine(mid, top, color, 1.5f);
		drawList->AddLine(mid, botR, color, 1.5f);
		drawList->AddLine(mid, botL, color, 1.5f);
	}

	inline void DrawMeshIcon(ImDrawList* drawList, ImVec2 pos, float size, ImU32 color = IM_COL32(181, 203, 232, 255))
	{
		DrawNode3DIcon(drawList, pos, size, color);
	}

	inline void DrawCameraIcon(ImDrawList* drawList, ImVec2 pos, float size, ImU32 color = IM_COL32(83, 223, 186, 255))
	{
		// Video camera outline
		float bodyW = size * 0.65f;
		float bodyH = size * 0.55f;
		drawList->AddRectFilled(ImVec2(pos.x, pos.y + size * 0.25f), ImVec2(pos.x + bodyW, pos.y + size * 0.25f + bodyH), color, 2.0f);
		ImVec2 lensP1(pos.x + bodyW, pos.y + size * 0.35f);
		ImVec2 lensP2(pos.x + bodyW, pos.y + size * 0.70f);
		ImVec2 lensP3(pos.x + size, pos.y + size * 0.85f);
		ImVec2 lensP4(pos.x + size, pos.y + size * 0.20f);
		ImVec2 lensPts[4] = { lensP1, lensP4, lensP3, lensP2 };
		drawList->AddConvexPolyFilled(lensPts, 4, color);
	}

	inline void DrawLightIcon(ImDrawList* drawList, ImVec2 pos, float size, ImU32 color = IM_COL32(255, 224, 102, 255))
	{
		// Sun with rays
		float cx = pos.x + size * 0.5f;
		float cy = pos.y + size * 0.5f;
		float r = size * 0.25f;
		drawList->AddCircleFilled(ImVec2(cx, cy), r, color);

		float rayLen = size * 0.45f;
		for (int i = 0; i < 8; ++i)
		{
			float angle = i * (3.14159265f / 4.0f);
			float cosA = std::cos(angle);
			float sinA = std::sin(angle);
			drawList->AddLine(
				ImVec2(cx + (r + 1.5f) * cosA, cy + (r + 1.5f) * sinA),
				ImVec2(cx + rayLen * cosA, cy + rayLen * sinA),
				color, 1.2f
			);
		}
	}

	inline void DrawRigidBodyIcon(ImDrawList* drawList, ImVec2 pos, float size, ImU32 color = IM_COL32(141, 165, 243, 255))
	{
		// Physics sphere with equator ring
		float cx = pos.x + size * 0.5f;
		float cy = pos.y + size * 0.5f;
		float r = size * 0.4f;
		drawList->AddCircle(ImVec2(cx, cy), r, color, 16, 1.5f);
		drawList->AddEllipse(ImVec2(cx, cy), ImVec2(r, r * 0.4f), color, 0.0f, 16, 1.2f);
	}

	inline void DrawColliderIcon(ImDrawList* drawList, ImVec2 pos, float size, ImU32 color = IM_COL32(109, 224, 167, 255))
	{
		// Dashed box
		drawList->AddRect(
			ImVec2(pos.x + 1.0f, pos.y + 1.0f),
			ImVec2(pos.x + size - 1.0f, pos.y + size - 1.0f),
			color, 2.0f, 0, 1.5f
		);
	}

	inline void DrawScriptIcon(ImDrawList* drawList, ImVec2 pos, float size, ImU32 color = IM_COL32(142, 239, 151, 255))
	{
		// Curly braces { } or document scroll
		float w = size * 0.7f;
		float h = size * 0.9f;
		float x0 = pos.x + size * 0.15f;
		float y0 = pos.y + size * 0.05f;

		// Document body
		drawList->AddRect(ImVec2(x0, y0), ImVec2(x0 + w, y0 + h), color, 2.0f, 0, 1.5f);
		// Text lines
		drawList->AddLine(ImVec2(x0 + w * 0.25f, y0 + h * 0.35f), ImVec2(x0 + w * 0.75f, y0 + h * 0.35f), color, 1.2f);
		drawList->AddLine(ImVec2(x0 + w * 0.25f, y0 + h * 0.55f), ImVec2(x0 + w * 0.75f, y0 + h * 0.55f), color, 1.2f);
		drawList->AddLine(ImVec2(x0 + w * 0.25f, y0 + h * 0.75f), ImVec2(x0 + w * 0.55f, y0 + h * 0.75f), color, 1.2f);
	}

	inline void DrawFolderIcon(ImDrawList* drawList, ImVec2 pos, float size, ImU32 color = IM_COL32(245, 192, 74, 255))
	{
		float x = pos.x;
		float y = pos.y;
		float w = size;
		float h = size * 0.8f;
		// Tab
		drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + w * 0.45f, y + h * 0.3f), color, 1.0f);
		// Main folder body
		drawList->AddRectFilled(ImVec2(x, y + h * 0.2f), ImVec2(x + w, y + h), color, 2.0f);
	}

	inline void DrawFileIcon(ImDrawList* drawList, ImVec2 pos, float size, ImU32 color = IM_COL32(200, 205, 215, 255))
	{
		float x = pos.x + size * 0.15f;
		float y = pos.y + size * 0.05f;
		float w = size * 0.7f;
		float h = size * 0.9f;
		drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), color, 1.5f);
		// Corner fold
		ImVec2 fold[3] = { ImVec2(x + w * 0.6f, y), ImVec2(x + w, y + h * 0.35f), ImVec2(x + w * 0.6f, y + h * 0.35f) };
		drawList->AddTriangleFilled(fold[0], fold[1], fold[2], IM_COL32(160, 165, 175, 255));
	}
}
