#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include <string>
#include <vector>
#include <functional>
#include <string_view>
#include <cstdint>
#include <entt/entt.hpp>

namespace Core
{
	class Scene;

	enum class FieldType : uint8_t
	{
		Int,
		Float,
		Bool,
		String,
		Vec2,
		Vec3,
		Vec4,
		Color,
		Quat,
		Enum
	};

	struct DF_API FieldAttribute
	{
		bool hasRange = false;
		float rangeMin = 0.0f;
		float rangeMax = 1.0f;
		std::string tooltip;
		bool isColor = false;
		bool hideInInspector = false;
		bool readOnly = false;
		bool noSerialize = false;
		std::string renameOldName;
	};

	struct DF_API FieldDesc
	{
		std::string name;
		FieldType type;
		FieldAttribute attribute;
	};

	struct DF_API ComponentDesc
	{
		std::string name;
		uint64_t typeId = 0;
		bool isAuthored = true;
		std::vector<FieldDesc> fields;

		// Operations on entt::registry via Scene
		std::function<bool(const Scene&, entt::entity)> hasComponent;
		std::function<void(Scene&, entt::entity)> addDefaultComponent;
		std::function<void(Scene&, entt::entity)> removeComponent;

		// Deep copy from src entity to dst entity
		std::function<void(const Scene& srcScene, entt::entity srcEntity, Scene& dstScene, entt::entity dstEntity)> copyComponent;

		// Serialization interface (opaque JSON pointer to confine nlohmann/json to .cpp)
		std::function<void(const Scene&, entt::entity, void* jsonNode)> serializeToJson;
		std::function<void(Scene&, entt::entity, const void* jsonNode)> deserializeFromJson;
	};

	class DF_API ComponentRegistry
	{
	public:
		static ComponentRegistry& Get();

		void registerComponent(ComponentDesc desc);
		const std::vector<ComponentDesc>& getAll() const { return m_components; }
		const ComponentDesc* findByName(std::string_view name) const;
		const ComponentDesc* findByTypeId(uint64_t typeId) const;

		// Register built-in engine components
		void registerBuiltinTypes();

	private:
		ComponentRegistry() = default;
		std::vector<ComponentDesc> m_components;
	};
}
