#include "D3D11RHI.h"
#include "D3D11FormatUtils.h"
#include "Engine/Core/Log/Log.h"

#include <d3dcompiler.h>
#include <dxgidebug.h>
#include <algorithm>

namespace Core::RHI
{
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

		// 1. Enumerate adapters via DXGI Factory to find dedicated high-performance discrete GPU
		ComPtr<IDXGIFactory1> dxgiFactory1;
		CreateDXGIFactory1(IID_PPV_ARGS(dxgiFactory1.GetAddressOf()));

		ComPtr<IDXGIAdapter1> selectedAdapter;
		SIZE_T maxDedicatedVram = 0;

		if (dxgiFactory1)
		{
			ComPtr<IDXGIAdapter1> curAdapter;
			for (UINT i = 0; dxgiFactory1->EnumAdapters1(i, curAdapter.ReleaseAndGetAddressOf()) != DXGI_ERROR_NOT_FOUND; ++i)
			{
				DXGI_ADAPTER_DESC1 desc{};
				curAdapter->GetDesc1(&desc);
				if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
				{
					continue;
				}

				char nameBuf[256];
				WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);
				DF_LOG_INFO("RHI", "Found GPU Adapter [{}]: '{}' (Dedicated VRAM: {} MB, Shared: {} MB)",
					i, nameBuf, static_cast<uint32_t>(desc.DedicatedVideoMemory / (1024 * 1024)),
					static_cast<uint32_t>(desc.SharedSystemMemory / (1024 * 1024)));

				if (desc.DedicatedVideoMemory > maxDedicatedVram || !selectedAdapter)
				{
					maxDedicatedVram = desc.DedicatedVideoMemory;
					selectedAdapter = curAdapter;
				}
			}
		}

		HRESULT hr = E_FAIL;
		if (selectedAdapter)
		{
			DXGI_ADAPTER_DESC1 selDesc{};
			selectedAdapter->GetDesc1(&selDesc);
			char selName[256];
			WideCharToMultiByte(CP_UTF8, 0, selDesc.Description, -1, selName, sizeof(selName), nullptr, nullptr);
			DF_LOG_INFO("RHI", "Selecting High-Performance Discrete GPU: '{}' ({} MB VRAM)", selName, static_cast<uint32_t>(selDesc.DedicatedVideoMemory / (1024 * 1024)));

			hr = D3D11CreateDevice(
				selectedAdapter.Get(),
				D3D_DRIVER_TYPE_UNKNOWN, // Must be D3D_DRIVER_TYPE_UNKNOWN when pAdapter != nullptr
				nullptr,
				createDeviceFlags,
				featureLevels,
				static_cast<UINT>(std::size(featureLevels)),
				D3D11_SDK_VERSION,
				m_device.GetAddressOf(),
				&featureLevelOut,
				context.GetAddressOf());
		}

		// Fallback to default hardware adapter if selected adapter failed
		if (FAILED(hr))
		{
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
		}

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
			if (selectedAdapter)
			{
				hr = D3D11CreateDevice(
					selectedAdapter.Get(),
					D3D_DRIVER_TYPE_UNKNOWN,
					nullptr,
					createDeviceFlags,
					featureLevels,
					static_cast<UINT>(std::size(featureLevels)),
					D3D11_SDK_VERSION,
					m_device.GetAddressOf(),
					&featureLevelOut,
					context.GetAddressOf());
			}

			if (FAILED(hr))
			{
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
			}

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

		// Obtain DXGI Factory & active adapter description
		ComPtr<IDXGIDevice> dxgiDevice;
		if (SUCCEEDED(m_device.As(&dxgiDevice)))
		{
			ComPtr<IDXGIAdapter> adapter;
			if (SUCCEEDED(dxgiDevice->GetAdapter(adapter.GetAddressOf())))
			{
				DXGI_ADAPTER_DESC desc{};
				adapter->GetDesc(&desc);
				char nameBuf[256];
				WideCharToMultiByte(CP_UTF8, 0, desc.Description, -1, nameBuf, sizeof(nameBuf), nullptr, nullptr);
				m_adapterDescription = nameBuf;
				m_dedicatedVramMB = static_cast<uint32_t>(desc.DedicatedVideoMemory / (1024 * 1024));
				adapter->GetParent(IID_PPV_ARGS(m_dxgiFactory.GetAddressOf()));
			}
		}

		DF_LOG_INFO("RHI", "Active GPU: '{}' (Dedicated VRAM: {} MB, Feature Level {})",
			m_adapterDescription, m_dedicatedVramMB,
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
		bDesc.Usage = ToD3D11Usage(desc.usage);
		bDesc.BindFlags = ToD3D11BindFlags(desc.bindFlags);
		bDesc.CPUAccessFlags = (desc.usage == BufferUsage::Dynamic) ? D3D11_CPU_ACCESS_WRITE : 0;
		bDesc.StructureByteStride = static_cast<UINT>(desc.structureByteStride);
		if (desc.structureByteStride > 0)
		{
			bDesc.MiscFlags |= D3D11_RESOURCE_MISC_BUFFER_STRUCTURED;
			bDesc.BindFlags |= D3D11_BIND_SHADER_RESOURCE;
		}
		if (hasFlag(desc.bindFlags, BufferBindFlags::UnorderedAccess))
		{
			bDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		}
		if (hasFlag(desc.bindFlags, BufferBindFlags::IndirectDrawArgs))
		{
			bDesc.MiscFlags |= D3D11_RESOURCE_MISC_DRAWINDIRECT_ARGS;
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

		ComPtr<ID3D11UnorderedAccessView> uav;
		if (hasFlag(desc.bindFlags, BufferBindFlags::UnorderedAccess))
		{
			D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
			uavDesc.ViewDimension = D3D11_UAV_DIMENSION_BUFFER;
			uavDesc.Buffer.FirstElement = 0;
			if (desc.structureByteStride > 0)
			{
				uavDesc.Format = DXGI_FORMAT_UNKNOWN;
				uavDesc.Buffer.NumElements = static_cast<UINT>(desc.size / desc.structureByteStride);
				uavDesc.Buffer.Flags = 0;
			}
			else
			{
				uavDesc.Format = DXGI_FORMAT_R32_UINT;
				uavDesc.Buffer.NumElements = static_cast<UINT>(desc.size / sizeof(uint32_t));
				uavDesc.Buffer.Flags = 0;
			}
			const HRESULT uavHr = m_device->CreateUnorderedAccessView(buffer.Get(), &uavDesc, uav.GetAddressOf());
			if (FAILED(uavHr))
			{
				DF_LOG_ERROR("RHI", "Failed to create UAV for buffer: HRESULT 0x{:08X}", uavHr);
			}
		}

		return Result<std::shared_ptr<IGpuBuffer>, std::string>::ok(std::make_shared<D3D11Buffer>(desc, buffer, srv, uav));
	}

	Result<std::shared_ptr<IGpuTexture>, std::string> D3D11RHIDevice::createTexture(const GpuTextureDesc& desc, const void* initialData)
	{
		if (initialData)
		{
			SubresourceData sub{};
			sub.data = initialData;
			sub.rowPitch = desc.width * GetFormatBytesPerPixel(desc.format);
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

		if (desc.depth > 1)
		{
			D3D11_TEXTURE3D_DESC t3Desc{};
			t3Desc.Width = desc.width;
			t3Desc.Height = desc.height;
			t3Desc.Depth = desc.depth;
			t3Desc.MipLevels = desc.mipLevels;
			t3Desc.Format = ToDXGIFormat(desc.format);
			t3Desc.Usage = ToD3D11Usage(desc.usage);
			t3Desc.BindFlags = ToD3D11BindFlags(desc.bindFlags);
			t3Desc.CPUAccessFlags = (desc.usage == BufferUsage::Dynamic) ? D3D11_CPU_ACCESS_WRITE : 0;

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

			ComPtr<ID3D11Texture3D> texture3D;
			const HRESULT hr = m_device->CreateTexture3D(&t3Desc, d3dSub.empty() ? nullptr : d3dSub.data(), texture3D.GetAddressOf());
			if (FAILED(hr))
			{
				return Result<std::shared_ptr<IGpuTexture>, std::string>::error("CreateTexture3D failed with HRESULT " + std::to_string(hr));
			}

			ComPtr<ID3D11ShaderResourceView> srv;
			if (desc.bindFlags & BufferBindFlags::ShaderResource)
			{
				D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
				srvDesc.Format = ToDXGIFormat(desc.format);
				srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE3D;
				srvDesc.Texture3D.MostDetailedMip = 0;
				srvDesc.Texture3D.MipLevels = (desc.mipLevels > 0) ? desc.mipLevels : static_cast<UINT>(-1);
				m_device->CreateShaderResourceView(texture3D.Get(), &srvDesc, srv.GetAddressOf());
			}

			return Result<std::shared_ptr<IGpuTexture>, std::string>::ok(
				std::make_shared<D3D11Texture>(desc, texture3D, srv, nullptr, nullptr)
			);
		}

		D3D11_TEXTURE2D_DESC tDesc{};
		tDesc.Width = desc.width;
		tDesc.Height = desc.height;
		tDesc.MipLevels = desc.mipLevels;
		tDesc.ArraySize = desc.arraySize;
		tDesc.SampleDesc.Count = desc.sampleCount;
		tDesc.SampleDesc.Quality = 0;
		tDesc.Usage = ToD3D11Usage(desc.usage);
		tDesc.BindFlags = ToD3D11BindFlags(desc.bindFlags);
		if (hasFlag(desc.bindFlags, BufferBindFlags::UnorderedAccess))
		{
			tDesc.BindFlags |= D3D11_BIND_UNORDERED_ACCESS;
		}
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
			// Depth buffers typeless (e.g. R32_TYPELESS with D32_FLOAT DSV and R32_FLOAT SRV)
			tDesc.Format = ToTypicalTypeless(desc.format);
		}
		else
		{
			tDesc.Format = ToDXGIFormat(desc.format);
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
			srvDesc.Format = isDepth ? ToDepthSRVFormat(desc.format) : ToDXGIFormat(desc.format);

			m_device->CreateShaderResourceView(texture.Get(), &srvDesc, srv.GetAddressOf());
		}

		ComPtr<ID3D11RenderTargetView> rtv;
		if (desc.bindFlags & BufferBindFlags::RenderTarget)
		{
			D3D11_RENDER_TARGET_VIEW_DESC rtvDesc{};
			rtvDesc.Format = ToDXGIFormat(desc.format);
			rtvDesc.ViewDimension = D3D11_RTV_DIMENSION_TEXTURE2D;
			m_device->CreateRenderTargetView(texture.Get(), &rtvDesc, rtv.GetAddressOf());
		}

		ComPtr<ID3D11DepthStencilView> dsv;
		if (isDepth)
		{
			D3D11_DEPTH_STENCIL_VIEW_DESC dsvDesc{};
			dsvDesc.Format = ToDSVFormat(desc.format);
			dsvDesc.ViewDimension = D3D11_DSV_DIMENSION_TEXTURE2D;
			m_device->CreateDepthStencilView(texture.Get(), &dsvDesc, dsv.GetAddressOf());
		}

		ComPtr<ID3D11UnorderedAccessView> uav;
		std::vector<ComPtr<ID3D11UnorderedAccessView>> mipUAVs;
		if (hasFlag(desc.bindFlags, BufferBindFlags::UnorderedAccess))
		{
			const uint32_t numMips = (desc.mipLevels > 0) ? desc.mipLevels : 1;
			mipUAVs.resize(numMips);
			for (uint32_t mip = 0; mip < numMips; ++mip)
			{
				D3D11_UNORDERED_ACCESS_VIEW_DESC uavDesc{};
				uavDesc.Format = ToDXGIFormat(desc.format);
				uavDesc.ViewDimension = D3D11_UAV_DIMENSION_TEXTURE2D;
				uavDesc.Texture2D.MipSlice = mip;
				const HRESULT uavHr = m_device->CreateUnorderedAccessView(texture.Get(), &uavDesc, mipUAVs[mip].GetAddressOf());
				if (FAILED(uavHr))
				{
					DF_LOG_ERROR("RHI", "Failed to create UAV for texture mip {}: HRESULT 0x{:08X}", mip, uavHr);
				}
			}
			if (!mipUAVs.empty())
			{
				uav = mipUAVs[0];
			}
		}

		return Result<std::shared_ptr<IGpuTexture>, std::string>::ok(
			std::make_shared<D3D11Texture>(desc, texture, srv, rtv, dsv, uav, std::move(mipUAVs)));
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

	Result<std::shared_ptr<IHullShader>, std::string> D3D11RHIDevice::createHullShader(std::span<const uint8_t> bytecode)
	{
		if (!m_device || bytecode.empty())
		{
			return Result<std::shared_ptr<IHullShader>, std::string>::error("Invalid device or bytecode");
		}

		ComPtr<ID3D11HullShader> hs;
		const HRESULT hr = m_device->CreateHullShader(bytecode.data(), bytecode.size(), nullptr, hs.GetAddressOf());
		if (FAILED(hr))
		{
			return Result<std::shared_ptr<IHullShader>, std::string>::error("CreateHullShader failed with HRESULT " + std::to_string(hr));
		}

		return Result<std::shared_ptr<IHullShader>, std::string>::ok(
			std::make_shared<D3D11HullShader>(hs, bytecode));
	}

	Result<std::shared_ptr<IDomainShader>, std::string> D3D11RHIDevice::createDomainShader(std::span<const uint8_t> bytecode)
	{
		if (!m_device || bytecode.empty())
		{
			return Result<std::shared_ptr<IDomainShader>, std::string>::error("Invalid device or bytecode");
		}

		ComPtr<ID3D11DomainShader> ds;
		const HRESULT hr = m_device->CreateDomainShader(bytecode.data(), bytecode.size(), nullptr, ds.GetAddressOf());
		if (FAILED(hr))
		{
			return Result<std::shared_ptr<IDomainShader>, std::string>::error("CreateDomainShader failed with HRESULT " + std::to_string(hr));
		}

		return Result<std::shared_ptr<IDomainShader>, std::string>::ok(
			std::make_shared<D3D11DomainShader>(ds, bytecode));
	}

	Result<std::shared_ptr<IComputeShader>, std::string> D3D11RHIDevice::createComputeShader(std::span<const uint8_t> bytecode)
	{
		if (!m_device || bytecode.empty())
		{
			return Result<std::shared_ptr<IComputeShader>, std::string>::error("Invalid device or bytecode");
		}

		ComPtr<ID3D11ComputeShader> cs;
		const HRESULT hr = m_device->CreateComputeShader(bytecode.data(), bytecode.size(), nullptr, cs.GetAddressOf());
		if (FAILED(hr))
		{
			return Result<std::shared_ptr<IComputeShader>, std::string>::error("CreateComputeShader failed with HRESULT " + std::to_string(hr));
		}

		return Result<std::shared_ptr<IComputeShader>, std::string>::ok(
			std::make_shared<D3D11ComputeShader>(cs, bytecode));
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
			d.Format = ToDXGIFormat(elem.format);
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
			rDesc.CullMode = ToD3D11CullMode(desc.cullMode);
			rDesc.FillMode = ToD3D11FillMode(desc.fillMode);
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
			dDesc.DepthFunc = ToD3D11Comparison(desc.depthFunc);
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
			bDesc.RenderTarget[0].SrcBlend = ToD3D11BlendFactor(desc.srcBlend);
			bDesc.RenderTarget[0].DestBlend = ToD3D11BlendFactor(desc.destBlend);
			bDesc.RenderTarget[0].BlendOp = ToD3D11BlendOp(desc.blendOp);
			bDesc.RenderTarget[0].SrcBlendAlpha = ToD3D11BlendFactor(desc.srcBlendAlpha);
			bDesc.RenderTarget[0].DestBlendAlpha = ToD3D11BlendFactor(desc.destBlendAlpha);
			bDesc.RenderTarget[0].BlendOpAlpha = ToD3D11BlendOp(desc.blendOpAlpha);
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
			bool isComparison = (desc.comparisonFunc != ComparisonFunc::Never);
			sDesc.Filter = ToD3D11Filter(desc.filter, isComparison);
			sDesc.AddressU = ToD3D11Address(desc.addressU);
			sDesc.AddressV = ToD3D11Address(desc.addressV);
			sDesc.AddressW = ToD3D11Address(desc.addressW);
			sDesc.ComparisonFunc = ToD3D11Comparison(desc.comparisonFunc);
			sDesc.MinLOD = desc.minLOD;
			sDesc.MaxLOD = desc.maxLOD;
			sDesc.BorderColor[0] = desc.borderColor[0];
			sDesc.BorderColor[1] = desc.borderColor[1];
			sDesc.BorderColor[2] = desc.borderColor[2];
			sDesc.BorderColor[3] = desc.borderColor[3];
			sDesc.MaxAnisotropy = std::clamp(desc.maxAnisotropy, 1u, 16u);

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

		BOOL allowTearing = FALSE;
		ComPtr<IDXGIFactory5> factory5;
		if (SUCCEEDED(m_dxgiFactory.As(&factory5)))
		{
			factory5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING, &allowTearing, sizeof(allowTearing));
		}

		DXGI_SWAP_CHAIN_DESC1 desc{};
		desc.Width = width;
		desc.Height = height;
		desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		desc.Stereo = FALSE;
		desc.SampleDesc.Count = 1;
		desc.SampleDesc.Quality = 0;
		desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		desc.BufferCount = 3; // Triple buffering for smooth non-blocking flip presentation
		desc.Scaling = DXGI_SCALING_STRETCH;
		desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		desc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
		desc.Flags = allowTearing ? DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING : 0;

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
			std::make_unique<D3D11SwapChain>(swapChain1, backBuffer, hwnd, allowTearing != FALSE));
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

	std::shared_ptr<IRHIDevice> createD3D11Device(bool enableDebugLayer)
	{
		auto device = std::make_shared<D3D11RHIDevice>(enableDebugLayer);
		if (device->initialize())
		{
			return device;
		}
		return nullptr;
	}
}
