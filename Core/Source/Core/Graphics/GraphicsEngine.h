#pragma once
#include "Prerequisites.h"
#include "Core/Math/Vector4d.h"
#include "Core/Math/Box.h"

#include "Core/Graphics/Texture2D.h"
#include "Core/Graphics/SwapChain.h"
#include "UIRender.h"
#include <vector>

#include <d3d11.h>
#include <wrl.h>


namespace Core
{
	class RenderStateManager;

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
		Font2DPtr CreateFont2D(const wchar_t* path);
	public:
		void clearColor(const  SwapChainPtr& swap_chain, const Vector4d& color);
		void clearDepthStencil(const  SwapChainPtr& swap_chain);
		void clearState();

		void clearColor(const  Texture2DPtr& render_target, const Vector4d& color);
		void clearDepthStencil(const  Texture2DPtr& depth_stencil);

		void setRenderTarget(const  Texture2DPtr& render_target, const  Texture2DPtr& depth_stencil);

		void setVertexBuffer(const  VertexBufferPtr& buffer);
		void setIndexBuffer(const  IndexBufferPtr& buffer);


		void drawTriangleList(int vertex_count, int start_vertex_index);
		void drawIndexedTriangleList(int index_count, int start_vertex_index, int start_index_location);
		void drawIndexedInstanced(int index_count_per_instance, int instance_count, int start_index_location, int base_vertex_location, int start_instance_location);
		void drawTriangleStrip(int vertex_count, int start_vertex_index);
		void drawFilledRectangle(const Box& size, MaterialPtr material);

		void setViewportSize(int width, int height);
		void setShader(const  ShaderPtr& shader);
		void setConstantBuffer(const  ConstantBufferPtr& buffer);
		void setConstantBuffer(unsigned int slot, const ConstantBufferPtr& buffer);
		void setConstantBuffer(unsigned int slot, ID3D11Buffer* buffer);
		void setRasterizerState(bool cull_front);

		RenderStateManager* getRenderStateManager();
		ID3D11Device* getD3DDevice() const;
		ID3D11DeviceContext* getImmediateContext() const;

		void setTexture(const  TexturePtr* texture, unsigned int num_textures);
		void setMaterial(const  MaterialPtr& material);
		void drawMesh(const  MeshPtr& mesh, const std::vector<MaterialPtr>& list_materials);
		void drawSphere(const MeshPtr& mesh, const std::vector<MaterialPtr>& list_materials);
		void drawImage(const Texture2DPtr& texture, const Box& size);
	private:
		void compilePrivateShaders();
		void initRasterizerState();
	private:
		std::unique_ptr<class RenderStateManager> m_stateManager;
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
		Microsoft::WRL::ComPtr <ID3D11BlendState> m_blend_state;
	private:
		std::unique_ptr<UIRender> m_uiRender;
	private:
		unsigned char m_meshLayoutByteCode[1024];
		size_t m_meshLayoutSize = 0;
	private:
		unsigned char m_uiLayoutByteCode[1024];
		size_t m_uiLayoutSize = 0;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_uiVertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_uiPixelShader;
	private:
		friend class  SwapChain;
		friend class  VertexBuffer;
		friend class  IndexBuffer;
		friend class  ConstantBuffer;
		friend class  Texture2D;
		friend class  Shader;
		friend class  Font2D;
		friend class  UIRender;
	};
}



