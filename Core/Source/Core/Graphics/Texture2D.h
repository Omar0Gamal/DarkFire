#pragma once
#include "Prerequisites.h"
#include "Core/Math/Box.h"
#include <d3d11.h>
#include <wrl.h>

namespace Core
{
	struct Texture2DDesc
	{
		Box size = {};
		TextureType type = TextureType::Normal;
	};

	class Texture2D
	{
	public:
		Texture2D(const Texture2DDesc& desc, GraphicsEngine* system);
		Texture2D(const wchar_t* path, GraphicsEngine* system);
	private:
		Microsoft::WRL::ComPtr<ID3D11Texture2D> m_texture = nullptr;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_shader_res_view = nullptr;
		Microsoft::WRL::ComPtr<ID3D11SamplerState> m_sampler_state = nullptr;
		Microsoft::WRL::ComPtr<ID3D11RenderTargetView> m_render_target_view = nullptr;
		Microsoft::WRL::ComPtr<ID3D11DepthStencilView> m_depth_stencil_view = nullptr;
		Texture2DDesc m_desc = {};
	private:
		friend class  GraphicsEngine;
	};
}

