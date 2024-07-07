#include "Texture2D.h"
#include "GraphicsEngine.h"
#include <DirectXTex.h>
#include "Core/log/Log.h"

namespace Core
{
	Texture2D::Texture2D(const Texture2DDesc& desc, GraphicsEngine* system)
	{
		auto engine = system;

		D3D11_TEXTURE2D_DESC tex_desc = {};
		tex_desc.Width = desc.size.width;
		tex_desc.Height = desc.size.height;

		if (desc.type == TextureType::Normal)
			tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		else if (desc.type == TextureType::RenderTarget)
			tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		else if (desc.type == TextureType::DepthStencil)
			tex_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

		tex_desc.Usage = D3D11_USAGE_DEFAULT;

		if (desc.type == TextureType::Normal)
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		else if (desc.type == TextureType::RenderTarget)
			tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		else if (desc.type == TextureType::DepthStencil)
			tex_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		tex_desc.MipLevels = 1;
		tex_desc.SampleDesc.Count = 1;
		tex_desc.SampleDesc.Quality = 0;
		tex_desc.MiscFlags = 0;
		tex_desc.ArraySize = 1;
		tex_desc.CPUAccessFlags = 0;

		auto hr = engine->m_d3dDevice->CreateTexture2D(&tex_desc, nullptr, (ID3D11Texture2D**)&m_texture);
		if (FAILED(hr))
		{
			darklog.error(L"Texture2D not created successfully");
			return;
		}

		if (desc.type == TextureType::RenderTarget)
		{
			hr = engine->m_d3dDevice->CreateShaderResourceView(this->m_texture.Get(), NULL, &this->m_shader_res_view);
			if (FAILED(hr))
			{
				darklog.error(L"Texture2D not created successfully");
				return;
			}

			hr = engine->m_d3dDevice->CreateRenderTargetView(this->m_texture.Get(), NULL, &this->m_render_target_view);
			if (FAILED(hr))
			{
				darklog.error(L"Texture2D not created successfully");
				return;
			}
		}
		else if (desc.type == TextureType::DepthStencil)
		{
			hr = engine->m_d3dDevice->CreateDepthStencilView(this->m_texture.Get(), NULL, &this->m_depth_stencil_view);
			if (FAILED(hr))
			{
				darklog.error(L"Texture2D not created successfully");
				return;
			}
		}

		m_desc = desc;
	}
	Texture2D::Texture2D(const wchar_t* path, GraphicsEngine* system)
	{
		auto engine = system;
		DirectX::ScratchImage image_data;
		HRESULT res = DirectX::LoadFromWICFile(path, DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image_data);

		DirectX::TexMetadata meta_data = {};
		DirectX::ScratchImage mip_chain = {};
		DirectX::GenerateMipMaps(image_data.GetImages(), image_data.GetImageCount(), image_data.GetMetadata(), DirectX::TEX_FILTER_DEFAULT | DirectX::TEX_FILTER_SEPARATE_ALPHA, 0, mip_chain);

		if (SUCCEEDED(res))
		{
			res = DirectX::CreateTexture(engine->m_d3dDevice.Get(), mip_chain.GetImages(),
				mip_chain.GetImageCount(), mip_chain.GetMetadata(), (ID3D11Resource**)m_texture.GetAddressOf());

			if (FAILED(res))
			{
				darklog.error(L"Texture2D not created successfully");
				return;
			}

			D3D11_SHADER_RESOURCE_VIEW_DESC desc = {};
			desc.Format = image_data.GetMetadata().format;
			desc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
			desc.Texture2D.MipLevels = (UINT)mip_chain.GetMetadata().mipLevels;
			desc.Texture2D.MostDetailedMip = 0;

			D3D11_SAMPLER_DESC sampler_desc = {};
			sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
			sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
			sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
			sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;
			sampler_desc.MinLOD = 0.0f;
			sampler_desc.MaxLOD = (FLOAT)mip_chain.GetMetadata().mipLevels;

			res = engine->m_d3dDevice->CreateSamplerState(&sampler_desc, &m_sampler_state);
			if (FAILED(res))
			{
				darklog.error(L"Texture2D not created successfully");
				return;
			}

			res = engine->m_d3dDevice->CreateShaderResourceView(m_texture.Get(), &desc,
				&m_shader_res_view);
			if (FAILED(res))
			{
				darklog.error(L"Texture2D not created successfully");
				return;
			}
		}
		else
		{
			darklog.error(L"Texture2D not created successfully");
			return;
		}
	}
}