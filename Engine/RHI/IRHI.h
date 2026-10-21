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
	};

	class DF_API IGpuTexture : public IGpuResource
	{
	public:
		virtual const GpuTextureDesc& getDesc() const = 0;
		virtual void* getShaderResourceView() const = 0;
		virtual void* getRenderTargetView() const = 0;
		virtual void* getDepthStencilView() const = 0;
	};

	class DF_API IVertexShader : public IGpuResource
	{
	public:
		virtual const std::vector<uint8_t>& getBytecode() const = 0;
	};

	class DF_API IPixelShader : public IGpuResource
	{
	public:
		virtual const std::vector<uint8_t>& getBytecode() const = 0;
	};

	class DF_API IInputLayout : public IGpuResource {};
	class DF_API IRasterizerState : public IGpuResource {};
	class DF_API IDepthStencilState : public IGpuResource {};
	class DF_API IBlendState : public IGpuResource {};
	class DF_API ISamplerState : public IGpuResource {};

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
	};

	class DF_API IDeviceContext
	{
	public:
		virtual ~IDeviceContext() = default;

		virtual void setViewport(const Viewport& viewport) = 0;
		virtual void setRenderTargets(std::span<std::shared_ptr<IGpuTexture>> rtvTextures, std::shared_ptr<IGpuTexture> dsvTexture) = 0;
		virtual void clearRenderTarget(const std::shared_ptr<IGpuTexture>& texture, const Color& color) = 0;
		virtual void clearDepthStencil(const std::shared_ptr<IGpuTexture>& texture, float depth = 0.0f, uint8_t stencil = 0) = 0; // Reversed-Z clears depth to 0.0f

		virtual void setPrimitiveTopology(PrimitiveTopology topology) = 0;
		virtual void setInputLayout(const std::shared_ptr<IInputLayout>& layout) = 0;
		virtual void setVertexBuffer(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t stride, uint32_t offset) = 0;
		virtual void setIndexBuffer(const std::shared_ptr<IGpuBuffer>& buffer, PixelFormat format, uint32_t offset) = 0;

		virtual void setVertexShader(const std::shared_ptr<IVertexShader>& vs) = 0;
		virtual void setPixelShader(const std::shared_ptr<IPixelShader>& ps) = 0;

		virtual void setConstantBufferVS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) = 0;
		virtual void setConstantBufferPS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) = 0;
		virtual void setConstantBufferSubrangeVS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t firstConstant16, uint32_t numConstants16) = 0;
		virtual void setConstantBufferSubrangePS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t firstConstant16, uint32_t numConstants16) = 0;

		virtual void setShaderResourceVS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture) = 0;
		virtual void setShaderResourcePS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture) = 0;
		virtual void setSamplerVS(uint32_t slot, const std::shared_ptr<ISamplerState>& sampler) = 0;
		virtual void setSamplerPS(uint32_t slot, const std::shared_ptr<ISamplerState>& sampler) = 0;

		virtual void setRasterizerState(const std::shared_ptr<IRasterizerState>& state) = 0;
		virtual void setDepthStencilState(const std::shared_ptr<IDepthStencilState>& state, uint32_t stencilRef = 0) = 0;
		virtual void setBlendState(const std::shared_ptr<IBlendState>& state, const float blendFactor[4] = nullptr) = 0;

		virtual void draw(uint32_t vertexCount, uint32_t startVertexLocation) = 0;
		virtual void drawIndexed(uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation) = 0;

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
		virtual std::shared_ptr<IDeviceContext> getImmediate() = 0;

		virtual void* getNativeDevice() const { return nullptr; }
		virtual void* getNativeContext() const { return nullptr; }

		virtual GpuMemoryStats queryMemoryStats() const { return {}; }

		virtual Result<std::shared_ptr<IGpuBuffer>, std::string> createBuffer(const GpuBufferDesc& desc, const void* initialData = nullptr) = 0;
		virtual Result<std::shared_ptr<IGpuTexture>, std::string> createTexture(const GpuTextureDesc& desc, const void* initialData = nullptr) = 0;

		virtual Result<std::shared_ptr<IVertexShader>, std::string> createVertexShader(std::span<const uint8_t> bytecode) = 0;
		virtual Result<std::shared_ptr<IPixelShader>, std::string> createPixelShader(std::span<const uint8_t> bytecode) = 0;

		virtual Result<std::shared_ptr<IInputLayout>, std::string> createInputLayout(std::span<const InputElementDesc> elements, std::span<const uint8_t> vsBytecode) = 0;

		virtual std::shared_ptr<IRasterizerState> getOrCreateRasterizerState(const RasterizerStateDesc& desc) = 0;
		virtual std::shared_ptr<IDepthStencilState> getOrCreateDepthStencilState(const DepthStencilStateDesc& desc) = 0;
		virtual std::shared_ptr<IBlendState> getOrCreateBlendState(const BlendStateDesc& desc) = 0;
		virtual std::shared_ptr<ISamplerState> getOrCreateSamplerState(const SamplerStateDesc& desc) = 0;

		virtual Result<std::unique_ptr<ISwapChain>, std::string> createSwapChain(void* windowHandle, uint32_t width, uint32_t height) = 0;
	};
}
