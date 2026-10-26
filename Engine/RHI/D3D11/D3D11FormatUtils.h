#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/RHI/RHIDefines.h"

#include <d3d11_1.h>
#include <dxgi1_4.h>
#include <cstdint>

namespace Core::RHI
{
	class D3D11Buffer;
	class D3D11Texture;
	using D3D11GpuBuffer = D3D11Buffer;
	using D3D11GpuTexture = D3D11Texture;

	// Format conversions
	DXGI_FORMAT ToDXGIFormat(PixelFormat format) noexcept;
	inline DXGI_FORMAT ToDxgiFormat(PixelFormat format) noexcept { return ToDXGIFormat(format); }

	DXGI_FORMAT ToTypicalTypeless(PixelFormat format) noexcept;
	DXGI_FORMAT ToDepthSRVFormat(PixelFormat format) noexcept;
	DXGI_FORMAT ToDSVFormat(PixelFormat format) noexcept;
	uint32_t GetFormatBytesPerPixel(PixelFormat format) noexcept;
	inline uint32_t GetBytesPerPixel(PixelFormat format) noexcept { return GetFormatBytesPerPixel(format); }

	// Usage and Bind flags
	D3D11_USAGE ToD3D11Usage(BufferUsage usage) noexcept;
	inline D3D11_USAGE ToDxgiUsage(BufferUsage usage) noexcept { return ToD3D11Usage(usage); }

	UINT ToD3D11BindFlags(BufferBindFlags flags) noexcept;
	inline UINT ToDxgiBindFlags(BufferBindFlags flags) noexcept { return ToD3D11BindFlags(flags); }

	// Topology
	D3D11_PRIMITIVE_TOPOLOGY ToD3D11Topology(PrimitiveTopology topo) noexcept;
	inline D3D11_PRIMITIVE_TOPOLOGY ToDxgiTopology(PrimitiveTopology topo) noexcept { return ToD3D11Topology(topo); }

	// Pipeline states
	D3D11_CULL_MODE ToD3D11CullMode(CullMode mode) noexcept;
	inline D3D11_CULL_MODE ToDxgiCullMode(CullMode mode) noexcept { return ToD3D11CullMode(mode); }

	D3D11_FILL_MODE ToD3D11FillMode(FillMode mode) noexcept;
	inline D3D11_FILL_MODE ToDxgiFillMode(FillMode mode) noexcept { return ToD3D11FillMode(mode); }

	D3D11_COMPARISON_FUNC ToD3D11Comparison(ComparisonFunc func) noexcept;
	inline D3D11_COMPARISON_FUNC ToDxgiComparison(ComparisonFunc func) noexcept { return ToD3D11Comparison(func); }

	D3D11_BLEND ToD3D11BlendFactor(BlendFactor factor) noexcept;
	inline D3D11_BLEND ToDxgiBlendFactor(BlendFactor factor) noexcept { return ToD3D11BlendFactor(factor); }

	D3D11_BLEND_OP ToD3D11BlendOp(BlendOp op) noexcept;
	inline D3D11_BLEND_OP ToDxgiBlendOp(BlendOp op) noexcept { return ToD3D11BlendOp(op); }

	D3D11_FILTER ToD3D11Filter(FilterMode filter, bool isComparison = false) noexcept;
	inline D3D11_FILTER ToDxgiFilter(FilterMode filter, bool isComparison = false) noexcept { return ToD3D11Filter(filter, isComparison); }

	D3D11_TEXTURE_ADDRESS_MODE ToD3D11Address(AddressMode mode) noexcept;
	inline D3D11_TEXTURE_ADDRESS_MODE ToDxgiAddress(AddressMode mode) noexcept { return ToD3D11Address(mode); }
}
