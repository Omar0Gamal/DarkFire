#include "Reflection.h"

namespace Core
{
	using TypeRegistry = ComponentRegistry;

	// Modular registration helper functions
	void registerComponentTypes(TypeRegistry& registry);
	void registerComponentSerializers(TypeRegistry& registry);

	ComponentRegistry& ComponentRegistry::Get()
	{
		static ComponentRegistry instance;
		return instance;
	}

	void ComponentRegistry::registerComponent(ComponentDesc desc)
	{
		for (auto& existing : m_components)
		{
			if (existing.typeId == desc.typeId)
			{
				existing = std::move(desc);
				return;
			}
		}
		m_components.push_back(std::move(desc));
	}

	const ComponentDesc* ComponentRegistry::findByName(std::string_view name) const
	{
		for (const auto& desc : m_components)
		{
			if (desc.name == name)
			{
				return &desc;
			}
		}
		return nullptr;
	}

	const ComponentDesc* ComponentRegistry::findByTypeId(uint64_t typeId) const
	{
		for (const auto& desc : m_components)
		{
			if (desc.typeId == typeId)
			{
				return &desc;
			}
		}
		return nullptr;
	}

	void ComponentRegistry::registerBuiltinTypes()
	{
		registerComponentTypes(*this);
		registerComponentSerializers(*this);
	}
}
