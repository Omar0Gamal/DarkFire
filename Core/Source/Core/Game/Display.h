#pragma once
#include "Core/Window/Window.h"
#include "Core/Graphics/Prerequisites.h"
#include "Core/Game/Game.h"
namespace Core
{
	class Display : public Window
	{
	public:
		Display(const wchar_t* title, int width, int height, Game* game);
		virtual ~Display();
	public:
		virtual void onSize(const  Box& size) override;
	public:
		SwapChainPtr getSwapChain();
	private:
		Game* m_game = nullptr;
		SwapChainPtr m_swapChain;
		friend class Game;
	};
}
