#include "Engine/Platform/Input.h"

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
