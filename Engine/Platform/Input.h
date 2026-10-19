#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Platform/KeyCodes.h"
#include <array>

namespace Platform
{
	using Core::Vector2;

	class DF_API Input
	{
	public:
		static void init();
		static void newFrame();

		static bool isKeyPressed(KeyCode key) noexcept;
		static bool isKeyJustPressed(KeyCode key) noexcept;
		static bool isKeyJustReleased(KeyCode key) noexcept;

		static bool isMouseButtonPressed(MouseCode button) noexcept;
		static bool isMouseButtonJustPressed(MouseCode button) noexcept;
		static bool isMouseButtonJustReleased(MouseCode button) noexcept;

		static Vector2 getMousePosition() noexcept;
		static Vector2 getMouseDelta() noexcept;
		static float getMouseScrollDelta() noexcept;

		// Called by Window message handler
		static void onKeyDown(KeyCode key) noexcept;
		static void onKeyUp(KeyCode key) noexcept;
		static void onMouseButtonDown(MouseCode button) noexcept;
		static void onMouseButtonUp(MouseCode button) noexcept;
		static void onMouseMove(float x, float y) noexcept;
		static void onMouseScroll(float delta) noexcept;

	private:
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
		};

		static InputState s_state;
	};
}
