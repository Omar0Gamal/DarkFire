#include "Editor/Panels/ConsolePanel.h"
#include <imgui.h>
#include <algorithm>

namespace Editor
{
	ConsolePanel::ConsolePanel() = default;

	void ConsolePanel::onImGuiRender()
	{
		ImGui::Begin("Console");

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
		ImGui::SetNextItemWidth(200.0f);
		ImGui::InputTextWithHint("##LogFilter", "Filter...", m_filterBuffer, sizeof(m_filterBuffer));

		ImGui::Separator();

		// Messages list
		ImGui::BeginChild("LogScrollingRegion", ImVec2(0, 0), false, ImGuiWindowFlags_HorizontalScrollbar);

		auto messages = Core::Log::Get().getRecentMessages(1000);

		for (const auto& msg : messages)
		{
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
			const char* levelStr = "INFO";
			switch (msg.level)
			{
			case Core::LogLevel::Trace:
				color = ImVec4{ 0.6f, 0.6f, 0.6f, 1.0f };
				levelStr = "TRACE";
				break;
			case Core::LogLevel::Info:
				color = ImVec4{ 0.9f, 0.9f, 0.9f, 1.0f };
				levelStr = "INFO";
				break;
			case Core::LogLevel::Warn:
				color = ImVec4{ 1.0f, 0.85f, 0.2f, 1.0f };
				levelStr = "WARN";
				break;
			case Core::LogLevel::Error:
				color = ImVec4{ 0.95f, 0.25f, 0.25f, 1.0f };
				levelStr = "ERROR";
				break;
			case Core::LogLevel::Fatal:
				color = ImVec4{ 1.0f, 0.1f, 0.5f, 1.0f };
				levelStr = "FATAL";
				break;
			default:
				break;
			}

			ImGui::TextDisabled("[%s] [%s]", msg.timestamp.c_str(), msg.category.c_str());
			ImGui::SameLine();
			ImGui::PushStyleColor(ImGuiCol_Text, color);
			ImGui::TextUnformatted(msg.text.c_str());
			ImGui::PopStyleColor();
		}

		if (m_autoScroll && ImGui::GetScrollY() >= ImGui::GetScrollMaxY())
		{
			ImGui::SetScrollHereY(1.0f);
		}

		ImGui::EndChild();
		ImGui::End();
	}
}
