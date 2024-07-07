#pragma once

#include "Core/Math/Box.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

namespace Core
{
	class Window
	{
	public:
		Window();
		Window(const wchar_t* title, int width, int height);
		virtual ~Window();
	public:
		Box getClientSize();
		Box getScreenSize();
	public:
		//EVENTS
		virtual void onCreate() {}
		virtual void onUpdate() {}
		virtual void onDestroy() {}
		virtual void onFocus() {}
		virtual void onKillFocus() {}
		virtual void onSize(const Box& size) {}
	protected:
		HWND m_hwnd;
		const wchar_t* m_title;
		int m_width;
		int m_height;
	};
}

