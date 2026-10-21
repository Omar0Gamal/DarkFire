#include "Editor/Clipboard.h"
#include "Editor/Commands/EntityCommands.h"
#include "Engine/Core/Serialization/SceneSerializer.h"
#ifdef DF_EDITOR
#include <imgui.h>
#endif

namespace Editor
{
	std::string Clipboard::s_clipboardJson = "";

	void Clipboard::Copy(const std::vector<Core::Entity>& entities, Core::Scene* scene)
	{
		if (!scene || entities.empty()) return;

		Core::SceneSerializer serializer(scene);
		s_clipboardJson = serializer.serializeEntities(entities);
#ifdef DF_EDITOR
		if (ImGui::GetCurrentContext())
		{
			ImGui::SetClipboardText(s_clipboardJson.c_str());
		}
#endif
	}

	std::vector<Core::Entity> Clipboard::Cut(const std::vector<Core::Entity>& entities, Core::Scene* scene, CommandHistory& history)
	{
		if (!scene || entities.empty()) return {};

		Copy(entities, scene);

		auto transaction = std::make_unique<TransactionCommand>("Cut Entities");
		for (auto entity : entities)
		{
			if (entity.isValid())
			{
				transaction->addCommand(std::make_unique<DeleteEntityCommand>(scene, entity));
			}
		}

		history.execute(std::move(transaction));
		return {};
	}

	std::vector<Core::Entity> Clipboard::Paste(Core::Scene* scene, CommandHistory& history)
	{
		if (!scene) return {};

		std::string jsonStr = s_clipboardJson;
#ifdef DF_EDITOR
		if (ImGui::GetCurrentContext())
		{
			const char* osClip = ImGui::GetClipboardText();
			if (osClip && std::string_view(osClip).find("\"entities\"") != std::string_view::npos)
			{
				jsonStr = osClip;
			}
		}
#endif

		if (jsonStr.empty()) return {};

		Core::SceneSerializer serializer(scene);
		auto pasted = serializer.deserializeEntities(jsonStr, true, Core::Vector3(1.0f, 0.0f, 1.0f));

		if (!pasted.empty())
		{
			auto transaction = std::make_unique<TransactionCommand>("Paste Entities");
			for (auto entity : pasted)
			{
				transaction->addCommand(std::make_unique<CreateEntityCommand>(scene, entity));
			}
			history.execute(std::move(transaction));
		}

		return pasted;
	}

	std::vector<Core::Entity> Clipboard::Duplicate(const std::vector<Core::Entity>& entities, Core::Scene* scene, CommandHistory& history)
	{
		if (!scene || entities.empty()) return {};

		Core::SceneSerializer serializer(scene);
		std::string serialized = serializer.serializeEntities(entities);
		auto duplicated = serializer.deserializeEntities(serialized, true, Core::Vector3(1.0f, 0.0f, 1.0f));

		if (!duplicated.empty())
		{
			auto transaction = std::make_unique<TransactionCommand>("Duplicate Entities");
			for (auto entity : duplicated)
			{
				transaction->addCommand(std::make_unique<CreateEntityCommand>(scene, entity));
			}
			history.execute(std::move(transaction));
		}

		return duplicated;
	}

	bool Clipboard::HasClipboardData()
	{
		if (!s_clipboardJson.empty()) return true;
#ifdef DF_EDITOR
		if (ImGui::GetCurrentContext())
		{
			const char* osClip = ImGui::GetClipboardText();
			return osClip && std::string_view(osClip).find("\"entities\"") != std::string_view::npos;
		}
#endif
		return false;
	}
}
