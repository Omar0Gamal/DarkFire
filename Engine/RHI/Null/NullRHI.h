#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/RHI/IRHI.h"
#include <functional>
#include <unordered_map>
#include <vector>

namespace Core::RHI
{
	// Hashed State Cache with Hash Collision Handling via Bucket Chaining
	template<typename DescType, typename StateType>
	class StateCache
	{
	public:
		std::shared_ptr<StateType> getOrCreate(const DescType& desc, uint64_t hash, std::function<std::shared_ptr<StateType>()> createFn)
		{
			auto it = m_cache.find(hash);
			if (it != m_cache.end())
			{
				for (auto& entry : it->second)
				{
					if (entry.first == desc)
					{
						return entry.second;
					}
				}
				// Hash collision: different desc produced the same hash. Chain it!
				auto newState = createFn();
				it->second.emplace_back(desc, newState);
				m_collisionCount++;
				return newState;
			}

			auto newState = createFn();
			m_cache[hash].emplace_back(desc, newState);
			return newState;
		}

		[[nodiscard]] uint32_t getCollisionCount() const noexcept { return m_collisionCount; }
		[[nodiscard]] size_t size() const noexcept
		{
			size_t total = 0;
			for (const auto& [k, v] : m_cache)
			{
				total += v.size();
			}
			return total;
		}

		void clear()
		{
			m_cache.clear();
			m_collisionCount = 0;
		}

	private:
		std::unordered_map<uint64_t, std::vector<std::pair<DescType, std::shared_ptr<StateType>>>> m_cache;
		uint32_t m_collisionCount = 0;
	};

	class DF_API NullBuffer : public IGpuBuffer
	{
	public:
		explicit NullBuffer(const GpuBufferDesc& desc, const void* initialData = nullptr);
		~NullBuffer() override = default;

		void* getNativeHandle() const override { return const_cast<uint8_t*>(m_data.data()); }
		const GpuBufferDesc& getDesc() const override { return m_desc; }
		void* getShaderResourceView() const override { return const_cast<uint8_t*>(m_data.data()); }
		void* getUnorderedAccessView() const override { return const_cast<uint8_t*>(m_data.data()); }
		std::vector<uint8_t>& getData() { return m_data; }
		const std::vector<uint8_t>& getData() const { return m_data; }

	private:
		GpuBufferDesc m_desc;
		std::vector<uint8_t> m_data;
	};

	class DF_API NullTexture : public IGpuTexture
	{
	public:
		explicit NullTexture(const GpuTextureDesc& desc, const void* initialData = nullptr);
		~NullTexture() override = default;

		void* getNativeHandle() const override { return const_cast<uint8_t*>(m_data.data()); }
		const GpuTextureDesc& getDesc() const override { return m_desc; }
		void* getShaderResourceView() const override { return const_cast<NullTexture*>(this); }
		void* getRenderTargetView() const override { return const_cast<NullTexture*>(this); }
		void* getDepthStencilView() const override { return const_cast<NullTexture*>(this); }
		void* getUnorderedAccessView() const override { return const_cast<NullTexture*>(this); }

		[[nodiscard]] bool isCleared() const noexcept { return m_isCleared; }
		[[nodiscard]] const Color& getClearColor() const noexcept { return m_clearColor; }
		[[nodiscard]] float getClearDepth() const noexcept { return m_clearDepth; }
		[[nodiscard]] uint8_t getClearStencil() const noexcept { return m_clearStencil; }

		void setClearedColor(const Color& c)
		{
			m_clearColor = c;
			m_isCleared = true;
		}

		void setClearedDepthStencil(float depth, uint8_t stencil)
		{
			m_clearDepth = depth;
			m_clearStencil = stencil;
			m_isCleared = true;
		}

	private:
		GpuTextureDesc m_desc;
		std::vector<uint8_t> m_data;
		Color m_clearColor{};
		float m_clearDepth = 0.0f;
		uint8_t m_clearStencil = 0;
		bool m_isCleared = false;
	};

	class DF_API NullVertexShader : public IVertexShader
	{
	public:
		explicit NullVertexShader(std::span<const uint8_t> bytecode)
			: m_bytecode(bytecode.begin(), bytecode.end()) {}
		void* getNativeHandle() const override { return const_cast<uint8_t*>(m_bytecode.data()); }
		const std::vector<uint8_t>& getBytecode() const override { return m_bytecode; }
	private:
		std::vector<uint8_t> m_bytecode;
	};

	class DF_API NullPixelShader : public IPixelShader
	{
	public:
		explicit NullPixelShader(std::span<const uint8_t> bytecode)
			: m_bytecode(bytecode.begin(), bytecode.end()) {}
		void* getNativeHandle() const override { return const_cast<uint8_t*>(m_bytecode.data()); }
		const std::vector<uint8_t>& getBytecode() const override { return m_bytecode; }
	private:
		std::vector<uint8_t> m_bytecode;
	};

	class DF_API NullHullShader : public IHullShader
	{
	public:
		explicit NullHullShader(std::span<const uint8_t> bytecode)
			: m_bytecode(bytecode.begin(), bytecode.end()) {}
		void* getNativeHandle() const override { return const_cast<uint8_t*>(m_bytecode.data()); }
		const std::vector<uint8_t>& getBytecode() const override { return m_bytecode; }
	private:
		std::vector<uint8_t> m_bytecode;
	};

	class DF_API NullDomainShader : public IDomainShader
	{
	public:
		explicit NullDomainShader(std::span<const uint8_t> bytecode)
			: m_bytecode(bytecode.begin(), bytecode.end()) {}
		void* getNativeHandle() const override { return const_cast<uint8_t*>(m_bytecode.data()); }
		const std::vector<uint8_t>& getBytecode() const override { return m_bytecode; }
	private:
		std::vector<uint8_t> m_bytecode;
	};

	class DF_API NullComputeShader : public IComputeShader
	{
	public:
		explicit NullComputeShader(std::span<const uint8_t> bytecode)
			: m_bytecode(bytecode.begin(), bytecode.end()) {}
		void* getNativeHandle() const override { return const_cast<uint8_t*>(m_bytecode.data()); }
		const std::vector<uint8_t>& getBytecode() const override { return m_bytecode; }
	private:
		std::vector<uint8_t> m_bytecode;
	};

	class DF_API NullInputLayout : public IInputLayout
	{
	public:
		explicit NullInputLayout(std::span<const InputElementDesc> elements)
			: m_elements(elements.begin(), elements.end()) {}
		void* getNativeHandle() const override { return const_cast<NullInputLayout*>(this); }
		const std::vector<InputElementDesc>& getElements() const { return m_elements; }
	private:
		std::vector<InputElementDesc> m_elements;
	};

	class DF_API NullRasterizerState : public IRasterizerState
	{
	public:
		explicit NullRasterizerState(const RasterizerStateDesc& desc) : m_desc(desc) {}
		void* getNativeHandle() const override { return const_cast<NullRasterizerState*>(this); }
		const RasterizerStateDesc& getDesc() const { return m_desc; }
	private:
		RasterizerStateDesc m_desc;
	};

	class DF_API NullDepthStencilState : public IDepthStencilState
	{
	public:
		explicit NullDepthStencilState(const DepthStencilStateDesc& desc) : m_desc(desc) {}
		void* getNativeHandle() const override { return const_cast<NullDepthStencilState*>(this); }
		const DepthStencilStateDesc& getDesc() const { return m_desc; }
	private:
		DepthStencilStateDesc m_desc;
	};

	class DF_API NullBlendState : public IBlendState
	{
	public:
		explicit NullBlendState(const BlendStateDesc& desc) : m_desc(desc) {}
		void* getNativeHandle() const override { return const_cast<NullBlendState*>(this); }
		const BlendStateDesc& getDesc() const { return m_desc; }
	private:
		BlendStateDesc m_desc;
	};

	class DF_API NullSamplerState : public ISamplerState
	{
	public:
		explicit NullSamplerState(const SamplerStateDesc& desc) : m_desc(desc) {}
		void* getNativeHandle() const override { return const_cast<NullSamplerState*>(this); }
		const SamplerStateDesc& getDesc() const { return m_desc; }
	private:
		SamplerStateDesc m_desc;
	};

	class DF_API NullSwapChain : public ISwapChain
	{
	public:
		NullSwapChain(uint32_t width, uint32_t height);
		~NullSwapChain() override = default;

		void present(bool vsync) override;
		void resize(uint32_t width, uint32_t height) override;
		std::shared_ptr<IGpuTexture> getBackBufferTexture() override { return m_backBuffer; }
		bool setMaximumFrameLatency(uint32_t) override { return true; }
		bool waitForNextFrame(uint32_t) override { return true; }

		[[nodiscard]] uint32_t getPresentCount() const noexcept { return m_presentCount; }
		[[nodiscard]] uint32_t getWidth() const noexcept { return m_width; }
		[[nodiscard]] uint32_t getHeight() const noexcept { return m_height; }

	private:
		uint32_t m_width = 0;
		uint32_t m_height = 0;
		uint32_t m_presentCount = 0;
		std::shared_ptr<NullTexture> m_backBuffer;
	};

	class DF_API NullDeviceContext : public IDeviceContext
	{
	public:
		NullDeviceContext() = default;
		~NullDeviceContext() override = default;

		void setViewport(const Viewport& viewport) override { m_viewport = viewport; }
		void setRenderTargets(std::span<const std::shared_ptr<IGpuTexture>> rtvTextures, std::shared_ptr<IGpuTexture> dsvTexture) override;
		void clearRenderTarget(const std::shared_ptr<IGpuTexture>& texture, const Color& color) override;
		void clearDepthStencil(const std::shared_ptr<IGpuTexture>& texture, float depth = 0.0f, uint8_t stencil = 0) override;

		void setPrimitiveTopology(PrimitiveTopology topology) override { m_topology = topology; }
		void setInputLayout(const std::shared_ptr<IInputLayout>& layout) override { m_inputLayout = layout; }
		void setVertexBuffer(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t stride, uint32_t offset) override;
		void setIndexBuffer(const std::shared_ptr<IGpuBuffer>& buffer, PixelFormat format, uint32_t offset) override;

		void setVertexShader(const std::shared_ptr<IVertexShader>& vs) override { m_vertexShader = vs; }
		void setPixelShader(const std::shared_ptr<IPixelShader>& ps) override { m_pixelShader = ps; }
		void setComputeShader(IComputeShader* shader) override { m_computeShader = shader; }

		void setConstantBufferVS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) override;
		void setConstantBufferPS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) override;
		void setConstantBufferCS(uint32_t slot, IGpuBuffer* buffer) override { (void)slot; (void)buffer; }
		void setConstantBufferSubrangeVS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t firstConstant16, uint32_t numConstants16) override;
		void setConstantBufferSubrangePS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer, uint32_t firstConstant16, uint32_t numConstants16) override;

		void setShaderResourceVS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture) override;
		void setShaderResourceBufferVS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) override;
		void setShaderResourcePS(uint32_t slot, const std::shared_ptr<IGpuTexture>& texture) override;
		void setShaderResourceBufferPS(uint32_t slot, const std::shared_ptr<IGpuBuffer>& buffer) override;
		void setShaderResourceCS(uint32_t slot, IGpuTexture* texture) override { (void)slot; (void)texture; }
		void setShaderResourceBufferCS(uint32_t slot, IGpuBuffer* buffer) override { (void)slot; (void)buffer; }
		void setUnorderedAccessViewCS(uint32_t slot, IGpuTexture* texture, uint32_t mipSlice = 0) override { (void)slot; (void)texture; (void)mipSlice; }
		void setUnorderedAccessBufferCS(uint32_t slot, IGpuBuffer* buffer) override { (void)slot; (void)buffer; }
		void setSamplerVS(uint32_t slot, const std::shared_ptr<ISamplerState>& sampler) override;
		void setSamplerPS(uint32_t slot, const std::shared_ptr<ISamplerState>& sampler) override;
		void setSamplerCS(uint32_t slot, IGpuSampler* sampler) override { (void)slot; (void)sampler; }

		void setRasterizerState(const std::shared_ptr<IRasterizerState>& state) override { m_rasterizerState = state; }
		void setDepthStencilState(const std::shared_ptr<IDepthStencilState>& state, uint32_t stencilRef = 0) override
		{
			m_depthStencilState = state;
			m_stencilRef = stencilRef;
		}
		void setBlendState(const std::shared_ptr<IBlendState>& state, const float blendFactor[4] = nullptr) override
		{
			m_blendState = state;
			if (blendFactor)
			{
				m_blendFactor[0] = blendFactor[0];
				m_blendFactor[1] = blendFactor[1];
				m_blendFactor[2] = blendFactor[2];
				m_blendFactor[3] = blendFactor[3];
			}
		}

		void draw(uint32_t vertexCount, uint32_t startVertexLocation) override;
		void drawIndexed(uint32_t indexCount, uint32_t startIndexLocation, int32_t baseVertexLocation) override;
		void drawIndexedInstanced(uint32_t indexCountPerInstance, uint32_t instanceCount, uint32_t startIndexLocation, int32_t baseVertexLocation, uint32_t startInstanceLocation) override;
		void drawIndexedInstancedIndirect(const std::shared_ptr<IGpuBuffer>& argumentBuffer, uint32_t offsetBytes) override;
		void dispatch(uint32_t threadGroupCountX, uint32_t threadGroupCountY, uint32_t threadGroupCountZ) override
		{
			(void)threadGroupCountX; (void)threadGroupCountY; (void)threadGroupCountZ;
			m_dispatchCallCount++;
		}

		Result<void*, std::string> map(const std::shared_ptr<IGpuBuffer>& buffer, bool discard) override;
		void unmap(const std::shared_ptr<IGpuBuffer>& buffer) override;
		void updateSubresource(const std::shared_ptr<IGpuBuffer>& buffer, const void* data, size_t size) override;

		[[nodiscard]] uint32_t getDrawCallCount() const noexcept { return m_drawCallCount; }
		[[nodiscard]] uint32_t getDrawIndexedCallCount() const noexcept { return m_drawIndexedCallCount; }
		[[nodiscard]] uint32_t getDispatchCallCount() const noexcept { return m_dispatchCallCount; }
		[[nodiscard]] const Viewport& getViewport() const noexcept { return m_viewport; }

	private:
		Viewport m_viewport{};
		std::vector<std::shared_ptr<IGpuTexture>> m_renderTargets;
		std::shared_ptr<IGpuTexture> m_depthStencilTarget;
		PrimitiveTopology m_topology = PrimitiveTopology::TriangleList;
		std::shared_ptr<IInputLayout> m_inputLayout;
		std::shared_ptr<IVertexShader> m_vertexShader;
		std::shared_ptr<IPixelShader> m_pixelShader;
		IComputeShader* m_computeShader = nullptr;
		std::shared_ptr<IRasterizerState> m_rasterizerState;
		std::shared_ptr<IDepthStencilState> m_depthStencilState;
		std::shared_ptr<IBlendState> m_blendState;
		float m_blendFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
		uint32_t m_stencilRef = 0;
		uint32_t m_drawCallCount = 0;
		uint32_t m_drawIndexedCallCount = 0;
		uint32_t m_dispatchCallCount = 0;
	};

	class DF_API NullRHIDevice : public IRHIDevice
	{
	public:
		NullRHIDevice();
		~NullRHIDevice() override = default;

		std::string_view getBackendName() const override { return "Null"; }
		std::shared_ptr<IDeviceContext> getImmediate() override { return m_immediateContext; }

		Result<std::shared_ptr<IGpuBuffer>, std::string> createBuffer(const GpuBufferDesc& desc, const void* initialData = nullptr) override;
		Result<std::shared_ptr<IGpuTexture>, std::string> createTexture(const GpuTextureDesc& desc, const void* initialData = nullptr) override;

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

		// Hash calculation helpers
		static uint64_t hashRasterizerDesc(const RasterizerStateDesc& desc) noexcept;
		static uint64_t hashDepthStencilDesc(const DepthStencilStateDesc& desc) noexcept;
		static uint64_t hashBlendDesc(const BlendStateDesc& desc) noexcept;
		static uint64_t hashSamplerDesc(const SamplerStateDesc& desc) noexcept;

		// Cache inspection for testing collision handling
		StateCache<RasterizerStateDesc, IRasterizerState>& getRasterizerCache() { return m_rasterizerCache; }
		StateCache<DepthStencilStateDesc, IDepthStencilState>& getDepthStencilCache() { return m_depthStencilCache; }
		StateCache<BlendStateDesc, IBlendState>& getBlendCache() { return m_blendCache; }
		StateCache<SamplerStateDesc, ISamplerState>& getSamplerCache() { return m_samplerCache; }

	private:
		std::shared_ptr<NullDeviceContext> m_immediateContext;
		StateCache<RasterizerStateDesc, IRasterizerState> m_rasterizerCache;
		StateCache<DepthStencilStateDesc, IDepthStencilState> m_depthStencilCache;
		StateCache<BlendStateDesc, IBlendState> m_blendCache;
		StateCache<SamplerStateDesc, ISamplerState> m_samplerCache;
	};
}
