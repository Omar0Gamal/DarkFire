#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Result.h"
#include "RHIDefines.h"
#include <memory>
#include <string_view>
#include <span>

namespace Core::RHI
{
	class DF_API IGpuResource
	{
	public:
		virtual ~IGpuResource() = default;
		virtual void* getNativeHandle() const = 0;
	};

	class DF_API IGpuBuffer : public IGpuResource
	{
	public:
		virtual const GpuBufferDesc& getDesc() const = 0;
		virtual void* getShaderResourceView() const { return nullptr; }
		virtual void* getUnorderedAccessView() const { return nullptr; }
	};

	class DF_API IGpuTexture : public IGpuResource
	{
	public:
		virtual const GpuTextureDesc& getDesc() const = 0;
		virtual void* getShaderResourceView() const = 0;
		virtual void* getRenderTargetView() const = 0;
		virtual void* getDepthStencilView() const = 0;
		virtual void* getUnorderedAccessView() const { return nullptr; }
	};

	class DF_API IShader : public IGpuResource
	{
	public:
		virtual const std::vector<uint8_t>& getBytecode() const = 0;
	};

	class DF_API IVertexShader : public IShader {};
	class DF_API IPixelShader : public IShader {};
	class DF_API IHullShader : public IShader {};
	class DF_API IDomainShader : public IShader {};
	class DF_API IComputeShader : public IShader {};

	class DF_API IInputLayout : public IGpuResource {};
	class DF_API IRasterizerState : public IGpuResource {};
	class DF_API IDepthStencilState : public IGpuResource {};
	class DF_API IBlendState : public IGpuResource {};
	class DF_API ISamplerState : public IGpuResource {};

	using IGpuSampler = ISamplerState;
	class IDeviceContext;
	class IRHIDevice;
	using IGpuContext = IDeviceContext;
	using IGpuDevice = IRHIDevice;

	template<typename T, typename E = std::string>
	using Expected = Core::Result<T, E>;

	struct GpuMemoryStats
	{
		uint64_t totalBudget = 0;
		uint64_t currentUsage = 0;
		uint64_t availableForReservation = 0;
		uint64_t currentReservation = 0;
	};

	struct ContextStats
	{
		uint32_t stateChanges = 0;
		uint32_t redundantBindsSkipped = 0;
	};

	class DF_API ISwapChain
	{
	public:
		virtual ~ISwapChain() = default;
		virtual void present(bool vsync) = 0;
		virtual void resize(uint32_t width, uint32_t height) = 0;
		virtual std::shared_ptr<IGpuTexture> getBackBufferTexture() = 0;

		// Milestone 4.1: Low-latency waitable object controls
		virtual bool setMaximumFrameLatency(uint32_t maxLatency) { (void)maxLatency; return false; }
		virtual bool waitForNextFrame(uint32_t timeoutMs = 1000) { (void)timeoutMs; return true; }
		virtual bool isWaitable() const noexcept { return false; }
	};

	class DF_API IDeviceContext
	{
	public:
		virtual ~IDeviceContext() = default;

		virtual void setViewport(const Viewport& viewport) = 0;
		virtual void setRenderTargets(std::span<const std::shared_ptr<IGpuTexture>> rtvTextures, std::shared_ptr<IGpuTexture> dsvTexture) = 0;

		void setRenderTargets(std::initializer_list<std::shared_ptr<IGpuTexture>> rtvs, std::shared_ptr<IGpuTexture> dsvTexture = nullptr)
		{
			setRenderTargets(std::span<const std::shared_ptr<IGpuTexture>>(std::data(rtvs), rtvs.size()), std::move(dsvTexture));
		}
		virtual void clearRenderTarget(const std::shared_ptr<IGpuTexture>& texture, const Color& color) = 0;
		virtual void clearDepthStencil(const std::shared_ptr<IGpuTexture>& texture, float depth = 0.0f, uint8_t stencil = 0) = 0; // Reversed-Z clears depth to 0.0f

		virtual void setPrimitiveTopology(PrimitiveTopology topology) = 0;
		virtual void setInputLayout(const std::shared_ptr<IInputLayout>& layout) = 0;
		virtual void setVertexBuffer(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t stride, uint32_t offset) = 0;
		virtual void setIndexBuffer(const std::shared_ptr<IGpuBuffer>& buffer, PixelFormat format, uint32_t offset) = 0;

		virtual void setVertexShader(const std::shared_ptr<IVertexShader>& vs) = 0;
		virtual void setPixelShader(const std::shared_ptr<IPixelShader>& ps) = 0;
		virtual void setHullShader(const std::shared_ptr<IHullShader>& hs) { (void)hs; }
		virtual void setDomainShader(const std::shared_ptr<IDomainShader>& ds) { (void)ds; }
		virtual void setComputeShader(IComputeShader* shader) = 0;

		virtual void setConstantBufferVS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) = 0;
		virtual void setConstantBufferPS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) = 0;
		virtual void setConstantBufferHS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) { (void)slot; (void)buffer; }
		virtual void setConstantBufferDS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) { (void)slot; (void)buffer; }
		virtual void setConstantBufferCS(uint32_t slot, IGpuBuffer* buffer) = 0;
		virtual void setConstantBufferSubrangeVS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t firstConstant16, uint32_t numConstants16) = 0;
		virtual void setConstantBufferSubrangePS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t firstConstant16, uint32_t numConstants16) = 0;
		virtual void setConstantBufferSubrangeDS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t firstConstant16, uint32_t numConstants16) { (void)slot; (void)buffer; (void)firstConstant16; (void)numConstants16; }

		virtual void setShaderResourceVS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture) = 0;
		virtual void setShaderResourceBufferVS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) = 0;
		virtual void setShaderResourcePS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture) = 0;
		virtual void setShaderResourceBufferPS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) = 0;
		virtual void setShaderResourceDS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture) { (void)slot; (void)texture; }
		virtual void setShaderResourceCS(uint32_t slot, IGpuTexture* texture) = 0;
		virtual void setShaderResourceBufferCS(uint32_t slot, IGpuBuffer* buffer) = 0;
		virtual void setUnorderedAccessViewCS(uint32_t slot, IGpuTexture* texture, uint32_t mipSlice = 0) = 0;
		virtual void setUnorderedAccessBufferCS(uint32_t slot, IGpuBuffer* buffer) = 0;
		virtual void setSamplerVS(uint32_t slot, const std::shared_ptr<ISamplerState>& sampler) = 0;
		virtual void setSamplerPS(uint32_t slot, const std::shared_ptr<ISamplerState>& sampler) = 0;
		virtual void setSamplerDS(uint32_t slot, const std::shared_ptr<ISamplerState>& sampler) { (void)slot; (void)sampler; }
		virtual void setSamplerCS(uint32_t slot, IGpuSampler* sampler) = 0;

		// Convenience shared_ptr overloads
		void setComputeShader(const std::shared_ptr<IComputeShader>& shader) { setComputeShader(shader.get()); }
		void setConstantBufferCS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) { setConstantBufferCS(slot, buffer.get()); }
		void setShaderResourceCS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture) { setShaderResourceCS(slot, texture.get()); }
		void setShaderResourceBufferCS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) { setShaderResourceBufferCS(slot, buffer.get()); }
		void setUnorderedAccessViewCS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture, uint32_t mipSlice = 0) { setUnorderedAccessViewCS(slot, texture.get(), mipSlice); }
		void setUnorderedAccessBufferCS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) { setUnorderedAccessBufferCS(slot, buffer.get()); }
		void setSamplerCS(uint32_t slot, const std::shared_ptr<ISamplerState>& sampler) { setSamplerCS(slot, sampler.get()); }

		virtual void setRasterizerState(const std::shared_ptr<IRasterizerState>& state) = 0;
		virtual void setDepthStencilState(const std::shared_ptr<IDepthStencilState>& state, uint32_t stencilRef = 0) = 0;
		virtual void setBlendState(const std::shared_ptr<IBlendState>& state, const float blendFactor[4] = nullptr) = 0;

		virtual void draw(uint32_t vertexCount, uint32_t startVertexLocation) = 0;
		virtual void drawIndexed(uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation) = 0;
		virtual void drawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) = 0;
		virtual void drawIndexedInstancedIndirect(const std::shared_ptr<IGpuBuffer>& argumentBuffer, uint32_t offsetBytes) = 0;
		virtual void dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) = 0;

		virtual Result<void*, std::string> map(const std::shared_ptr<IGpuBuffer>& buffer, bool discard) = 0;
		virtual void unmap(const std::shared_ptr<IGpuBuffer>& buffer) = 0;
		virtual void updateSubresource(const std::shared_ptr<IGpuBuffer>& buffer, const void* data, size_t size) = 0;

		virtual ContextStats getStats() const { return {}; }
		virtual void resetStats() {}
	};

	class DF_API IRHIDevice
	{
	public:
		virtual ~IRHIDevice() = default;

		virtual std::string_view getBackendName() const = 0;
		virtual std::string getAdapterName() const { return "Default GPU"; }
		virtual uint32_t getDedicatedVramMB() const { return 0; }
		virtual std::shared_ptr<IDeviceContext> getImmediate() = 0;

		virtual void* getNativeDevice() const { return nullptr; }
		virtual void* getNativeContext() const { return nullptr; }

		virtual GpuMemoryStats queryMemoryStats() const { return {}; }

		virtual Result<std::shared_ptr<IGpuBuffer>, std::string> createBuffer(const GpuBufferDesc& desc, const void* initialData = nullptr) = 0;
		virtual Result<std::shared_ptr<IGpuTexture>, std::string> createTexture(const GpuTextureDesc& desc, const void* initialData = nullptr) = 0;
		virtual Result<std::shared_ptr<IGpuTexture>, std::string> createTextureSubresources(const GpuTextureDesc& desc, std::span<const SubresourceData> subresources)
		{
			if (!subresources.empty())
				return createTexture(desc, subresources[0].data);
			return createTexture(desc, nullptr);
		}

		virtual Result<std::shared_ptr<IVertexShader>, std::string> createVertexShader(std::span<const uint8_t> bytecode) = 0;
		virtual Result<std::shared_ptr<IPixelShader>, std::string> createPixelShader(std::span<const uint8_t> bytecode) = 0;
		virtual Result<std::shared_ptr<IHullShader>, std::string> createHullShader(std::span<const uint8_t> bytecode) = 0;
		virtual Result<std::shared_ptr<IDomainShader>, std::string> createDomainShader(std::span<const uint8_t> bytecode) = 0;
		virtual Result<std::shared_ptr<IComputeShader>, std::string> createComputeShader(std::span<const uint8_t> bytecode) = 0;
		virtual Result<std::shared_ptr<IComputeShader>, std::string> createComputeShader(const void* shaderBytecode, size_t bytecodeLength)
		{
			if (!shaderBytecode || bytecodeLength == 0)
			{
				return Result<std::shared_ptr<IComputeShader>, std::string>::error("Invalid shader bytecode or empty length");
			}
			return createComputeShader(std::span<const uint8_t>(static_cast<const uint8_t*>(shaderBytecode), bytecodeLength));
		}

		virtual Result<std::shared_ptr<IInputLayout>, std::string> createInputLayout(std::span<const InputElementDesc> elements, std::span<const uint8_t> vsBytecode) = 0;

		virtual std::shared_ptr<IRasterizerState> getOrCreateRasterizerState(const RasterizerStateDesc& desc) = 0;
		virtual std::shared_ptr<IDepthStencilState> getOrCreateDepthStencilState(const DepthStencilStateDesc& desc) = 0;
		virtual std::shared_ptr<IBlendState> getOrCreateBlendState(const BlendStateDesc& desc) = 0;
		virtual std::shared_ptr<ISamplerState> getOrCreateSamplerState(const SamplerStateDesc& desc) = 0;

		virtual Result<std::unique_ptr<ISwapChain>, std::string> createSwapChain(void* windowHandle, uint32_t width, uint32_t height) = 0;
	};
}
