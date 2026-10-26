#include "Engine/Core/Console/CVar.h"
#include "Engine/Core/Log/Log.h"
#include <sstream>
#include <algorithm>

namespace Core
{
	int32_t CVar::asInt() const noexcept
	{
		if (std::holds_alternative<int32_t>(value)) return std::get<int32_t>(value);
		if (std::holds_alternative<float>(value)) return static_cast<int32_t>(std::get<float>(value));
		if (std::holds_alternative<bool>(value)) return std::get<bool>(value) ? 1 : 0;
		if (std::holds_alternative<std::string>(value))
		{
			try { return std::stoi(std::get<std::string>(value)); }
			catch (...) { return 0; }
		}
		return 0;
	}

	float CVar::asFloat() const noexcept
	{
		if (std::holds_alternative<float>(value)) return std::get<float>(value);
		if (std::holds_alternative<int32_t>(value)) return static_cast<float>(std::get<int32_t>(value));
		if (std::holds_alternative<bool>(value)) return std::get<bool>(value) ? 1.0f : 0.0f;
		if (std::holds_alternative<std::string>(value))
		{
			try { return std::stof(std::get<std::string>(value)); }
			catch (...) { return 0.0f; }
		}
		return 0.0f;
	}

	bool CVar::asBool() const noexcept
	{
		if (std::holds_alternative<bool>(value)) return std::get<bool>(value);
		if (std::holds_alternative<int32_t>(value)) return std::get<int32_t>(value) != 0;
		if (std::holds_alternative<float>(value)) return std::get<float>(value) != 0.0f;
		if (std::holds_alternative<std::string>(value))
		{
			const auto& str = std::get<std::string>(value);
			return str == "1" || str == "true" || str == "True" || str == "on" || str == "yes";
		}
		return false;
	}

	std::string CVar::asString() const
	{
		if (std::holds_alternative<std::string>(value)) return std::get<std::string>(value);
		if (std::holds_alternative<int32_t>(value)) return std::to_string(std::get<int32_t>(value));
		if (std::holds_alternative<float>(value)) return std::to_string(std::get<float>(value));
		if (std::holds_alternative<bool>(value)) return std::get<bool>(value) ? "true" : "false";
		return "";
	}

	void CVar::set(int32_t val)
	{
		value = val;
		if (onChange) onChange(value);
	}

	void CVar::set(float val)
	{
		value = val;
		if (onChange) onChange(value);
	}

	void CVar::set(bool val)
	{
		value = val;
		if (onChange) onChange(value);
	}

	void CVar::set(std::string_view val)
	{
		if (std::holds_alternative<int32_t>(value))
		{
			try { value = std::stoi(std::string(val)); }
			catch (...) {}
		}
		else if (std::holds_alternative<float>(value))
		{
			try { value = std::stof(std::string(val)); }
			catch (...) {}
		}
		else if (std::holds_alternative<bool>(value))
		{
			std::string lower(val);
			std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
			value = (lower == "1" || lower == "true" || lower == "on" || lower == "yes");
		}
		else
		{
			value = std::string(val);
		}

		if (onChange) onChange(value);
	}

	CVarSystem& CVarSystem::Get()
	{
		static CVarSystem s_instance;
		return s_instance;
	}

	CVarSystem::CVarSystem()
	{
		registerBuiltinCVars();
	}

	void CVarSystem::registerBuiltinCVars()
	{
		registerCVar("stat.fps", "Toggles framerate and frame time display", 0);
		registerCVar("stat.rhi", "Toggles GPU RHI telemetry display", 0);
		registerCVar("r.wireframe", "Toggles wireframe rendering overlay", false);
		registerCVar("r.vsync", "Toggles VSync on the active swapchain", true);
		registerCVar("net.sim.latency", "Simulated packet roundtrip latency in ms", 0.0f);
		registerCVar("net.sim.packetloss", "Simulated packet drop rate percentage (0 - 100)", 0.0f);
		registerCVar("audio.mastervolume", "Master audio output bus volume multiplier (0.0 - 1.0)", 1.0f);

		registerCommand("help", "Lists all available console variables and commands", [this](const std::vector<std::string>& args) {
			(void)args;
			Core::Log::Get().info("CVar", "=== DarkFire Console Variables ===");
			for (const auto& [name, cvar] : m_cvars)
			{
				Core::Log::Get().info("CVar", "  {} = {}  ({})", name, cvar.asString(), cvar.description);
			}
			Core::Log::Get().info("CVar", "=== DarkFire Commands ===");
			for (const auto& [name, cmd] : m_commands)
			{
				Core::Log::Get().info("CVar", "  {}  ({})", name, cmd.description);
			}
		});

		registerCommand("clear", "Clears the output log", [](const std::vector<std::string>&) {
			Core::Log::Get().clear();
		});
	}

	CVar* CVarSystem::registerCVar(
		std::string_view name,
		std::string_view description,
		int32_t defaultValue,
		CVarFlags flags,
		std::function<void(const CVarValue&)> onChange
	)
	{
		std::string key(name);
		CVar var;
		var.name = key;
		var.description = std::string(description);
		var.value = defaultValue;
		var.defaultValue = defaultValue;
		var.flags = flags;
		var.onChange = std::move(onChange);
		m_cvars[key] = std::move(var);
		return &m_cvars[key];
	}

	CVar* CVarSystem::registerCVar(
		std::string_view name,
		std::string_view description,
		float defaultValue,
		CVarFlags flags,
		std::function<void(const CVarValue&)> onChange
	)
	{
		std::string key(name);
		CVar var;
		var.name = key;
		var.description = std::string(description);
		var.value = defaultValue;
		var.defaultValue = defaultValue;
		var.flags = flags;
		var.onChange = std::move(onChange);
		m_cvars[key] = std::move(var);
		return &m_cvars[key];
	}

	CVar* CVarSystem::registerCVar(
		std::string_view name,
		std::string_view description,
		bool defaultValue,
		CVarFlags flags,
		std::function<void(const CVarValue&)> onChange
	)
	{
		std::string key(name);
		CVar var;
		var.name = key;
		var.description = std::string(description);
		var.value = defaultValue;
		var.defaultValue = defaultValue;
		var.flags = flags;
		var.onChange = std::move(onChange);
		m_cvars[key] = std::move(var);
		return &m_cvars[key];
	}

	CVar* CVarSystem::registerCVar(
		std::string_view name,
		std::string_view description,
		std::string_view defaultValue,
		CVarFlags flags,
		std::function<void(const CVarValue&)> onChange
	)
	{
		std::string key(name);
		CVar var;
		var.name = key;
		var.description = std::string(description);
		var.value = std::string(defaultValue);
		var.defaultValue = std::string(defaultValue);
		var.flags = flags;
		var.onChange = std::move(onChange);
		m_cvars[key] = std::move(var);
		return &m_cvars[key];
	}

	void CVarSystem::registerCommand(
		std::string_view name,
		std::string_view description,
		CVarCommandFunc callback
	)
	{
		std::string key(name);
		m_commands[key] = CVarCommand{ key, std::string(description), std::move(callback) };
	}

	CVar* CVarSystem::findCVar(std::string_view name)
	{
		auto it = m_cvars.find(std::string(name));
		return (it != m_cvars.end()) ? &it->second : nullptr;
	}

	const CVar* CVarSystem::findCVar(std::string_view name) const
	{
		auto it = m_cvars.find(std::string(name));
		return (it != m_cvars.end()) ? &it->second : nullptr;
	}

	bool CVarSystem::execute(std::string_view commandLine)
	{
		// Trim whitespace
		while (!commandLine.empty() && std::isspace(static_cast<unsigned char>(commandLine.front())))
		{
			commandLine.remove_prefix(1);
		}
		while (!commandLine.empty() && std::isspace(static_cast<unsigned char>(commandLine.back())))
		{
			commandLine.remove_suffix(1);
		}
		if (commandLine.empty()) return false;

		std::vector<std::string> tokens;
		std::stringstream ss{ std::string(commandLine) };
		std::string token;
		while (ss >> token)
		{
			tokens.push_back(token);
		}
		if (tokens.empty()) return false;

		std::string cmdName = tokens[0];

		// 1. Check commands
		auto cmdIt = m_commands.find(cmdName);
		if (cmdIt != m_commands.end())
		{
			std::vector<std::string> args(tokens.begin() + 1, tokens.end());
			cmdIt->second.callback(args);
			return true;
		}

		// 2. Check CVars
		auto cvarIt = m_cvars.find(cmdName);
		if (cvarIt != m_cvars.end())
		{
			if (tokens.size() > 1)
			{
				// Set value
				std::string newVal;
				for (size_t i = 1; i < tokens.size(); ++i)
				{
					if (i > 1) newVal += " ";
					newVal += tokens[i];
				}
				cvarIt->second.set(newVal);
				Core::Log::Get().info("CVar", "{} = {}", cmdName, cvarIt->second.asString());
			}
			else
			{
				// Print current value
				Core::Log::Get().info("CVar", "{} = {} ({})", cmdName, cvarIt->second.asString(), cvarIt->second.description);
			}
			return true;
		}

		Core::Log::Get().warn("CVar", "Unknown command or variable '{}'. Type 'help' for available options.", cmdName);
		return false;
	}

	std::vector<std::string> CVarSystem::getCompletions(std::string_view prefix) const
	{
		std::vector<std::string> results;
		std::string lowerPrefix(prefix);
		std::transform(lowerPrefix.begin(), lowerPrefix.end(), lowerPrefix.begin(), ::tolower);

		for (const auto& [name, _] : m_cvars)
		{
			std::string lowerName = name;
			std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
			if (lowerName.starts_with(lowerPrefix))
			{
				results.push_back(name);
			}
		}

		for (const auto& [name, _] : m_commands)
		{
			std::string lowerName = name;
			std::transform(lowerName.begin(), lowerName.end(), lowerName.begin(), ::tolower);
			if (lowerName.starts_with(lowerPrefix))
			{
				results.push_back(name);
			}
		}

		std::sort(results.begin(), results.end());
		return results;
	}
}
