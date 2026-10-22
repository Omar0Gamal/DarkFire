#include "Editor/Inspector/FieldDrawers.h"
#include <imgui.h>
#include <cctype>
#include <cstring>
#include <algorithm>

namespace Editor
{
	std::string FormatDisplayName(std::string_view name)
	{
		std::string s(name);
		const std::string compSuffix = "Component";
		if (s.length() > compSuffix.length() &&
			s.compare(s.length() - compSuffix.length(), compSuffix.length(), compSuffix) == 0)
		{
			s = s.substr(0, s.length() - compSuffix.length());
		}

		std::string result;
		result.reserve(s.length() + 4);

		for (size_t i = 0; i < s.length(); ++i)
		{
			char c = s[i];
			if (i == 0)
			{
				result += static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
			}
			else
			{
				if (std::isupper(static_cast<unsigned char>(c)) &&
					(std::islower(static_cast<unsigned char>(s[i - 1])) ||
					 (i + 1 < s.length() && std::islower(static_cast<unsigned char>(s[i + 1])))))
				{
					result += ' ';
				}
				result += c;
			}
		}
		return result;
	}

	bool DrawVec3Control(
		const char* label,
		Core::Vector3& values,
		float resetValue,
		float columnWidth,
		bool* outActivated,
		bool* outDeactivated,
		bool isScale,
		bool uniformScale
	)
	{
		bool modified = false;
		ImGui::PushID(label);

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text("%s", label);
		ImGui::SameLine();
		if (ImGui::SmallButton("↺"))
		{
			values = Core::Vector3(resetValue, resetValue, resetValue);
			modified = true;
			if (outDeactivated) *outDeactivated = true;
		}
		if (ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("Reset property to default");
		}
		ImGui::NextColumn();

		float totalWidth = ImGui::CalcItemWidth();
		float itemWidth = (totalWidth - 2.0f * ImGui::GetStyle().ItemSpacing.x) / 3.0f;
		if (itemWidth < 50.0f) itemWidth = 50.0f;

		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

		float lineHeight = ImGui::GetFontSize() + ImGui::GetStyle().FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

		Core::Vector3 oldValues = values;

		// X
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		if (ImGui::Button("X", buttonSize))
		{
			values.x = resetValue;
			modified = true;
			if (outDeactivated) *outDeactivated = true;
		}
		ImGui::PopStyleColor();
		ImGui::SameLine();
		ImGui::SetNextItemWidth(itemWidth);
		if (ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f"))
		{
			modified = true;
			if (isScale && uniformScale && oldValues.x != 0.0f)
			{
				float ratio = values.x / oldValues.x;
				values.y *= ratio;
				values.z *= ratio;
			}
		}
		if (ImGui::IsItemActivated() && outActivated) *outActivated = true;
		if (ImGui::IsItemDeactivatedAfterEdit() && outDeactivated) *outDeactivated = true;
		ImGui::SameLine();

		// Y
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		if (ImGui::Button("Y", buttonSize))
		{
			values.y = resetValue;
			modified = true;
			if (outDeactivated) *outDeactivated = true;
		}
		ImGui::PopStyleColor();
		ImGui::SameLine();
		ImGui::SetNextItemWidth(itemWidth);
		if (ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f"))
		{
			modified = true;
			if (isScale && uniformScale && oldValues.y != 0.0f)
			{
				float ratio = values.y / oldValues.y;
				values.x *= ratio;
				values.z *= ratio;
			}
		}
		if (ImGui::IsItemActivated() && outActivated) *outActivated = true;
		if (ImGui::IsItemDeactivatedAfterEdit() && outDeactivated) *outDeactivated = true;
		ImGui::SameLine();

		// Z
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		if (ImGui::Button("Z", buttonSize))
		{
			values.z = resetValue;
			modified = true;
			if (outDeactivated) *outDeactivated = true;
		}
		ImGui::PopStyleColor();
		ImGui::SameLine();
		ImGui::SetNextItemWidth(itemWidth);
		if (ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f"))
		{
			modified = true;
			if (isScale && uniformScale && oldValues.z != 0.0f)
			{
				float ratio = values.z / oldValues.z;
				values.y *= ratio;
				values.z *= ratio;
			}
		}
		if (ImGui::IsItemActivated() && outActivated) *outActivated = true;
		if (ImGui::IsItemDeactivatedAfterEdit() && outDeactivated) *outDeactivated = true;

		ImGui::PopStyleVar();
		ImGui::Columns(1);
		ImGui::PopID();

		return modified;
	}

	bool DrawField(const Core::FieldDesc& field, void* componentData)
	{
		if (field.attribute.hideInInspector || !field.access)
		{
			return false;
		}

		void* target = field.access(componentData);
		if (!target)
		{
			return false;
		}

		std::string label = FormatDisplayName(field.name);
		bool modified = false;

		if (field.attribute.readOnly)
		{
			ImGui::BeginDisabled(true);
		}

		switch (field.type)
		{
		case Core::FieldType::Int:
		{
			int currentVal = 0;
			if (field.elementSize == 1) currentVal = *static_cast<uint8_t*>(target);
			else if (field.elementSize == 2) currentVal = *static_cast<int16_t*>(target);
			else if (field.elementSize == 4) currentVal = *static_cast<int32_t*>(target);
			else if (field.elementSize == 8) currentVal = static_cast<int>(*static_cast<int64_t*>(target));

			if (field.attribute.hasRange)
			{
				modified = ImGui::SliderInt(label.c_str(), &currentVal,
					static_cast<int>(field.attribute.rangeMin),
					static_cast<int>(field.attribute.rangeMax));
			}
			else
			{
				modified = ImGui::DragInt(label.c_str(), &currentVal, 1.0f);
			}

			if (modified)
			{
				if (field.elementSize == 1) *static_cast<uint8_t*>(target) = static_cast<uint8_t>(currentVal);
				else if (field.elementSize == 2) *static_cast<int16_t*>(target) = static_cast<int16_t>(currentVal);
				else if (field.elementSize == 4) *static_cast<int32_t*>(target) = static_cast<int32_t>(currentVal);
				else if (field.elementSize == 8) *static_cast<int64_t*>(target) = static_cast<int64_t>(currentVal);
			}
			break;
		}
		case Core::FieldType::Float:
		{
			float* val = static_cast<float*>(target);
			if (field.attribute.hasRange)
			{
				modified = ImGui::SliderFloat(label.c_str(), val, field.attribute.rangeMin, field.attribute.rangeMax, "%.3f");
			}
			else
			{
				float speed = 0.05f;
				if (field.name.find("intensity") != std::string::npos ||
					field.name.find("Intensity") != std::string::npos ||
					field.name.find("range") != std::string::npos ||
					field.name.find("Range") != std::string::npos ||
					field.name.find("far") != std::string::npos ||
					field.name.find("mass") != std::string::npos)
				{
					speed = 0.5f;
				}
				modified = ImGui::DragFloat(label.c_str(), val, speed, 0.0f, 0.0f, "%.3f");
			}
			break;
		}
		case Core::FieldType::Bool:
		{
			bool* val = static_cast<bool*>(target);
			modified = ImGui::Checkbox(label.c_str(), val);
			break;
		}
		case Core::FieldType::String:
		{
			auto* str = static_cast<std::string*>(target);
			char buf[512];
			memset(buf, 0, sizeof(buf));
			strncpy_s(buf, str->c_str(), sizeof(buf) - 1);
			if (ImGui::InputText(label.c_str(), buf, sizeof(buf)))
			{
				*str = buf;
				modified = true;
			}
			break;
		}
		case Core::FieldType::Vec2:
		{
			auto* vec = static_cast<Core::Vector2*>(target);
			float vals[2] = { vec->x, vec->y };
			if (ImGui::DragFloat2(label.c_str(), vals, 0.05f, 0.0f, 0.0f, "%.2f"))
			{
				vec->x = vals[0];
				vec->y = vals[1];
				modified = true;
			}
			break;
		}
		case Core::FieldType::Vec3:
		{
			auto* vec = static_cast<Core::Vector3*>(target);
			modified = DrawVec3Control(label.c_str(), *vec);
			break;
		}
		case Core::FieldType::Vec4:
		{
			auto* vec = static_cast<Core::Vector4*>(target);
			float vals[4] = { vec->x, vec->y, vec->z, vec->w };
			if (ImGui::DragFloat4(label.c_str(), vals, 0.05f, 0.0f, 0.0f, "%.2f"))
			{
				vec->x = vals[0];
				vec->y = vals[1];
				vec->z = vals[2];
				vec->w = vals[3];
				modified = true;
			}
			break;
		}
		case Core::FieldType::Color:
		{
			auto* col = static_cast<Core::Color*>(target);
			float arr[4] = { col->R(), col->G(), col->B(), col->A() };
			if (ImGui::ColorEdit4(label.c_str(), arr))
			{
				*col = Core::Color(arr[0], arr[1], arr[2], arr[3]);
				modified = true;
			}
			break;
		}
		case Core::FieldType::Enum:
		{
			int currentVal = 0;
			if (field.elementSize == 1) currentVal = *static_cast<uint8_t*>(target);
			else if (field.elementSize == 2) currentVal = *static_cast<uint16_t*>(target);
			else currentVal = *static_cast<int*>(target);

			std::vector<const char*> items;
			items.reserve(field.enumNames.size());
			for (const auto& enumName : field.enumNames)
			{
				items.push_back(enumName.c_str());
			}

			if (!items.empty())
			{
				if (ImGui::Combo(label.c_str(), &currentVal, items.data(), static_cast<int>(items.size())))
				{
					if (field.elementSize == 1) *static_cast<uint8_t*>(target) = static_cast<uint8_t>(currentVal);
					else if (field.elementSize == 2) *static_cast<uint16_t*>(target) = static_cast<uint16_t>(currentVal);
					else *static_cast<int*>(target) = currentVal;
					modified = true;
				}
			}
			break;
		}
		default:
			break;
		}

		if (!field.attribute.tooltip.empty() && ImGui::IsItemHovered())
		{
			ImGui::SetTooltip("%s", field.attribute.tooltip.c_str());
		}

		if (field.attribute.readOnly)
		{
			ImGui::EndDisabled();
		}

		return modified;
	}
}
