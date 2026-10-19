#pragma once

#include "Core/Graphics/Prerequisites.h"
#include "Core/Math/Vector2d.h"
#include "Core/Math/Box.h"

namespace Core
{
	class InputManager
	{
	public:
		InputManager();
		virtual ~InputManager() = default;

		virtual bool isKeyDown(const Key& key);
		virtual bool isKeyUp(const Key& key);
		virtual bool isKeyPressed(const Key& key);
		virtual bool isKeyReleased(const Key& key);

		virtual bool isMouseDown(const MouseButton& button);
		virtual bool isMouseUp(const MouseButton& button);
		virtual bool isMousePressed(const MouseButton& button);

		virtual float getMouseXAxis();
		virtual float getMouseYAxis();

		virtual Vector2d getMousePosition() const { return m_current_mouse_pos; }
		virtual Vector2d getDeltaMouse() const { return m_deltaMouse; }

		virtual void enablePlayMode(bool enable);
		bool isPlayModeEnabled() const { return m_playEnable; }

		void setScreenArea(const Box& area);
		void update();

	public:
		static short getWinKey(const Key& key);

	private:
		short m_keys_state[256] = {};
		short m_old_keys_state[256] = {};
		short m_keys_state_res[256] = {};

		bool m_playEnable = false;
		bool m_first_time = true;

		Box m_screenArea;

		Vector2d m_current_mouse_pos;
		Vector2d m_old_mouse_pos;
		Vector2d m_deltaMouse;
	};
}
