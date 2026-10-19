#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Result.h"
#include "Engine/RHI/IRHI.h"
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace Core::Renderer
{
	struct DF_API ShaderParamDesc
	{
		std::string name;
		uint32_t offset = 0;
		uint32_t size = 0;
	};

	struct DF_API ShaderParamLayout
	{
		std::vector<ShaderParamDesc> params;
		uint32_t totalSize = 0;
	};

	class DF_API ShaderLibrary
	{
	public:
		explicit ShaderLibrary(RHI::IRHIDevice& device, std::string shaderDir = "Engine/Shaders");
		~ShaderLibrary() = default;

		Result<std::shared_ptr<RHI::IVertexShader>, std::string> getOrCreateVertexShader(
			const std::string& shaderName,
			const std::string& entryPoint = "main",
			const std::vector<std::string>& keywords = {});

		Result<std::shared_ptr<RHI::IPixelShader>, std::string> getOrCreatePixelShader(
			const std::string& shaderName,
			const std::string& entryPoint = "main",
			const std::vector<std::string>& keywords = {});

		Result<std::vector<uint8_t>, std::string> compileSource(
			const std::string& source,
			const std::string& sourceName,
			const std::string& entryPoint,
			const std::string& profile,
			const std::vector<std::string>& keywords = {});

		[[nodiscard]] std::shared_ptr<RHI::IPixelShader> getFallbackPixelShader() const { return m_fallbackPixelShader; }
		[[nodiscard]] const std::string& getLastError() const { return m_lastError; }

		void reloadShaders();

	private:
		RHI::IRHIDevice& m_device;
		std::string m_shaderDir;
		std::string m_lastError;
		std::shared_ptr<RHI::IPixelShader> m_fallbackPixelShader;

		std::unordered_map<std::string, std::shared_ptr<RHI::IVertexShader>> m_vertexShaders;
		std::unordered_map<std::string, std::shared_ptr<RHI::IPixelShader>> m_pixelShaders;
		std::unordered_map<std::string, std::vector<uint8_t>> m_bytecodeCache;

		void initializeFallbackShaders();
		static std::string buildKey(const std::string& name, const std::string& entry, const std::vector<std::string>& keywords);
	};
}
