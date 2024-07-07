#include "Shader.h"
#include "GraphicsEngine.h"
#include <d3dcompiler.h>
#include "Core/Log/Log.h"
#include "Core/Util/String.h"

namespace Core
{
	Shader::Shader(const ShaderDesc& desc, GraphicsEngine* system)
	{
		ID3DBlob* blob = nullptr;
		ID3DBlob* error_blob = nullptr;

		if (FAILED(D3DCompileFromFile(desc.vertexShaderFilePath, nullptr, nullptr, desc.vertexShaderEntryPointName, "vs_5_0", 0, 0, &blob, &error_blob)))
		{
			std::wstring msg = ToWide((char*)error_blob->GetBufferPointer());
			darklog.error(L"Failed to compile vertex shader:" +  msg);
			return;
		}
		if (FAILED(system->m_d3dDevice->CreateVertexShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &m_vertexShader)))
		{
			darklog.error(L"Failed to create vertex shader");
			return;
		}
			
		if (FAILED(D3DCompileFromFile(desc.pixelShaderFilePath, nullptr, nullptr, desc.pixelShaderEntryPointName, "ps_5_0", 0, 0, &blob, &error_blob)))
		{
			std::wstring msg = ToWide((char*)error_blob->GetBufferPointer());
			darklog.error(L"Failed to compile pixel shader:" + msg);
			return;
		}
		if (FAILED(system->m_d3dDevice->CreatePixelShader(blob->GetBufferPointer(), blob->GetBufferSize(), nullptr, &m_pixelShader)))
		{
			darklog.error(L"Failed to create pixel shader");
			return;
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