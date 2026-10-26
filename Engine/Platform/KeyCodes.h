#pragma once

#include <cstdint>

namespace Platform
{
	enum class KeyCode : uint16_t
	{
		Unknown = 0,

		// Letters
		A = 0x41,
		B = 0x42,
		C = 0x43,
		D = 0x44,
		E = 0x45,
		F = 0x46,
		G = 0x47,
		H = 0x48,
		I = 0x49,
		J = 0x4A,
		K = 0x4B,
		L = 0x4C,
		M = 0x4D,
		N = 0x4E,
		O = 0x4F,
		P = 0x50,
		Q = 0x51,
		R = 0x52,
		S = 0x53,
		T = 0x54,
		U = 0x55,
		V = 0x56,
		W = 0x57,
		X = 0x58,
		Y = 0x59,
		Z = 0x5A,

		// Numbers
		Num0 = 0x30,
		Num1 = 0x31,
		Num2 = 0x32,
		Num3 = 0x33,
		Num4 = 0x34,
		Num5 = 0x35,
		Num6 = 0x36,
		Num7 = 0x37,
		Num8 = 0x38,
		Num9 = 0x39,

		// Function Keys
		F1 = 0x70,
		F2 = 0x71,
		F3 = 0x72,
		F4 = 0x73,
		F5 = 0x74,
		F6 = 0x75,
		F7 = 0x76,
		F8 = 0x77,
		F9 = 0x78,
		F10 = 0x79,
		F11 = 0x7A,
		F12 = 0x7B,

		// Controls & Navigation
		Escape = 0x1B,
		Space = 0x20,
		Enter = 0x0D,
		Tab = 0x09,
		Backspace = 0x08,
		Insert = 0x2D,
		Delete = 0x2E,
		PageUp = 0x21,
		PageDown = 0x22,
		Home = 0x24,
		End = 0x23,
		Left = 0x25,
		Up = 0x26,
		Right = 0x27,
		Down = 0x28,

		// Modifiers
		LeftShift = 0xA0,
		RightShift = 0xA1,
		LeftControl = 0xA2,
		RightControl = 0xA3,
		LeftAlt = 0xA4,
		RightAlt = 0xA5,

		Count = 512
	};

	enum class MouseCode : uint8_t
	{
		Left = 0,
		Right = 1,
		Middle = 2,
		Button4 = 3,
		Button5 = 4,

		Count = 5
	};

	enum class GamepadButton : uint8_t
	{
		DPadUp = 0, DPadDown, DPadLeft, DPadRight,
		Start, Back, LeftThumb, RightThumb,
		LeftBumper, RightBumper,
		A, B, X, Y,
		Count
	};

	enum class GamepadAxis : uint8_t
	{
		LeftStickX = 0, LeftStickY,
		RightStickX, RightStickY,
		LeftTrigger, RightTrigger,
		Count
	};
}
