#include "ShaderLibrary.h"
#include "Engine/Runtime/EngineContext.h"
#include "Engine/Core/Log/Log.h"

#include <d3dcompiler.h>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Core::Renderer
{
	namespace
	{
		const char* g_FallbackMagentaShaderHLSL =
			"struct PSInput { float4 pos : SV_Position; };\n"
			"float4 main(PSInput input) : SV_Target0\n"
			"{\n"
			"    return float4(1.0f, 0.0f, 1.0f, 1.0f);\n"
			"}\n";
	}

	ShaderLibrary::ShaderLibrary(RHI::IRHIDevice& device, std::string shaderDir)
		: m_device(device)
		, m_shaderDir(std::move(shaderDir))
	{
		initializeFallbackShaders();
	}

	void ShaderLibrary::initializeFallbackShaders()
	{
		auto compileRes = compileSource(
			g_FallbackMagentaShaderHLSL,
			"EmbeddedFallbackMagenta.hlsl",
			"main",
			"ps_5_0");

		if (compileRes)
		{
			auto psRes = m_device.createPixelShader(*compileRes);
			if (psRes)
			{
				m_fallbackPixelShader = *psRes;
			}
		}
		else
		{
			// If D3DCompile is unavailable in headless test environment, provide dummy bytecode
			const uint8_t dummyBytecode[] = { 0xDE, 0xAD, 0xBE, 0xEF };
			auto psRes = m_device.createPixelShader(dummyBytecode);
			if (psRes)
			{
				m_fallbackPixelShader = *psRes;
			}
		}
	}

	std::string ShaderLibrary::buildKey(
		const std::string& name,
		const std::string& entry,
		const std::vector<std::string>& keywords)
	{
		std::string key = name + "@" + entry;
		for (const auto& kw : keywords)
		{
			key += "+" + kw;
		}
		return key;
	}

	Result<std::vector<uint8_t>, std::string> ShaderLibrary::compileSource(
		const std::string& source,
		const std::string& sourceName,
		const std::string& entryPoint,
		const std::string& profile,
		const std::vector<std::string>& keywords)
	{
		std::vector<D3D_SHADER_MACRO> defines;
		for (const auto& kw : keywords)
		{
			defines.push_back({ kw.c_str(), "1" });
		}
		defines.push_back({ nullptr, nullptr });

		UINT flags = D3DCOMPILE_ENABLE_STRICTNESS | D3DCOMPILE_PACK_MATRIX_ROW_MAJOR;
#if defined(DF_DEBUG)
		flags |= D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		flags |= D3DCOMPILE_OPTIMIZATION_LEVEL3;
#endif

		ID3DBlob* codeBlob = nullptr;
		ID3DBlob* errorBlob = nullptr;

		const HRESULT hr = D3DCompile(
			source.data(),
			source.size(),
			sourceName.c_str(),
			defines.data(),
			D3D_COMPILE_STANDARD_FILE_INCLUDE,
			entryPoint.c_str(),
			profile.c_str(),
			flags,
			0,
			&codeBlob,
			&errorBlob);

		if (FAILED(hr))
		{
			std::string errorMsg;
			if (errorBlob)
			{
				errorMsg = static_cast<const char*>(errorBlob->GetBufferPointer());
				errorBlob->Release();
			}
			else
			{
				errorMsg = "Unknown compilation error (HRESULT: " + std::to_string(hr) + ")";
			}
			if (codeBlob)
			{
				codeBlob->Release();
			}
			return Result<std::vector<uint8_t>, std::string>::error(errorMsg);
		}

		if (errorBlob)
		{
			errorBlob->Release();
		}

		std::vector<uint8_t> bytecode(
			static_cast<const uint8_t*>(codeBlob->GetBufferPointer()),
			static_cast<const uint8_t*>(codeBlob->GetBufferPointer()) + codeBlob->GetBufferSize());
		codeBlob->Release();

		return Result<std::vector<uint8_t>, std::string>::ok(std::move(bytecode));
	}

	Result<std::shared_ptr<RHI::IVertexShader>, std::string> ShaderLibrary::getOrCreateVertexShader(
		const std::string& shaderName,
		const std::string& entryPoint,
		const std::vector<std::string>& keywords)
	{
		const std::string key = buildKey(shaderName, entryPoint, keywords);
		auto it = m_vertexShaders.find(key);
		if (it != m_vertexShaders.end())
		{
			return Result<std::shared_ptr<RHI::IVertexShader>, std::string>::ok(it->second);
		}

		// Try loading precompiled bytecode blob (.cso) from disk or VFS
		std::string baseStem = std::filesystem::path(shaderName).stem().string();
		std::vector<uint8_t> precompiledBytecode;

		std::filesystem::path csoPath = std::filesystem::path(m_shaderDir) / (baseStem + ".cso");
		if (!std::filesystem::exists(csoPath))
		{
			if (std::filesystem::exists("Engine/Shaders/" + baseStem + ".cso")) csoPath = "Engine/Shaders/" + baseStem + ".cso";
			else if (std::filesystem::exists("../../../Engine/Shaders/" + baseStem + ".cso")) csoPath = "../../../Engine/Shaders/" + baseStem + ".cso";
		}
		if (std::filesystem::exists(csoPath))
		{
			std::ifstream csoFile(csoPath, std::ios::binary | std::ios::ate);
			if (csoFile.is_open())
			{
				auto sz = csoFile.tellg();
				csoFile.seekg(0, std::ios::beg);
				precompiledBytecode.resize(static_cast<size_t>(sz));
				csoFile.read(reinterpret_cast<char*>(precompiledBytecode.data()), sz);
			}
		}

		if (precompiledBytecode.empty())
		{
			auto& vfs = Core::EngineContext::Get().vfs;
			auto vfsRes = vfs.readBinaryFile("/Shaders/" + baseStem + ".cso");
			if (vfsRes.isOk())
			{
				precompiledBytecode = std::move(vfsRes.value());
			}
			else
			{
				auto vfsRes2 = vfs.readBinaryFile("/engine/Shaders/" + baseStem + ".cso");
				if (vfsRes2.isOk())
				{
					precompiledBytecode = std::move(vfsRes2.value());
				}
			}
		}

		if (!precompiledBytecode.empty())
		{
			auto vsRes = m_device.createVertexShader(precompiledBytecode);
			if (vsRes)
			{
				m_vertexShaders[key] = *vsRes;
				m_bytecodeCache[key] = precompiledBytecode;
				return vsRes;
			}
		}

		std::filesystem::path filePath = std::filesystem::path(m_shaderDir) / shaderName;
		if (!std::filesystem::exists(filePath))
		{
			if (std::filesystem::exists("Engine/Shaders/" + shaderName)) filePath = "Engine/Shaders/" + shaderName;
			else if (std::filesystem::exists("../../../Engine/Shaders/" + shaderName)) filePath = "../../../Engine/Shaders/" + shaderName;
		}
		std::ifstream file(filePath);
		if (!file.is_open())
		{
			m_lastError = "Shader file not found: " + filePath.string();
			DF_LOG_ERROR("Shader", "{}", m_lastError);
			return Result<std::shared_ptr<RHI::IVertexShader>, std::string>::error(m_lastError);
		}

		std::stringstream ss;
		ss << file.rdbuf();
		const std::string source = ss.str();

		auto compileRes = compileSource(source, filePath.string(), entryPoint, "vs_5_0", keywords);
		if (!compileRes)
		{
			m_lastError = compileRes.error();
			DF_LOG_ERROR("Shader", "Vertex Shader compilation error in {}: {}", shaderName, m_lastError);
			return Result<std::shared_ptr<RHI::IVertexShader>, std::string>::error(m_lastError);
		}

		auto vsRes = m_device.createVertexShader(*compileRes);
		if (!vsRes)
		{
			m_lastError = vsRes.error();
			DF_LOG_ERROR("Shader", "Failed to create Vertex Shader GPU resource for {}: {}", shaderName, m_lastError);
			return Result<std::shared_ptr<RHI::IVertexShader>, std::string>::error(m_lastError);
		}

		m_vertexShaders[key] = *vsRes;
		m_bytecodeCache[key] = *compileRes;
		return vsRes;
	}

	Result<std::shared_ptr<RHI::IPixelShader>, std::string> ShaderLibrary::getOrCreatePixelShader(
		const std::string& shaderName,
		const std::string& entryPoint,
		const std::vector<std::string>& keywords)
	{
		const std::string key = buildKey(shaderName, entryPoint, keywords);
		auto it = m_pixelShaders.find(key);
		if (it != m_pixelShaders.end())
		{
			return Result<std::shared_ptr<RHI::IPixelShader>, std::string>::ok(it->second);
		}

		// Try loading precompiled bytecode blob (.cso) from disk or VFS
		std::string baseStem = std::filesystem::path(shaderName).stem().string();
		std::vector<uint8_t> precompiledBytecode;

		std::filesystem::path csoPath = std::filesystem::path(m_shaderDir) / (baseStem + ".cso");
		if (!std::filesystem::exists(csoPath))
		{
			if (std::filesystem::exists("Engine/Shaders/" + baseStem + ".cso")) csoPath = "Engine/Shaders/" + baseStem + ".cso";
			else if (std::filesystem::exists("../../../Engine/Shaders/" + baseStem + ".cso")) csoPath = "../../../Engine/Shaders/" + baseStem + ".cso";
		}
		if (std::filesystem::exists(csoPath))
		{
			std::ifstream csoFile(csoPath, std::ios::binary | std::ios::ate);
			if (csoFile.is_open())
			{
				auto sz = csoFile.tellg();
				csoFile.seekg(0, std::ios::beg);
				precompiledBytecode.resize(static_cast<size_t>(sz));
				csoFile.read(reinterpret_cast<char*>(precompiledBytecode.data()), sz);
			}
		}

		if (precompiledBytecode.empty())
		{
			auto& vfs = Core::EngineContext::Get().vfs;
			auto vfsRes = vfs.readBinaryFile("/Shaders/" + baseStem + ".cso");
			if (vfsRes.isOk())
			{
				precompiledBytecode = std::move(vfsRes.value());
			}
			else
			{
				auto vfsRes2 = vfs.readBinaryFile("/engine/Shaders/" + baseStem + ".cso");
				if (vfsRes2.isOk())
				{
					precompiledBytecode = std::move(vfsRes2.value());
				}
			}
		}

		if (!precompiledBytecode.empty())
		{
			auto psRes = m_device.createPixelShader(precompiledBytecode);
			if (psRes)
			{
				m_pixelShaders[key] = *psRes;
				return psRes;
			}
		}

		std::filesystem::path filePath = std::filesystem::path(m_shaderDir) / shaderName;
		if (!std::filesystem::exists(filePath))
		{
			if (std::filesystem::exists("Engine/Shaders/" + shaderName)) filePath = "Engine/Shaders/" + shaderName;
			else if (std::filesystem::exists("../../../Engine/Shaders/" + shaderName)) filePath = "../../../Engine/Shaders/" + shaderName;
		}
		std::ifstream file(filePath);
		if (!file.is_open())
		{
			m_lastError = "Shader file not found: " + filePath.string();
			DF_LOG_ERROR("Shader", "{}. Returning magenta fallback.", m_lastError);
			return Result<std::shared_ptr<RHI::IPixelShader>, std::string>::ok(m_fallbackPixelShader);
		}

		std::stringstream ss;
		ss << file.rdbuf();
		const std::string source = ss.str();

		auto compileRes = compileSource(source, filePath.string(), entryPoint, "ps_5_0", keywords);
		if (!compileRes)
		{
			m_lastError = compileRes.error();
			DF_LOG_ERROR("Shader", "Pixel Shader compilation error in {}: {}. Returning magenta fallback.", shaderName, m_lastError);
			// Return fallback magenta shader without crashing
			return Result<std::shared_ptr<RHI::IPixelShader>, std::string>::ok(m_fallbackPixelShader);
		}

		auto psRes = m_device.createPixelShader(*compileRes);
		if (!psRes)
		{
			m_lastError = psRes.error();
			DF_LOG_ERROR("Shader", "Failed to create Pixel Shader GPU resource for {}: {}. Returning magenta fallback.", shaderName, m_lastError);
			return Result<std::shared_ptr<RHI::IPixelShader>, std::string>::ok(m_fallbackPixelShader);
		}

		m_pixelShaders[key] = *psRes;
		m_bytecodeCache[key] = *compileRes;
		return psRes;
	}

	void ShaderLibrary::reloadShaders()
	{
		DF_LOG_INFO("Shader", "ShaderLibrary: Reloading shaders...");
		m_vertexShaders.clear();
		m_pixelShaders.clear();
		m_bytecodeCache.clear();
	}
}
