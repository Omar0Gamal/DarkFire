#include "D3D11RHI.h"
#include "Engine/Core/Log/Log.h"

#include <d3dcompiler.h>
#include <dxgidebug.h>
#include <algorithm>

namespace Core::RHI
{
	namespace
	{
		DXGI_FORMAT ToDxgiFormat(PixelFormat format) noexcept
		{
			switch (format)
			{
				case PixelFormat::R8_UNORM: return DXGI_FORMAT_R8_UNORM;
				case PixelFormat::R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
				case PixelFormat::R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
				case PixelFormat::R8G8B8A8_SNORM: return DXGI_FORMAT_R8G8B8A8_SNORM;
				case PixelFormat::R16G16B16A16_FLOAT: return DXGI_FORMAT_R16G16B16A16_FLOAT;
				case PixelFormat::R32G32B32A32_FLOAT: return DXGI_FORMAT_R32G32B32A32_FLOAT;
				case PixelFormat::R32G32B32_FLOAT: return DXGI_FORMAT_R32G32B32_FLOAT;
				case PixelFormat::R32G32_FLOAT: return DXGI_FORMAT_R32G32_FLOAT;
				case PixelFormat::R32_FLOAT: return DXGI_FORMAT_R32_FLOAT;
				case PixelFormat::R32_TYPELESS: return DXGI_FORMAT_R32_TYPELESS;
				case PixelFormat::D32_FLOAT: return DXGI_FORMAT_D32_FLOAT;
				case PixelFormat::D24_UNORM_S8_UINT: return DXGI_FORMAT_D24_UNORM_S8_UINT;
				case PixelFormat::R11G11B10_FLOAT: return DXGI_FORMAT_R11G11B10_FLOAT;
				case PixelFormat::R32_UINT: return DXGI_FORMAT_R32_UINT;
				default: return DXGI_FORMAT_UNKNOWN;
			}
		}

		D3D11_USAGE ToDxgiUsage(BufferUsage usage) noexcept
		{
			switch (usage)
			{
				case BufferUsage::Default: return D3D11_USAGE_DEFAULT;
				case BufferUsage::Immutable: return D3D11_USAGE_IMMUTABLE;
				case BufferUsage::Dynamic: return D3D11_USAGE_DYNAMIC;
				case BufferUsage::Staging: return D3D11_USAGE_STAGING;
				default: return D3D11_USAGE_DEFAULT;
			}
		}

		UINT ToDxgiBindFlags(BufferBindFlags flags) noexcept
		{
			UINT result = 0;
			if (flags & BufferBindFlags::VertexBuffer) result |= D3D11_BIND_VERTEX_BUFFER;
			if (flags & BufferBindFlags::IndexBuffer) result |= D3D11_BIND_INDEX_BUFFER;
			if (flags & BufferBindFlags::ConstantBuffer) result |= D3D11_BIND_CONSTANT_BUFFER;
			if (flags & BufferBindFlags::ShaderResource) result |= D3D11_BIND_SHADER_RESOURCE;
			if (flags & BufferBindFlags::RenderTarget) result |= D3D11_BIND_RENDER_TARGET;
			if (flags & BufferBindFlags::DepthStencil) result |= D3D11_BIND_DEPTH_STENCIL;
			if (flags & BufferBindFlags::UnorderedAccess) result |= D3D11_BIND_UNORDERED_ACCESS;
			return result;
		}

		D3D11_CULL_MODE ToDxgiCullMode(CullMode mode) noexcept
		{
			switch (mode)
			{
				case CullMode::None: return D3D11_CULL_NONE;
				case CullMode::Front: return D3D11_CULL_FRONT;
				case CullMode::Back: return D3D11_CULL_BACK;
				default: return D3D11_CULL_BACK;
			}
		}

		D3D11_FILL_MODE ToDxgiFillMode(FillMode mode) noexcept
		{
			switch (mode)
			{
				case FillMode::Solid: return D3D11_FILL_SOLID;
				case FillMode::Wireframe: return D3D11_FILL_WIREFRAME;
				default: return D3D11_FILL_SOLID;
			}
		}

		D3D11_COMPARISON_FUNC ToDxgiComparison(ComparisonFunc func) noexcept
		{
			switch (func)
			{
				case ComparisonFunc::Never: return D3D11_COMPARISON_NEVER;
				case ComparisonFunc::Less: return D3D11_COMPARISON_LESS;
				case ComparisonFunc::Equal: return D3D11_COMPARISON_EQUAL;
				case ComparisonFunc::LessEqual: return D3D11_COMPARISON_LESS_EQUAL;
				case ComparisonFunc::Greater: return D3D11_COMPARISON_GREATER;
				case ComparisonFunc::NotEqual: return D3D11_COMPARISON_NOT_EQUAL;
				case ComparisonFunc::GreaterEqual: return D3D11_COMPARISON_GREATER_EQUAL;
				case ComparisonFunc::Always: return D3D11_COMPARISON_ALWAYS;
				default: return D3D11_COMPARISON_ALWAYS;
			}
		}

		D3D11_BLEND ToDxgiBlendFactor(BlendFactor factor) noexcept
		{
			switch (factor)
			{
				case BlendFactor::Zero: return D3D11_BLEND_ZERO;
				case BlendFactor::One: return D3D11_BLEND_ONE;
				case BlendFactor::SrcColor: return D3D11_BLEND_SRC_COLOR;
				case BlendFactor::InvSrcColor: return D3D11_BLEND_INV_SRC_COLOR;
				case BlendFactor::SrcAlpha: return D3D11_BLEND_SRC_ALPHA;
				case BlendFactor::InvSrcAlpha: return D3D11_BLEND_INV_SRC_ALPHA;
				case BlendFactor::DestAlpha: return D3D11_BLEND_DEST_ALPHA;
				case BlendFactor::InvDestAlpha: return D3D11_BLEND_INV_DEST_ALPHA;
				case BlendFactor::DestColor: return D3D11_BLEND_DEST_COLOR;
				case BlendFactor::InvDestColor: return D3D11_BLEND_INV_DEST_COLOR;
				default: return D3D11_BLEND_ONE;
			}
		}

		D3D11_BLEND_OP ToDxgiBlendOp(BlendOp op) noexcept
		{
			switch (op)
			{
				case BlendOp::Add: return D3D11_BLEND_OP_ADD;
				case BlendOp::Subtract: return D3D11_BLEND_OP_SUBTRACT;
				case BlendOp::RevSubtract: return D3D11_BLEND_OP_REV_SUBTRACT;
				case BlendOp::Min: return D3D11_BLEND_OP_MIN;
				case BlendOp::Max: return D3D11_BLEND_OP_MAX;
				default: return D3D11_BLEND_OP_ADD;
			}
		}

		D3D11_FILTER ToDxgiFilter(FilterMode filter) noexcept
		{
			switch (filter)
			{
				case FilterMode::Point: return D3D11_FILTER_MIN_MAG_MIP_POINT;
				case FilterMode::Linear: return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
				case FilterMode::Anisotropic: return D3D11_FILTER_ANISOTROPIC;
				default: return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			}
		}

		D3D11_TEXTURE_ADDRESS_MODE ToDxgiAddress(AddressMode mode) noexcept
		{
			switch (mode)
			{
				case AddressMode::Wrap: return D3D11_TEXTURE_ADDRESS_WRAP;
				case AddressMode::Clamp: return D3D11_TEXTURE_ADDRESS_CLAMP;
				case AddressMode::Mirror: return D3D11_TEXTURE_ADDRESS_MIRROR;
				case AddressMode::Border: return D3D11_TEXTURE_ADDRESS_BORDER;
				default: return D3D11_TEXTURE_ADDRESS_WRAP;
			}
		}

		D3D11_PRIMITIVE_TOPOLOGY ToDxgiTopology(PrimitiveTopology topo) noexcept
		{
			switch (topo)
			{
				case PrimitiveTopology::PointList: return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
				case PrimitiveTopology::LineList: return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
				case PrimitiveTopology::LineStrip: return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
				case PrimitiveTopology::TriangleList: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
				case PrimitiveTopology::TriangleStrip: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
				default: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			}
		}
	}

	void D3D11StateTracker::reset()
	{
		currentInputLayout = nullptr;
		currentTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
		currentVS = nullptr;
		currentPS = nullptr;
		currentRS = nullptr;
		currentDSS = nullptr;
		currentStencilRef = 0;
		currentBS = nullptr;
		currentBlendFactor[0] = currentBlendFactor[1] = currentBlendFactor[2] = currentBlendFactor[3] = 1.0f;

		for (size_t i = 0; i < MAX_SLOTS; ++i)
		{
			currentVBs[i] = nullptr;
			currentVBStrides[i] = 0;
			currentVBOffsets[i] = 0;
			currentVSConstantBuffers[i] = nullptr;
			currentPSConstantBuffers[i] = nullptr;
			currentVSSRVs[i] = nullptr;
			currentPSSRVs[i] = nullptr;
			currentVSSamplers[i] = nullptr;
			currentPSSamplers[i] = nullptr;
		}

		currentIB = nullptr;
		currentIBFormat = DXGI_FORMAT_UNKNOWN;
		currentIBOffset = 0;
		currentViewport = {};
	}

	D3D11SwapChain::D3D11SwapChain(ComPtr<IDXGISwapChain1> swapChain, std::shared_ptr<D3D11Texture> backBuffer, HWND hwnd)
		: m_swapChain(std::move(swapChain))
		, m_backBuffer(std::move(backBuffer))
		, m_hwnd(hwnd)
	{
	}

	void D3D11SwapChain::present(bool vsync)
	{
		if (m_swapChain)
		{
			m_swapChain->Present(vsync ? 1 : 0, 0);
		}
	}

	void D3D11SwapChain::resize(uint32_t width, uint32_t height)
	{
		if (!m_swapChain || width == 0 || height == 0)
		{
			return;
		}

		m_backBuffer.reset();
		const HRESULT hr = m_swapChain->ResizeBuffers(2, width, height, DXGI_FORMAT_R8G8B8A8_UNORM, 0);
		if (FAILED(hr))
		{
			DF_LOG_ERROR("RHI", "D3D11SwapChain::resize: ResizeBuffers failed with HRESULT 0x{:08X}", hr);
			return;
		}

		ComPtr<ID3D11Texture2D> backBufferTex;
		m_swapChain->GetBuffer(0, IID_PPV_ARGS(backBufferTex.GetAddressOf()));
		if (backBufferTex)
		{
			ComPtr<ID3D11Device> device;
			backBufferTex->GetDevice(device.GetAddressOf());

			ComPtr<ID3D11RenderTargetView> rtv;
			device->CreateRenderTargetView(backBufferTex.Get(), nullptr, rtv.GetAddressOf());

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
	}

	D3D11DeviceContext::D3D11DeviceContext(ComPtr<ID3D11DeviceContext> context, ComPtr<ID3D11DeviceContext1> context1)
		: m_context(std::move(context))
		, m_context1(std::move(context1))
	{
		m_tracker.reset();
	}

	void D3D11DeviceContext::clearTrackedState()
	{
		m_tracker.reset();
		if (m_context)
		{
			m_context->ClearState();
		}
	}

	void D3D11DeviceContext::setViewport(const Viewport& viewport)
	{
		if (m_tracker.currentViewport.TopLeftX == viewport.x &&
			m_tracker.currentViewport.TopLeftY == viewport.y &&
			m_tracker.currentViewport.Width == viewport.width &&
			m_tracker.currentViewport.Height == viewport.height &&
			m_tracker.currentViewport.MinDepth == viewport.minDepth &&
			m_tracker.currentViewport.MaxDepth == viewport.maxDepth)
		{
			m_stats.redundantBindsSkipped++;
			return;
		}

		m_stats.stateChanges++;
		D3D11_VIEWPORT vp{};
		vp.TopLeftX = viewport.x;
		vp.TopLeftY = viewport.y;
		vp.Width = viewport.width;
		vp.Height = viewport.height;
		vp.MinDepth = viewport.minDepth;
		vp.MaxDepth = viewport.maxDepth;

		m_context->RSSetViewports(1, &vp);
		m_tracker.currentViewport = vp;
	}

	void D3D11DeviceContext::setRenderTargets(std::span<std::shared_ptr<IGpuTexture>> rtvTextures, std::shared_ptr<IGpuTexture> dsvTexture)
	{
		ID3D11RenderTargetView* rtvs[8] = {};
		const UINT count = static_cast<UINT>(std::min<size_t>(rtvTextures.size(), 8));
		for (UINT i = 0; i < count; ++i)
		{
			if (rtvTextures[i])
			{
				rtvs[i] = static_cast<D3D11Texture*>(rtvTextures[i].get())->getD3D11RTV();
			}
		}

		ID3D11DepthStencilView* dsv = nullptr;
		if (dsvTexture)
		{
			dsv = static_cast<D3D11Texture*>(dsvTexture.get())->getD3D11DSV();
		}

		m_stats.stateChanges++;
		m_context->OMSetRenderTargets(count, rtvs, dsv);
	}

	void D3D11DeviceContext::clearRenderTarget(const std::shared_ptr<IGpuTexture>& texture, const Color& color)
	{
		if (!texture)
		{
			return;
		}
		auto* d3dTex = static_cast<D3D11Texture*>(texture.get());
		if (auto* rtv = d3dTex->getD3D11RTV())
		{
			const float clearColor[4] = { color.x, color.y, color.z, color.w };
			m_context->ClearRenderTargetView(rtv, clearColor);
		}
	}

	void D3D11DeviceContext::clearDepthStencil(const std::shared_ptr<IGpuTexture>& texture, float depth, uint8_t stencil)
	{
		if (!texture)
		{
			return;
		}
		auto* d3dTex = static_cast<D3D11Texture*>(texture.get());
		if (auto* dsv = d3dTex->getD3D11DSV())
		{
			m_context->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, depth, stencil);
		}
	}

	void D3D11DeviceContext::setPrimitiveTopology(PrimitiveTopology topology)
	{
		const D3D11_PRIMITIVE_TOPOLOGY d3dTopology = ToDxgiTopology(topology);
		if (m_tracker.currentTopology != d3dTopology)
		{
			m_context->IASetPrimitiveTopology(d3dTopology);
			m_tracker.currentTopology = d3dTopology;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setInputLayout(const std::shared_ptr<IInputLayout>& layout)
	{
		ID3D11InputLayout* d3dLayout = layout ? static_cast<D3D11InputLayout*>(layout.get())->getD3D11Layout() : nullptr;
		if (m_tracker.currentInputLayout != d3dLayout)
		{
			m_context->IASetInputLayout(d3dLayout);
			m_tracker.currentInputLayout = d3dLayout;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setVertexBuffer(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t stride, uint32_t offset)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11Buffer* d3dBuffer = buffer ? static_cast<D3D11Buffer*>(buffer.get())->getD3D11Buffer() : nullptr;
		if (m_tracker.currentVBs[slot] != d3dBuffer || m_tracker.currentVBStrides[slot] != stride || m_tracker.currentVBOffsets[slot] != offset)
		{
			UINT d3dStride = stride;
			UINT d3dOffset = offset;
			m_context->IASetVertexBuffers(slot, 1, &d3dBuffer, &d3dStride, &d3dOffset);
			m_tracker.currentVBs[slot] = d3dBuffer;
			m_tracker.currentVBStrides[slot] = stride;
			m_tracker.currentVBOffsets[slot] = offset;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setIndexBuffer(const std::shared_ptr<IGpuBuffer>& buffer, PixelFormat format, uint32_t offset)
	{
		ID3D11Buffer* d3dBuffer = buffer ? static_cast<D3D11Buffer*>(buffer.get())->getD3D11Buffer() : nullptr;
		const DXGI_FORMAT dxgiFormat = ToDxgiFormat(format);
		if (m_tracker.currentIB != d3dBuffer || m_tracker.currentIBFormat != dxgiFormat || m_tracker.currentIBOffset != offset)
		{
			m_context->IASetIndexBuffer(d3dBuffer, dxgiFormat, offset);
			m_tracker.currentIB = d3dBuffer;
			m_tracker.currentIBFormat = dxgiFormat;
			m_tracker.currentIBOffset = offset;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setVertexShader(const std::shared_ptr<IVertexShader>& vs)
	{
		ID3D11VertexShader* d3dVS = vs ? static_cast<D3D11VertexShader*>(vs.get())->getD3D11VS() : nullptr;
		if (m_tracker.currentVS != d3dVS)
		{
			m_context->VSSetShader(d3dVS, nullptr, 0);
			m_tracker.currentVS = d3dVS;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setPixelShader(const std::shared_ptr<IPixelShader>& ps)
	{
		ID3D11PixelShader* d3dPS = ps ? static_cast<D3D11PixelShader*>(ps.get())->getD3D11PS() : nullptr;
		if (m_tracker.currentPS != d3dPS)
		{
			m_context->PSSetShader(d3dPS, nullptr, 0);
			m_tracker.currentPS = d3dPS;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setConstantBufferVS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11Buffer* d3dBuffer = buffer ? static_cast<D3D11Buffer*>(buffer.get())->getD3D11Buffer() : nullptr;
		if (m_tracker.currentVSConstantBuffers[slot] != d3dBuffer)
		{
			m_context->VSSetConstantBuffers(slot, 1, &d3dBuffer);
			m_tracker.currentVSConstantBuffers[slot] = d3dBuffer;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setConstantBufferPS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11Buffer* d3dBuffer = buffer ? static_cast<D3D11Buffer*>(buffer.get())->getD3D11Buffer() : nullptr;
		if (m_tracker.currentPSConstantBuffers[slot] != d3dBuffer)
		{
			m_context->PSSetConstantBuffers(slot, 1, &d3dBuffer);
			m_tracker.currentPSConstantBuffers[slot] = d3dBuffer;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setConstantBufferSubrangeVS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t firstConstant16, uint32_t numConstants16)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11Buffer* d3dBuffer = buffer ? static_cast<D3D11Buffer*>(buffer.get())->getD3D11Buffer() : nullptr;
		if (m_context1 && numConstants16 > 0)
		{
			UINT first = firstConstant16;
			UINT count = numConstants16;
			m_context1->VSSetConstantBuffers1(slot, 1, &d3dBuffer, &first, &count);
			m_tracker.currentVSConstantBuffers[slot] = d3dBuffer;
			m_stats.stateChanges++;
		}
		else
		{
			setConstantBufferVS(slot, buffer);
		}
	}

	void D3D11DeviceContext::setConstantBufferSubrangePS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t firstConstant16, uint32_t numConstants16)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11Buffer* d3dBuffer = buffer ? static_cast<D3D11Buffer*>(buffer.get())->getD3D11Buffer() : nullptr;
		if (m_context1 && numConstants16 > 0)
		{
			UINT first = firstConstant16;
			UINT count = numConstants16;
			m_context1->PSSetConstantBuffers1(slot, 1, &d3dBuffer, &first, &count);
			m_tracker.currentPSConstantBuffers[slot] = d3dBuffer;
			m_stats.stateChanges++;
		}
		else
		{
			setConstantBufferPS(slot, buffer);
		}
	}

	void D3D11DeviceContext::setShaderResourceVS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11ShaderResourceView* srv = texture ? static_cast<D3D11Texture*>(texture.get())->getD3D11SRV() : nullptr;
		if (m_tracker.currentVSSRVs[slot] != srv)
		{
			m_context->VSSetShaderResources(slot, 1, &srv);
			m_tracker.currentVSSRVs[slot] = srv;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setShaderResourcePS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11ShaderResourceView* srv = texture ? static_cast<D3D11Texture*>(texture.get())->getD3D11SRV() : nullptr;
		if (m_tracker.currentPSSRVs[slot] != srv)
		{
			m_context->PSSetShaderResources(slot, 1, &srv);
			m_tracker.currentPSSRVs[slot] = srv;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setShaderResourceBufferPS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11ShaderResourceView* srv = buffer ? static_cast<D3D11Buffer*>(buffer.get())->getD3D11SRV() : nullptr;
		if (m_tracker.currentPSSRVs[slot] != srv)
		{
			m_context->PSSetShaderResources(slot, 1, &srv);
			m_tracker.currentPSSRVs[slot] = srv;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setSamplerVS(uint32_t slot, const std::shared_ptr<ISamplerState>& sampler)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11SamplerState* s = sampler ? static_cast<D3D11SamplerState*>(sampler.get())->getD3D11State() : nullptr;
		if (m_tracker.currentVSSamplers[slot] != s)
		{
			m_context->VSSetSamplers(slot, 1, &s);
			m_tracker.currentVSSamplers[slot] = s;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setSamplerPS(uint32_t slot, const std::shared_ptr<ISamplerState>& sampler)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11SamplerState* s = sampler ? static_cast<D3D11SamplerState*>(sampler.get())->getD3D11State() : nullptr;
		if (m_tracker.currentPSSamplers[slot] != s)
		{
			m_context->PSSetSamplers(slot, 1, &s);
			m_tracker.currentPSSamplers[slot] = s;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setRasterizerState(const std::shared_ptr<IRasterizerState>& state)
	{
		ID3D11RasterizerState* rs = state ? static_cast<D3D11RasterizerState*>(state.get())->getD3D11State() : nullptr;
		if (m_tracker.currentRS != rs)
		{
			m_context->RSSetState(rs);
			m_tracker.currentRS = rs;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setDepthStencilState(const std::shared_ptr<IDepthStencilState>& state, uint32_t stencilRef)
	{
		ID3D11DepthStencilState* dss = state ? static_cast<D3D11DepthStencilState*>(state.get())->getD3D11State() : nullptr;
		if (m_tracker.currentDSS != dss || m_tracker.currentStencilRef != stencilRef)
		{
			m_context->OMSetDepthStencilState(dss, stencilRef);
			m_tracker.currentDSS = dss;
			m_tracker.currentStencilRef = stencilRef;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setBlendState(const std::shared_ptr<IBlendState>& state, const float blendFactor[4])
	{
		ID3D11BlendState* bs = state ? static_cast<D3D11BlendState*>(state.get())->getD3D11State() : nullptr;
		float bf[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		if (blendFactor)
		{
			bf[0] = blendFactor[0];
			bf[1] = blendFactor[1];
			bf[2] = blendFactor[2];
			bf[3] = blendFactor[3];
		}

		if (m_tracker.currentBS != bs ||
			m_tracker.currentBlendFactor[0] != bf[0] ||
			m_tracker.currentBlendFactor[1] != bf[1] ||
			m_tracker.currentBlendFactor[2] != bf[2] ||
			m_tracker.currentBlendFactor[3] != bf[3])
		{
			m_context->OMSetBlendState(bs, bf, 0xFFFFFFFF);
			m_tracker.currentBS = bs;
			m_tracker.currentBlendFactor[0] = bf[0];
			m_tracker.currentBlendFactor[1] = bf[1];
			m_tracker.currentBlendFactor[2] = bf[2];
			m_tracker.currentBlendFactor[3] = bf[3];
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::draw(uint32_t vertexCount, uint32_t startVertexLocation)
	{
		m_context->Draw(vertexCount, startVertexLocation);
	}

	void D3D11DeviceContext::drawIndexed(uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation)
	{
		m_context->DrawIndexed(indexCount, startIndexLocation, baseVertexLocation);
	}

	Result<void*, std::string> D3D11DeviceContext::map(const std::shared_ptr<IGpuBuffer>& buffer, bool discard)
	{
		if (!buffer)
		{
			return Result<void*, std::string>::error("Null buffer in D3D11DeviceContext::map");
		}
		auto* d3dBuf = static_cast<D3D11Buffer*>(buffer.get())->getD3D11Buffer();
		D3D11_MAPPED_SUBRESOURCE mapped{};
		const D3D11_MAP mapType = discard ? D3D11_MAP_WRITE_DISCARD : D3D11_MAP_WRITE_NO_OVERWRITE;
		HRESULT hr = m_context->Map(d3dBuf, 0, mapType, 0, &mapped);
		if (FAILED(hr) && !discard)
		{
			// Fallback to WRITE_DISCARD if NO_OVERWRITE is rejected (e.g. constant buffers)
			hr = m_context->Map(d3dBuf, 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped);
		}
		if (FAILED(hr))
		{
			return Result<void*, std::string>::error("D3D11 Map failed with HRESULT " + std::to_string(hr));
		}
		return Result<void*, std::string>::ok(mapped.pData);
	}

	void D3D11DeviceContext::unmap(const std::shared_ptr<IGpuBuffer>& buffer)
	{
		if (buffer)
		{
			auto* d3dBuf = static_cast<D3D11Buffer*>(buffer.get())->getD3D11Buffer();
			m_context->Unmap(d3dBuf, 0);
		}
	}

	void D3D11DeviceContext::updateSubresource(const std::shared_ptr<IGpuBuffer>& buffer, const void* data, size_t size)
	{
		if (buffer && data)
		{
			auto* d3dBuf = static_cast<D3D11Buffer*>(buffer.get())->getD3D11Buffer();
			const auto& desc = buffer->getDesc();
			const bool isCB = (desc.bindFlags & BufferBindFlags::ConstantBuffer);
			if (!isCB && size > 0 && size < desc.size)
			{
				D3D11_BOX box{};
				box.left = 0;
				box.right = static_cast<UINT>(size);
				box.top = 0;
				box.bottom = 1;
				box.front = 0;
				box.back = 1;
				m_context->UpdateSubresource(d3dBuf, 0, &box, data, 0, 0);
			}
			else
			{
				m_context->UpdateSubresource(d3dBuf, 0, nullptr, data, 0, 0);
			}
		}
	}

	D3D11RHIDevice::D3D11RHIDevice(bool enableDebugLayer)
		: m_enableDebugLayer(enableDebugLayer)
	{
	}

	D3D11RHIDevice::~D3D11RHIDevice()
	{
		shutdown();
	}

	bool D3D11RHIDevice::initialize()
	{
		UINT createDeviceFlags = 0;
#if defined(DF_DEBUG)
		createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
#else
		if (m_enableDebugLayer)
		{
			createDeviceFlags |= D3D11_CREATE_DEVICE_DEBUG;
		}
#endif

		const D3D_FEATURE_LEVEL featureLevels[] =
		{
			D3D_FEATURE_LEVEL_11_1,
			D3D_FEATURE_LEVEL_11_0
		};

		D3D_FEATURE_LEVEL featureLevelOut = D3D_FEATURE_LEVEL_11_0;
		ComPtr<ID3D11DeviceContext> context;

		// Try hardware adapter first
		HRESULT hr = D3D11CreateDevice(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			createDeviceFlags,
			featureLevels,
			static_cast<UINT>(std::size(featureLevels)),
			D3D11_SDK_VERSION,
			m_device.GetAddressOf(),
			&featureLevelOut,
			context.GetAddressOf());

		// Fallback to WARP if hardware adapter is unavailable (headless environments / CI)
		if (FAILED(hr))
		{
			DF_LOG_WARN("RHI", "D3D11CreateDevice with HARDWARE failed (0x{:08X}). Falling back to WARP.", hr);
			hr = D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_WARP,
				nullptr,
				createDeviceFlags,
				featureLevels,
				static_cast<UINT>(std::size(featureLevels)),
				D3D11_SDK_VERSION,
				m_device.GetAddressOf(),
				&featureLevelOut,
				context.GetAddressOf());
		}

		if (FAILED(hr) && (createDeviceFlags & D3D11_CREATE_DEVICE_DEBUG))
		{
			// If debug layer failed to load (SDK layer not installed), retry without debug flag
			createDeviceFlags &= ~D3D11_CREATE_DEVICE_DEBUG;
			hr = D3D11CreateDevice(
				nullptr,
				D3D_DRIVER_TYPE_HARDWARE,
				nullptr,
				createDeviceFlags,
				featureLevels,
				static_cast<UINT>(std::size(featureLevels)),
				D3D11_SDK_VERSION,
				m_device.GetAddressOf(),
				&featureLevelOut,
				context.GetAddressOf());

			if (FAILED(hr))
			{
				hr = D3D11CreateDevice(
					nullptr,
					D3D_DRIVER_TYPE_WARP,
					nullptr,
					createDeviceFlags,
					featureLevels,
					static_cast<UINT>(std::size(featureLevels)),
					D3D11_SDK_VERSION,
					m_device.GetAddressOf(),
					&featureLevelOut,
					context.GetAddressOf());
			}
		}

		if (FAILED(hr))
		{
			DF_LOG_ERROR("RHI", "D3D11CreateDevice completely failed with HRESULT 0x{:08X}", hr);
			return false;
		}

		// Query D3D11.1 interfaces
		m_device.As(&m_device1);
		ComPtr<ID3D11DeviceContext1> context1;
		context.As(&context1);

		m_immediateContext = std::make_shared<D3D11DeviceContext>(context, context1);

		// Obtain DXGI Factory
		ComPtr<IDXGIDevice> dxgiDevice;
		if (SUCCEEDED(m_device.As(&dxgiDevice)))
		{
			ComPtr<IDXGIAdapter> adapter;
			if (SUCCEEDED(dxgiDevice->GetAdapter(adapter.GetAddressOf())))
			{
				adapter->GetParent(IID_PPV_ARGS(m_dxgiFactory.GetAddressOf()));
			}
		}

		DF_LOG_INFO("RHI", "D3D11 RHI initialized successfully (Feature Level {})",
			(featureLevelOut == D3D_FEATURE_LEVEL_11_1 ? "11_1" : "11_0"));
		return true;
	}

	void D3D11RHIDevice::shutdown()
	{
		if (m_immediateContext)
		{
			m_immediateContext->clearTrackedState();
			m_immediateContext.reset();
		}

		m_rasterizerCache.clear();
		m_depthStencilCache.clear();
		m_blendCache.clear();
		m_samplerCache.clear();

		m_dxgiFactory.Reset();
		m_device1.Reset();

#if defined(DF_DEBUG)
		if (m_device)
		{
			ComPtr<ID3D11Debug> d3dDebug;
			if (SUCCEEDED(m_device.As(&d3dDebug)))
			{
				d3dDebug->ReportLiveDeviceObjects(D3D11_RLDO_SUMMARY | D3D11_RLDO_DETAIL | D3D11_RLDO_IGNORE_INTERNAL);
			}
		}
#endif

		m_device.Reset();
	}

	Result<std::shared_ptr<IGpuBuffer>, std::string> D3D11RHIDevice::createBuffer(const GpuBufferDesc& desc, const void* initialData)
	{
		if (!m_device)
		{
			return Result<std::shared_ptr<IGpuBuffer>, std::string>::error("D3D11 Device not initialized");
		}

		D3D11_BUFFER_DESC bDesc{};
		bDesc.ByteWidth = static_cast<UINT>(desc.size);
		bDesc.Usage = ToDxgiUsage(desc.usage);
		bDesc.BindFlags = ToDxgiBindFlags(desc.bindFlags);
		bDesc.CPUAccessFlags = (desc.usage == BufferUsage::Dynamic) ? D3D11_CPU_ACCESS_WRITE : 0;
		bDesc.StructureByteStride = static_cast<UINT>(desc.structureByteStride);
		if (desc.structureByteStride > 0)
		{
			bDesc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			bDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
		}

		D3D11_SUBRESOURCE_DATA subData{};
		subData.pSysMem = initialData;

		ComPtr<ID3D11Buffer> buffer;
		const HRESULT hr = m_device->CreateBuffer(&bDesc, initialData ? &subData : nullptr, buffer.GetAddressOf());
		if (FAILED(hr))
		{
			return Result<std::shared_ptr<IGpuBuffer>, std::string>::error("CreateBuffer failed with HRESULT " + std::to_string(hr));
		}

		ComPtr<ID3D11ShaderResourceView> srv;
		if ((bDesc.BindFlags & D3D11_BIND_SHADER_RESOURCE) && desc.structureByteStride > 0)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.Format = DXGI_FORMAT_UNKNOWN;
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_BUFFER;
			srvDesc.Buffer.FirstElement = 0;
			srvDesc.Buffer.NumElements = static_cast<UINT>(desc.size / desc.structureByteStride);
			const HRESULT srvHr = m_device->CreateShaderResourceView(buffer.Get(), &srvDesc, srv.GetAddressOf());
			if (FAILED(srvHr))
			{
				DF_LOG_ERROR("RHI", "Failed to create SRV for structured buffer: HRESULT 0x{:08X}", srvHr);
			}
		}

		return Result<std::shared_ptr<IGpuBuffer>, std::string>::ok(std::make_shared<D3D11Buffer>(desc, buffer, srv));
	}

		static uint32_t GetBytesPerPixel(PixelFormat format) noexcept
		{
			switch (format)
			{
				case PixelFormat::R8_UNORM: return 1;
				case PixelFormat::R8G8B8A8_UNORM:
				case PixelFormat::R8G8B8A8_UNORM_SRGB:
				case PixelFormat::R8G8B8A8_SNORM:
				case PixelFormat::R32_FLOAT:
				case PixelFormat::R32_TYPELESS:
				case PixelFormat::D32_FLOAT:
				case PixelFormat::D24_UNORM_S8_UINT:
				case PixelFormat::R11G11B10_FLOAT:
				case PixelFormat::R32_UINT: return 4;
				case PixelFormat::R32G32_FLOAT:
				case PixelFormat::R16G16B16A16_FLOAT: return 8;
				case PixelFormat::R32G32B32_FLOAT: return 12;
				case PixelFormat::R32G32B32A32_FLOAT: return 16;
				default: return 4;
			}
		}

	Result<std::shared_ptr<IGpuTexture>, std::string> D3D11RHIDevice::createTexture(const GpuTextureDesc& desc, const void* initialData)
	{
		if (initialData)
		{
			SubresourceData sub{};
			sub.data = initialData;
			sub.rowPitch = desc.width * GetBytesPerPixel(desc.format);
			sub.slicePitch = sub.rowPitch * desc.height;
			return createTextureSubresources(desc, std::span<const SubresourceData>(&sub, 1));
		}
		return createTextureSubresources(desc, {});
	}

	Result<std::shared_ptr<IGpuTexture>, std::string> D3D11RHIDevice::createTextureSubresources(const GpuTextureDesc& desc, std::span<const SubresourceData> subresources)
	{
		if (!m_device)
		{
			return Result<std::shared_ptr<IGpuTexture>, std::string>::error("D3D11 Device not initialized");
		}

		D3D11_TEXTURE2D_DESC tDesc{};
		tDesc.Width = desc.width;
		tDesc.Height = desc.height;
		tDesc.MipLevels = desc.mipLevels;
		tDesc.ArraySize = desc.arraySize;
		tDesc.SampleDesc.Count = desc.sampleCount;
		tDesc.SampleDesc.Quality = 0;
		tDesc.Usage = ToDxgiUsage(desc.usage);
		tDesc.BindFlags = ToDxgiBindFlags(desc.bindFlags);
		if (desc.usage == BufferUsage::Dynamic)
		{
			tDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		}
		else if (desc.usage == BufferUsage::Staging)
		{
			tDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ | D3D11_CPU_ACCESS_WRITE;
		}
		else
		{
			tDesc.CPUAccessFlags = 0;
		}

		const bool isDepth = (desc.bindFlags & BufferBindFlags::DepthStencil);
		if (isDepth)
		{
			// Depth buffers as R32_TYPELESS with D32_FLOAT DSV and R32_FLOAT SRV
			tDesc.Format = DXGI_FORMAT_R32_TYPELESS;
		}
		else
		{
			tDesc.Format = ToDxgiFormat(desc.format);
		}

		std::vector<D3D11_SUBRESOURCE_DATA> d3dSub;
		if (!subresources.empty())
		{
			d3dSub.resize(subresources.size());
			for (size_t i = 0; i < subresources.size(); ++i)
			{
				d3dSub[i].pSysMem = subresources[i].data;
				d3dSub[i].SysMemPitch = subresources[i].rowPitch;
				d3dSub[i].SysMemSlicePitch = subresources[i].slicePitch;
			}
		}

		ComPtr<ID3D11Texture2D> texture;
		const HRESULT hr = m_device->CreateTexture2D(&tDesc, d3dSub.empty() ? nullptr : d3dSub.data(), texture.GetAddressOf());
		if (FAILED(hr))
		{
			return Result<std::shared_ptr<IGpuTexture>, std::string>::error("CreateTexture2D failed with HRESULT " + std::to_string(hr));
		}

		ComPtr<ID3D11ShaderResourceView> srv;
		if (desc.bindFlags & BufferBindFlags::ShaderResource)
		{
			D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
			srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			srvDesc.Texture2D.MostDetailedMip = 0;
			srvDesc.Texture2D.MipLevels = (desc.mipLevels > 0) ? desc.mipLevels : static_cast<UINT>(-1);
			srvDesc.Format = isDepth ? DXGI_FORMAT_R32_FLOAT : ToDxgiFormat(desc.format);

			m_device->CreateShaderResourceView(texture.Get(), &srvDesc, srv.GetAddressOf());
		}

		ComPtr<ID3D11RenderTargetView> rtv;
		if (desc.bindFlags & BufferBindFlags::RenderTarget)
		{
			D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
			rtvDesc.Format = ToDxgiFormat(desc.format);
			rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			m_device->CreateRenderTargetView(texture.Get(), &rtvDesc, rtv.GetAddressOf());
		}

		ComPtr<ID3D11DepthStencilView> dsv;
		if (isDepth)
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
			dsvDesc.Format = DXGI_FORMAT_D32_FLOAT;
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			m_device->CreateDepthStencilView(texture.Get(), &dsvDesc, dsv.GetAddressOf());
		}

		return Result<std::shared_ptr<IGpuTexture>, std::string>::ok(
			std::make_shared<D3D11Texture>(desc, texture, srv, rtv, dsv));
	}

	Result<std::shared_ptr<IVertexShader>, std::string> D3D11RHIDevice::createVertexShader(std::span<const uint8_t> bytecode)
	{
		if (!m_device || bytecode.empty())
		{
			return Result<std::shared_ptr<IVertexShader>, std::string>::error("Invalid device or bytecode");
		}

		ComPtr<ID3D11VertexShader> vs;
		const HRESULT hr = m_device->CreateVertexShader(bytecode.data(), bytecode.size(), nullptr, vs.GetAddressOf());
		if (FAILED(hr))
		{
			return Result<std::shared_ptr<IVertexShader>, std::string>::error("CreateVertexShader failed with HRESULT " + std::to_string(hr));
		}

		return Result<std::shared_ptr<IVertexShader>, std::string>::ok(
			std::make_shared<D3D11VertexShader>(vs, bytecode));
	}

	Result<std::shared_ptr<IPixelShader>, std::string> D3D11RHIDevice::createPixelShader(std::span<const uint8_t> bytecode)
	{
		if (!m_device || bytecode.empty())
		{
			return Result<std::shared_ptr<IPixelShader>, std::string>::error("Invalid device or bytecode");
		}

		ComPtr<ID3D11PixelShader> ps;
		const HRESULT hr = m_device->CreatePixelShader(bytecode.data(), bytecode.size(), nullptr, ps.GetAddressOf());
		if (FAILED(hr))
		{
			return Result<std::shared_ptr<IPixelShader>, std::string>::error("CreatePixelShader failed with HRESULT " + std::to_string(hr));
		}

		return Result<std::shared_ptr<IPixelShader>, std::string>::ok(
			std::make_shared<D3D11PixelShader>(ps, bytecode));
	}

	Result<std::shared_ptr<IInputLayout>, std::string> D3D11RHIDevice::createInputLayout(
		std::span<const InputElementDesc> elements,
		std::span<const uint8_t> vsBytecode)
	{
		if (!m_device || elements.empty() || vsBytecode.empty())
		{
			return Result<std::shared_ptr<IInputLayout>, std::string>::error("Invalid parameters for createInputLayout");
		}

		std::vector<D3D11_INPUT_ELEMENT_DESC> d3dElements;
		d3dElements.reserve(elements.size());

		for (const auto& elem : elements)
		{
			D3D11_INPUT_ELEMENT_DESC d{};
			d.SemanticName = elem.semanticName.c_str();
			d.SemanticIndex = elem.semanticIndex;
			d.Format = ToDxgiFormat(elem.format);
			d.InputSlot = elem.inputSlot;
			d.AlignedByteOffset = (elem.alignedByteOffset == 0xFFFFFFFF) ? D3D11_APPEND_ALIGNED_ELEMENT : elem.alignedByteOffset;
			d.InputSlotClass = elem.isPerInstance ? D3D11_INPUT_PER_INSTANCE_DATA : D3D11_INPUT_PER_VERTEX_DATA;
			d.InstanceDataStepRate = elem.isPerInstance ? 1 : 0;
			d3dElements.push_back(d);
		}

		ComPtr<ID3D11InputLayout> layout;
		const HRESULT hr = m_device->CreateInputLayout(
			d3dElements.data(),
			static_cast<UINT>(d3dElements.size()),
			vsBytecode.data(),
			vsBytecode.size(),
			layout.GetAddressOf());

		if (FAILED(hr))
		{
			return Result<std::shared_ptr<IInputLayout>, std::string>::error("CreateInputLayout failed with HRESULT " + std::to_string(hr));
		}

		return Result<std::shared_ptr<IInputLayout>, std::string>::ok(std::make_shared<D3D11InputLayout>(layout));
	}

	std::shared_ptr<IRasterizerState> D3D11RHIDevice::getOrCreateRasterizerState(const RasterizerStateDesc& desc)
	{
		const uint64_t hash = NullRHIDevice::hashRasterizerDesc(desc);
		return m_rasterizerCache.getOrCreate(desc, hash, [&]() -> std::shared_ptr<IRasterizerState> {
			D3D11_RASTERIZER_DESC rDesc{};
			rDesc.CullMode = ToDxgiCullMode(desc.cullMode);
			rDesc.FillMode = ToDxgiFillMode(desc.fillMode);
			rDesc.FrontCounterClockwise = desc.frontCounterClockwise ? TRUE : FALSE;
			rDesc.DepthBias = desc.depthBias;
			rDesc.SlopeScaledDepthBias = desc.slopeScaledDepthBias;
			rDesc.DepthClipEnable = desc.depthClipEnable ? TRUE : FALSE;
			rDesc.ScissorEnable = desc.scissorEnable ? TRUE : FALSE;

			ComPtr<ID3D11RasterizerState> state;
			m_device->CreateRasterizerState(&rDesc, state.GetAddressOf());
			return std::make_shared<D3D11RasterizerState>(desc, state);
		});
	}

	std::shared_ptr<IDepthStencilState> D3D11RHIDevice::getOrCreateDepthStencilState(const DepthStencilStateDesc& desc)
	{
		const uint64_t hash = NullRHIDevice::hashDepthStencilDesc(desc);
		return m_depthStencilCache.getOrCreate(desc, hash, [&]() -> std::shared_ptr<IDepthStencilState> {
			D3D11_DEPTH_STENCIL_DESC dDesc{};
			dDesc.DepthEnable = desc.depthEnable ? TRUE : FALSE;
			dDesc.DepthWriteMask = desc.depthWrite ? D3D11_DEPTH_WRITE_MASK_ALL : D3D11_DEPTH_WRITE_MASK_ZERO;
			dDesc.DepthFunc = ToDxgiComparison(desc.depthFunc);
			dDesc.StencilEnable = desc.stencilEnable ? TRUE : FALSE;

			ComPtr<ID3D11DepthStencilState> state;
			m_device->CreateDepthStencilState(&dDesc, state.GetAddressOf());
			return std::make_shared<D3D11DepthStencilState>(desc, state);
		});
	}

	std::shared_ptr<IBlendState> D3D11RHIDevice::getOrCreateBlendState(const BlendStateDesc& desc)
	{
		const uint64_t hash = NullRHIDevice::hashBlendDesc(desc);
		return m_blendCache.getOrCreate(desc, hash, [&]() -> std::shared_ptr<IBlendState> {
			D3D11_BLEND_DESC bDesc{};
			bDesc.RenderTarget[0].BlendEnable = desc.blendEnable ? TRUE : FALSE;
			bDesc.RenderTarget[0].SrcBlend = ToDxgiBlendFactor(desc.srcBlend);
			bDesc.RenderTarget[0].DestBlend = ToDxgiBlendFactor(desc.destBlend);
			bDesc.RenderTarget[0].BlendOp = ToDxgiBlendOp(desc.blendOp);
			bDesc.RenderTarget[0].SrcBlendAlpha = ToDxgiBlendFactor(desc.srcBlendAlpha);
			bDesc.RenderTarget[0].DestBlendAlpha = ToDxgiBlendFactor(desc.destBlendAlpha);
			bDesc.RenderTarget[0].BlendOpAlpha = ToDxgiBlendOp(desc.blendOpAlpha);
			bDesc.RenderTarget[0].RenderTargetWriteMask = desc.renderTargetWriteMask;

			ComPtr<ID3D11BlendState> state;
			m_device->CreateBlendState(&bDesc, state.GetAddressOf());
			return std::make_shared<D3D11BlendState>(desc, state);
		});
	}

	std::shared_ptr<ISamplerState> D3D11RHIDevice::getOrCreateSamplerState(const SamplerStateDesc& desc)
	{
		const uint64_t hash = NullRHIDevice::hashSamplerDesc(desc);
		return m_samplerCache.getOrCreate(desc, hash, [&]() -> std::shared_ptr<ISamplerState> {
			D3D11_SAMPLER_DESC sDesc{};
			sDesc.Filter = ToDxgiFilter(desc.filter);
			sDesc.AddressU = ToDxgiAddress(desc.addressU);
			sDesc.AddressV = ToDxgiAddress(desc.addressV);
			sDesc.AddressW = ToDxgiAddress(desc.addressW);
			sDesc.ComparisonFunc = ToDxgiComparison(desc.comparisonFunc);
			sDesc.MinLOD = desc.minLOD;
			sDesc.MaxLOD = desc.maxLOD;

			ComPtr<ID3D11SamplerState> state;
			m_device->CreateSamplerState(&sDesc, state.GetAddressOf());
			return std::make_shared<D3D11SamplerState>(desc, state);
		});
	}

	Result<std::unique_ptr<ISwapChain>, std::string> D3D11RHIDevice::createSwapChain(void* windowHandle, uint32_t width, uint32_t height)
	{
		auto hwnd = static_cast<HWND>(windowHandle);
		if (!hwnd || !m_dxgiFactory || width == 0 || height == 0)
		{
			return Result<std::unique_ptr<ISwapChain>, std::string>::error("Invalid parameters for createSwapChain");
		}

		DXGI_SWAP_CHAIN_DESC1 desc{};
		desc.Width = width;
		desc.Height = height;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Stereo = FALSE;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = 2;
		desc.Scaling = DXGI_SCALING_STRETCH;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		desc.Flags = 0;

		ComPtr<IDXGISwapChain1> swapChain1;
		const HRESULT hr = m_dxgiFactory->CreateSwapChainForHwnd(
			m_device.Get(),
			hwnd,
			&desc,
			nullptr,
			nullptr,
			swapChain1.GetAddressOf());

		if (FAILED(hr))
		{
			return Result<std::unique_ptr<ISwapChain>, std::string>::error("CreateSwapChainForHwnd failed with HRESULT " + std::to_string(hr));
		}

		// Disable automatic Alt+Enter
		m_dxgiFactory->MakeWindowAssociation(hwnd, DXGI_MWA_NO_ALT_ENTER);

		ComPtr<ID3D11Texture2D> backBufferTex;
		swapChain1->GetBuffer(0, IID_PPV_ARGS(backBufferTex.GetAddressOf()));

		ComPtr<ID3D11RenderTargetView> rtv;
		m_device->CreateRenderTargetView(backBufferTex.Get(), nullptr, rtv.GetAddressOf());

		GpuTextureDesc tDesc{};
		tDesc.width = width;
		tDesc.height = height;
		tDesc.depth = 1;
		tDesc.mipLevels = 1;
		tDesc.arraySize = 1;
		tDesc.format = PixelFormat::R8G8B8A8_UNORM;
		tDesc.bindFlags = BufferBindFlags::RenderTarget | BufferBindFlags::ShaderResource;

		auto backBuffer = std::make_shared<D3D11Texture>(tDesc, backBufferTex, nullptr, rtv, nullptr);
		return Result<std::unique_ptr<ISwapChain>, std::string>::ok(
			std::make_unique<D3D11SwapChain>(swapChain1, backBuffer, hwnd));
	}

	GpuMemoryStats D3D11RHIDevice::queryMemoryStats() const
	{
		GpuMemoryStats stats{};
		if (!m_device)
		{
			return stats;
		}

		ComPtr<IDXGIDevice> dxgiDevice;
		if (SUCCEEDED(m_device.As(&dxgiDevice)))
		{
			ComPtr<IDXGIAdapter> adapter;
			if (SUCCEEDED(dxgiDevice->GetAdapter(adapter.GetAddressOf())))
			{
				ComPtr<IDXGIAdapter3> adapter3;
				if (SUCCEEDED(adapter.As(&adapter3)))
				{
					DXGI_QUERY_VIDEO_MEMORY_INFO info{};
					if (SUCCEEDED(adapter3->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &info)))
					{
						stats.totalBudget = info.Budget;
						stats.currentUsage = info.CurrentUsage;
						stats.availableForReservation = info.AvailableForReservation;
						stats.currentReservation = info.CurrentReservation;
					}
				}
			}
		}

		return stats;
	}
}
