#include "D3D11RHI.h"
#include "D3D11FormatUtils.h"
#include "Engine/Core/Log/Log.h"

#include <algorithm>

namespace Core::RHI
{
	void D3D11StateTracker::reset()
	{
		currentRTVCount = 0;
		for (size_t i = 0; i < 8; ++i)
		{
			currentRTVs[i] = nullptr;
		}
		currentDSV = nullptr;

		currentInputLayout = nullptr;
		currentTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
		currentVS = nullptr;
		currentPS = nullptr;
		currentHS = nullptr;
		currentDS = nullptr;
		currentCS = nullptr;
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
			currentCSConstantBuffers[i] = nullptr;
			currentHSConstantBuffers[i] = nullptr;
			currentDSConstantBuffers[i] = nullptr;
			currentVSSRVs[i] = nullptr;
			currentPSSRVs[i] = nullptr;
			currentCSSRVs[i] = nullptr;
			currentDSSRVs[i] = nullptr;
			currentCSUAVs[i] = nullptr;
			currentVSSamplers[i] = nullptr;
			currentPSSamplers[i] = nullptr;
			currentCSSamplers[i] = nullptr;
			currentDSSamplers[i] = nullptr;
		}

		currentIB = nullptr;
		currentIBFormat = DXGI_FORMAT_UNKNOWN;
		currentIBOffset = 0;
		currentViewport = {};
	}

	D3D11DeviceContext::D3D11DeviceContext(ComPtr<ID3D11DeviceContext> context, ComPtr<ID3D11DeviceContext1> context1)
		: m_context(std::move(context))
		, m_context1(std::move(context1))
	{
		m_tracker.reset();
	}

	D3D11DeviceContext::~D3D11DeviceContext() = default;

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

	void D3D11DeviceContext::setScissorRect(int32_t x, int32_t y, uint32_t width, uint32_t height)
	{
		m_stats.stateChanges++;
		D3D11_RECT rect{};
		rect.left = static_cast<LONG>(x);
		rect.top = static_cast<LONG>(y);
		rect.right = static_cast<LONG>(x + static_cast<int32_t>(width));
		rect.bottom = static_cast<LONG>(y + static_cast<int32_t>(height));
		m_context->RSSetScissorRects(1, &rect);
	}

	void D3D11DeviceContext::setRenderTargets(std::span<const std::shared_ptr<IGpuTexture>> rtvTextures, std::shared_ptr<IGpuTexture> dsvTexture)
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

		if (count == m_tracker.currentRTVCount && dsv == m_tracker.currentDSV)
		{
			bool match = true;
			for (UINT i = 0; i < count; ++i)
			{
				if (rtvs[i] != m_tracker.currentRTVs[i])
				{
					match = false;
					break;
				}
			}
			if (match)
			{
				m_stats.redundantBindsSkipped++;
				return;
			}
		}

		m_stats.stateChanges++;
		m_context->OMSetRenderTargets(count, rtvs, dsv);

		m_tracker.currentRTVCount = count;
		m_tracker.currentDSV = dsv;
		for (UINT i = 0; i < 8; ++i)
		{
			m_tracker.currentRTVs[i] = (i < count) ? rtvs[i] : nullptr;
		}
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
		const D3D11_PRIMITIVE_TOPOLOGY d3dTopology = ToD3D11Topology(topology);
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
		const DXGI_FORMAT dxgiFormat = ToDXGIFormat(format);
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

	void D3D11DeviceContext::setHullShader(const std::shared_ptr<IHullShader>& hs)
	{
		ID3D11HullShader* d3dHS = hs ? static_cast<D3D11HullShader*>(hs.get())->getD3D11HS() : nullptr;
		if (m_tracker.currentHS != d3dHS)
		{
			m_context->HSSetShader(d3dHS, nullptr, 0);
			m_tracker.currentHS = d3dHS;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setDomainShader(const std::shared_ptr<IDomainShader>& ds)
	{
		ID3D11DomainShader* d3dDS = ds ? static_cast<D3D11DomainShader*>(ds.get())->getD3D11DS() : nullptr;
		if (m_tracker.currentDS != d3dDS)
		{
			m_context->DSSetShader(d3dDS, nullptr, 0);
			m_tracker.currentDS = d3dDS;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setComputeShader(IComputeShader* shader)
	{
		ID3D11ComputeShader* d3dCS = shader ? static_cast<ID3D11ComputeShader*>(shader->getNativeHandle()) : nullptr;
		if (m_tracker.currentCS != d3dCS)
		{
			m_context->CSSetShader(d3dCS, nullptr, 0);
			m_tracker.currentCS = d3dCS;
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

	void D3D11DeviceContext::setConstantBufferHS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11Buffer* d3dBuffer = buffer ? static_cast<D3D11Buffer*>(buffer.get())->getD3D11Buffer() : nullptr;
		if (m_tracker.currentHSConstantBuffers[slot] != d3dBuffer)
		{
			m_context->HSSetConstantBuffers(slot, 1, &d3dBuffer);
			m_tracker.currentHSConstantBuffers[slot] = d3dBuffer;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setConstantBufferDS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11Buffer* d3dBuffer = buffer ? static_cast<D3D11Buffer*>(buffer.get())->getD3D11Buffer() : nullptr;
		if (m_tracker.currentDSConstantBuffers[slot] != d3dBuffer)
		{
			m_context->DSSetConstantBuffers(slot, 1, &d3dBuffer);
			m_tracker.currentDSConstantBuffers[slot] = d3dBuffer;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setConstantBufferCS(uint32_t slot, IGpuBuffer* buffer)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11Buffer* d3dBuffer = buffer ? static_cast<D3D11Buffer*>(buffer)->getD3D11Buffer() : nullptr;
		if (m_tracker.currentCSConstantBuffers[slot] != d3dBuffer)
		{
			m_context->CSSetConstantBuffers(slot, 1, &d3dBuffer);
			m_tracker.currentCSConstantBuffers[slot] = d3dBuffer;
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

	void D3D11DeviceContext::setConstantBufferSubrangeDS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t firstConstant16, uint32_t numConstants16)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11Buffer* d3dBuffer = buffer ? static_cast<D3D11Buffer*>(buffer.get())->getD3D11Buffer() : nullptr;
		if (m_context1 && numConstants16 > 0)
		{
			UINT first = firstConstant16;
			UINT count = numConstants16;
			m_context1->DSSetConstantBuffers1(slot, 1, &d3dBuffer, &first, &count);
			m_stats.stateChanges++;
		}
		else
		{
			setConstantBufferDS(slot, buffer);
		}
	}

	void D3D11DeviceContext::setConstantBufferSubrangeCS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t firstConstant16, uint32_t numConstants16)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11Buffer* d3dBuffer = buffer ? static_cast<D3D11Buffer*>(buffer.get())->getD3D11Buffer() : nullptr;
		if (m_context1 && numConstants16 > 0)
		{
			UINT first = firstConstant16;
			UINT count = numConstants16;
			m_context1->CSSetConstantBuffers1(slot, 1, &d3dBuffer, &first, &count);
			m_tracker.currentCSConstantBuffers[slot] = d3dBuffer;
			m_stats.stateChanges++;
		}
		else
		{
			setConstantBufferCS(slot, buffer);
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

	void D3D11DeviceContext::setShaderResourceBufferVS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11ShaderResourceView* srv = buffer ? static_cast<D3D11Buffer*>(buffer.get())->getD3D11SRV() : nullptr;
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

	void D3D11DeviceContext::setShaderResourceDS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11ShaderResourceView* srv = texture ? static_cast<D3D11Texture*>(texture.get())->getD3D11SRV() : nullptr;
		if (m_tracker.currentDSSRVs[slot] != srv)
		{
			m_context->DSSetShaderResources(slot, 1, &srv);
			m_tracker.currentDSSRVs[slot] = srv;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setShaderResourceCS(uint32_t slot, IGpuTexture* texture)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11ShaderResourceView* srv = texture ? static_cast<D3D11Texture*>(texture)->getD3D11SRV() : nullptr;
		if (m_tracker.currentCSSRVs[slot] != srv)
		{
			m_context->CSSetShaderResources(slot, 1, &srv);
			m_tracker.currentCSSRVs[slot] = srv;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setShaderResourceBufferCS(uint32_t slot, IGpuBuffer* buffer)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11ShaderResourceView* srv = buffer ? static_cast<D3D11Buffer*>(buffer)->getD3D11SRV() : nullptr;
		if (m_tracker.currentCSSRVs[slot] != srv)
		{
			m_context->CSSetShaderResources(slot, 1, &srv);
			m_tracker.currentCSSRVs[slot] = srv;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setShaderResourceBufferCS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer)
	{
		setShaderResourceBufferCS(slot, buffer.get());
	}

	void D3D11DeviceContext::setUnorderedAccessViewCS(uint32_t slot, IGpuTexture* texture, uint32_t mipSlice)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11UnorderedAccessView* uav = texture ? static_cast<D3D11Texture*>(texture)->getD3D11UAV(mipSlice) : nullptr;
		UINT initCount = 0xFFFFFFFF;
		m_context->CSSetUnorderedAccessViews(slot, 1, &uav, &initCount);
		m_tracker.currentCSUAVs[slot] = uav;
		m_stats.stateChanges++;
	}

	void D3D11DeviceContext::setUnorderedAccessViewCS(uint32_t slot, ID3D11UnorderedAccessView* uav, uint32_t initialCount)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		UINT initCount = initialCount;
		m_context->CSSetUnorderedAccessViews(slot, 1, &uav, &initCount);
		m_tracker.currentCSUAVs[slot] = uav;
		m_stats.stateChanges++;
	}

	void D3D11DeviceContext::setUnorderedAccessBufferCS(uint32_t slot, IGpuBuffer* buffer)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11UnorderedAccessView* uav = buffer ? static_cast<D3D11Buffer*>(buffer)->getD3D11UAV() : nullptr;
		UINT initCount = 0xFFFFFFFF;
		m_context->CSSetUnorderedAccessViews(slot, 1, &uav, &initCount);
		m_tracker.currentCSUAVs[slot] = uav;
		m_stats.stateChanges++;
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

	void D3D11DeviceContext::setSamplerDS(uint32_t slot, const std::shared_ptr<ISamplerState>& sampler)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11SamplerState* s = sampler ? static_cast<D3D11SamplerState*>(sampler.get())->getD3D11State() : nullptr;
		if (m_tracker.currentDSSamplers[slot] != s)
		{
			m_context->DSSetSamplers(slot, 1, &s);
			m_tracker.currentDSSamplers[slot] = s;
			m_stats.stateChanges++;
		}
		else
		{
			m_stats.redundantBindsSkipped++;
		}
	}

	void D3D11DeviceContext::setSamplerCS(uint32_t slot, IGpuSampler* sampler)
	{
		if (slot >= D3D11StateTracker::MAX_SLOTS) return;
		ID3D11SamplerState* s = sampler ? static_cast<D3D11SamplerState*>(sampler)->getD3D11State() : nullptr;
		if (m_tracker.currentCSSamplers[slot] != s)
		{
			m_context->CSSetSamplers(slot, 1, &s);
			m_tracker.currentCSSamplers[slot] = s;
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

	void D3D11DeviceContext::drawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation)
	{
		m_context->DrawIndexedInstanced(indexCountPerInstance, instanceCount, startIndexLocation, baseVertexLocation, startInstanceLocation);
	}

	void D3D11DeviceContext::drawIndexedInstancedIndirect(const std::shared_ptr<IGpuBuffer>& argumentBuffer, uint32_t offsetBytes)
	{
		if (!argumentBuffer) return;
		auto* nativeBuf = static_cast<D3D11Buffer*>(argumentBuffer.get())->getD3D11Buffer();
		if (nativeBuf)
		{
			m_context->DrawIndexedInstancedIndirect(nativeBuf, offsetBytes);
		}
	}

	void D3D11DeviceContext::dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ)
	{
		m_context->Dispatch(threadGroupCountX, threadGroupCountY, threadGroupCountZ);
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
}
