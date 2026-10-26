#include "Engine/Platform/Input.h"
#include <algorithm>
#include <cmath>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <Windows.h>
#include <Xinput.h>
#pragma comment(lib, "xinput.lib")
#endif

namespace Platform
{
	Input::InputState Input::s_state;

	void Input::init()
	{
		s_state = InputState{};
	}

	void Input::newFrame()
	{
		s_state.previousKeys = s_state.currentKeys;
		s_state.previousButtons = s_state.currentButtons;
		s_state.mouseDelta = Vector2(0.0f, 0.0f);
		s_state.mouseScrollDelta = 0.0f;

		for (uint32_t p = 0; p < kMaxGamepads; ++p)
		{
			s_state.gamepads[p].previousButtons = s_state.gamepads[p].currentButtons;

#ifdef _WIN32
			XINPUT_STATE xstate;
			ZeroMemory(&xstate, sizeof(XINPUT_STATE));

			DWORD dwResult = XInputGetState(static_cast<DWORD>(p), &xstate);
			if (dwResult == ERROR_SUCCESS)
			{
				s_state.gamepads[p].connected = true;

				const WORD wButtons = xstate.Gamepad.wButtons;
				auto& btns = s_state.gamepads[p].currentButtons;
				btns[static_cast<size_t>(GamepadButton::DPadUp)] = (wButtons & XINPUT_GAMEPAD_DPAD_UP) != 0;
				btns[static_cast<size_t>(GamepadButton::DPadDown)] = (wButtons & XINPUT_GAMEPAD_DPAD_DOWN) != 0;
				btns[static_cast<size_t>(GamepadButton::DPadLeft)] = (wButtons & XINPUT_GAMEPAD_DPAD_LEFT) != 0;
				btns[static_cast<size_t>(GamepadButton::DPadRight)] = (wButtons & XINPUT_GAMEPAD_DPAD_RIGHT) != 0;
				btns[static_cast<size_t>(GamepadButton::Start)] = (wButtons & XINPUT_GAMEPAD_START) != 0;
				btns[static_cast<size_t>(GamepadButton::Back)] = (wButtons & XINPUT_GAMEPAD_BACK) != 0;
				btns[static_cast<size_t>(GamepadButton::LeftThumb)] = (wButtons & XINPUT_GAMEPAD_LEFT_THUMB) != 0;
				btns[static_cast<size_t>(GamepadButton::RightThumb)] = (wButtons & XINPUT_GAMEPAD_RIGHT_THUMB) != 0;
				btns[static_cast<size_t>(GamepadButton::LeftBumper)] = (wButtons & XINPUT_GAMEPAD_LEFT_SHOULDER) != 0;
				btns[static_cast<size_t>(GamepadButton::RightBumper)] = (wButtons & XINPUT_GAMEPAD_RIGHT_SHOULDER) != 0;
				btns[static_cast<size_t>(GamepadButton::A)] = (wButtons & XINPUT_GAMEPAD_A) != 0;
				btns[static_cast<size_t>(GamepadButton::B)] = (wButtons & XINPUT_GAMEPAD_B) != 0;
				btns[static_cast<size_t>(GamepadButton::X)] = (wButtons & XINPUT_GAMEPAD_X) != 0;
				btns[static_cast<size_t>(GamepadButton::Y)] = (wButtons & XINPUT_GAMEPAD_Y) != 0;

				// Thumbsticks with 0.15f radial deadzone
				constexpr float deadzone = 0.15f;

				// Left Stick
				float lx = static_cast<float>(xstate.Gamepad.sThumbLX) / 32767.0f;
				float ly = static_cast<float>(xstate.Gamepad.sThumbLY) / 32767.0f;
				lx = std::clamp(lx, -1.0f, 1.0f);
				ly = std::clamp(ly, -1.0f, 1.0f);
				const float lMag = std::sqrt(lx * lx + ly * ly);
				if (lMag < deadzone)
				{
					lx = 0.0f;
					ly = 0.0f;
				}
				else
				{
					const float normMag = std::min(1.0f, (lMag - deadzone) / (1.0f - deadzone));
					lx = (lx / lMag) * normMag;
					ly = (ly / lMag) * normMag;
				}

				// Right Stick
				float rx = static_cast<float>(xstate.Gamepad.sThumbRX) / 32767.0f;
				float ry = static_cast<float>(xstate.Gamepad.sThumbRY) / 32767.0f;
				rx = std::clamp(rx, -1.0f, 1.0f);
				ry = std::clamp(ry, -1.0f, 1.0f);
				const float rMag = std::sqrt(rx * rx + ry * ry);
				if (rMag < deadzone)
				{
					rx = 0.0f;
					ry = 0.0f;
				}
				else
				{
					const float normMag = std::min(1.0f, (rMag - deadzone) / (1.0f - deadzone));
					rx = (rx / rMag) * normMag;
					ry = (ry / rMag) * normMag;
				}

				// Triggers normalized 0.0f .. 1.0f
				const float lt = static_cast<float>(xstate.Gamepad.bLeftTrigger) / 255.0f;
				const float rt = static_cast<float>(xstate.Gamepad.bRightTrigger) / 255.0f;

				auto& axes = s_state.gamepads[p].axes;
				axes[static_cast<size_t>(GamepadAxis::LeftStickX)] = lx;
				axes[static_cast<size_t>(GamepadAxis::LeftStickY)] = ly;
				axes[static_cast<size_t>(GamepadAxis::RightStickX)] = rx;
				axes[static_cast<size_t>(GamepadAxis::RightStickY)] = ry;
				axes[static_cast<size_t>(GamepadAxis::LeftTrigger)] = lt;
				axes[static_cast<size_t>(GamepadAxis::RightTrigger)] = rt;
			}
			else
			{
				s_state.gamepads[p].connected = false;
				s_state.gamepads[p].currentButtons.fill(false);
				s_state.gamepads[p].axes.fill(0.0f);
			}
#else
			s_state.gamepads[p].connected = false;
			s_state.gamepads[p].currentButtons.fill(false);
			s_state.gamepads[p].axes.fill(0.0f);
#endif
		}
	}

	bool Input::isKeyPressed(KeyCode key) noexcept
	{
		auto index = static_cast<size_t>(key);
		return (index < static_cast<size_t>(KeyCode::Count)) ? s_state.currentKeys[index] : false;
	}

	bool Input::isKeyJustPressed(KeyCode key) noexcept
	{
		auto index = static_cast<size_t>(key);
		if (index >= static_cast<size_t>(KeyCode::Count))
		{
			return false;
		}
		return s_state.currentKeys[index] && !s_state.previousKeys[index];
	}

	bool Input::isKeyJustReleased(KeyCode key) noexcept
	{
		auto index = static_cast<size_t>(key);
		if (index >= static_cast<size_t>(KeyCode::Count))
		{
			return false;
		}
		return !s_state.currentKeys[index] && s_state.previousKeys[index];
	}

	bool Input::isMouseButtonPressed(MouseCode button) noexcept
	{
		auto index = static_cast<size_t>(button);
		return (index < static_cast<size_t>(MouseCode::Count)) ? s_state.currentButtons[index] : false;
	}

	bool Input::isMouseButtonJustPressed(MouseCode button) noexcept
	{
		auto index = static_cast<size_t>(button);
		if (index >= static_cast<size_t>(MouseCode::Count))
		{
			return false;
		}
		return s_state.currentButtons[index] && !s_state.previousButtons[index];
	}

	bool Input::isMouseButtonJustReleased(MouseCode button) noexcept
	{
		auto index = static_cast<size_t>(button);
		if (index >= static_cast<size_t>(MouseCode::Count))
		{
			return false;
		}
		return !s_state.currentButtons[index] && s_state.previousButtons[index];
	}

	Vector2 Input::getMousePosition() noexcept
	{
		return s_state.mousePosition;
	}

	Vector2 Input::getMouseDelta() noexcept
	{
		return s_state.mouseDelta;
	}

	float Input::getMouseScrollDelta() noexcept
	{
		return s_state.mouseScrollDelta;
	}

	bool Input::isGamepadConnected(uint32_t playerIndex) noexcept
	{
		if (playerIndex >= kMaxGamepads) return false;
		return s_state.gamepads[playerIndex].connected;
	}

	bool Input::isGamepadButtonPressed(GamepadButton button, uint32_t playerIndex) noexcept
	{
		if (playerIndex >= kMaxGamepads) return false;
		auto idx = static_cast<size_t>(button);
		if (idx >= static_cast<size_t>(GamepadButton::Count)) return false;
		return s_state.gamepads[playerIndex].currentButtons[idx];
	}

	bool Input::isGamepadButtonJustPressed(GamepadButton button, uint32_t playerIndex) noexcept
	{
		if (playerIndex >= kMaxGamepads) return false;
		auto idx = static_cast<size_t>(button);
		if (idx >= static_cast<size_t>(GamepadButton::Count)) return false;
		return s_state.gamepads[playerIndex].currentButtons[idx] && !s_state.gamepads[playerIndex].previousButtons[idx];
	}

	bool Input::isGamepadButtonJustReleased(GamepadButton button, uint32_t playerIndex) noexcept
	{
		if (playerIndex >= kMaxGamepads) return false;
		auto idx = static_cast<size_t>(button);
		if (idx >= static_cast<size_t>(GamepadButton::Count)) return false;
		return !s_state.gamepads[playerIndex].currentButtons[idx] && s_state.gamepads[playerIndex].previousButtons[idx];
	}

	float Input::getGamepadAxis(GamepadAxis axis, uint32_t playerIndex) noexcept
	{
		if (playerIndex >= kMaxGamepads) return 0.0f;
		auto idx = static_cast<size_t>(axis);
		if (idx >= static_cast<size_t>(GamepadAxis::Count)) return 0.0f;
		return s_state.gamepads[playerIndex].axes[idx];
	}

	void Input::setGamepadVibration(float leftMotor, float rightMotor, uint32_t playerIndex) noexcept
	{
		if (playerIndex >= kMaxGamepads) return;
#ifdef _WIN32
		XINPUT_VIBRATION vib;
		leftMotor = std::clamp(leftMotor, 0.0f, 1.0f);
		rightMotor = std::clamp(rightMotor, 0.0f, 1.0f);
		vib.wLeftMotorSpeed = static_cast<WORD>(leftMotor * 65535.0f);
		vib.wRightMotorSpeed = static_cast<WORD>(rightMotor * 65535.0f);
		XInputSetState(static_cast<DWORD>(playerIndex), &vib);
#else
		(void)leftMotor;
		(void)rightMotor;
#endif
	}

	void Input::bindAction(const std::string& actionName, KeyCode key, GamepadButton gamepadBtn)
	{
		s_state.actionBindings[actionName].push_back(ActionBinding{ key, gamepadBtn });
	}

	void Input::bindAxis(const std::string& axisName, KeyCode positiveKey, KeyCode negativeKey, GamepadAxis gamepadAxis)
	{
		s_state.axisBindings[axisName].push_back(AxisBinding{ positiveKey, negativeKey, gamepadAxis });
	}

	bool Input::isActionPressed(const std::string& actionName, uint32_t playerIndex) noexcept
	{
		auto it = s_state.actionBindings.find(actionName);
		if (it == s_state.actionBindings.end()) return false;

		for (const auto& binding : it->second)
		{
			if (binding.key != KeyCode::Unknown && isKeyPressed(binding.key))
			{
				return true;
			}
			if (binding.gamepadBtn != GamepadButton::Count && isGamepadButtonPressed(binding.gamepadBtn, playerIndex))
			{
				return true;
			}
		}
		return false;
	}

	bool Input::isActionJustPressed(const std::string& actionName, uint32_t playerIndex) noexcept
	{
		auto it = s_state.actionBindings.find(actionName);
		if (it == s_state.actionBindings.end()) return false;

		for (const auto& binding : it->second)
		{
			if (binding.key != KeyCode::Unknown && isKeyJustPressed(binding.key))
			{
				return true;
			}
			if (binding.gamepadBtn != GamepadButton::Count && isGamepadButtonJustPressed(binding.gamepadBtn, playerIndex))
			{
				return true;
			}
		}
		return false;
	}

	float Input::getAxis(const std::string& axisName, uint32_t playerIndex) noexcept
	{
		auto it = s_state.axisBindings.find(axisName);
		if (it == s_state.axisBindings.end()) return 0.0f;

		float value = 0.0f;
		for (const auto& binding : it->second)
		{
			float keyVal = 0.0f;
			if (binding.positiveKey != KeyCode::Unknown && isKeyPressed(binding.positiveKey))
			{
				keyVal += 1.0f;
			}
			if (binding.negativeKey != KeyCode::Unknown && isKeyPressed(binding.negativeKey))
			{
				keyVal -= 1.0f;
			}

			float padVal = 0.0f;
			if (binding.gamepadAxis != GamepadAxis::Count)
			{
				padVal = getGamepadAxis(binding.gamepadAxis, playerIndex);
			}

			value += (keyVal + padVal);
		}

		return std::clamp(value, -1.0f, 1.0f);
	}

	void Input::onKeyDown(KeyCode key) noexcept
	{
		auto index = static_cast<size_t>(key);
		if (index < static_cast<size_t>(KeyCode::Count))
		{
			s_state.currentKeys[index] = true;
		}
	}

	void Input::onKeyUp(KeyCode key) noexcept
	{
		auto index = static_cast<size_t>(key);
		if (index < static_cast<size_t>(KeyCode::Count))
		{
			s_state.currentKeys[index] = false;
		}
	}

	void Input::onMouseButtonDown(MouseCode button) noexcept
	{
		auto index = static_cast<size_t>(button);
		if (index < static_cast<size_t>(MouseCode::Count))
		{
			s_state.currentButtons[index] = true;
		}
	}

	void Input::onMouseButtonUp(MouseCode button) noexcept
	{
		auto index = static_cast<size_t>(button);
		if (index < static_cast<size_t>(MouseCode::Count))
		{
			s_state.currentButtons[index] = false;
		}
	}

	void Input::onMouseMove(float x, float y) noexcept
	{
		if (s_state.firstMouseMove)
		{
			s_state.mousePosition = Vector2(x, y);
			s_state.previousMousePosition = s_state.mousePosition;
			s_state.firstMouseMove = false;
			return;
		}

		s_state.mouseDelta.x += (x - s_state.mousePosition.x);
		s_state.mouseDelta.y += (y - s_state.mousePosition.y);
		s_state.mousePosition = Vector2(x, y);
	}

	void Input::onMouseScroll(float delta) noexcept
	{
		s_state.mouseScrollDelta += delta;
	}
}
