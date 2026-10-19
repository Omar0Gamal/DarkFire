#include "Engine/Core/Scripting/ScriptRegistry.h"

namespace Core
{
	ScriptRegistry& ScriptRegistry::Get()
	{
		static ScriptRegistry instance;
		return instance;
	}

	std::shared_ptr<ScriptableEntity> ScriptRegistry::instantiate(const std::string& name) const
	{
		auto it = m_factories.find(name);
		if (it != m_factories.end() && it->second)
		{
			return it->second();
		}
		return nullptr;
	}

	bool ScriptRegistry::isRegistered(const std::string& name) const
	{
		return m_factories.find(name) != m_factories.end();
	}

	void ScriptRegistry::clear()
	{
		m_factories.clear();
		m_scriptNames.clear();
	}
}
