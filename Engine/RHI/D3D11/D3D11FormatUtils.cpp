#include "D3D11FormatUtils.h"
#include "D3D11RHI.h"

namespace Core::RHI
{
	DXGI_FORMAT ToDXGIFormat(PixelFormat format) noexcept
	{
		switch (format)
		{
			case PixelFormat::R8_UNORM: return DXGI_FORMAT_R8_UNORM;
			case PixelFormat::R8G8B8A8_UNORM: return DXGI_FORMAT_R8G8B8A8_UNORM;
			case PixelFormat::R8G8B8A8_UNORM_SRGB: return DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
			case PixelFormat::R8G8B8A8_SNORM: return DXGI_FORMAT_R8G8B8A8_SNORM;
			case PixelFormat::R16G16_FLOAT: return DXGI_FORMAT_R16G16_FLOAT;
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
			case PixelFormat::BC1_UNORM: return DXGI_FORMAT_BC1_UNORM;
			case PixelFormat::BC1_UNORM_SRGB: return DXGI_FORMAT_BC1_UNORM_SRGB;
			case PixelFormat::BC3_UNORM: return DXGI_FORMAT_BC3_UNORM;
			case PixelFormat::BC3_UNORM_SRGB: return DXGI_FORMAT_BC3_UNORM_SRGB;
			case PixelFormat::BC4_UNORM: return DXGI_FORMAT_BC4_UNORM;
			case PixelFormat::BC5_UNORM: return DXGI_FORMAT_BC5_UNORM;
			case PixelFormat::BC7_UNORM: return DXGI_FORMAT_BC7_UNORM;
			case PixelFormat::BC7_UNORM_SRGB: return DXGI_FORMAT_BC7_UNORM_SRGB;
			default: return DXGI_FORMAT_UNKNOWN;
		}
	}

	DXGI_FORMAT ToTypicalTypeless(PixelFormat format) noexcept
	{
		switch (format)
		{
			case PixelFormat::D32_FLOAT:
			case PixelFormat::R32_FLOAT:
			case PixelFormat::R32_TYPELESS:
				return DXGI_FORMAT_R32_TYPELESS;
			case PixelFormat::D24_UNORM_S8_UINT:
				return DXGI_FORMAT_R24G8_TYPELESS;
			case PixelFormat::R8G8B8A8_UNORM:
			case PixelFormat::R8G8B8A8_UNORM_SRGB:
			case PixelFormat::R8G8B8A8_SNORM:
				return DXGI_FORMAT_R8G8B8A8_TYPELESS;
			case PixelFormat::R16G16_FLOAT:
				return DXGI_FORMAT_R16G16_TYPELESS;
			case PixelFormat::R16G16B16A16_FLOAT:
				return DXGI_FORMAT_R16G16B16A16_TYPELESS;
			case PixelFormat::R32G32B32A32_FLOAT:
				return DXGI_FORMAT_R32G32B32A32_TYPELESS;
			default:
				return ToDXGIFormat(format);
		}
	}

	DXGI_FORMAT ToDepthSRVFormat(PixelFormat format) noexcept
	{
		switch (format)
		{
			case PixelFormat::D32_FLOAT:
			case PixelFormat::R32_FLOAT:
			case PixelFormat::R32_TYPELESS:
				return DXGI_FORMAT_R32_FLOAT;
			case PixelFormat::D24_UNORM_S8_UINT:
				return DXGI_FORMAT_R24_UNORM_X8_TYPELESS;
			default:
				return ToDXGIFormat(format);
		}
	}

	DXGI_FORMAT ToDSVFormat(PixelFormat format) noexcept
	{
		switch (format)
		{
			case PixelFormat::D32_FLOAT:
			case PixelFormat::R32_FLOAT:
			case PixelFormat::R32_TYPELESS:
				return DXGI_FORMAT_D32_FLOAT;
			case PixelFormat::D24_UNORM_S8_UINT:
				return DXGI_FORMAT_D24_UNORM_S8_UINT;
			default:
				return ToDXGIFormat(format);
		}
	}

	uint32_t GetFormatBytesPerPixel(PixelFormat format) noexcept
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
			case PixelFormat::R16G16_FLOAT: return 4;
			case PixelFormat::R32G32_FLOAT:
			case PixelFormat::R16G16B16A16_FLOAT: return 8;
			case PixelFormat::BC1_UNORM:
			case PixelFormat::BC1_UNORM_SRGB:
			case PixelFormat::BC4_UNORM: return 1;
			case PixelFormat::BC3_UNORM:
			case PixelFormat::BC3_UNORM_SRGB:
			case PixelFormat::BC5_UNORM:
			case PixelFormat::BC7_UNORM:
			case PixelFormat::BC7_UNORM_SRGB: return 1;
			case PixelFormat::R32G32B32_FLOAT: return 12;
			case PixelFormat::R32G32B32A32_FLOAT: return 16;
			default: return 4;
		}
	}

	D3D11_USAGE ToD3D11Usage(BufferUsage usage) noexcept
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

	UINT ToD3D11BindFlags(BufferBindFlags flags) noexcept
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

	D3D11_PRIMITIVE_TOPOLOGY ToD3D11Topology(PrimitiveTopology topo) noexcept
	{
		switch (topo)
		{
			case PrimitiveTopology::PointList: return D3D11_PRIMITIVE_TOPOLOGY_POINTLIST;
			case PrimitiveTopology::LineList: return D3D11_PRIMITIVE_TOPOLOGY_LINELIST;
			case PrimitiveTopology::LineStrip: return D3D11_PRIMITIVE_TOPOLOGY_LINESTRIP;
			case PrimitiveTopology::TriangleList: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
			case PrimitiveTopology::TriangleStrip: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP;
			case PrimitiveTopology::PatchList3ControlPoints: return D3D11_PRIMITIVE_TOPOLOGY_3_CONTROL_POINT_PATCHLIST;
			default: return D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		}
	}

	D3D11_CULL_MODE ToD3D11CullMode(CullMode mode) noexcept
	{
		switch (mode)
		{
			case CullMode::None: return D3D11_CULL_NONE;
			case CullMode::Front: return D3D11_CULL_FRONT;
			case CullMode::Back: return D3D11_CULL_BACK;
			default: return D3D11_CULL_BACK;
		}
	}

	D3D11_FILL_MODE ToD3D11FillMode(FillMode mode) noexcept
	{
		switch (mode)
		{
			case FillMode::Solid: return D3D11_FILL_SOLID;
			case FillMode::Wireframe: return D3D11_FILL_WIREFRAME;
			default: return D3D11_FILL_SOLID;
		}
	}

	D3D11_COMPARISON_FUNC ToD3D11Comparison(ComparisonFunc func) noexcept
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

	D3D11_BLEND ToD3D11BlendFactor(BlendFactor factor) noexcept
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

	D3D11_BLEND_OP ToD3D11BlendOp(BlendOp op) noexcept
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

	D3D11_FILTER ToD3D11Filter(FilterMode filter, bool isComparison) noexcept
	{
		if (isComparison)
		{
			switch (filter)
			{
				case FilterMode::Point: return D3D11_FILTER_COMPARISON_MIN_MAG_MIP_POINT;
				case FilterMode::Linear: return D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
				case FilterMode::Anisotropic: return D3D11_FILTER_COMPARISON_ANISOTROPIC;
				default: return D3D11_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT;
			}
		}
		switch (filter)
		{
			case FilterMode::Point: return D3D11_FILTER_MIN_MAG_MIP_POINT;
			case FilterMode::Linear: return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
			case FilterMode::Anisotropic: return D3D11_FILTER_ANISOTROPIC;
			default: return D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		}
	}

	D3D11_TEXTURE_ADDRESS_MODE ToD3D11Address(AddressMode mode) noexcept
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

	// Resource constructors and destructors
	D3D11Buffer::D3D11Buffer(const GpuBufferDesc& desc, ComPtr<ID3D11Buffer> buffer, ComPtr<ID3D11ShaderResourceView> srv)
		: m_desc(desc), m_buffer(std::move(buffer)), m_srv(std::move(srv)) {}
	D3D11Buffer::~D3D11Buffer() = default;

	D3D11Texture::D3D11Texture(
		const GpuTextureDesc& desc,
		ComPtr<ID3D11Resource> resource,
		ComPtr<ID3D11ShaderResourceView> srv,
		ComPtr<ID3D11RenderTargetView> rtv,
		ComPtr<ID3D11DepthStencilView> dsv)
		: m_desc(desc)
		, m_resource(std::move(resource))
		, m_srv(std::move(srv))
		, m_rtv(std::move(rtv))
		, m_dsv(std::move(dsv)) {}
	D3D11Texture::~D3D11Texture() = default;

	D3D11VertexShader::D3D11VertexShader(ComPtr<ID3D11VertexShader> vs, std::span<const uint8_t> bytecode)
		: m_vs(std::move(vs)), m_bytecode(bytecode.begin(), bytecode.end()) {}
	D3D11VertexShader::~D3D11VertexShader() = default;

	D3D11PixelShader::D3D11PixelShader(ComPtr<ID3D11PixelShader> ps, std::span<const uint8_t> bytecode)
		: m_ps(std::move(ps)), m_bytecode(bytecode.begin(), bytecode.end()) {}
	D3D11PixelShader::~D3D11PixelShader() = default;

	D3D11HullShader::D3D11HullShader(ComPtr<ID3D11HullShader> hs, std::span<const uint8_t> bytecode)
		: m_hs(std::move(hs)), m_bytecode(bytecode.begin(), bytecode.end()) {}
	D3D11HullShader::~D3D11HullShader() = default;

	D3D11DomainShader::D3D11DomainShader(ComPtr<ID3D11DomainShader> ds, std::span<const uint8_t> bytecode)
		: m_ds(std::move(ds)), m_bytecode(bytecode.begin(), bytecode.end()) {}
	D3D11DomainShader::~D3D11DomainShader() = default;

	D3D11ComputeShader::D3D11ComputeShader(ComPtr<ID3D11ComputeShader> cs, std::span<const uint8_t> bytecode)
		: m_cs(std::move(cs)), m_bytecode(bytecode.begin(), bytecode.end()) {}
	D3D11ComputeShader::~D3D11ComputeShader() = default;

	D3D11InputLayout::D3D11InputLayout(ComPtr<ID3D11InputLayout> layout)
		: m_layout(std::move(layout)) {}
	D3D11InputLayout::~D3D11InputLayout() = default;

	D3D11RasterizerState::D3D11RasterizerState(const RasterizerStateDesc& desc, ComPtr<ID3D11RasterizerState> state)
		: m_desc(desc), m_state(std::move(state)) {}
	D3D11RasterizerState::~D3D11RasterizerState() = default;

	D3D11DepthStencilState::D3D11DepthStencilState(const DepthStencilStateDesc& desc, ComPtr<ID3D11DepthStencilState> state)
		: m_desc(desc), m_state(std::move(state)) {}
	D3D11DepthStencilState::~D3D11DepthStencilState() = default;

	D3D11BlendState::D3D11BlendState(const BlendStateDesc& desc, ComPtr<ID3D11BlendState> state)
		: m_desc(desc), m_state(std::move(state)) {}
	D3D11BlendState::~D3D11BlendState() = default;

	D3D11SamplerState::D3D11SamplerState(const SamplerStateDesc& desc, ComPtr<ID3D11SamplerState> state)
		: m_desc(desc), m_state(std::move(state)) {}
	D3D11SamplerState::~D3D11SamplerState() = default;
}
