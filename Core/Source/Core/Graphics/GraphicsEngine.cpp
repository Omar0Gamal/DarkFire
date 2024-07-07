#include "Core/Graphics/GraphicsEngine.h"
#include "Core/Graphics/SwapChain.h"
#include "Core/Graphics/GraphicsEngine.h"
#include "Core/Graphics/VertexBuffer.h"
#include "Core/Graphics/IndexBuffer.h"
#include "Core/Graphics/ConstantBuffer.h"
#include "Core/Graphics/Shader.h"
#include "Core/Resource/Material.h"
#include "Core/Resource/Texture.h"
#include "Core/Resource/Mesh.h"
#include "Core/log/Log.h"

#include <d3dcompiler.h>
namespace Core
{
	GraphicsEngine::GraphicsEngine()
	{
		D3D_DRIVER_TYPE driverTypes[] =
		{
			D3D_DRIVER_TYPE_HARDWARE,
			D3D_DRIVER_TYPE_WARP,
			D3D_DRIVER_TYPE_REFERENCE,
		};

		UINT creationFlags = D3D11_CREATE_DEVICE_BGRA_SUPPORT;
		#if defined(DEBUG) || defined(_DEBUG)
			//creationFlags |= D3D11_CREATE_DEVICE_DEBUG;
		#endif

		D3D_FEATURE_LEVEL featureLevels[] =
		{
			D3D_FEATURE_LEVEL_11_0,
		};

		UINT numDriverTypes = ARRAYSIZE(driverTypes);
		UINT numFeatureLevels = ARRAYSIZE(featureLevels);


		HRESULT hr = 0;
		D3D_FEATURE_LEVEL m_feature_level;
		for (UINT driverTypeIndex = 0; driverTypeIndex < numDriverTypes; driverTypeIndex++)
		{
			auto driverType = driverTypes[driverTypeIndex];
			hr = D3D11CreateDevice(NULL, driverType, NULL, creationFlags, featureLevels, numFeatureLevels,
				D3D11_SDK_VERSION, &m_d3dDevice, &m_feature_level, &m_immediateContext);

			if (SUCCEEDED(hr))
			{
				break;
			}
		}

		if (FAILED(hr))
		{
			darklog.error(L"Failed to create D3D11 device");
			return;
		}

		m_d3dDevice->QueryInterface(__uuidof(IDXGIDevice), (void**)&m_dxgiDevice);
		m_dxgiDevice->GetParent(__uuidof(IDXGIAdapter), (void**)&m_dxgiAdapter);
		m_dxgiAdapter->GetParent(__uuidof(IDXGIFactory), (void**)&m_dxgiFactory);

		initRasterizerState();
		compilePrivateShaders();
	}

	SwapChainPtr GraphicsEngine::CreateSwapChain(HWND hwnd, int width, int height)
	{
		return std::make_shared<SwapChain>(hwnd, width, height, this);
	}
	VertexBufferPtr GraphicsEngine::CreateVertexBuffer(const VertexBufferDesc& desc)
	{
		return std::make_shared<VertexBuffer>(desc, this);
	}
	IndexBufferPtr GraphicsEngine::CreateIndexBuffer(const IndexBufferDesc& desc)
	{
		return std::make_shared<IndexBuffer>(desc, this);
	}
	ConstantBufferPtr GraphicsEngine::CreateConstantBuffer(const ConstantBufferDesc& desc)
	{
		return std::make_shared<ConstantBuffer>(desc, this);
	}
	ShaderPtr GraphicsEngine::CreateShader(const ShaderDesc& desc)
	{
		return std::make_shared<Shader>(desc, this);
	}
	Texture2DPtr GraphicsEngine::CreateTexture2D(const Texture2DDesc& desc)
	{
		return std::make_shared<Texture2D>(desc, this);
	}
	Texture2DPtr GraphicsEngine::CreateTexture2D(const wchar_t* path)
	{
		return std::make_shared<Texture2D>(path, this);
	}



	void GraphicsEngine::clearColor(const SwapChainPtr& swap_chain, const Vector4d& color)
	{
		FLOAT clear_color[] = { color.x, color.y, color.z, color.w };
		auto rtv = reinterpret_cast<ID3D11RenderTargetView*>(swap_chain->getRenderTargetView());
		auto dsv = reinterpret_cast<ID3D11DepthStencilView*>(swap_chain->getDepthStencilView());

		m_immediateContext->ClearRenderTargetView(rtv, clear_color);
		m_immediateContext->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
		m_immediateContext->OMSetRenderTargets(1, &rtv, dsv);
	}
	void GraphicsEngine::clearDepthStencil(const SwapChainPtr& swap_chain)
	{
		auto dsv = reinterpret_cast<ID3D11DepthStencilView*>(swap_chain->getDepthStencilView());
		m_immediateContext->ClearDepthStencilView(dsv, D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
	}
	void GraphicsEngine::clearColor(const Texture2DPtr& render_target, const Vector4d& color)
	{
		if (render_target->m_desc.type != TextureType::RenderTarget) return;

		FLOAT clear_color[] = { color.x, color.y, color.z, color.w };
		m_immediateContext->ClearRenderTargetView(render_target->m_render_target_view.Get(), clear_color);
	}
	void GraphicsEngine::clearDepthStencil(const Texture2DPtr& depth_stencil)
	{
		if (depth_stencil->m_desc.type != TextureType::DepthStencil) return;
		m_immediateContext->ClearDepthStencilView((ID3D11DepthStencilView*)depth_stencil->m_depth_stencil_view.Get(), D3D11_CLEAR_DEPTH | D3D11_CLEAR_STENCIL, 1, 0);
	}
	void GraphicsEngine::setRenderTarget(const Texture2DPtr& render_target, const Texture2DPtr& depth_stencil)
	{
		if (render_target->m_desc.type != TextureType::RenderTarget) return;
		if (depth_stencil->m_desc.type != TextureType::DepthStencil) return;

		m_immediateContext->OMSetRenderTargets(1, render_target->m_render_target_view.GetAddressOf(), (ID3D11DepthStencilView*)depth_stencil->m_depth_stencil_view.Get());
	}
	void GraphicsEngine::setVertexBuffer(const VertexBufferPtr& buffer)
	{
		unsigned int stride = buffer->getVertexSize();
		unsigned int offset = 0;
		auto buf = reinterpret_cast<ID3D11Buffer*>(buffer->getBuffer());
		m_immediateContext->IASetVertexBuffers(0, 1, &buf, &stride, &offset);
		m_immediateContext->IASetInputLayout((ID3D11InputLayout*)buffer->getLayout());
	}
	void GraphicsEngine::setIndexBuffer(const IndexBufferPtr& buffer)
	{
		m_immediateContext->IASetIndexBuffer((ID3D11Buffer*)buffer->getBuffer(), DXGI_FORMAT_R32_UINT, 0);
	}
	void GraphicsEngine::drawTriangleList(int vertex_count, int start_vertex_index)
	{
		m_immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_immediateContext->Draw(vertex_count, start_vertex_index);
	}
	void GraphicsEngine::drawIndexedTriangleList(int index_count, int start_vertex_index, int start_index_location)
	{
		m_immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		m_immediateContext->DrawIndexed(index_count, start_index_location, start_vertex_index);
	}
	void GraphicsEngine::drawTriangleStrip(int vertex_count, int start_vertex_index)
	{
		m_immediateContext->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLESTRIP);
		m_immediateContext->Draw(vertex_count, start_vertex_index);
	}
	void GraphicsEngine::setViewportSize(int width, int height)
	{
		D3D11_VIEWPORT vp = {};
		vp.Width = (FLOAT)width;
		vp.Height = (FLOAT)height;
		vp.MinDepth = 0.0f;
		vp.MaxDepth = 1.0f;
		m_immediateContext->RSSetViewports(1, &vp);
	}
	void GraphicsEngine::setShader(const ShaderPtr& shader)
	{
		m_immediateContext->VSSetShader((ID3D11VertexShader*)shader->getVertexShader(), nullptr, 0);
		m_immediateContext->PSSetShader((ID3D11PixelShader*)shader->getPixelShader(), nullptr, 0);
	}
	void GraphicsEngine::setConstantBuffer(const ConstantBufferPtr& buffer)
	{
		auto buf = reinterpret_cast<ID3D11Buffer*>(buffer->getBuffer());
		m_immediateContext->VSSetConstantBuffers(0, 1, &buf);
		m_immediateContext->PSSetConstantBuffers(0, 1, &buf);
	}
	void GraphicsEngine::setTexture(const TexturePtr* texture, unsigned int num_textures)
	{
		ID3D11ShaderResourceView* list_res[32];
		ID3D11SamplerState* list_sampler[32];
		for (unsigned int i = 0; i < num_textures; i++)
		{
			list_res[i] = (ID3D11ShaderResourceView*)texture[i]->m_texture->m_shader_res_view.Get();
			list_sampler[i] = (ID3D11SamplerState*)texture[i]->m_texture->m_sampler_state.Get();
		}

		m_immediateContext->VSSetShaderResources(0, num_textures, list_res);
		m_immediateContext->VSSetSamplers(0, num_textures, list_sampler);
		m_immediateContext->PSSetShaderResources(0, num_textures, list_res);
		m_immediateContext->PSSetSamplers(0, num_textures, list_sampler);
	}
	void GraphicsEngine::setMaterial(const MaterialPtr& material)
	{
		setRasterizerState((material->m_cull_mode == CullMode::Front));

		if (material->m_constant_buffer)
			setConstantBuffer(material->m_constant_buffer);

		setShader(material->m_shader);
		if (material->m_vec_textures.size())
		{

			setTexture(&material->m_vec_textures[0], (UINT)material->m_vec_textures.size());
		}
	}
	void GraphicsEngine::setRasterizerState(bool cull_front)
	{
		if (cull_front)
			m_immediateContext->RSSetState(m_cull_front_state.Get());
		else
			m_immediateContext->RSSetState(m_cull_back_state.Get());
	}
	void GraphicsEngine::drawMesh(const MeshPtr& mesh, const std::vector<MaterialPtr>& list_materials)
	{
		for (unsigned int m = 0; m < list_materials.size(); m++)
		{
			if (m == list_materials.size()) break;

			MaterialSlot mat = mesh->getMaterialSlot(m);

			setMaterial(list_materials[m]);
			setVertexBuffer(mesh->getVertexBuffer());
			setIndexBuffer(mesh->getIndexBuffer());
			drawIndexedTriangleList((UINT)mat.num_indices, 0, (UINT)mat.start_index);
		}
	}
	void GraphicsEngine::drawSphere(const MeshPtr& mesh, const std::vector<MaterialPtr>& list_materials)
	{
		for (unsigned int m = 0; m < list_materials.size(); m++)
		{
			if (m == list_materials.size()) break;

			MaterialSlot mat = mesh->getMaterialSlot(m);

			setMaterial(list_materials[m]);
			setVertexBuffer(mesh->getVertexBuffer());
			setIndexBuffer(mesh->getIndexBuffer());
			m_immediateContext->RSSetState(m_debug_state.Get());
			drawIndexedTriangleList((UINT)mat.num_indices, 0, (UINT)mat.start_index);
		}

	}
	void GraphicsEngine::compilePrivateShaders()
	{
		ID3DBlob* blob = nullptr;
		ID3DBlob* error_blob = nullptr;

		auto layoutCode = R"(
			struct VS_INPUT
			{
				float4 position : POSITION0;
				float2 texcoord : TEXCOORD0;
				float3 normal : NORMAL0;
				float3 binormal : BINORMAL0;
				float3 tangent : TANGENT;
			};

			struct VS_OUTPUT
			{
				float4 position: SV_POSITION;
				float2 texcoord: TEXCOORD0;
			};

			VS_OUTPUT vsmain(VS_INPUT input)
			{
				VS_OUTPUT output = (VS_OUTPUT)0;
				return output;
			}
		)";

		auto codeLength = strlen(layoutCode);
		if (FAILED(D3DCompile(layoutCode, codeLength, "VertexMeshLayoutShader", nullptr, nullptr, "vsmain", "vs_5_0", 0, 0, &blob, &error_blob)))
		{
			if (error_blob) error_blob->Release();
			{
				darklog.error(L"Failed to compile VertexMeshLayoutShader");
				return;
			}
		}

		::memcpy(m_meshLayoutByteCode, blob->GetBufferPointer(), blob->GetBufferSize());
		m_meshLayoutSize = blob->GetBufferSize();
	}
	void GraphicsEngine::initRasterizerState()
	{
		D3D11_RASTERIZER_DESC desc = {};
		desc.CullMode = D3D11_CULL_FRONT;
		desc.DepthClipEnable = true;
		desc.FillMode = D3D11_FILL_SOLID;
		desc.FrontCounterClockwise = true;
		m_d3dDevice->CreateRasterizerState(&desc, &m_cull_front_state);

		desc.CullMode = D3D11_CULL_BACK;
		m_d3dDevice->CreateRasterizerState(&desc, &m_cull_back_state);

		desc.FillMode = D3D11_FILL_WIREFRAME;
		m_d3dDevice->CreateRasterizerState(&desc, &m_debug_state);
	}
}
