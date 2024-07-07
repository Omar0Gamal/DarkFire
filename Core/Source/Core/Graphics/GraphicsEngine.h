#pragma once
#include "Prerequisites.h"
#include "Core/Math/Vector4d.h"
#include "Core/Math/Box.h"

#include "Core/Graphics/Texture2D.h"
#include "Core/Graphics/SwapChain.h"
#include <vector>

#include <d3d11.h>
#include <wrl.h>

namespace Core
{
	class GraphicsEngine
	{
	public:
		GraphicsEngine();
	public:
		SwapChainPtr CreateSwapChain(HWND hwnd, int width, int height);
		VertexBufferPtr CreateVertexBuffer(const VertexBufferDesc& desc);
		IndexBufferPtr CreateIndexBuffer(const IndexBufferDesc& desc);
		ConstantBufferPtr CreateConstantBuffer(const ConstantBufferDesc& desc);
		ShaderPtr CreateShader(const ShaderDesc& desc);
		Texture2DPtr CreateTexture2D(const Texture2DDesc& desc);
		Texture2DPtr CreateTexture2D(const wchar_t* path);
	public:
		void clearColor(const  SwapChainPtr& swap_chain, const Vector4d& color);
		void clearDepthStencil(const  SwapChainPtr& swap_chain);

		void clearColor(const  Texture2DPtr& render_target, const Vector4d& color);
		void clearDepthStencil(const  Texture2DPtr& depth_stencil);

		void setRenderTarget(const  Texture2DPtr& render_target, const  Texture2DPtr& depth_stencil);

		void setVertexBuffer(const  VertexBufferPtr& buffer);
		void setIndexBuffer(const  IndexBufferPtr& buffer);


		void drawTriangleList(int vertex_count, int start_vertex_index);
		void drawIndexedTriangleList(int index_count, int start_vertex_index, int start_index_location);
		void drawTriangleStrip(int vertex_count, int start_vertex_index);

		void setViewportSize(int width, int height);
		void setShader(const  ShaderPtr& shader);
		void setConstantBuffer(const  ConstantBufferPtr& buffer);
		void setRasterizerState(bool cull_front);


		void setTexture(const  TexturePtr* texture, unsigned int num_textures);
		void setMaterial(const  MaterialPtr& material);
		void drawMesh(const  MeshPtr& mesh, const std::vector<MaterialPtr>& list_materials);
		void drawSphere(const MeshPtr& mesh, const std::vector<MaterialPtr>& list_materials);
	private:
		void compilePrivateShaders();
		void initRasterizerState();
	private:
		Microsoft::WRL::ComPtr <ID3D11Device> m_d3dDevice;
		Microsoft::WRL::ComPtr <ID3D11DeviceContext> m_immediateContext;
		Microsoft::WRL::ComPtr <IDXGIDevice> m_dxgiDevice;
		Microsoft::WRL::ComPtr <IDXGIAdapter> m_dxgiAdapter;
		Microsoft::WRL::ComPtr <IDXGIFactory> m_dxgiFactory;
	private:
		Microsoft::WRL::ComPtr <ID3D11RasterizerState> m_cull_front_state;
		Microsoft::WRL::ComPtr <ID3D11RasterizerState> m_cull_back_state;
		Microsoft::WRL::ComPtr <ID3D11RasterizerState> m_debug_state;
	private:
		unsigned char m_meshLayoutByteCode[1024];
		size_t m_meshLayoutSize = 0;
	private:
		friend class  SwapChain;
		friend class  VertexBuffer;
		friend class  IndexBuffer;
		friend class  ConstantBuffer;
		friend class  Texture2D;
		friend class  Shader;
	};
}



