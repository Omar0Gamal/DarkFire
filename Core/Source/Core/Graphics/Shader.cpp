#include "Shader.h"
#include "GraphicsEngine.h"
#include <d3dcompiler.h>
#include "Core/Log/Log.h"
#include "Core/Util/String.h"

namespace Core
{
	Shader::Shader(const ShaderDesc& desc, GraphicsEngine* system)
	{
		Microsoft::WRL::ComPtr<ID3DBlob> vsBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> psBlob;
		Microsoft::WRL::ComPtr<ID3DBlob> errorBlob;

		HRESULT hr = D3DCompileFromFile(desc.vertexShaderFilePath, nullptr, nullptr, 
			desc.vertexShaderEntryPointName, "vs_5_0", 0, 0, &vsBlob, &errorBlob);
		if (FAILED(hr))
		{
			std::wstring msg = errorBlob ? ToWide((char*)errorBlob->GetBufferPointer()) : L"File not found or compile failed";
			darklog.error(L"Failed to compile vertex shader: " + msg);
			throw std::runtime_error("Failed to compile vertex shader");
		}

		hr = system->m_d3dDevice->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, &m_vertexShader);
		if (FAILED(hr))
		{
			darklog.error(L"Failed to create vertex shader");
			throw std::runtime_error("Failed to create vertex shader");
		}

		errorBlob.Reset();
		hr = D3DCompileFromFile(desc.pixelShaderFilePath, nullptr, nullptr, 
			desc.pixelShaderEntryPointName, "ps_5_0", 0, 0, &psBlob, &errorBlob);
		if (FAILED(hr))
		{
			std::wstring msg = errorBlob ? ToWide((char*)errorBlob->GetBufferPointer()) : L"File not found or compile failed";
			darklog.error(L"Failed to compile pixel shader: " + msg);
			throw std::runtime_error("Failed to compile pixel shader");
		}

		hr = system->m_d3dDevice->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, &m_pixelShader);
		if (FAILED(hr))
		{
			darklog.error(L"Failed to create pixel shader");
			throw std::runtime_error("Failed to create pixel shader");
		}
	}

	void* Shader::getVertexShader()
	{
		return m_vertexShader.Get();
	}

	void* Shader::getPixelShader()
	{
		return m_pixelShader.Get();
	}
}