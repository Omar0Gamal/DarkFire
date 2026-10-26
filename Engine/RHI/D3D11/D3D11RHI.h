#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/RHI/IRHI.h"
#include "Engine/RHI/Null/NullRHI.h" // For StateCache template
#include "Engine/RHI/D3D11/D3D11FormatUtils.h"

#include <d3d11_1.h>
#include <dxgi1_5.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <unordered_map>
#include <span>
#include <memory>

namespace Core::RHI
{
	using Microsoft::WRL::ComPtr;

	class D3D11Buffer : public IGpuBuffer
	{
	public:
		D3D11Buffer(const GpuBufferDesc& desc, ComPtr<ID3D11Buffer> buffer, ComPtr<ID3D11ShaderResourceView> srv = nullptr);
		D3D11Buffer(const GpuBufferDesc& desc, ComPtr<ID3D11Buffer> buffer, ComPtr<ID3D11ShaderResourceView> srv, ComPtr<ID3D11UnorderedAccessView> uav)
			: m_desc(desc), m_buffer(std::move(buffer)), m_srv(std::move(srv)), m_uav(std::move(uav)) {}
		~D3D11Buffer() override;

		void* getNativeHandle() const override { return m_buffer.Get(); }
		const GpuBufferDesc& getDesc() const override { return m_desc; }
		ID3D11Buffer* getD3D11Buffer() const noexcept { return m_buffer.Get(); }
		void* getShaderResourceView() const override { return m_srv.Get(); }
		ID3D11ShaderResourceView* getD3D11SRV() const noexcept { return m_srv.Get(); }
		void* getUnorderedAccessView() const override { return m_uav.Get(); }
		ID3D11UnorderedAccessView* getD3D11UAV() const noexcept { return m_uav.Get(); }

	private:
		GpuBufferDesc m_desc;
		ComPtr<ID3D11Buffer> m_buffer;
		ComPtr<ID3D11ShaderResourceView> m_srv;
		ComPtr<ID3D11UnorderedAccessView> m_uav;
	};

	using D3D11GpuBuffer = D3D11Buffer;

	class D3D11Texture : public IGpuTexture
	{
	public:
		D3D11Texture(
			const GpuTextureDesc& desc,
			ComPtr<ID3D11Resource> resource,
			ComPtr<ID3D11ShaderResourceView> srv,
			ComPtr<ID3D11RenderTargetView> rtv,
			ComPtr<ID3D11DepthStencilView> dsv);
		D3D11Texture(
			const GpuTextureDesc& desc,
			ComPtr<ID3D11Resource> resource,
			ComPtr<ID3D11ShaderResourceView> srv,
			ComPtr<ID3D11RenderTargetView> rtv,
			ComPtr<ID3D11DepthStencilView> dsv,
			ComPtr<ID3D11UnorderedAccessView> uav,
			std::vector<ComPtr<ID3D11UnorderedAccessView>> mipUAVs = {})
			: m_desc(desc)
			, m_resource(std::move(resource))
			, m_srv(std::move(srv))
			, m_rtv(std::move(rtv))
			, m_dsv(std::move(dsv))
			, m_uav(std::move(uav))
			, m_mipUAVs(std::move(mipUAVs)) {}
		~D3D11Texture() override;

		void* getNativeHandle() const override { return m_resource.Get(); }
		const GpuTextureDesc& getDesc() const override { return m_desc; }
		void* getShaderResourceView() const override { return m_srv.Get(); }
		void* getRenderTargetView() const override { return m_rtv.Get(); }
		void* getDepthStencilView() const override { return m_dsv.Get(); }
		void* getUnorderedAccessView() const override { return m_uav.Get(); }

		ID3D11ShaderResourceView* getD3D11SRV() const noexcept { return m_srv.Get(); }
		ID3D11RenderTargetView* getD3D11RTV() const noexcept { return m_rtv.Get(); }
		ID3D11DepthStencilView* getD3D11DSV() const noexcept { return m_dsv.Get(); }
		ID3D11UnorderedAccessView* getD3D11UAV(uint32_t mipSlice = 0) const noexcept
		{
			if (mipSlice == 0) return m_uav.Get();
			if (mipSlice < m_mipUAVs.size()) return m_mipUAVs[mipSlice].Get();
			return nullptr;
		}
		ID3D11UnorderedAccessView* getUAV(uint32_t mipSlice = 0) const noexcept { return getD3D11UAV(mipSlice); }

	private:
		GpuTextureDesc m_desc;
		ComPtr<ID3D11Resource> m_resource;
		ComPtr<ID3D11ShaderResourceView> m_srv;
		ComPtr<ID3D11RenderTargetView> m_rtv;
		ComPtr<ID3D11DepthStencilView> m_dsv;
		ComPtr<ID3D11UnorderedAccessView> m_uav;
		std::vector<ComPtr<ID3D11UnorderedAccessView>> m_mipUAVs;
	};

	using D3D11GpuTexture = D3D11Texture;

	class D3D11VertexShader : public IVertexShader
	{
	public:
		D3D11VertexShader(ComPtr<ID3D11VertexShader> vs, std::span<const uint8_t> bytecode);
		~D3D11VertexShader() override;
		void* getNativeHandle() const override { return m_vs.Get(); }
		const std::vector<uint8_t>& getBytecode() const override { return m_bytecode; }
		ID3D11VertexShader* getD3D11VS() const noexcept { return m_vs.Get(); }

	private:
		ComPtr<ID3D11VertexShader> m_vs;
		std::vector<uint8_t> m_bytecode;
	};

	class D3D11PixelShader : public IPixelShader
	{
	public:
		D3D11PixelShader(ComPtr<ID3D11PixelShader> ps, std::span<const uint8_t> bytecode);
		~D3D11PixelShader() override;
		void* getNativeHandle() const override { return m_ps.Get(); }
		const std::vector<uint8_t>& getBytecode() const override { return m_bytecode; }
		ID3D11PixelShader* getD3D11PS() const noexcept { return m_ps.Get(); }

	private:
		ComPtr<ID3D11PixelShader> m_ps;
		std::vector<uint8_t> m_bytecode;
	};

	class D3D11HullShader : public IHullShader
	{
	public:
		D3D11HullShader(ComPtr<ID3D11HullShader> hs, std::span<const uint8_t> bytecode);
		~D3D11HullShader() override;
		void* getNativeHandle() const override { return m_hs.Get(); }
		const std::vector<uint8_t>& getBytecode() const override { return m_bytecode; }
		ID3D11HullShader* getD3D11HS() const noexcept { return m_hs.Get(); }

	private:
		ComPtr<ID3D11HullShader> m_hs;
		std::vector<uint8_t> m_bytecode;
	};

	class D3D11DomainShader : public IDomainShader
	{
	public:
		D3D11DomainShader(ComPtr<ID3D11DomainShader> ds, std::span<const uint8_t> bytecode);
		~D3D11DomainShader() override;
		void* getNativeHandle() const override { return m_ds.Get(); }
		const std::vector<uint8_t>& getBytecode() const override { return m_bytecode; }
		ID3D11DomainShader* getD3D11DS() const noexcept { return m_ds.Get(); }

	private:
		ComPtr<ID3D11DomainShader> m_ds;
		std::vector<uint8_t> m_bytecode;
	};

	class D3D11ComputeShader : public IComputeShader
	{
	public:
		D3D11ComputeShader(ComPtr<ID3D11ComputeShader> cs, std::span<const uint8_t> bytecode);
		~D3D11ComputeShader() override;
		void* getNativeHandle() const override { return m_cs.Get(); }
		const std::vector<uint8_t>& getBytecode() const override { return m_bytecode; }
		ID3D11ComputeShader* getD3D11CS() const noexcept { return m_cs.Get(); }

	private:
		ComPtr<ID3D11ComputeShader> m_cs;
		std::vector<uint8_t> m_bytecode;
	};

	class D3D11InputLayout : public IInputLayout
	{
	public:
		explicit D3D11InputLayout(ComPtr<ID3D11InputLayout> layout);
		~D3D11InputLayout() override;
		void* getNativeHandle() const override { return m_layout.Get(); }
		ID3D11InputLayout* getD3D11Layout() const noexcept { return m_layout.Get(); }

	private:
		ComPtr<ID3D11InputLayout> m_layout;
	};

	class D3D11RasterizerState : public IRasterizerState
	{
	public:
		D3D11RasterizerState(const RasterizerStateDesc& desc, ComPtr<ID3D11RasterizerState> state);
		~D3D11RasterizerState() override;
		void* getNativeHandle() const override { return m_state.Get(); }
		ID3D11RasterizerState* getD3D11State() const noexcept { return m_state.Get(); }
		const RasterizerStateDesc& getDesc() const noexcept { return m_desc; }

	private:
		RasterizerStateDesc m_desc;
		ComPtr<ID3D11RasterizerState> m_state;
	};

	class D3D11DepthStencilState : public IDepthStencilState
	{
	public:
		D3D11DepthStencilState(const DepthStencilStateDesc& desc, ComPtr<ID3D11DepthStencilState> state);
		~D3D11DepthStencilState() override;
		void* getNativeHandle() const override { return m_state.Get(); }
		ID3D11DepthStencilState* getD3D11State() const noexcept { return m_state.Get(); }
		const DepthStencilStateDesc& getDesc() const noexcept { return m_desc; }

	private:
		DepthStencilStateDesc m_desc;
		ComPtr<ID3D11DepthStencilState> m_state;
	};

	class D3D11BlendState : public IBlendState
	{
	public:
		D3D11BlendState(const BlendStateDesc& desc, ComPtr<ID3D11BlendState> state);
		~D3D11BlendState() override;
		void* getNativeHandle() const override { return m_state.Get(); }
		ID3D11BlendState* getD3D11State() const noexcept { return m_state.Get(); }
		const BlendStateDesc& getDesc() const noexcept { return m_desc; }

	private:
		BlendStateDesc m_desc;
		ComPtr<ID3D11BlendState> m_state;
	};

	class D3D11SamplerState : public ISamplerState
	{
	public:
		D3D11SamplerState(const SamplerStateDesc& desc, ComPtr<ID3D11SamplerState> state);
		~D3D11SamplerState() override;
		void* getNativeHandle() const override { return m_state.Get(); }
		ID3D11SamplerState* getD3D11State() const noexcept { return m_state.Get(); }
		const SamplerStateDesc& getDesc() const noexcept { return m_desc; }

	private:
		SamplerStateDesc m_desc;
		ComPtr<ID3D11SamplerState> m_state;
	};

	class D3D11SwapChain : public ISwapChain
	{
	public:
		D3D11SwapChain(ComPtr<IDXGISwapChain1> swapChain, std::shared_ptr<D3D11Texture> backBuffer, HWND hwnd, bool allowTearing = false, HANDLE waitableObject = nullptr);
		~D3D11SwapChain() override;

		void present(bool vsync) override;
		void resize(uint32_t width, uint32_t height) override;
		std::shared_ptr<IGpuTexture> getBackBufferTexture() override;
		void createRenderTarget(uint32_t width, uint32_t height);

		bool setMaximumFrameLatency(uint32_t maxLatency) override;
		bool waitForNextFrame(uint32_t timeoutMs = 1000) override;
		bool isWaitable() const noexcept override { return m_waitableObject != nullptr; }

	private:
		ComPtr<IDXGISwapChain1> m_swapChain;
		ComPtr<IDXGISwapChain2> m_swapChain2;
		std::shared_ptr<D3D11Texture> m_backBuffer;
		HWND m_hwnd = nullptr;
		bool m_allowTearing = false;
		HANDLE m_waitableObject = nullptr;
	};

	class D3D11StateTracker
	{
	public:
		void reset();

		ID3D11RenderTargetView* currentRTVs[8] = {};
		UINT currentRTVCount = 0;
		ID3D11DepthStencilView* currentDSV = nullptr;

		ID3D11InputLayout* currentInputLayout = nullptr;
		D3D11_PRIMITIVE_TOPOLOGY currentTopology = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
		ID3D11VertexShader* currentVS = nullptr;
		ID3D11PixelShader* currentPS = nullptr;
		ID3D11HullShader* currentHS = nullptr;
		ID3D11DomainShader* currentDS = nullptr;
		ID3D11ComputeShader* currentCS = nullptr;
		ID3D11RasterizerState* currentRS = nullptr;
		ID3D11DepthStencilState* currentDSS = nullptr;
		uint32_t currentStencilRef = 0;
		ID3D11BlendState* currentBS = nullptr;
		float currentBlendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };

		static constexpr size_t MAX_SLOTS = 32;
		ID3D11Buffer* currentVBs[MAX_SLOTS] = {};
		uint32_t currentVBStrides[MAX_SLOTS] = {};
		uint32_t currentVBOffsets[MAX_SLOTS] = {};
		ID3D11Buffer* currentIB = nullptr;
		DXGI_FORMAT currentIBFormat = DXGI_FORMAT_UNKNOWN;
		uint32_t currentIBOffset = 0;

		ID3D11Buffer* currentVSConstantBuffers[MAX_SLOTS] = {};
		ID3D11Buffer* currentPSConstantBuffers[MAX_SLOTS] = {};
		ID3D11Buffer* currentCSConstantBuffers[MAX_SLOTS] = {};
		ID3D11Buffer* currentHSConstantBuffers[MAX_SLOTS] = {};
		ID3D11Buffer* currentDSConstantBuffers[MAX_SLOTS] = {};
		ID3D11ShaderResourceView* currentVSSRVs[MAX_SLOTS] = {};
		ID3D11ShaderResourceView* currentPSSRVs[MAX_SLOTS] = {};
		ID3D11ShaderResourceView* currentCSSRVs[MAX_SLOTS] = {};
		ID3D11ShaderResourceView* currentDSSRVs[MAX_SLOTS] = {};
		ID3D11UnorderedAccessView* currentCSUAVs[MAX_SLOTS] = {};
		ID3D11SamplerState* currentVSSamplers[MAX_SLOTS] = {};
		ID3D11SamplerState* currentPSSamplers[MAX_SLOTS] = {};
		ID3D11SamplerState* currentCSSamplers[MAX_SLOTS] = {};
		ID3D11SamplerState* currentDSSamplers[MAX_SLOTS] = {};

		D3D11_VIEWPORT currentViewport{};
	};

	class D3D11DeviceContext : public IDeviceContext
	{
	public:
		D3D11DeviceContext(ComPtr<ID3D11DeviceContext> context, ComPtr<ID3D11DeviceContext1> context1);
		~D3D11DeviceContext() override;

		void setViewport(const Viewport& viewport) override;
		void setScissorRect(int32_t x, int32_t y, uint32_t width, uint32_t height);
		void setRenderTargets(std::span<const std::shared_ptr<IGpuTexture>> rtvTextures, std::shared_ptr<IGpuTexture> dsvTexture) override;
		void clearRenderTarget(const std::shared_ptr<IGpuTexture>& texture, const Color& color) override;
		void clearDepthStencil(const std::shared_ptr<IGpuTexture>& texture, float depth = 0.0f, uint8_t stencil = 0) override;

		void setPrimitiveTopology(PrimitiveTopology topology) override;
		void setInputLayout(const std::shared_ptr<IInputLayout>& layout) override;
		void setVertexBuffer(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t stride, uint32_t offset) override;
		void setIndexBuffer(const std::shared_ptr<IGpuBuffer>& buffer, PixelFormat format, uint32_t offset) override;

		void setVertexShader(const std::shared_ptr<IVertexShader>& vs) override;
		void setPixelShader(const std::shared_ptr<IPixelShader>& ps) override;
		void setHullShader(const std::shared_ptr<IHullShader>& hs) override;
		void setDomainShader(const std::shared_ptr<IDomainShader>& ds) override;
		void setComputeShader(IComputeShader* shader) override;
		void setComputeShader(const std::shared_ptr<D3D11ComputeShader>& cs) { setComputeShader(cs.get()); }

		void setConstantBufferVS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) override;
		void setConstantBufferPS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) override;
		void setConstantBufferHS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) override;
		void setConstantBufferDS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) override;
		void setConstantBufferCS(uint32_t slot, IGpuBuffer* buffer) override;
		void setConstantBufferCS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) { setConstantBufferCS(slot, buffer.get()); }
		void setConstantBufferSubrangeVS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t firstConstant16, uint32_t numConstants16) override;
		void setConstantBufferSubrangePS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t firstConstant16, uint32_t numConstants16) override;
		void setConstantBufferSubrangeDS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t firstConstant16, uint32_t numConstants16) override;
		void setConstantBufferSubrangeCS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t firstConstant16, uint32_t numConstants16);

		void setShaderResourceVS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture) override;
		void setShaderResourceBufferVS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) override;
		void setShaderResourcePS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture) override;
		void setShaderResourceBufferPS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) override;
		void setShaderResourceDS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture) override;
		void setShaderResourceCS(uint32_t slot, IGpuTexture* texture) override;
		void setShaderResourceCS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture) { setShaderResourceCS(slot, texture.get()); }
		void setShaderResourceBufferCS(uint32_t slot, IGpuBuffer* buffer) override;
		void setShaderResourceBufferCS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer);

		void setUnorderedAccessViewCS(uint32_t slot, IGpuTexture* texture, uint32_t mipSlice = 0) override;
		void setUnorderedAccessViewCS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture, uint32_t mipSlice = 0) { setUnorderedAccessViewCS(slot, texture.get(), mipSlice); }
		void setUnorderedAccessViewCS(uint32_t slot, ID3D11UnorderedAccessView* uav, uint32_t initialCount = 0xFFFFFFFF);
		void setUnorderedAccessBufferCS(uint32_t slot, IGpuBuffer* buffer) override;
		void setUnorderedAccessBufferCS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) { setUnorderedAccessBufferCS(slot, buffer.get()); }

		void setSamplerVS(uint32_t slot, const std::shared_ptr<ISamplerState>& sampler) override;
		void setSamplerPS(uint32_t slot, const std::shared_ptr<ISamplerState>& sampler) override;
		void setSamplerDS(uint32_t slot, const std::shared_ptr<ISamplerState>& sampler) override;
		void setSamplerCS(uint32_t slot, IGpuSampler* sampler) override;
		void setSamplerCS(uint32_t slot, const std::shared_ptr<ISamplerState>& sampler) { setSamplerCS(slot, sampler.get()); }

		void setRasterizerState(const std::shared_ptr<IRasterizerState>& state) override;
		void setDepthStencilState(const std::shared_ptr<IDepthStencilState>& state, uint32_t stencilRef = 0) override;
		void setBlendState(const std::shared_ptr<IBlendState>& state, const float blendFactor[4] = nullptr) override;

		void draw(uint32_t vertexCount, uint32_t startVertexLocation) override;
		void drawIndexed(uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation) override;
		void drawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) override;
		void drawIndexedInstancedIndirect(const std::shared_ptr<IGpuBuffer>& argumentBuffer, uint32_t offsetBytes) override;
		void dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) override;

		Result<void*, std::string> map(const std::shared_ptr<IGpuBuffer>& buffer, bool discard) override;
		void unmap(const std::shared_ptr<IGpuBuffer>& buffer) override;
		void updateSubresource(const std::shared_ptr<IGpuBuffer>& buffer, const void* data, size_t size) override;

		void clearTrackedState();
		ID3D11DeviceContext* getD3D11Context() const noexcept { return m_context.Get(); }
		ID3D11DeviceContext1* getD3D11Context1() const noexcept { return m_context1.Get(); }

		ContextStats getStats() const override { return m_stats; }
		void resetStats() override { m_stats = {}; }

	private:
		ComPtr<ID3D11DeviceContext> m_context;
		ComPtr<ID3D11DeviceContext1> m_context1;
		D3D11StateTracker m_tracker;
		ContextStats m_stats{};
	};

	class DF_API D3D11RHIDevice : public IRHIDevice
	{
	public:
		explicit D3D11RHIDevice(bool enableDebugLayer = false);
		~D3D11RHIDevice() override;

		bool initialize();
		void shutdown();

		std::string_view getBackendName() const override { return "D3D11"; }
		std::string getAdapterName() const override { return m_adapterDescription; }
		uint32_t getDedicatedVramMB() const override { return m_dedicatedVramMB; }
		std::shared_ptr<IDeviceContext> getImmediate() override { return m_immediateContext; }

		Result<std::shared_ptr<IGpuBuffer>, std::string> createBuffer(const GpuBufferDesc& desc, const void* initialData = nullptr) override;
		Result<std::shared_ptr<IGpuTexture>, std::string> createTexture(const GpuTextureDesc& desc, const void* initialData = nullptr) override;
		Result<std::shared_ptr<IGpuTexture>, std::string> createTextureSubresources(const GpuTextureDesc& desc, std::span<const SubresourceData> subresources) override;

		Result<std::shared_ptr<IVertexShader>, std::string> createVertexShader(std::span<const uint8_t> bytecode) override;
		Result<std::shared_ptr<IPixelShader>, std::string> createPixelShader(std::span<const uint8_t> bytecode) override;
		Result<std::shared_ptr<IHullShader>, std::string> createHullShader(std::span<const uint8_t> bytecode) override;
		Result<std::shared_ptr<IDomainShader>, std::string> createDomainShader(std::span<const uint8_t> bytecode) override;
		Result<std::shared_ptr<IComputeShader>, std::string> createComputeShader(std::span<const uint8_t> bytecode) override;

		Result<std::shared_ptr<IInputLayout>, std::string> createInputLayout(std::span<const InputElementDesc> elements, std::span<const uint8_t> vsBytecode) override;

		std::shared_ptr<IRasterizerState> getOrCreateRasterizerState(const RasterizerStateDesc& desc) override;
		std::shared_ptr<IDepthStencilState> getOrCreateDepthStencilState(const DepthStencilStateDesc& desc) override;
		std::shared_ptr<IBlendState> getOrCreateBlendState(const BlendStateDesc& desc) override;
		std::shared_ptr<ISamplerState> getOrCreateSamplerState(const SamplerStateDesc& desc) override;

		Result<std::unique_ptr<ISwapChain>, std::string> createSwapChain(void* windowHandle, uint32_t width, uint32_t height) override;

		GpuMemoryStats queryMemoryStats() const override;

		void* getNativeDevice() const override { return m_device.Get(); }
		void* getNativeContext() const override { return m_immediateContext ? m_immediateContext->getD3D11Context() : nullptr; }

		ID3D11Device* getD3D11Device() const noexcept { return m_device.Get(); }
		ID3D11Device1* getD3D11Device1() const noexcept { return m_device1.Get(); }

	private:
		bool m_enableDebugLayer = false;
		std::string m_adapterDescription = "Unknown D3D11 GPU";
		uint32_t m_dedicatedVramMB = 0;
		ComPtr<ID3D11Device> m_device;
		ComPtr<ID3D11Device1> m_device1;
		ComPtr<IDXGIFactory2> m_dxgiFactory;
		std::shared_ptr<D3D11DeviceContext> m_immediateContext;

		StateCache<RasterizerStateDesc, IRasterizerState> m_rasterizerCache;
		StateCache<DepthStencilStateDesc, IDepthStencilState> m_depthStencilCache;
		StateCache<BlendStateDesc, IBlendState> m_blendCache;
		StateCache<SamplerStateDesc, ISamplerState> m_samplerCache;
	};

	DF_API std::shared_ptr<IRHIDevice> createD3D11Device(bool enableDebugLayer = false);
}
