#pragma once

#include "Engine/Core/Defines.h"
#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <variant>
#include <string_view>

namespace Core
{
	enum class CVarFlags : uint32_t
	{
		None = 0,
		Archive = 1 << 0,
		ReadOnly = 1 << 1,
		Cheat = 1 << 2
	};

	inline CVarFlags operator|(CVarFlags a, CVarFlags b) noexcept { return static_cast<CVarFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b)); }
	inline bool operator&(CVarFlags a, CVarFlags b) noexcept { return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0; }

	using CVarValue = std::variant<int32_t, float, bool, std::string>;

	struct DF_API CVar
	{
		std::string name;
		std::string description;
		CVarValue value;
		CVarValue defaultValue;
		CVarFlags flags = CVarFlags::None;
		std::function<void(const CVarValue&)> onChange;

		int32_t asInt() const noexcept;
		float asFloat() const noexcept;
		bool asBool() const noexcept;
		std::string asString() const;

		void set(int32_t val);
		void set(float val);
		void set(bool val);
		void set(std::string_view val);
	};

	using CVarCommandFunc = std::function<void(const std::vector<std::string>& args)>;

	struct DF_API CVarCommand
	{
		std::string name;
		std::string description;
		CVarCommandFunc callback;
	};

	class DF_API CVarSystem
	{
	public:
		static CVarSystem& Get();

		CVar* registerCVar(
			std::string_view name,
			std::string_view description,
			int32_t defaultValue,
			CVarFlags flags = CVarFlags::None,
			std::function<void(const CVarValue&)> onChange = nullptr
		);

		CVar* registerCVar(
			std::string_view name,
			std::string_view description,
			float defaultValue,
			CVarFlags flags = CVarFlags::None,
			std::function<void(const CVarValue&)> onChange = nullptr
		);

		CVar* registerCVar(
			std::string_view name,
			std::string_view description,
			bool defaultValue,
			CVarFlags flags = CVarFlags::None,
			std::function<void(const CVarValue&)> onChange = nullptr
		);

		CVar* registerCVar(
			std::string_view name,
			std::string_view description,
			std::string_view defaultValue,
			CVarFlags flags = CVarFlags::None,
			std::function<void(const CVarValue&)> onChange = nullptr
		);

		void registerCommand(
			std::string_view name,
			std::string_view description,
			CVarCommandFunc callback
		);

		CVar* findCVar(std::string_view name);
		const CVar* findCVar(std::string_view name) const;

		bool execute(std::string_view commandLine);

		std::vector<std::string> getCompletions(std::string_view prefix) const;

		const std::unordered_map<std::string, CVar>& getCVars() const noexcept { return m_cvars; }
		const std::unordered_map<std::string, CVarCommand>& getCommands() const noexcept { return m_commands; }

	private:
		CVarSystem();
		void registerBuiltinCVars();

		std::unordered_map<std::string, CVar> m_cvars;
		std::unordered_map<std::string, CVarCommand> m_commands;
	};
}
