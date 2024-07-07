#include "InputManager.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace Core
{
	InputManager::InputManager() {}

	bool InputManager::isKeyDown(const  Key& key)
	{
		short keyWin = 0;

		if (key >= Key::A && key <= Key::Z)
			keyWin = 'A' + ((short)key - (short)Key::A);
		else if (key >= Key::N0 && key <= Key::N9)
			keyWin = '0' + ((short)key - (short)Key::N0);
		else if (key == Key::Shift)
			keyWin = VK_SHIFT;
		else if (key == Key::Escape)
			keyWin = VK_ESCAPE;
		else if (key == Key::Space)
			keyWin = VK_SPACE;

		return  (m_keys_state_res[keyWin] == 0);
	}

	bool InputManager::isKeyUp(const  Key& key)
	{
		short keyWin = 0;

		if (key >= Key::A && key <= Key::Z)
			keyWin = 'A' + ((short)key - 2);
		else if (key >= Key::N0 && key <= Key::N9)
			keyWin = '0' + ((short)key - 28);
		else if (key == Key::Shift)
			keyWin = VK_SHIFT;
		else if (key == Key::Escape)
			keyWin = VK_ESCAPE;
		else if (key == Key::Space)
			keyWin = VK_SPACE;

		return  (m_keys_state_res[keyWin] == 1);
	}

	bool InputManager::isMouseDown(const MouseButton& button)
	{
		if (button == MouseButton::Left)
		{
			auto state = m_keys_state_res[VK_LBUTTON];
			return (state == 0);

		}
		else if (button == MouseButton::Middle)
		{
			auto state = m_keys_state_res[VK_MBUTTON];
			return (state == 0);
		}
		else if (button == MouseButton::Right)
		{
			auto state = m_keys_state_res[VK_RBUTTON];
			return (state == 0);
		}

		return false;
	}

	bool InputManager::isMouseUp(const  MouseButton& button)
	{
		if (button == MouseButton::Left)
		{
			auto state = m_keys_state_res[VK_LBUTTON];
			return  (state == 1);

		}
		else if (button == MouseButton::Middle)
		{
			auto state = m_keys_state_res[VK_MBUTTON];
			return  (state == 1);
		}
		else if (button == MouseButton::Right)
		{
			auto state = m_keys_state_res[VK_RBUTTON];
			return (state == 1);
		}
		return false;
	}

	float InputManager::getMouseXAxis()
	{
		return m_deltaMouse.x;
	}

	float InputManager::getMouseYAxis()
	{
		return m_deltaMouse.y;
	}

	void InputManager::enablePlayMode(bool enable)
	{
		m_playEnable = enable;
		ShowCursor(!enable);
	}

	void InputManager::setScreenArea(const  Box& area)
	{
		m_screenArea = area;
	}

	void InputManager::update()
	{
		POINT current_mouse_pos = {};
		::GetCursorPos(&current_mouse_pos);

		if (current_mouse_pos.x != (int)m_old_mouse_pos.x || current_mouse_pos.y != (int)m_old_mouse_pos.y)
		{
			m_deltaMouse = Vector2d((float)current_mouse_pos.x - (float)m_old_mouse_pos.x, (float)current_mouse_pos.y - (float)m_old_mouse_pos.y);
		}
		else
		{
			m_deltaMouse = Vector2d(0, 0);
		}


		if (!m_playEnable)
		{
			m_old_mouse_pos = Vector2d((float)current_mouse_pos.x, (float)current_mouse_pos.y);
		}
		else
		{
			Vector2d center_screen = Vector2d(m_screenArea.left + (float)m_screenArea.width / 2.0f, m_screenArea.top + (float)m_screenArea.height / 2.0f);
			::SetCursorPos((int)center_screen.x, (int)center_screen.y);
			m_old_mouse_pos = center_screen;
		}


		for (unsigned int i = 0; i < 256; i++)
		{
			m_keys_state[i] = ::GetAsyncKeyState(i);


			if (m_keys_state[i] & 0x8001)
			{
				m_keys_state_res[i] = 0;
			}
			else // KEY IS UP
			{
				if (m_keys_state[i] != m_old_keys_state[i])
				{
					m_keys_state_res[i] = 1;
				}
				else
				{
					m_keys_state_res[i] = 2;
				}
			}
		}

		::memcpy(m_old_keys_state, m_keys_state, sizeof(short) * 256);
	}
}