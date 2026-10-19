#include "NullRHI.h"
#include "Engine/Core/Log/Log.h"
#include <cstring>

namespace Core::RHI
{
	namespace
	{
		inline uint64_t fnv1a64(const void* data, size_t size, uint64_t hash = 14695981039346656037ull) noexcept
		{
			const auto* bytes = static_cast<const uint8_t*>(data);
			for (size_t i = 0; i < size; ++i)
			{
				hash ^= bytes[i];
				hash *= 1099511628211ull;
			}
			return hash;
		}

		size_t getPixelFormatBytesPerPixel(PixelFormat format) noexcept
		{
			switch (format)
			{
				case PixelFormat::R8G8B8A8_UNORM:
				case PixelFormat::R8G8B8A8_UNORM_SRGB:
				case PixelFormat::R8G8B8A8_SNORM:
				case PixelFormat::D24_UNORM_S8_UINT:
				case PixelFormat::R32_FLOAT:
				case PixelFormat::R32_TYPELESS:
				case PixelFormat::D32_FLOAT:
				case PixelFormat::R11G11B10_FLOAT:
				case PixelFormat::R32_UINT:
					return 4;
				case PixelFormat::R16G16B16A16_FLOAT:
				case PixelFormat::R32G32_FLOAT:
					return 8;
				case PixelFormat::R32G32B32_FLOAT:
					return 12;
				case PixelFormat::R32G32B32A32_FLOAT:
					return 16;
				default:
					return 4;
			}
		}
	}

	NullBuffer::NullBuffer(const GpuBufferDesc& desc, const void* initialData)
		: m_desc(desc)
	{
		m_data.resize(desc.size);
		if (initialData != nullptr && desc.size > 0)
		{
			std::memcpy(m_data.data(), initialData, desc.size);
		}
	}

	NullTexture::NullTexture(const GpuTextureDesc& desc, const void* initialData)
		: m_desc(desc)
	{
		const size_t bpp = getPixelFormatBytesPerPixel(desc.format);
		const size_t totalBytes = static_cast<size_t>(desc.width) * desc.height * desc.depth * bpp;
		m_data.resize(totalBytes);
		if (initialData != nullptr && totalBytes > 0)
		{
			std::memcpy(m_data.data(), initialData, totalBytes);
		}
	}

	NullSwapChain::NullSwapChain(uint32_t width, uint32_t height)
		: m_width(width)
		, m_height(height)
	{
		GpuTextureDesc desc{};
		desc.width = width;
		desc.height = height;
		desc.depth = 1;
		desc.mipLevels = 1;
		desc.arraySize = 1;
		desc.format = PixelFormat::R8G8B8A8_UNORM;
		desc.bindFlags = BufferBindFlags::RenderTarget | BufferBindFlags::ShaderResource;
		m_backBuffer = std::make_shared<NullTexture>(desc);
	}

	void NullSwapChain::present(bool vsync)
	{
		(void)vsync;
		m_presentCount++;
	}

	void NullSwapChain::resize(uint32_t width, uint32_t height)
	{
		m_width = width;
		m_height = height;
		GpuTextureDesc desc{};
		desc.width = width;
		desc.height = height;
		desc.depth = 1;
		desc.mipLevels = 1;
		desc.arraySize = 1;
		desc.format = PixelFormat::R8G8B8A8_UNORM;
		desc.bindFlags = BufferBindFlags::RenderTarget | BufferBindFlags::ShaderResource;
		m_backBuffer = std::make_shared<NullTexture>(desc);
	}

	void NullDeviceContext::setRenderTargets(std::span<std::shared_ptr<IGpuTexture>> rtvTextures, std::shared_ptr<IGpuTexture> dsvTexture)
	{
		m_renderTargets.assign(rtvTextures.begin(), rtvTextures.end());
		m_depthStencilTarget = dsvTexture;
	}

	void NullDeviceContext::clearRenderTarget(const std::shared_ptr<IGpuTexture>& texture, const Color& color)
	{
		if (auto* nullTex = dynamic_cast<NullTexture*>(texture.get()))
		{
			nullTex->setClearedColor(color);
		}
	}

	void NullDeviceContext::clearDepthStencil(const std::shared_ptr<IGpuTexture>& texture, float depth, uint8_t stencil)
	{
		if (auto* nullTex = dynamic_cast<NullTexture*>(texture.get()))
		{
			nullTex->setClearedDepthStencil(depth, stencil);
		}
	}

	void NullDeviceContext::setVertexBuffer(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t stride, uint32_t offset)
	{
		(void)slot;
		(void)buffer;
		(void)stride;
		(void)offset;
	}

	void NullDeviceContext::setIndexBuffer(const std::shared_ptr<IGpuBuffer>& buffer, PixelFormat format, uint32_t offset)
	{
		(void)buffer;
		(void)format;
		(void)offset;
	}

	void NullDeviceContext::setConstantBufferVS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer)
	{
		(void)slot;
		(void)buffer;
	}

	void NullDeviceContext::setConstantBufferPS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer)
	{
		(void)slot;
		(void)buffer;
	}

	void NullDeviceContext::setConstantBufferSubrangeVS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t firstConstant16, uint32_t numConstants16)
	{
		(void)slot;
		(void)buffer;
		(void)firstConstant16;
		(void)numConstants16;
	}

	void NullDeviceContext::setConstantBufferSubrangePS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t firstConstant16, uint32_t numConstants16)
	{
		(void)slot;
		(void)buffer;
		(void)firstConstant16;
		(void)numConstants16;
	}

	void NullDeviceContext::setShaderResourceVS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture)
	{
		(void)slot;
		(void)texture;
	}

	void NullDeviceContext::setShaderResourcePS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture)
	{
		(void)slot;
		(void)texture;
	}

	void NullDeviceContext::setSamplerVS(uint32_t slot, const std::shared_ptr<ISamplerState>& sampler)
	{
		(void)slot;
		(void)sampler;
	}

	void NullDeviceContext::setSamplerPS(uint32_t slot, const std::shared_ptr<ISamplerState>& sampler)
	{
		(void)slot;
		(void)sampler;
	}

	void NullDeviceContext::draw(uint32_t vertexCount, uint32_t startVertexLocation)
	{
		(void)vertexCount;
		(void)startVertexLocation;
		m_drawCallCount++;
	}

	void NullDeviceContext::drawIndexed(uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation)
	{
		(void)indexCount;
		(void)startIndexLocation;
		(void)baseVertexLocation;
		m_drawIndexedCallCount++;
	}

	Result<void*, std::string> NullDeviceContext::map(const std::shared_ptr<IGpuBuffer>& buffer, bool discard)
	{
		(void)discard;
		if (!buffer)
		{
			return Result<void*, std::string>::error("Null buffer passed to map");
		}
		auto* nullBuf = dynamic_cast<NullBuffer*>(buffer.get());
		if (!nullBuf)
		{
			return Result<void*, std::string>::error("Invalid buffer type in NullDeviceContext::map");
		}
		return Result<void*, std::string>::ok(nullBuf->getData().data());
	}

	void NullDeviceContext::unmap(const std::shared_ptr<IGpuBuffer>& buffer)
	{
		(void)buffer;
	}

	void NullDeviceContext::updateSubresource(const std::shared_ptr<IGpuBuffer>& buffer, const void* data, size_t size)
	{
		if (!buffer || !data || size == 0)
		{
			return;
		}
		if (auto* nullBuf = dynamic_cast<NullBuffer*>(buffer.get()))
		{
			const size_t copySize = (size < nullBuf->getData().size()) ? size : nullBuf->getData().size();
			std::memcpy(nullBuf->getData().data(), data, copySize);
		}
	}

	NullRHIDevice::NullRHIDevice()
		: m_immediateContext(std::make_shared<NullDeviceContext>())
	{
	}

	Result<std::shared_ptr<IGpuBuffer>, std::string> NullRHIDevice::createBuffer(const GpuBufferDesc& desc, const void* initialData)
	{
		return Result<std::shared_ptr<IGpuBuffer>, std::string>::ok(std::make_shared<NullBuffer>(desc, initialData));
	}

	Result<std::shared_ptr<IGpuTexture>, std::string> NullRHIDevice::createTexture(const GpuTextureDesc& desc, const void* initialData)
	{
		return Result<std::shared_ptr<IGpuTexture>, std::string>::ok(std::make_shared<NullTexture>(desc, initialData));
	}

	Result<std::shared_ptr<IVertexShader>, std::string> NullRHIDevice::createVertexShader(std::span<const uint8_t> bytecode)
	{
		return Result<std::shared_ptr<IVertexShader>, std::string>::ok(std::make_shared<NullVertexShader>(bytecode));
	}

	Result<std::shared_ptr<IPixelShader>, std::string> NullRHIDevice::createPixelShader(std::span<const uint8_t> bytecode)
	{
		return Result<std::shared_ptr<IPixelShader>, std::string>::ok(std::make_shared<NullPixelShader>(bytecode));
	}

	Result<std::shared_ptr<IInputLayout>, std::string> NullRHIDevice::createInputLayout(std::span<const InputElementDesc> elements, std::span<const uint8_t> vsBytecode)
	{
		(void)vsBytecode;
		return Result<std::shared_ptr<IInputLayout>, std::string>::ok(std::make_shared<NullInputLayout>(elements));
	}

	uint64_t NullRHIDevice::hashRasterizerDesc(const RasterizerStateDesc& desc) noexcept
	{
		uint64_t h = fnv1a64(&desc.cullMode, sizeof(desc.cullMode));
		h = fnv1a64(&desc.fillMode, sizeof(desc.fillMode), h);
		h = fnv1a64(&desc.frontCounterClockwise, sizeof(desc.frontCounterClockwise), h);
		h = fnv1a64(&desc.depthBias, sizeof(desc.depthBias), h);
		h = fnv1a64(&desc.slopeScaledDepthBias, sizeof(desc.slopeScaledDepthBias), h);
		h = fnv1a64(&desc.depthClipEnable, sizeof(desc.depthClipEnable), h);
		h = fnv1a64(&desc.scissorEnable, sizeof(desc.scissorEnable), h);
		return h;
	}

	uint64_t NullRHIDevice::hashDepthStencilDesc(const DepthStencilStateDesc& desc) noexcept
	{
		uint64_t h = fnv1a64(&desc.depthEnable, sizeof(desc.depthEnable));
		h = fnv1a64(&desc.depthWrite, sizeof(desc.depthWrite), h);
		h = fnv1a64(&desc.depthFunc, sizeof(desc.depthFunc), h);
		h = fnv1a64(&desc.stencilEnable, sizeof(desc.stencilEnable), h);
		return h;
	}

	uint64_t NullRHIDevice::hashBlendDesc(const BlendStateDesc& desc) noexcept
	{
		uint64_t h = fnv1a64(&desc.blendEnable, sizeof(desc.blendEnable));
		h = fnv1a64(&desc.srcBlend, sizeof(desc.srcBlend), h);
		h = fnv1a64(&desc.destBlend, sizeof(desc.destBlend), h);
		h = fnv1a64(&desc.blendOp, sizeof(desc.blendOp), h);
		h = fnv1a64(&desc.srcBlendAlpha, sizeof(desc.srcBlendAlpha), h);
		h = fnv1a64(&desc.destBlendAlpha, sizeof(desc.destBlendAlpha), h);
		h = fnv1a64(&desc.blendOpAlpha, sizeof(desc.blendOpAlpha), h);
		h = fnv1a64(&desc.renderTargetWriteMask, sizeof(desc.renderTargetWriteMask), h);
		return h;
	}

	uint64_t NullRHIDevice::hashSamplerDesc(const SamplerStateDesc& desc) noexcept
	{
		uint64_t h = fnv1a64(&desc.filter, sizeof(desc.filter));
		h = fnv1a64(&desc.addressU, sizeof(desc.addressU), h);
		h = fnv1a64(&desc.addressV, sizeof(desc.addressV), h);
		h = fnv1a64(&desc.addressW, sizeof(desc.addressW), h);
		h = fnv1a64(&desc.comparisonFunc, sizeof(desc.comparisonFunc), h);
		h = fnv1a64(&desc.minLOD, sizeof(desc.minLOD), h);
		h = fnv1a64(&desc.maxLOD, sizeof(desc.maxLOD), h);
		return h;
	}

	std::shared_ptr<IRasterizerState> NullRHIDevice::getOrCreateRasterizerState(const RasterizerStateDesc& desc)
	{
		const uint64_t hash = hashRasterizerDesc(desc);
		return m_rasterizerCache.getOrCreate(desc, hash, [&]() {
			return std::make_shared<NullRasterizerState>(desc);
		});
	}

	std::shared_ptr<IDepthStencilState> NullRHIDevice::getOrCreateDepthStencilState(const DepthStencilStateDesc& desc)
	{
		const uint64_t hash = hashDepthStencilDesc(desc);
		return m_depthStencilCache.getOrCreate(desc, hash, [&]() {
			return std::make_shared<NullDepthStencilState>(desc);
		});
	}

	std::shared_ptr<IBlendState> NullRHIDevice::getOrCreateBlendState(const BlendStateDesc& desc)
	{
		const uint64_t hash = hashBlendDesc(desc);
		return m_blendCache.getOrCreate(desc, hash, [&]() {
			return std::make_shared<NullBlendState>(desc);
		});
	}

	std::shared_ptr<ISamplerState> NullRHIDevice::getOrCreateSamplerState(const SamplerStateDesc& desc)
	{
		const uint64_t hash = hashSamplerDesc(desc);
		return m_samplerCache.getOrCreate(desc, hash, [&]() {
			return std::make_shared<NullSamplerState>(desc);
		});
	}

	Result<std::unique_ptr<ISwapChain>, std::string> NullRHIDevice::createSwapChain(void* windowHandle, uint32_t width, uint32_t height)
	{
		(void)windowHandle;
		return Result<std::unique_ptr<ISwapChain>, std::string>::ok(std::make_unique<NullSwapChain>(width, height));
	}
}
