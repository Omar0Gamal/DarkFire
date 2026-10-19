#pragma once

#include "Engine/Core/Defines.h"
#include <string>
#include <cstdint>
#include <functional>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>

namespace Platform
{
	struct DF_API WindowDesc
	{
		std::string title = "DarkFire Engine";
		uint32_t width = 1280;
		uint32_t height = 720;
		bool resizable = true;
		bool fullscreen = false;
		bool vsync = true;
	};

	class DF_API Window
	{
	public:
		using ResizeCallback = std::function<void(uint32_t width, uint32_t height)>;
		using CloseCallback = std::function<void()>;

		Window();
		~Window();

		Window(const Window&) = delete;
		Window& operator=(const Window&) = delete;

		bool init(const WindowDesc& desc);
		void shutdown();

		void pollEvents();

		HWND getNativeHandle() const noexcept { return m_hwnd; }
		uint32_t getWidth() const noexcept { return m_width; }
		uint32_t getHeight() const noexcept { return m_height; }
		float getAspectRatio() const noexcept
		{
			return (m_height > 0) ? static_cast<float>(m_width) / static_cast<float>(m_height) : 1.0f;
		}

		bool shouldClose() const noexcept { return m_shouldClose; }
		void setShouldClose(bool close) noexcept { m_shouldClose = close; }

		bool isMinimized() const noexcept { return m_isMinimized; }
		bool isFocused() const noexcept { return m_isFocused; }

		void setTitle(const std::string& title);
		void setResizeCallback(ResizeCallback callback) { m_resizeCallback = std::move(callback); }
		void setCloseCallback(CloseCallback callback) { m_closeCallback = std::move(callback); }

		// Raw input hooks for Input subsystem
		void onKeyMessage(uint16_t vkCode, bool isDown);
		void onMouseButtonMessage(uint8_t button, bool isDown);
		void onMouseMoveMessage(int32_t x, int32_t y);
		void onMouseWheelMessage(float delta);

	private:
		static LRESULT CALLBACK StaticWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
		LRESULT HandleMessage(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

		HWND m_hwnd = nullptr;
		HINSTANCE m_hinstance = nullptr;
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		bool m_shouldClose = false;
		bool m_isMinimized = false;
		bool m_isFocused = false;

		ResizeCallback m_resizeCallback;
		CloseCallback m_closeCallback;
	};
}
