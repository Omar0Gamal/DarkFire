#pragma once
#include "Prerequisites.h"
#include <d3d11.h>
#include <wrl.h>
namespace Core
{
	class Shader
	{
	public:
		Shader(const ShaderDesc& desc, GraphicsEngine* system);
	public:
		void* getVertexShader();
		void* getPixelShader();
	private:
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
		GraphicsEngine* m_system = nullptr;
	};
}

