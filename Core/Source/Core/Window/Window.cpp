#include "Window.h"
#include "Core/log/Log.h"
namespace Core
{
    Window::Window()
        : Window(L"Dark Engine",800,600)
    {
    }

	LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
	{
		switch (msg)
		{
		case WM_NCCREATE:
		{
			auto createStruct = reinterpret_cast<CREATESTRUCT*>(lparam);
			if (createStruct)
			{
				SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(createStruct->lpCreateParams));
			}
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
		case WM_SIZE:
		{
			// Event fired when the window is resized
			auto window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
			if (window) window->onSize(window->getClientSize());
			break;
		}
		case WM_SETFOCUS:
		{
			// Event fired when the window get focus
			auto window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
			if (window) window->onFocus();
			break;
		}
		case WM_KILLFOCUS:
		{
			// Event fired when the window lost focus
			auto window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
			if(window) window->onKillFocus();
			break;
		}
		case WM_DESTROY:
		{
			// Event fired when the window is destroyed
			auto window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
			if(window)window->onDestroy();
			//::PostQuitMessage(0);
			break;
		}
		case WM_MOVE:
		{
			// Event fired when the window is destroyed
			auto window = reinterpret_cast<Window*>(GetWindowLongPtr(hwnd, GWLP_USERDATA));
			if (window) window->onSize(window->getClientSize());
			//::PostQuitMessage(0);
			break;
		}
		case WM_CLOSE:
		{
			PostQuitMessage(0);
			break;
		}

		default:
			return DefWindowProc(hwnd, msg, wparam, lparam);
		}
		return NULL;
	}

	Window::Window(const wchar_t* title, int width, int height)
		: m_hwnd(NULL), m_width(width), m_height(height), m_title(title)
	{
		WNDCLASSEX wc = {};
		wc.cbSize = sizeof(WNDCLASSEX);
		wc.lpszClassName = L"DarkWindow";
		wc.lpfnWndProc = &WndProc;

		ATOM classId = 0;
		if (!GetClassInfoEx(GetModuleHandle(nullptr), L"DarkWindow", &wc))
		{
			classId = RegisterClassEx(&wc);
			if (!classId) throw std::runtime_error("RegisterClassEx failed");
		}

		RECT wr = { 0, 0, m_width, m_height };
		AdjustWindowRect(&wr, WS_OVERLAPPEDWINDOW, FALSE);

		m_hwnd = CreateWindowEx(NULL, L"DarkWindow", m_title,
			WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, 
			wr.right - wr.left, wr.bottom - wr.top, 
			HWND(), HMENU(), GetModuleHandle(nullptr), this);

		if (!m_hwnd)
		{
			darklog.error(L"Failed to create window");
			return;
		}

		SetWindowLongPtr(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

		ShowWindow(m_hwnd, SW_SHOW);
		UpdateWindow(m_hwnd);
	}

	

	Window::~Window()
	{
		if (m_hwnd)
		{
			SetWindowLongPtr(m_hwnd, GWLP_USERDATA, 0);
			if (!DestroyWindow(m_hwnd))
			{
				darklog.error(L"Failed to destroy window");
				return;
			}
			m_hwnd = NULL;
		}
		UnregisterClass(L"DarkWindow", GetModuleHandle(nullptr));
	}

	Box Window::getClientSize()
	{
		RECT rc = {};
		GetClientRect(this->m_hwnd, &rc);
		ClientToScreen(this->m_hwnd, (LPPOINT)&rc.left);
		ClientToScreen(this->m_hwnd, (LPPOINT)&rc.right);
		return Box(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
	}

	Box Window::getScreenSize()
	{
		RECT rc = {};
		rc.right = GetSystemMetrics(SM_CXSCREEN);
		rc.bottom = GetSystemMetrics(SM_CYSCREEN);
		return {0,0,(unsigned int)rc.right - rc.left ,(unsigned int)rc.bottom-rc.top};
	}
}
