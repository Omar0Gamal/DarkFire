#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Platform/KeyCodes.h"
#include <array>
#include <string>
#include <vector>
#include <unordered_map>

namespace Platform
{
	using Core::Vector2;

	class DF_API Input
	{
	public:
		static constexpr uint32_t kMaxGamepads = 4;

		static void init();
		static void newFrame();

		// Keyboard
		static bool isKeyPressed(KeyCode key) noexcept;
		static bool isKeyJustPressed(KeyCode key) noexcept;
		static bool isKeyJustReleased(KeyCode key) noexcept;

		// Mouse
		static bool isMouseButtonPressed(MouseCode button) noexcept;
		static bool isMouseButtonJustPressed(MouseCode button) noexcept;
		static bool isMouseButtonJustReleased(MouseCode button) noexcept;

		static Vector2 getMousePosition() noexcept;
		static Vector2 getMouseDelta() noexcept;
		static float getMouseScrollDelta() noexcept;

		// Gamepad (XInput)
		static bool isGamepadConnected(uint32_t playerIndex = 0) noexcept;
		static bool isGamepadButtonPressed(GamepadButton button, uint32_t playerIndex = 0) noexcept;
		static bool isGamepadButtonJustPressed(GamepadButton button, uint32_t playerIndex = 0) noexcept;
		static bool isGamepadButtonJustReleased(GamepadButton button, uint32_t playerIndex = 0) noexcept;
		static float getGamepadAxis(GamepadAxis axis, uint32_t playerIndex = 0) noexcept;
		static void setGamepadVibration(float leftMotor, float rightMotor, uint32_t playerIndex = 0) noexcept;

		// Action & Axis Mapping
		static void bindAction(const std::string& actionName, KeyCode key, GamepadButton gamepadBtn = GamepadButton::Count);
		static void bindAxis(const std::string& axisName, KeyCode positiveKey, KeyCode negativeKey, GamepadAxis gamepadAxis = GamepadAxis::Count);
		static bool isActionPressed(const std::string& actionName, uint32_t playerIndex = 0) noexcept;
		static bool isActionJustPressed(const std::string& actionName, uint32_t playerIndex = 0) noexcept;
		static float getAxis(const std::string& axisName, uint32_t playerIndex = 0) noexcept;

		// Called by Window message handler
		static void onKeyDown(KeyCode key) noexcept;
		static void onKeyUp(KeyCode key) noexcept;
		static void onMouseButtonDown(MouseCode button) noexcept;
		static void onMouseButtonUp(MouseCode button) noexcept;
		static void onMouseMove(float x, float y) noexcept;
		static void onMouseScroll(float delta) noexcept;

	private:
		struct ActionBinding
		{
			KeyCode key = KeyCode::Unknown;
			GamepadButton gamepadBtn = GamepadButton::Count;
		};

		struct AxisBinding
		{
			KeyCode positiveKey = KeyCode::Unknown;
			KeyCode negativeKey = KeyCode::Unknown;
			GamepadAxis gamepadAxis = GamepadAxis::Count;
		};

		struct GamepadState
		{
			bool connected = false;
			std::array<bool, static_cast<size_t>(GamepadButton::Count)> currentButtons{};
			std::array<bool, static_cast<size_t>(GamepadButton::Count)> previousButtons{};
			std::array<float, static_cast<size_t>(GamepadAxis::Count)> axes{};
		};

		struct InputState
		{
			std::array<bool, static_cast<size_t>(KeyCode::Count)> currentKeys{};
			std::array<bool, static_cast<size_t>(KeyCode::Count)> previousKeys{};

			std::array<bool, static_cast<size_t>(MouseCode::Count)> currentButtons{};
			std::array<bool, static_cast<size_t>(MouseCode::Count)> previousButtons{};

			Vector2 mousePosition{ 0.0f, 0.0f };
			Vector2 previousMousePosition{ 0.0f, 0.0f };
			Vector2 mouseDelta{ 0.0f, 0.0f };
			float mouseScrollDelta = 0.0f;
			bool firstMouseMove = true;

			std::array<GamepadState, kMaxGamepads> gamepads{};
			std::unordered_map<std::string, std::vector<ActionBinding>> actionBindings;
			std::unordered_map<std::string, std::vector<AxisBinding>> axisBindings;
		};

		static InputState s_state;
	};
}
