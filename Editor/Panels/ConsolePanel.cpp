#include "Editor/Panels/ConsolePanel.h"
#include "Editor/UI/DockLayout.h"
#include "Engine/Core/Console/CVar.h"
#include <imgui.h>
#include <algorithm>
#include <cstring>

namespace Editor
{
	ConsolePanel::ConsolePanel() = default;

	int ConsolePanel::TextEditCallback(ImGuiInputTextCallbackData* data)
	{
		auto* panel = static_cast<ConsolePanel*>(data->UserData);
		if (!panel) return 0;

		if (data->EventFlag == ImGuiInputTextFlags_CallbackCompletion)
		{
			// Tab completion
			const char* wordEnd = data->Buf + data->CursorPos;
			const char* wordStart = wordEnd;
			while (wordStart > data->Buf)
			{
				const char c = wordStart[-1];
				if (c == ' ' || c == '\t' || c == ',' || c == ';')
					break;
				wordStart--;
			}

			std::string prefix(wordStart, static_cast<size_t>(wordEnd - wordStart));
			if (!prefix.empty())
			{
				auto matches = Core::CVarSystem::Get().getCompletions(prefix);
				if (matches.empty())
				{
					Core::Log::Get().info("CVar", "No match for \"{}\"", prefix);
				}
				else if (matches.size() == 1)
				{
					// Single match - auto-complete and append a space
					data->DeleteChars(static_cast<int>(wordStart - data->Buf), static_cast<int>(wordEnd - wordStart));
					data->InsertChars(data->CursorPos, matches[0].c_str());
					data->InsertChars(data->CursorPos, " ");
				}
				else
				{
					// Multiple matches - log them all
					Core::Log::Get().info("CVar", "Matches for \"{}\":", prefix);
					for (const auto& match : matches)
					{
						Core::Log::Get().info("CVar", "  {}", match);
					}
				}
			}
		}
		else if (data->EventFlag == ImGuiInputTextFlags_CallbackHistory)
		{
			// Up / Down history navigation
			const int prevHistoryIndex = panel->m_historyIndex;
			if (data->EventKey == ImGuiKey_UpArrow)
			{
				if (panel->m_historyIndex == -1)
					panel->m_historyIndex = static_cast<int>(panel->m_history.size()) - 1;
				else if (panel->m_historyIndex > 0)
					panel->m_historyIndex--;
			}
			else if (data->EventKey == ImGuiKey_DownArrow)
			{
				if (panel->m_historyIndex != -1)
				{
					if (++panel->m_historyIndex >= static_cast<int>(panel->m_history.size()))
						panel->m_historyIndex = -1;
				}
			}

			if (prevHistoryIndex != panel->m_historyIndex)
			{
				const char* historyStr = (panel->m_historyIndex >= 0 && panel->m_historyIndex < static_cast<int>(panel->m_history.size()))
					? panel->m_history[panel->m_historyIndex].c_str() : "";
				data->DeleteChars(0, data->BufTextLen);
				data->InsertChars(0, historyStr);
			}
		}

		return 0;
	}

	void ConsolePanel::onImGuiRender(bool* p_open)
	{
		ImGui::Begin(UI::PanelNames::Console, p_open);

		// Filter Buttons
		ImGui::Checkbox("Trace", &m_showTrace);
		ImGui::SameLine();
		ImGui::Checkbox("Info", &m_showInfo);
		ImGui::SameLine();
		ImGui::Checkbox("Warn", &m_showWarn);
		ImGui::SameLine();
		ImGui::Checkbox("Error", &m_showError);
		ImGui::SameLine();
		ImGui::Checkbox("Fatal", &m_showFatal);

		ImGui::SameLine();
		ImGui::Spacing();
		ImGui::SameLine();
		ImGui::Checkbox("Auto-Scroll", &m_autoScroll);

		ImGui::SameLine();
		if (ImGui::Button("Clear"))
		{
			Core::Log::Get().clear();
			m_selectedIndex = -1;
		}

		ImGui::SameLine();
		if (ImGui::Button("Copy All"))
		{
			auto msgs = Core::Log::Get().getRecentMessages(1000);
			std::string allText;
			for (const auto& m : msgs)
			{
				allText += "[" + m.timestamp + "] [" + m.category + "] " + m.text + "\n";
			}
			ImGui::SetClipboardText(allText.c_str());
		}

		ImGui::SameLine();
		ImGui::SetNextItemWidth(160.0f);
		ImGui::InputTextWithHint("##LogFilter", "Filter...", m_filterBuffer, sizeof(m_filterBuffer));

		ImGui::Separator();

		// Reserve height for command line input at bottom
		const float footerHeightToReserve = ImGui::GetStyle().ItemSpacing.y + ImGui::GetFrameHeightWithSpacing();
		ImGui::BeginChild("LogScrollingRegion", ImVec2(0, -footerHeightToReserve), false, ImGuiWindowFlags_HorizontalScrollbar);

		auto messages = Core::Log::Get().getRecentMessages(1000);

		for (size_t i = 0; i < messages.size(); ++i)
		{
			const auto& msg = messages[i];

			// Severity visibility
			if (msg.level == Core::LogLevel::Trace && !m_showTrace) continue;
			if (msg.level == Core::LogLevel::Info && !m_showInfo) continue;
			if (msg.level == Core::LogLevel::Warn && !m_showWarn) continue;
			if (msg.level == Core::LogLevel::Error && !m_showError) continue;
			if (msg.level == Core::LogLevel::Fatal && !m_showFatal) continue;

			// Text filter
			if (m_filterBuffer[0] != '\0')
			{
				std::string lowerText = msg.text;
				std::string lowerCat = msg.category;
				std::string lowerFilter = m_filterBuffer;
				std::transform(lowerText.begin(), lowerText.end(), lowerText.begin(), ::tolower);
				std::transform(lowerCat.begin(), lowerCat.end(), lowerCat.begin(), ::tolower);
				std::transform(lowerFilter.begin(), lowerFilter.end(), lowerFilter.begin(), ::tolower);

				if (lowerText.find(lowerFilter) == std::string::npos && lowerCat.find(lowerFilter) == std::string::npos)
				{
					continue;
				}
			}

			// Color based on level
			ImVec4 color{ 1.0f, 1.0f, 1.0f, 1.0f };
			switch (msg.level)
			{
			case Core::LogLevel::Trace: color = ImVec4{ 0.6f, 0.6f, 0.6f, 1.0f }; break;
			case Core::LogLevel::Info:  color = ImVec4{ 0.9f, 0.9f, 0.9f, 1.0f }; break;
			case Core::LogLevel::Warn:  color = ImVec4{ 1.0f, 0.85f, 0.2f, 1.0f }; break;
			case Core::LogLevel::Error: color = ImVec4{ 0.95f, 0.25f, 0.25f, 1.0f }; break;
			case Core::LogLevel::Fatal: color = ImVec4{ 1.0f, 0.1f, 0.5f, 1.0f }; break;
			default: break;
			}

			ImGui::PushID(static_cast<int>(i));
			std::string rowLabel = "[" + msg.timestamp + "] [" + msg.category + "] " + msg.text;
			bool selected = (m_selectedIndex == static_cast<int>(i));

			ImGui::PushStyleColor(ImGuiCol_Text, color);
			if (ImGui::Selectable(rowLabel.c_str(), selected))
			{
				m_selectedIndex = static_cast<int>(i);
			}
			ImGui::PopStyleColor();

			if (ImGui::BeginPopupContextItem())
			{
				if (ImGui::MenuItem("Copy Entry"))
				{
					ImGui::SetClipboardText(rowLabel.c_str());
				}
				ImGui::EndPopup();
			}

			ImGui::PopID();
		}

		if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
		{
			ImGui::SetScrollHereY(1.0f);
		}

		ImGui::EndChild();

		ImGui::Separator();

		// Interactive CVar Command Line Input
		bool reclaimFocus = false;
		ImGuiInputTextFlags inputFlags = ImGuiInputTextFlags_EnterReturnsTrue |
		                                 ImGuiInputTextFlags_CallbackCompletion |
		                                 ImGuiInputTextFlags_CallbackHistory;

		ImGui::PushItemWidth(-1);
		if (ImGui::InputTextWithHint("##ConsoleCommandLine", "Enter command or CVar (e.g. 'stat fps', 'r.wireframe 1', 'help')...", m_inputBuffer, sizeof(m_inputBuffer), inputFlags, &TextEditCallback, (void*)this))
		{
			if (m_inputBuffer[0] != '\0')
			{
				std::string cmd(m_inputBuffer);
				Core::Log::Get().info("Console", "> {}", cmd);
				Core::CVarSystem::Get().execute(cmd);

				// Record history
				m_history.push_back(cmd);
				m_historyIndex = -1;
				m_inputBuffer[0] = '\0';
			}
			reclaimFocus = true;
		}

		ImGui::SetItemDefaultFocus();
		if (reclaimFocus || m_reclaimFocus)
		{
			ImGui::SetKeyboardFocusHere(-1);
			m_reclaimFocus = false;
		}
		ImGui::PopItemWidth();

		ImGui::End();
	}
}
