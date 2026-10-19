#include "Texture2D.h"
#include "GraphicsEngine.h"
#include <DirectXTex.h>
#include "Core/log/Log.h"
#include <filesystem>
#include <cwctype>

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
		else if (desc.type == TextureType::RenderTargetHDR)
			tex_desc.Format = DXGI_FORMAT_R16G16B16A16_FLOAT;
		else if (desc.type == TextureType::DepthStencil)
			tex_desc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;

		tex_desc.Usage = D3D11_USAGE_DEFAULT;

		if (desc.type == TextureType::Normal)
			tex_desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
		else if (desc.type == TextureType::RenderTarget || desc.type == TextureType::RenderTargetHDR)
			tex_desc.BindFlags = D3D11_BIND_RENDER_TARGET | D3D11_BIND_SHADER_RESOURCE;
		else if (desc.type == TextureType::DepthStencil)
			tex_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;

		tex_desc.MipLevels = 1;
		tex_desc.SampleDesc.Count = 1;
		tex_desc.SampleDesc.Quality = 0;
		tex_desc.MiscFlags = 0;
		tex_desc.ArraySize = 1;
		tex_desc.CPUAccessFlags = 0;

		auto hr = engine->m_d3dDevice->CreateTexture2D(&tex_desc, nullptr, m_texture.ReleaseAndGetAddressOf());
		if (FAILED(hr))
		{
			darklog.error(L"Texture2D not created successfully");
			throw std::runtime_error("CreateTexture2D failed");
		}

		if (desc.type == TextureType::RenderTarget || desc.type == TextureType::RenderTargetHDR || desc.type == TextureType::Normal)
		{
			hr = engine->m_d3dDevice->CreateShaderResourceView(this->m_texture.Get(), NULL, this->m_shader_res_view.ReleaseAndGetAddressOf());
			if (FAILED(hr))
			{
				darklog.error(L"Texture2D SRV not created successfully");
				throw std::runtime_error("CreateShaderResourceView failed");
			}

			if (desc.type == TextureType::RenderTarget || desc.type == TextureType::RenderTargetHDR)
			{
				hr = engine->m_d3dDevice->CreateRenderTargetView(this->m_texture.Get(), NULL, this->m_render_target_view.ReleaseAndGetAddressOf());
				if (FAILED(hr))
				{
					darklog.error(L"Texture2D RTV not created successfully");
					throw std::runtime_error("CreateRenderTargetView failed");
				}
			}
		}
		else if (desc.type == TextureType::DepthStencil)
		{
			hr = engine->m_d3dDevice->CreateDepthStencilView(this->m_texture.Get(), NULL, this->m_depth_stencil_view.ReleaseAndGetAddressOf());
			if (FAILED(hr))
			{
				darklog.error(L"Texture2D DSV not created successfully");
				throw std::runtime_error("CreateDepthStencilView failed");
			}
		}

		m_desc = desc;
	}
	Texture2D::Texture2D(const wchar_t* path, GraphicsEngine* system)
	{
		auto engine = system;
		DirectX::ScratchImage image_data;
		std::filesystem::path filePath(path);
		auto ext = filePath.extension().wstring();
		for (auto& c : ext) c = static_cast<wchar_t>(towlower(c));

		HRESULT res = E_FAIL;
		if (std::filesystem::exists(filePath))
		{
			if (ext == L".dds")
			{
				res = DirectX::LoadFromDDSFile(path, DirectX::DDS_FLAGS_NONE, nullptr, image_data);
			}
			else if (ext == L".tga")
			{
				res = DirectX::LoadFromTGAFile(path, nullptr, image_data);
			}
			else
			{
				res = DirectX::LoadFromWICFile(path, DirectX::WIC_FLAGS_IGNORE_SRGB, nullptr, image_data);
			}
		}

		// Fallback check: if failed or missing, generate procedural 2x2 magenta/black checkerboard
		if (FAILED(res) || image_data.GetImageCount() == 0)
		{
			darklog.warn(L"Texture2D: Failed to load texture file: " + std::wstring(path) + L". Generating procedural fallback checkerboard.");

			DirectX::ScratchImage fallback;
			HRESULT hrFallback = fallback.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, 2, 2, 1, 1);
			if (SUCCEEDED(hrFallback))
			{
				const DirectX::Image* img = fallback.GetImage(0, 0, 0);
				if (img && img->pixels)
				{
					uint32_t* pixels = reinterpret_cast<uint32_t*>(img->pixels);
					pixels[0] = 0xFFFF00FF; // Magenta
					pixels[1] = 0xFF000000; // Black
					pixels[2] = 0xFF000000; // Black
					pixels[3] = 0xFFFF00FF; // Magenta
				}
				image_data = std::move(fallback);
			}
			else
			{
				darklog.error(L"Failed to create fallback texture.");
				return;
			}
		}

		// Determine if mipmaps should be generated
		const DirectX::ScratchImage* finalImage = &image_data;
		DirectX::ScratchImage mip_chain;
		if (image_data.GetMetadata().mipLevels <= 1 && image_data.GetMetadata().width > 1 && image_data.GetMetadata().height > 1)
		{
			res = DirectX::GenerateMipMaps(image_data.GetImages(), image_data.GetImageCount(), image_data.GetMetadata(), 
				DirectX::TEX_FILTER_DEFAULT | DirectX::TEX_FILTER_SEPARATE_ALPHA, 0, mip_chain);
			if (SUCCEEDED(res))
			{
				finalImage = &mip_chain;
			}
		}

		res = DirectX::CreateTexture(engine->m_d3dDevice.Get(), finalImage->GetImages(),
			finalImage->GetImageCount(), finalImage->GetMetadata(), (ID3D11Resource**)m_texture.ReleaseAndGetAddressOf());
		if (FAILED(res))
		{
			darklog.error(L"Texture2D CreateTexture failed: " + std::wstring(path));
			return;
		}

		D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
		srvDesc.Format = finalImage->GetMetadata().format;
		srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
		srvDesc.Texture2D.MipLevels = (UINT)finalImage->GetMetadata().mipLevels;
		srvDesc.Texture2D.MostDetailedMip = 0;

		D3D11_SAMPLER_DESC sampler_desc = {};
		sampler_desc.AddressU = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressV = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.AddressW = D3D11_TEXTURE_ADDRESS_WRAP;
		sampler_desc.Filter = D3D11_FILTER_ANISOTROPIC;
		sampler_desc.MinLOD = 0.0f;
		sampler_desc.MaxLOD = (FLOAT)finalImage->GetMetadata().mipLevels;

		res = engine->m_d3dDevice->CreateSamplerState(&sampler_desc, m_sampler_state.ReleaseAndGetAddressOf());
		if (FAILED(res))
		{
			darklog.error(L"Texture2D sampler state not created successfully");
		}

		res = engine->m_d3dDevice->CreateShaderResourceView(m_texture.Get(), &srvDesc,
			m_shader_res_view.ReleaseAndGetAddressOf());
		if (FAILED(res))
		{
			darklog.error(L"Texture2D SRV not created successfully");
		}

		m_desc.size.width = (int)finalImage->GetMetadata().width;
		m_desc.size.height = (int)finalImage->GetMetadata().height;
		m_desc.type = TextureType::Normal;
	}
}