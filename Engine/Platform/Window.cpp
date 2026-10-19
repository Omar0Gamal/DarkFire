#include "Engine/Platform/Window.h"
#include "Engine/Platform/Input.h"
#include "Engine/Core/Log/Log.h"

#if defined(DF_EDITOR)
extern LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
#endif

namespace Platform
{
	static std::wstring Utf8ToWide(std::string_view utf8Str)
	{
		if (utf8Str.empty())
		{
			return L"";
		}
		int sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, utf8Str.data(), static_cast<int>(utf8Str.size()), nullptr, 0);
		std::wstring result(sizeNeeded, 0);
		MultiByteToWideChar(CP_UTF8, 0, utf8Str.data(), static_cast<int>(utf8Str.size()), result.data(), sizeNeeded);
		return result;
	}

	Window::Window()
		: m_hinstance(GetModuleHandleW(nullptr))
	{
	}

	Window::~Window()
	{
		shutdown();
	}

	bool Window::init(const WindowDesc& desc)
	{
		m_width = desc.width;
		m_height = desc.height;

		const wchar_t* className = L"DarkFireWindowClass";

		WNDCLASSEXW wc{};
		wc.cbSize = sizeof(WNDCLASSEXW);
		wc.style = CS_HREDRAW | CS_VREDRAW | CS_OWNDC;
		wc.lpfnWndProc = StaticWndProc;
		wc.hInstance = m_hinstance;
		wc.hCursor = LoadCursorW(nullptr, IDC_ARROW);
		wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
		wc.lpszClassName = className;

		RegisterClassExW(&wc);

		DWORD style = WS_OVERLAPPEDWINDOW;
		if (!desc.resizable)
		{
			style &= ~(WS_THICKFRAME | WS_MAXIMIZEBOX);
		}

		RECT wr{ 0, 0, static_cast<LONG>(m_width), static_cast<LONG>(m_height) };
		AdjustWindowRect(&wr, style, FALSE);

		std::wstring wideTitle = Utf8ToWide(desc.title);

		m_hwnd = CreateWindowExW(
			0,
			className,
			wideTitle.c_str(),
			style,
			CW_USEDEFAULT, CW_USEDEFAULT,
			wr.right - wr.left,
			wr.bottom - wr.top,
			nullptr,
			nullptr,
			m_hinstance,
			this
		);

		if (!m_hwnd)
		{
			Core::Log::Get().fatal("Platform", "Failed to create Win32 window!");
			return false;
		}

		SetWindowLongPtrW(m_hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(this));

		ShowWindow(m_hwnd, SW_SHOW);
		UpdateWindow(m_hwnd);

		m_isFocused = true;
		Input::init();

		Core::Log::Get().info("Platform", "Window '{}' created ({}, {})", desc.title, m_width, m_height);
		return true;
	}

	void Window::shutdown()
	{
		if (m_hwnd)
		{
			DestroyWindow(m_hwnd);
			m_hwnd = nullptr;
		}
	}

	void Window::pollEvents()
	{
		MSG msg;
		while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			if (msg.message == WM_QUIT)
			{
				m_shouldClose = true;
			}
			TranslateMessage(&msg);
			DispatchMessageW(&msg);
		}
	}

	void Window::setTitle(const std::string& title)
	{
		if (m_hwnd)
		{
			std::wstring wideTitle = Utf8ToWide(title);
			SetWindowTextW(m_hwnd, wideTitle.c_str());
		}
	}

	LRESULT CALLBACK Window::StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
		Window* window = reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd, GWLP_USERDATA));
		if (!window && msg == WM_CREATE)
		{
			CREATESTRUCTW* cs = reinterpret_cast<CREATESTRUCTW*>(lParam);
			window = reinterpret_cast<Window*>(cs->lpCreateParams);
			SetWindowLongPtrW(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(window));
		}

		if (window)
		{
			return window->HandleMessage(hwnd, msg, wParam, lParam);
		}

		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}

	LRESULT Window::HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
	{
#if defined(DF_EDITOR)
		if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
		{
			return 1;
		}
#endif

		switch (msg)
		{
		case WM_CLOSE:
		{
			m_shouldClose = true;
			if (m_closeCallback)
			{
				m_closeCallback();
			}
			return 0;
		}
		case WM_SIZE:
		{
			if (wParam == SIZE_MINIMIZED)
			{
				m_isMinimized = true;
			}
			else
			{
				m_isMinimized = false;
				m_width = LOWORD(lParam);
				m_height = HIWORD(lParam);
				if (m_resizeCallback && m_width > 0 && m_height > 0)
				{
					m_resizeCallback(m_width, m_height);
				}
			}
			return 0;
		}
		case WM_SETFOCUS:
			m_isFocused = true;
			return 0;
		case WM_KILLFOCUS:
			m_isFocused = false;
			return 0;
		case WM_KEYDOWN:
		case WM_SYSKEYDOWN:
		{
			auto key = static_cast<KeyCode>(wParam);
			Input::onKeyDown(key);
			return 0;
		}
		case WM_KEYUP:
		case WM_SYSKEYUP:
		{
			auto key = static_cast<KeyCode>(wParam);
			Input::onKeyUp(key);
			return 0;
		}
		case WM_LBUTTONDOWN:
			Input::onMouseButtonDown(MouseCode::Left);
			return 0;
		case WM_LBUTTONUP:
			Input::onMouseButtonUp(MouseCode::Left);
			return 0;
		case WM_RBUTTONDOWN:
			Input::onMouseButtonDown(MouseCode::Right);
			return 0;
		case WM_RBUTTONUP:
			Input::onMouseButtonUp(MouseCode::Right);
			return 0;
		case WM_MBUTTONDOWN:
			Input::onMouseButtonDown(MouseCode::Middle);
			return 0;
		case WM_MBUTTONUP:
			Input::onMouseButtonUp(MouseCode::Middle);
			return 0;
		case WM_MOUSEMOVE:
		{
			int32_t x = static_cast<short>(LOWORD(lParam));
			int32_t y = static_cast<short>(HIWORD(lParam));
			Input::onMouseMove(static_cast<float>(x), static_cast<float>(y));
			return 0;
		}
		case WM_MOUSEWHEEL:
		{
			float delta = static_cast<float>(GET_WHEEL_DELTA_WPARAM(wParam)) / static_cast<float>(WHEEL_DELTA);
			Input::onMouseScroll(delta);
			return 0;
		}
		case WM_DESTROY:
			PostQuitMessage(0);
			return 0;
		}

		return DefWindowProcW(hwnd, msg, wParam, lParam);
	}
}
