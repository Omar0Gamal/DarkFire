#pragma once

#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Entity.h"
#include "Editor/Commands/CommandHistory.h"
#include <vector>
#include <string>

namespace Editor
{
	class Clipboard
	{
	public:
		static void Copy(const std::vector<Core::Entity>& entities, Core::Scene* scene);
		static std::vector<Core::Entity> Cut(const std::vector<Core::Entity>& entities, Core::Scene* scene, CommandHistory& history);
		static std::vector<Core::Entity> Paste(Core::Scene* scene, CommandHistory& history);
		static std::vector<Core::Entity> Duplicate(const std::vector<Core::Entity>& entities, Core::Scene* scene, CommandHistory& history);

		static bool HasClipboardData();
		static const std::string& GetInternalClipboard() noexcept { return s_clipboardJson; }

	private:
		static std::string s_clipboardJson;
	};
}
