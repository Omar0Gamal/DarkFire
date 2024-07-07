#include "Display.h"
#include "Core/Graphics/GraphicsEngine.h"
#include "Core/Graphics/SwapChain.h"


namespace Core
{
	Display::Display(const wchar_t* title, int width, int height, GamePtr game)
		: Window(title, width, height)
	{
		m_game = game;
		m_swapChain = m_game->getGraphicsEngine()->CreateSwapChain(m_hwnd, width, height);
	}
	Display::~Display()
	{
	}
	void Display::onSize(const Box& size)
	{
		m_swapChain->resize(size);
		m_game->onDisplaySize(size);
	}
	SwapChainPtr Display::getSwapChain()
	{
		return m_swapChain;
	}
}