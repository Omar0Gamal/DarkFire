#include "SwapChain.h"
#include "GraphicsEngine.h"
#include <Core/log/Log.h>

namespace Core
{
	SwapChain::SwapChain(HWND hwnd, int width, int height, GraphicsEngine* system)
		: m_system(system)
	{
		auto device = m_system->m_d3dDevice;

		DXGI_SWAP_CHAIN_DESC d3d11Desc = {};
		d3d11Desc.BufferCount = 1;
		d3d11Desc.BufferDesc.Width = (width > 0) ? width : 1;
		d3d11Desc.BufferDesc.Height = (height > 0) ? height : 1;
		d3d11Desc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		d3d11Desc.BufferDesc.RefreshRate.Numerator = 60;
		d3d11Desc.BufferDesc.RefreshRate.Denominator = 1;
		d3d11Desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		d3d11Desc.OutputWindow = hwnd;
		d3d11Desc.SampleDesc.Count = 1;
		d3d11Desc.SampleDesc.Quality = 0;
		d3d11Desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		d3d11Desc.Windowed = TRUE;

		HRESULT hr = m_system->m_dxgiFactory->CreateSwapChain(device.Get(), &d3d11Desc, &m_swap_chain);

		if (FAILED(hr))
		{
			darklog.error(L"Failed to create swap chain");
			return;
		}

		Box size;
		size.width = width;
		size.height = height;

		reloadBuffers(size);
	}

	SwapChain::~SwapChain()
	{
		if (m_swap_chain)
		{
			m_swap_chain->SetFullscreenState(FALSE, nullptr);
		}
	}

	bool SwapChain::present(bool vsync)
	{
		m_swap_chain->Present(vsync, NULL);
		return true;
	}
	void* SwapChain::getRenderTargetView()
	{
		return m_rtv.Get();
	}
	void* SwapChain::getDepthStencilView()
	{
		return m_dsv.Get();
	}
	void SwapChain::resize(const Box& size)
	{
		if (!m_swap_chain) return;

		if (m_system && m_system->m_immediateContext)
		{
			ID3D11RenderTargetView* nullRTV = nullptr;
			m_system->m_immediateContext->OMSetRenderTargets(1, &nullRTV, nullptr);
			m_system->m_immediateContext->ClearState();
			m_system->m_immediateContext->Flush();
		}

		m_rtv.Reset();
		m_dsv.Reset();

		UINT width = (size.width > 0) ? size.width : 1;
		UINT height = (size.height > 0) ? size.height : 1;

		HRESULT hr = m_swap_chain->ResizeBuffers(1, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
		if (FAILED(hr))
		{
			darklog.error(L"Failed to resize swap chain buffers");
			return;
		}

		reloadBuffers(size);
	}
	void SwapChain::setFullScreen(bool fullscreen, const Box& size)
	{
		resize(size);
		m_swap_chain->SetFullscreenState(fullscreen, nullptr);
	}
	void SwapChain::reloadBuffers(const Box& size)
	{
		HRESULT hr = 0;
		auto device = m_system->m_d3dDevice;
		Microsoft::WRL::ComPtr<ID3D11Texture2D> back_buffer;
		hr = m_swap_chain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)back_buffer.GetAddressOf());

		if (FAILED(hr))
		{
			darklog.error(L"Failed to get back buffer");
			return;
		}

		hr = device->CreateRenderTargetView(back_buffer.Get(), NULL, &m_rtv);

		if (FAILED(hr))
		{
			darklog.error(L"Failed to create render target view");
			return;
		}

		D3D11_TEXTURE2D_DESC tex_desc = {};
		tex_desc.Width = (size.width > 0) ? size.width : 1;
		tex_desc.Height = (size.height > 0) ? size.height : 1;
		tex_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
		tex_desc.Usage = D3D11_USAGE_DEFAULT;
		tex_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
		tex_desc.MipLevels = 1;
		tex_desc.SampleDesc.Count = 1;
		tex_desc.SampleDesc.Quality = 0;
		tex_desc.MiscFlags = 0;
		tex_desc.ArraySize = 1;
		tex_desc.CPUAccessFlags = 0;

		Microsoft::WRL::ComPtr<ID3D11Texture2D> depth_buffer;
		hr = device->CreateTexture2D(&tex_desc, nullptr, &depth_buffer);

		if (FAILED(hr))
		{
			darklog.error(L"Failed to create depth stencil buffer");
			return;
		}

		hr = device->CreateDepthStencilView(depth_buffer.Get(), NULL, &m_dsv);

		if (FAILED(hr))
		{
			darklog.error(L"Failed to create depth stencil view");
			return;
		}
	}
}
