#include "InputManager.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace Core
{
	InputManager::InputManager()
	{
	}

	short InputManager::getWinKey(const Key& key)
	{
		if (key >= Key::A && key <= Key::Z)
			return 'A' + ((short)key - (short)Key::A);
		if (key >= Key::N0 && key <= Key::N9)
			return '0' + ((short)key - (short)Key::N0);
		if (key >= Key::F1 && key <= Key::F12)
			return VK_F1 + ((short)key - (short)Key::F1);
		if (key == Key::Shift)
			return VK_SHIFT;
		if (key == Key::Escape)
			return VK_ESCAPE;
		if (key == Key::Space)
			return VK_SPACE;
		if (key == Key::Up)
			return VK_UP;
		if (key == Key::Down)
			return VK_DOWN;
		if (key == Key::Left)
			return VK_LEFT;
		if (key == Key::Right)
			return VK_RIGHT;
		return 0;
	}

	bool InputManager::isKeyDown(const Key& key)
	{
		short keyWin = getWinKey(key);
		if (keyWin < 0 || keyWin >= 256) return false;
		return (m_keys_state_res[keyWin] == 0);
	}

	bool InputManager::isKeyUp(const Key& key)
	{
		short keyWin = getWinKey(key);
		if (keyWin < 0 || keyWin >= 256) return false;
		return (m_keys_state_res[keyWin] == 1);
	}

	bool InputManager::isKeyPressed(const Key& key)
	{
		short keyWin = getWinKey(key);
		if (keyWin < 0 || keyWin >= 256) return false;
		return (m_keys_state[keyWin] & 0x8000) && !(m_old_keys_state[keyWin] & 0x8000);
	}

	bool InputManager::isKeyReleased(const Key& key)
	{
		short keyWin = getWinKey(key);
		if (keyWin < 0 || keyWin >= 256) return false;
		return !(m_keys_state[keyWin] & 0x8000) && (m_old_keys_state[keyWin] & 0x8000);
	}

	bool InputManager::isMouseDown(const MouseButton& button)
	{
		int vKey = (button == MouseButton::Left) ? VK_LBUTTON : (button == MouseButton::Right) ? VK_RBUTTON : VK_MBUTTON;
		return (m_keys_state_res[vKey] == 0);
	}

	bool InputManager::isMouseUp(const MouseButton& button)
	{
		int vKey = (button == MouseButton::Left) ? VK_LBUTTON : (button == MouseButton::Right) ? VK_RBUTTON : VK_MBUTTON;
		return (m_keys_state_res[vKey] == 1);
	}

	bool InputManager::isMousePressed(const MouseButton& button)
	{
		int vKey = (button == MouseButton::Left) ? VK_LBUTTON : (button == MouseButton::Right) ? VK_RBUTTON : VK_MBUTTON;
		return (m_keys_state[vKey] & 0x8000) && !(m_old_keys_state[vKey] & 0x8000);
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
		::ShowCursor(!enable);
	}

	void InputManager::setScreenArea(const Box& area)
	{
		m_screenArea = area;
	}

	void InputManager::update()
	{
		POINT current_mouse_pos = {};
		::GetCursorPos(&current_mouse_pos);

		m_current_mouse_pos = Vector2d(static_cast<float>(current_mouse_pos.x), static_cast<float>(current_mouse_pos.y));

		if (m_first_time)
		{
			m_old_mouse_pos = m_current_mouse_pos;
			m_first_time = false;
		}

		m_deltaMouse = Vector2d(m_current_mouse_pos.x - m_old_mouse_pos.x, m_current_mouse_pos.y - m_old_mouse_pos.y);

		if (!m_playEnable)
		{
			m_old_mouse_pos = m_current_mouse_pos;
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

			if (m_keys_state[i] & 0x8000)
			{
				m_keys_state_res[i] = 0; // Down
			}
			else
			{
				if (m_old_keys_state[i] & 0x8000)
				{
					m_keys_state_res[i] = 1; // Just released
				}
				else
				{
					m_keys_state_res[i] = 2; // Up
				}
			}
		}

		::memcpy(m_old_keys_state, m_keys_state, sizeof(short) * 256);
	}
}