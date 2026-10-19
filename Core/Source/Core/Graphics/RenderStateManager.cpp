#include "RenderStateManager.h"
#include "Core/Log/Log.h"

namespace Core
{
	RenderStateManager::RenderStateManager(ID3D11Device* device, ID3D11DeviceContext* context)
		: m_device(device), m_context(context)
	{
		initRasterizerStates();
		initBlendStates();
		initDepthStencilStates();
		initSamplerStates();
		m_initialized = true;
	}

	void RenderStateManager::initRasterizerStates()
	{
		D3D11_RASTERIZER_DESC desc = {};
		desc.FrontCounterClockwise = FALSE;
		desc.DepthClipEnable = TRUE;

		// Cull Back
		desc.CullMode = D3D11_CULL_BACK;
		desc.FillMode = D3D11_FILL_SOLID;
		m_device->CreateRasterizerState(&desc, m_cullBack.ReleaseAndGetAddressOf());

		// Cull Front
		desc.CullMode = D3D11_CULL_FRONT;
		desc.FillMode = D3D11_FILL_SOLID;
		m_device->CreateRasterizerState(&desc, m_cullFront.ReleaseAndGetAddressOf());

		// Cull None (Double-Sided)
		desc.CullMode = D3D11_CULL_NONE;
		desc.FillMode = D3D11_FILL_SOLID;
		m_device->CreateRasterizerState(&desc, m_cullNone.ReleaseAndGetAddressOf());

		// Wireframe
		desc.CullMode = D3D11_CULL_NONE;
		desc.FillMode = D3D11_FILL_WIREFRAME;
		m_device->CreateRasterizerState(&desc, m_wireframe.ReleaseAndGetAddressOf());
	}

	void RenderStateManager::initBlendStates()
	{
		D3D11_BLEND_DESC desc = {};
		desc.AlphaToCoverageEnable = FALSE;
		desc.IndependentBlendEnable = FALSE;

		// Opaque (No Blending)
		desc.RenderTarget[0].BlendEnable = FALSE;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		m_device->CreateBlendState(&desc, m_blendOpaque.ReleaseAndGetAddressOf());

		// Alpha Blend
		desc.RenderTarget[0].BlendEnable = TRUE;
		desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D11_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_INV_SRC_ALPHA;
		desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		m_device->CreateBlendState(&desc, m_blendAlpha.ReleaseAndGetAddressOf());

		// Additive Blend
		desc.RenderTarget[0].BlendEnable = TRUE;
		desc.RenderTarget[0].SrcBlend = D3D11_BLEND_SRC_ALPHA;
		desc.RenderTarget[0].DestBlend = D3D11_BLEND_ONE;
		desc.RenderTarget[0].BlendOp = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].SrcBlendAlpha = D3D11_BLEND_ONE;
		desc.RenderTarget[0].DestBlendAlpha = D3D11_BLEND_ONE;
		desc.RenderTarget[0].BlendOpAlpha = D3D11_BLEND_OP_ADD;
		desc.RenderTarget[0].RenderTargetWriteMask = D3D11_COLOR_WRITE_ENABLE_ALL;
		m_device->CreateBlendState(&desc, m_blendAdditive.ReleaseAndGetAddressOf());
	}

	void RenderStateManager::initDepthStencilStates()
	{
		D3D11_DEPTH_STENCIL_DESC desc = {};

		// Default: Read & Write, LessEqual
		desc.DepthEnable = TRUE;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ALL;
		desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		desc.StencilEnable = FALSE;
		m_device->CreateDepthStencilState(&desc, m_depthDefault.ReleaseAndGetAddressOf());

		// ReadOnly: Read-only depth (for particles, transparents), LessEqual
		desc.DepthEnable = TRUE;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = D3D11_COMPARISON_LESS_EQUAL;
		desc.StencilEnable = FALSE;
		m_device->CreateDepthStencilState(&desc, m_depthReadOnly.ReleaseAndGetAddressOf());

		// Disabled: No depth testing or writing (for post-processing and UI)
		desc.DepthEnable = FALSE;
		desc.DepthWriteMask = D3D11_DEPTH_WRITE_MASK_ZERO;
		desc.DepthFunc = D3D11_COMPARISON_ALWAYS;
		desc.StencilEnable = FALSE;
		m_device->CreateDepthStencilState(&desc, m_depthDisabled.ReleaseAndGetAddressOf());
	}

	void RenderStateManager::initSamplerStates()
	{
		D3D11_SAMPLER_DESC desc = {};
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.ComparisonFunc = D3D11_COMPARISON_ALWAYS;
		desc.MinLOD = 0;
		desc.MaxLOD = D3D11_FLOAT32_MAX;

		// Linear Wrap
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		m_device->CreateSamplerState(&desc, m_samplerLinearWrap.ReleaseAndGetAddressOf());

		// Linear Clamp
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_LINEAR;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		m_device->CreateSamplerState(&desc, m_samplerLinearClamp.ReleaseAndGetAddressOf());

		// Point Clamp
		desc.Filter = D3D11_FILTER_MIN_MAG_MIP_POINT;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_CLAMP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_CLAMP;
		m_device->CreateSamplerState(&desc, m_samplerPointClamp.ReleaseAndGetAddressOf());

		// Anisotropic Wrap
		desc.Filter = D3D11_FILTER_ANISOTROPIC;
		desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		desc.MaxAnisotropy = 16;
		m_device->CreateSamplerState(&desc, m_samplerAnisotropicWrap.ReleaseAndGetAddressOf());
	}

	void RenderStateManager::applyRasterizerState(RasterizerStateType type)
	{
		ID3D11RasterizerState* state = getRasterizerState(type);
		if (state)
		{
			m_context->RSSetState(state);
			m_currentRasterizer = type;
		}
	}

	void RenderStateManager::applyBlendState(BlendStateType type)
	{
		ID3D11BlendState* state = getBlendState(type);
		if (state)
		{
			float blendFactor[4] = { 0.0f, 0.0f, 0.0f, 0.0f };
			m_context->OMSetBlendState(state, blendFactor, 0xffffffff);
			m_currentBlend = type;
		}
	}

	void RenderStateManager::applyDepthStencilState(DepthStencilStateType type, UINT stencilRef)
	{
		ID3D11DepthStencilState* state = getDepthStencilState(type);
		if (state)
		{
			m_context->OMSetDepthStencilState(state, stencilRef);
			m_currentDepthStencil = type;
		}
	}

	void RenderStateManager::applySamplerState(SamplerStateType type, UINT slot, bool vs, bool ps)
	{
		ID3D11SamplerState* state = getSamplerState(type);
		if (!state)
			return;

		if (vs)
			m_context->VSSetSamplers(slot, 1, &state);
		if (ps)
			m_context->PSSetSamplers(slot, 1, &state);
	}

	ID3D11RasterizerState* RenderStateManager::getRasterizerState(RasterizerStateType type) const
	{
		switch (type)
		{
		case RasterizerStateType::CullBack:  return m_cullBack.Get();
		case RasterizerStateType::CullFront: return m_cullFront.Get();
		case RasterizerStateType::CullNone:  return m_cullNone.Get();
		case RasterizerStateType::Wireframe: return m_wireframe.Get();
		default: return m_cullBack.Get();
		}
	}

	ID3D11BlendState* RenderStateManager::getBlendState(BlendStateType type) const
	{
		switch (type)
		{
		case BlendStateType::Opaque:    return m_blendOpaque.Get();
		case BlendStateType::AlphaBlend: return m_blendAlpha.Get();
		case BlendStateType::Additive:   return m_blendAdditive.Get();
		default: return m_blendOpaque.Get();
		}
	}

	ID3D11DepthStencilState* RenderStateManager::getDepthStencilState(DepthStencilStateType type) const
	{
		switch (type)
		{
		case DepthStencilStateType::Default:  return m_depthDefault.Get();
		case DepthStencilStateType::ReadOnly: return m_depthReadOnly.Get();
		case DepthStencilStateType::Disabled: return m_depthDisabled.Get();
		default: return m_depthDefault.Get();
		}
	}

	ID3D11SamplerState* RenderStateManager::getSamplerState(SamplerStateType type) const
	{
		switch (type)
		{
		case SamplerStateType::LinearWrap:      return m_samplerLinearWrap.Get();
		case SamplerStateType::LinearClamp:     return m_samplerLinearClamp.Get();
		case SamplerStateType::PointClamp:      return m_samplerPointClamp.Get();
		case SamplerStateType::AnisotropicWrap: return m_samplerAnisotropicWrap.Get();
		default: return m_samplerLinearWrap.Get();
		}
	}
}
