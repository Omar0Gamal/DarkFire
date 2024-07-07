#pragma once
#include <d3d11.h>
#include <wrl.h>
#include "Core/Math/Box.h"
#include "Prerequisites.h"

namespace Core
{
	class SwapChain
	{
	public:
		SwapChain(HWND hwnd, int width, int height, GraphicsEngine* system);
		void setFullScreen(bool fullscreen, const Box& size);
		void resize(const Box& size);
		bool present(bool vsync);
		void* getRenderTargetView();
		void* getDepthStencilView();

	private:
		void reloadBuffers(const Box& size);

	private:
		Microsoft::WRL::ComPtr<IDXGISwapChain> m_swap_chain = nullptr;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_rtv = nullptr;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_dsv = nullptr;
		GraphicsEngine* m_system = nullptr;
	};
}
