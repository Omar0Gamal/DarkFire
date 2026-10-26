#include "D3D11RHI.h"
#include "D3D11FormatUtils.h"
#include "Engine/Core/Log/Log.h"

namespace Core::RHI
{
	D3D11SwapChain::D3D11SwapChain(ComPtr<IDXGISwapChain1> swapChain, std::shared_ptr<D3D11Texture> backBuffer, HWND hwnd, bool allowTearing, HANDLE waitableObject)
		: m_swapChain(std::move(swapChain))
		, m_backBuffer(std::move(backBuffer))
		, m_hwnd(hwnd)
		, m_allowTearing(allowTearing)
		, m_waitableObject(waitableObject)
	{
		if (m_swapChain)
		{
			m_swapChain.As(&m_swapChain2);
		}
	}

	D3D11SwapChain::~D3D11SwapChain()
	{
		if (m_waitableObject)
		{
			CloseHandle(m_waitableObject);
			m_waitableObject = nullptr;
		}
	}

	std::shared_ptr<IGpuTexture> D3D11SwapChain::getBackBufferTexture()
	{
		return m_backBuffer;
	}

	void D3D11SwapChain::createRenderTarget(uint32_t width, uint32_t height)
	{
		if (!m_swapChain)
		{
			return;
		}

		ComPtr<ID3D11Texture2D> backBufferTex;
		const HRESULT getBufHr = m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBufferTex.GetAddressOf()));
		if (FAILED(getBufHr) || !backBufferTex)
		{
			DF_LOG_ERROR("RHI", "D3D11SwapChain::createRenderTarget: GetBuffer failed with HRESULT 0x{:08X}", getBufHr);
			return;
		}

		ComPtr<ID3D11Device> device;
		backBufferTex->GetDevice(device.GetAddressOf());
		if (!device)
		{
			return;
		}

		ComPtr<ID3D11RenderTargetView> rtv;
		const HRESULT rtvHr = device->CreateRenderTargetView(backBufferTex.Get(), nullptr, rtv.GetAddressOf());
		if (FAILED(rtvHr))
		{
			DF_LOG_ERROR("RHI", "D3D11SwapChain::createRenderTarget: CreateRenderTargetView failed with HRESULT 0x{:08X}", rtvHr);
			return;
		}

		GpuTextureDesc desc{};
		desc.width = width;
		desc.height = height;
		desc.depth = 1;
		desc.mipLevels = 1;
		desc.arraySize = 1;
		desc.format = PixelFormat::R8G8B8A8_UNORM;
		desc.bindFlags = BufferBindFlags::RenderTarget | BufferBindFlags::ShaderResource;

		m_backBuffer = std::make_shared<D3D11Texture>(desc, backBufferTex, nullptr, rtv, nullptr);
	}

	void D3D11SwapChain::present(bool vsync)
	{
		if (m_swapChain)
		{
			UINT syncInterval = vsync ? 1 : 0;
			UINT flags = (m_allowTearing && !vsync) ? DXGI_PRESENT_ALLOW_TEARING : 0;
			m_swapChain->Present(syncInterval, flags);
		}
	}

	void D3D11SwapChain::resize(uint32_t width, uint32_t height)
	{
		if (!m_swapChain || width == 0 || height == 0)
		{
			return;
		}

		m_backBuffer.reset();
		UINT flags = m_allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;
		if (m_waitableObject)
		{
			flags |= DXGI_SWAP_CHAIN_FLAG_FRAME_LATENCY_WAITABLE_OBJECT;
		}
		const HRESULT hr = m_swapChain->ResizeBuffers(3, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, flags);
		if (FAILED(hr))
		{
			DF_LOG_ERROR("RHI", "D3D11SwapChain::resize: ResizeBuffers failed with HRESULT 0x{:08X}", hr);
			return;
		}

		createRenderTarget(width, height);
	}

	bool D3D11SwapChain::setMaximumFrameLatency(uint32_t maxLatency)
	{
		if (m_swapChain2)
		{
			HRESULT hr = m_swapChain2->SetMaximumFrameLatency(maxLatency);
			return SUCCEEDED(hr);
		}
		return false;
	}

	bool D3D11SwapChain::waitForNextFrame(uint32_t timeoutMs)
	{
		if (m_waitableObject)
		{
			DWORD result = WaitForSingleObjectEx(m_waitableObject, timeoutMs, TRUE);
			return (result == WAIT_OBJECT_0);
		}
		return true;
	}
}
