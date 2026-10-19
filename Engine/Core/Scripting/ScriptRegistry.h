#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Scripting/ScriptableEntity.h"
#include <string>
#include <memory>
#include <functional>
#include <unordered_map>
#include <vector>

namespace Core
{
	class DF_API ScriptRegistry
	{
	public:
		using ScriptFactory = std::function<std::shared_ptr<ScriptableEntity>()>;

		static ScriptRegistry& Get();

		template<typename T>
		void registerScript(const std::string& name)
		{
			static_assert(std::is_base_of_v<ScriptableEntity, T>, "T must derive from ScriptableEntity");
			m_factories[name] = []() -> std::shared_ptr<ScriptableEntity> {
				return std::make_shared<T>();
			};
			if (std::find(m_scriptNames.begin(), m_scriptNames.end(), name) == m_scriptNames.end())
			{
				m_scriptNames.push_back(name);
			}
		}

		std::shared_ptr<ScriptableEntity> instantiate(const std::string& name) const;
		bool isRegistered(const std::string& name) const;
		const std::vector<std::string>& getRegisteredNames() const noexcept { return m_scriptNames; }

		void clear();

	private:
		ScriptRegistry() = default;

		std::unordered_map<std::string, ScriptFactory> m_factories;
		std::vector<std::string> m_scriptNames;
	};
}
