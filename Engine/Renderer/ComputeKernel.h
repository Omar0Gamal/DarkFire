#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/RHI/IRHI.h"
#include <filesystem>
#include <memory>
#include <string>

namespace Core::Renderer
{
	class ShaderLibrary;

	class DF_API ComputeKernel
	{
	public:
		ComputeKernel() = default;
		ComputeKernel(
			RHI::IGpuDevice& device,
			ShaderLibrary& shaderLib,
			const std::string& name,
			const std::filesystem::path& shaderPath,
			uint32_t constantBufferSize = 0);
		~ComputeKernel() = default;

		ComputeKernel(const ComputeKernel&) = default;
		ComputeKernel& operator=(const ComputeKernel&) = default;
		ComputeKernel(ComputeKernel&&) noexcept = default;
		ComputeKernel& operator=(ComputeKernel&&) noexcept = default;

		bool init(
			RHI::IGpuDevice& device,
			ShaderLibrary& shaderLib,
			const std::string& name,
			const std::filesystem::path& shaderPath,
			uint32_t constantBufferSize = 0);

		[[nodiscard]] bool isValid() const noexcept;

		void updateConstants(RHI::IGpuContext& ctx, const void* data, size_t size);

		template<typename T>
		void setConstants(RHI::IGpuContext& ctx, const T& data)
		{
			updateConstants(ctx, &data, sizeof(T));
		}

		void setInput(RHI::IGpuContext& ctx, uint32_t slot, RHI::IGpuTexture* tex);
		void setInput(RHI::IGpuContext& ctx, uint32_t slot, const std::shared_ptr<RHI::IGpuTexture>& tex)
		{
			setInput(ctx, slot, tex.get());
		}

		void setInputBuffer(RHI::IGpuContext& ctx, uint32_t slot, RHI::IGpuBuffer* buf);
		void setInputBuffer(RHI::IGpuContext& ctx, uint32_t slot, const std::shared_ptr<RHI::IGpuBuffer>& buf)
		{
			setInputBuffer(ctx, slot, buf.get());
		}

		void setOutput(RHI::IGpuContext& ctx, uint32_t slot, RHI::IGpuTexture* uavTex, uint32_t mipSlice = 0);
		void setOutput(RHI::IGpuContext& ctx, uint32_t slot, const std::shared_ptr<RHI::IGpuTexture>& uavTex, uint32_t mipSlice = 0)
		{
			setOutput(ctx, slot, uavTex.get(), mipSlice);
		}

		void setOutputBuffer(RHI::IGpuContext& ctx, uint32_t slot, RHI::IGpuBuffer* uavBuf);
		void setOutputBuffer(RHI::IGpuContext& ctx, uint32_t slot, const std::shared_ptr<RHI::IGpuBuffer>& uavBuf)
		{
			setOutputBuffer(ctx, slot, uavBuf.get());
		}

		void setSampler(RHI::IGpuContext& ctx, uint32_t slot, RHI::IGpuSampler* sampler);
		void setSampler(RHI::IGpuContext& ctx, uint32_t slot, const std::shared_ptr<RHI::IGpuSampler>& sampler)
		{
			setSampler(ctx, slot, sampler.get());
		}

		void dispatchGroups(RHI::IGpuContext& ctx, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ = 1);

		void dispatchThreads(
			RHI::IGpuContext& ctx,
			uint32_t threadsX,
			uint32_t threadsY,
			uint32_t threadsZ = 1,
			uint32_t groupSizeX = 8,
			uint32_t groupSizeY = 8,
			uint32_t groupSizeZ = 1);

		[[nodiscard]] const std::string& getName() const noexcept { return m_name; }
		[[nodiscard]] const std::shared_ptr<RHI::IComputeShader>& getShader() const noexcept { return m_shader; }
		[[nodiscard]] const std::shared_ptr<RHI::IGpuBuffer>& getConstantBuffer() const noexcept { return m_constantBuffer; }

	private:
		std::string m_name;
		std::shared_ptr<RHI::IComputeShader> m_shader;
		std::shared_ptr<RHI::IGpuBuffer> m_constantBuffer;
	};
}
