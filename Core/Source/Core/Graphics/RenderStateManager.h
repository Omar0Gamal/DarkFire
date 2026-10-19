#pragma once
#include <d3d11.h>
#include <wrl.h>
#include <memory>
#include "Prerequisites.h"

namespace Core
{
	enum class RasterizerStateType
	{
		CullBack,
		CullFront,
		CullNone,
		Wireframe
	};

	enum class BlendStateType
	{
		Opaque,
		AlphaBlend,
		Additive
	};

	enum class DepthStencilStateType
	{
		Default,
		ReadOnly,
		Disabled
	};

	enum class SamplerStateType
	{
		LinearWrap,
		LinearClamp,
		PointClamp,
		AnisotropicWrap
	};

	class RenderStateManager
	{
	public:
		RenderStateManager(ID3D11Device* device, ID3D11DeviceContext* context);
		~RenderStateManager() = default;

		void applyRasterizerState(RasterizerStateType type);
		void applyBlendState(BlendStateType type);
		void applyDepthStencilState(DepthStencilStateType type, UINT stencilRef = 0);
		void applySamplerState(SamplerStateType type, UINT slot = 0, bool vs = true, bool ps = true);

		ID3D11RasterizerState* getRasterizerState(RasterizerStateType type) const;
		ID3D11BlendState* getBlendState(BlendStateType type) const;
		ID3D11DepthStencilState* getDepthStencilState(DepthStencilStateType type) const;
		ID3D11SamplerState* getSamplerState(SamplerStateType type) const;

	private:
		void initRasterizerStates();
		void initBlendStates();
		void initDepthStencilStates();
		void initSamplerStates();

	private:
		ID3D11Device* m_device = nullptr;
		ID3D11DeviceContext* m_context = nullptr;

		// Rasterizer states
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_cullBack;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_cullFront;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_cullNone;
		Microsoft::WRL::ComPtr<ID3D11RasterizerState> m_wireframe;

		// Blend states
		Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendOpaque;
		Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendAlpha;
		Microsoft::WRL::ComPtr<ID3D11BlendState> m_blendAdditive;

		// Depth Stencil states
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthDefault;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthReadOnly;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilState> m_depthDisabled;

		// Sampler states
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerLinearWrap;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerLinearClamp;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerPointClamp;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_samplerAnisotropicWrap;

		// Currently active states to avoid redundant D3D11 driver calls
		RasterizerStateType m_currentRasterizer = RasterizerStateType::CullBack;
		BlendStateType m_currentBlend = BlendStateType::Opaque;
		DepthStencilStateType m_currentDepthStencil = DepthStencilStateType::Default;
		bool m_initialized = false;
	};
}
