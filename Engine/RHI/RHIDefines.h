#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include <cstdint>
#include <string>
#include <vector>

namespace Core::RHI
{
	enum class PixelFormat : uint8_t
	{
		Unknown = 0,
		R8_UNORM,
		R8G8B8A8_UNORM,
		R8G8B8A8_UNORM_SRGB,
		R8G8B8A8_SNORM,
		R16G16B16A16_FLOAT,
		R32G32B32A32_FLOAT,
		R32G32B32_FLOAT,
		R32G32_FLOAT,
		R32_FLOAT,
		R32_TYPELESS,
		D32_FLOAT,
		D24_UNORM_S8_UINT,
		R11G11B10_FLOAT,
		R32_UINT
	};

	enum class PrimitiveTopology : uint8_t
	{
		PointList = 0,
		LineList,
		LineStrip,
		TriangleList,
		TriangleStrip
	};

	enum class BufferUsage : uint8_t
	{
		Default = 0,
		Immutable,
		Dynamic,
		Staging
	};

	enum class BufferBindFlags : uint32_t
	{
		None = 0,
		VertexBuffer = 1 << 0,
		IndexBuffer = 1 << 1,
		ConstantBuffer = 1 << 2,
		ShaderResource = 1 << 3,
		RenderTarget = 1 << 4,
		DepthStencil = 1 << 5,
		UnorderedAccess = 1 << 6
	};

	inline BufferBindFlags operator|(BufferBindFlags a, BufferBindFlags b) noexcept
	{
		return static_cast<BufferBindFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
	}

	inline bool operator&(BufferBindFlags a, BufferBindFlags b) noexcept
	{
		return (static_cast<uint32_t>(a) & static_cast<uint32_t>(b)) != 0;
	}

	enum class CullMode : uint8_t
	{
		None = 0,
		Front,
		Back
	};

	enum class FillMode : uint8_t
	{
		Solid = 0,
		Wireframe
	};

	enum class ComparisonFunc : uint8_t
	{
		Never = 0,
		Less,
		Equal,
		LessEqual,
		Greater,
		NotEqual,
		GreaterEqual, // Default for Reversed-Z
		Always
	};

	enum class FilterMode : uint8_t
	{
		Point = 0,
		Linear,
		Anisotropic
	};

	enum class AddressMode : uint8_t
	{
		Wrap = 0,
		Clamp,
		Mirror,
		Border
	};

	enum class BlendFactor : uint8_t
	{
		Zero = 0,
		One,
		SrcColor,
		InvSrcColor,
		SrcAlpha,
		InvSrcAlpha,
		DestAlpha,
		InvDestAlpha,
		DestColor,
		InvDestColor
	};

	enum class BlendOp : uint8_t
	{
		Add = 0,
		Subtract,
		RevSubtract,
		Min,
		Max
	};

	struct DF_API GpuBufferDesc
	{
		size_t size = 0;
		BufferUsage usage = BufferUsage::Default;
		BufferBindFlags bindFlags = BufferBindFlags::None;
		size_t structureByteStride = 0;
	};

	struct DF_API GpuTextureDesc
	{
		uint32_t width = 1;
		uint32_t height = 1;
		uint32_t depth = 1;
		uint32_t mipLevels = 1;
		uint32_t arraySize = 1;
		PixelFormat format = PixelFormat::R8G8B8A8_UNORM;
		BufferBindFlags bindFlags = BufferBindFlags::ShaderResource;
		BufferUsage usage = BufferUsage::Default;
		uint32_t sampleCount = 1;
	};

	struct DF_API SubresourceData
	{
		const void* data = nullptr;
		uint32_t rowPitch = 0;
		uint32_t slicePitch = 0;
	};

	struct DF_API RasterizerStateDesc
	{
		CullMode cullMode = CullMode::Back;
		FillMode fillMode = FillMode::Solid;
		bool frontCounterClockwise = false;
		int depthBias = 0;
		float slopeScaledDepthBias = 0.0f;
		bool depthClipEnable = true;
		bool scissorEnable = false;

		bool operator==(const RasterizerStateDesc& o) const noexcept
		{
			return cullMode == o.cullMode &&
				fillMode == o.fillMode &&
				frontCounterClockwise == o.frontCounterClockwise &&
				depthBias == o.depthBias &&
				slopeScaledDepthBias == o.slopeScaledDepthBias &&
				depthClipEnable == o.depthClipEnable &&
				scissorEnable == o.scissorEnable;
		}
	};

	struct DF_API DepthStencilStateDesc
	{
		bool depthEnable = true;
		bool depthWrite = true;
		ComparisonFunc depthFunc = ComparisonFunc::GreaterEqual; // Reversed-Z
		bool stencilEnable = false;

		bool operator==(const DepthStencilStateDesc& o) const noexcept
		{
			return depthEnable == o.depthEnable &&
				depthWrite == o.depthWrite &&
				depthFunc == o.depthFunc &&
				stencilEnable == o.stencilEnable;
		}
	};

	struct DF_API BlendStateDesc
	{
		bool blendEnable = false;
		BlendFactor srcBlend = BlendFactor::SrcAlpha;
		BlendFactor destBlend = BlendFactor::InvSrcAlpha;
		BlendOp blendOp = BlendOp::Add;
		BlendFactor srcBlendAlpha = BlendFactor::One;
		BlendFactor destBlendAlpha = BlendFactor::Zero;
		BlendOp blendOpAlpha = BlendOp::Add;
		uint8_t renderTargetWriteMask = 0x0F; // RGBA

		bool operator==(const BlendStateDesc& o) const noexcept
		{
			return blendEnable == o.blendEnable &&
				srcBlend == o.srcBlend &&
				destBlend == o.destBlend &&
				blendOp == o.blendOp &&
				srcBlendAlpha == o.srcBlendAlpha &&
				destBlendAlpha == o.destBlendAlpha &&
				blendOpAlpha == o.blendOpAlpha &&
				renderTargetWriteMask == o.renderTargetWriteMask;
		}
	};

	struct DF_API SamplerStateDesc
	{
		FilterMode filter = FilterMode::Linear;
		AddressMode addressU = AddressMode::Wrap;
		AddressMode addressV = AddressMode::Wrap;
		AddressMode addressW = AddressMode::Wrap;
		ComparisonFunc comparisonFunc = ComparisonFunc::Never;
		float minLOD = 0.0f;
		float maxLOD = 3.402823466e+38F;

		bool operator==(const SamplerStateDesc& o) const noexcept
		{
			return filter == o.filter &&
				addressU == o.addressU &&
				addressV == o.addressV &&
				addressW == o.addressW &&
				comparisonFunc == o.comparisonFunc &&
				minLOD == o.minLOD &&
				maxLOD == o.maxLOD;
		}
	};

	struct DF_API InputElementDesc
	{
		std::string semanticName;
		uint32_t semanticIndex = 0;
		PixelFormat format = PixelFormat::R32G32B32_FLOAT;
		uint32_t inputSlot = 0;
		uint32_t alignedByteOffset = 0xFFFFFFFF; // Auto-compute offset
		bool isPerInstance = false;
	};

	struct DF_API Viewport
	{
		float x = 0.0f;
		float y = 0.0f;
		float width = 0.0f;
		float height = 0.0f;
		float minDepth = 0.0f;
		float maxDepth = 1.0f;
	};
}
