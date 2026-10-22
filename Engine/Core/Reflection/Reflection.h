#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Scene/Scene.h"
#include <string>
#include <vector>
#include <functional>
#include <string_view>
#include <cstdint>
#include <entt/entt.hpp>

namespace Core
{
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
		FieldType type = FieldType::Float;
		size_t elementSize = sizeof(float);
		FieldAttribute attribute;
		std::vector<std::string> enumNames;
		std::function<void*(void* component)> access;
		std::function<const void*(const void* component)> constAccess;
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

		// Raw pointer access for reflection-driven UI and inspector
		std::function<void*(Scene&, entt::entity)> getComponentRaw;
		std::function<const void*(const Scene&, entt::entity)> getComponentRawConst;

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

	// Type traits for automatic FieldType deduction
	template<typename T> struct FieldTypeTraits;
	template<> struct FieldTypeTraits<int> { static constexpr FieldType type = FieldType::Int; };
	template<> struct FieldTypeTraits<uint8_t> { static constexpr FieldType type = FieldType::Int; };
	template<> struct FieldTypeTraits<uint32_t> { static constexpr FieldType type = FieldType::Int; };
	template<> struct FieldTypeTraits<uint64_t> { static constexpr FieldType type = FieldType::Int; };
	template<> struct FieldTypeTraits<int64_t> { static constexpr FieldType type = FieldType::Int; };
	template<> struct FieldTypeTraits<float> { static constexpr FieldType type = FieldType::Float; };
	template<> struct FieldTypeTraits<double> { static constexpr FieldType type = FieldType::Float; };
	template<> struct FieldTypeTraits<bool> { static constexpr FieldType type = FieldType::Bool; };
	template<> struct FieldTypeTraits<std::string> { static constexpr FieldType type = FieldType::String; };
	template<> struct FieldTypeTraits<Math::Vector2> { static constexpr FieldType type = FieldType::Vec2; };
	template<> struct FieldTypeTraits<Math::Vector3> { static constexpr FieldType type = FieldType::Vec3; };
	template<> struct FieldTypeTraits<Math::Vector4> { static constexpr FieldType type = FieldType::Vec4; };
	template<> struct FieldTypeTraits<Math::Quaternion> { static constexpr FieldType type = FieldType::Quat; };
	template<> struct FieldTypeTraits<Color> { static constexpr FieldType type = FieldType::Color; };
	template<> struct FieldTypeTraits<UUID> { static constexpr FieldType type = FieldType::String; };

	// Fluent ComponentBuilder to eliminate manual reflection boilerplate
	template<typename CompType>
	class ComponentBuilder
	{
	public:
		explicit ComponentBuilder(std::string name, bool authored = true)
		{
			m_desc.name = std::move(name);
			m_desc.typeId = entt::type_hash<CompType>::value();
			m_desc.isAuthored = authored;

			m_desc.hasComponent = [](const Scene& scene, entt::entity e) {
				return scene.getRegistry().all_of<CompType>(e);
			};
			m_desc.addDefaultComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().emplace_or_replace<CompType>(e);
			};
			m_desc.removeComponent = [](Scene& scene, entt::entity e) {
				scene.getRegistry().remove<CompType>(e);
			};
			m_desc.getComponentRaw = [](Scene& scene, entt::entity e) -> void* {
				return scene.getRegistry().try_get<CompType>(e);
			};
			m_desc.getComponentRawConst = [](const Scene& scene, entt::entity e) -> const void* {
				return scene.getRegistry().try_get<CompType>(e);
			};
			m_desc.copyComponent = [](const Scene& src, entt::entity s, Scene& dst, entt::entity d) {
				dst.getRegistry().emplace_or_replace<CompType>(d, src.getRegistry().get<CompType>(s));
			};
		}

		template<typename MemberType>
		ComponentBuilder& field(std::string name, MemberType CompType::* member, FieldAttribute attr = {})
		{
			FieldDesc f{};
			f.name = std::move(name);
			f.attribute = attr;
			f.type = FieldTypeTraits<MemberType>::type;
			f.elementSize = sizeof(MemberType);
			f.access = [member](void* comp) -> void* {
				return &(static_cast<CompType*>(comp)->*member);
			};
			f.constAccess = [member](const void* comp) -> const void* {
				return &(static_cast<const CompType*>(comp)->*member);
			};
			m_desc.fields.push_back(std::move(f));
			return *this;
		}

		template<typename EnumType>
		ComponentBuilder& enumField(std::string name, EnumType CompType::* member, std::vector<std::string> names, FieldAttribute attr = {})
		{
			FieldDesc f{};
			f.name = std::move(name);
			f.attribute = attr;
			f.type = FieldType::Enum;
			f.elementSize = sizeof(EnumType);
			f.enumNames = std::move(names);
			f.access = [member](void* comp) -> void* {
				return &(static_cast<CompType*>(comp)->*member);
			};
			f.constAccess = [member](const void* comp) -> const void* {
				return &(static_cast<const CompType*>(comp)->*member);
			};
			m_desc.fields.push_back(std::move(f));
			return *this;
		}

		ComponentBuilder& serialize(
			std::function<void(const Scene&, entt::entity, void*)> ser,
			std::function<void(Scene&, entt::entity, const void*)> deser)
		{
			m_desc.serializeToJson = std::move(ser);
			m_desc.deserializeFromJson = std::move(deser);
			return *this;
		}

		void registerSelf()
		{
			ComponentRegistry::Get().registerComponent(std::move(m_desc));
		}

	private:
		ComponentDesc m_desc;
	};
}
