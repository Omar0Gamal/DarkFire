#pragma once

#include <imgui.h>

namespace Editor::Theme
{
	inline void ApplyDarkFireEmberTheme(float dpiScale = 1.0f)
	{
		ImGuiStyle& style = ImGui::GetStyle();
		ImVec4* colors = style.Colors;

		float dpi = (dpiScale > 0.5f) ? dpiScale : 1.0f;

		// DarkFire Ember Palette
		// Sleek obsidian / charcoal with luminous fiery ember accents
		const ImVec4 cMainBg       = ImVec4(0.078f, 0.082f, 0.094f, 1.00f); // #141518 (Deep obsidian workspace)
		const ImVec4 cPanelBg      = ImVec4(0.106f, 0.110f, 0.129f, 1.00f); // #1B1C21 (Dock & panel background)
		const ImVec4 cFrameBg      = ImVec4(0.071f, 0.075f, 0.086f, 1.00f); // #121316 (Recessed input fields)
		const ImVec4 cFrameHovered = ImVec4(0.145f, 0.153f, 0.180f, 1.00f);
		const ImVec4 cFrameActive  = ImVec4(0.180f, 0.192f, 0.224f, 1.00f);
		const ImVec4 cBorder       = ImVec4(0.165f, 0.176f, 0.208f, 0.90f); // #2A2D35 (Crisp fine border)
		const ImVec4 cPopupBg      = ImVec4(0.086f, 0.090f, 0.106f, 0.98f); // #16171B

		// Primary Accent: DarkFire Ember (#FF6B35)
		const ImVec4 cAccent        = ImVec4(1.000f, 0.420f, 0.208f, 1.00f); // #FF6B35 (Luminous Ember)
		const ImVec4 cAccentHovered = ImVec4(1.000f, 0.522f, 0.333f, 1.00f); // #FF8555 (Warm Amber Flame)
		const ImVec4 cAccentActive  = ImVec4(0.910f, 0.333f, 0.133f, 1.00f); // #E85522 (Deep Core Glow)

		const ImVec4 cText         = ImVec4(0.929f, 0.937f, 0.945f, 1.00f); // Crisp off-white
		const ImVec4 cTextDisabled = ImVec4(0.490f, 0.510f, 0.561f, 1.00f);

		colors[ImGuiCol_Text]                  = cText;
		colors[ImGuiCol_TextDisabled]          = cTextDisabled;
		colors[ImGuiCol_WindowBg]              = cMainBg;
		colors[ImGuiCol_ChildBg]               = cPanelBg;
		colors[ImGuiCol_PopupBg]               = cPopupBg;
		colors[ImGuiCol_Border]                = cBorder;
		colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_FrameBg]               = cFrameBg;
		colors[ImGuiCol_FrameBgHovered]        = cFrameHovered;
		colors[ImGuiCol_FrameBgActive]         = cFrameActive;
		colors[ImGuiCol_TitleBg]               = cMainBg;
		colors[ImGuiCol_TitleBgActive]         = cPanelBg;
		colors[ImGuiCol_TitleBgCollapsed]      = cMainBg;
		colors[ImGuiCol_MenuBarBg]             = cMainBg;
		colors[ImGuiCol_ScrollbarBg]           = cMainBg;
		colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.20f, 0.22f, 0.26f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.28f, 0.31f, 0.36f, 1.00f);
		colors[ImGuiCol_ScrollbarGrabActive]   = cAccent;
		colors[ImGuiCol_CheckMark]             = cAccent;
		colors[ImGuiCol_SliderGrab]            = cAccent;
		colors[ImGuiCol_SliderGrabActive]      = cAccentActive;
		colors[ImGuiCol_Button]                = ImVec4(0.145f, 0.153f, 0.180f, 1.00f);
		colors[ImGuiCol_ButtonHovered]         = ImVec4(0.200f, 0.215f, 0.250f, 1.00f);
		colors[ImGuiCol_ButtonActive]          = cAccentActive;
		colors[ImGuiCol_Header]                = ImVec4(0.160f, 0.172f, 0.204f, 1.00f);
		colors[ImGuiCol_HeaderHovered]         = ImVec4(0.220f, 0.235f, 0.275f, 1.00f);
		colors[ImGuiCol_HeaderActive]          = cAccentActive;
		colors[ImGuiCol_Separator]             = cBorder;
		colors[ImGuiCol_SeparatorHovered]      = cAccentHovered;
		colors[ImGuiCol_SeparatorActive]       = cAccentActive;
		colors[ImGuiCol_ResizeGrip]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_ResizeGripHovered]     = cAccentHovered;
		colors[ImGuiCol_ResizeGripActive]      = cAccentActive;
		colors[ImGuiCol_Tab]                   = cMainBg;
		colors[ImGuiCol_TabHovered]            = ImVec4(0.20f, 0.22f, 0.26f, 1.00f);
		colors[ImGuiCol_TabSelected]           = cPanelBg;
		colors[ImGuiCol_TabSelectedOverline]   = cAccent;
		colors[ImGuiCol_TabDimmed]             = cMainBg;
		colors[ImGuiCol_TabDimmedSelected]     = cPanelBg;
		colors[ImGuiCol_TabDimmedSelectedOverline] = ImVec4(cAccent.x, cAccent.y, cAccent.z, 0.45f);
		colors[ImGuiCol_DockingPreview]        = ImVec4(cAccent.x, cAccent.y, cAccent.z, 0.40f);
		colors[ImGuiCol_DockingEmptyBg]        = cMainBg;
		colors[ImGuiCol_PlotLines]             = cAccent;
		colors[ImGuiCol_PlotLinesHovered]      = cAccentHovered;
		colors[ImGuiCol_PlotHistogram]         = cAccent;
		colors[ImGuiCol_PlotHistogramHovered]  = cAccentHovered;
		colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.12f, 0.13f, 0.15f, 1.00f);
		colors[ImGuiCol_TableBorderStrong]     = cBorder;
		colors[ImGuiCol_TableBorderLight]      = ImVec4(0.10f, 0.11f, 0.13f, 1.00f);
		colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
		colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
		colors[ImGuiCol_TextSelectedBg]        = ImVec4(cAccent.x, cAccent.y, cAccent.z, 0.35f);
		colors[ImGuiCol_DragDropTarget]        = cAccent;
		colors[ImGuiCol_NavCursor]             = cAccent;
		colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
		colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.08f, 0.08f, 0.08f, 0.30f);
		colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.04f, 0.04f, 0.05f, 0.70f);

		// Style Metrics
		style.WindowRounding    = 0.0f;
		style.ChildRounding     = 3.0f;
		style.FrameRounding     = 3.0f;
		style.PopupRounding     = 4.0f;
		style.ScrollbarRounding = 4.0f;
		style.GrabRounding      = 3.0f;
		style.TabRounding       = 4.0f;

		style.WindowBorderSize  = 1.0f;
		style.ChildBorderSize   = 1.0f;
		style.PopupBorderSize   = 1.0f;
		style.FrameBorderSize   = 1.0f;
		style.TabBorderSize     = 0.0f;

		// Base paddings before DPI scaling
		style.WindowPadding     = ImVec2(8.0f * dpi, 8.0f * dpi);
		style.FramePadding      = ImVec2(7.0f * dpi, 5.0f * dpi);
		style.CellPadding       = ImVec2(6.0f * dpi, 4.0f * dpi);
		style.ItemSpacing       = ImVec2(7.0f * dpi, 6.0f * dpi);
		style.ItemInnerSpacing  = ImVec2(5.0f * dpi, 4.0f * dpi);
		style.IndentSpacing     = 18.0f * dpi;
		style.ScrollbarSize     = 13.0f * dpi;
		style.GrabMinSize       = 10.0f * dpi;

		if (ImGui::GetIO().ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
		{
			style.WindowRounding = 0.0f;
			style.Colors[ImGuiCol_WindowBg].w = 1.0f;
		}
	}
}
