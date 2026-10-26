#include "ComputeKernel.h"
#include "ShaderLibrary.h"
#include "Engine/Core/Log/Log.h"

namespace Core::Renderer
{
	ComputeKernel::ComputeKernel(
		RHI::IGpuDevice& device,
		ShaderLibrary& shaderLib,
		const std::string& name,
		const std::filesystem::path& shaderPath,
		uint32_t constantBufferSize)
	{
		init(device, shaderLib, name, shaderPath, constantBufferSize);
	}

	bool ComputeKernel::init(
		RHI::IGpuDevice& device,
		ShaderLibrary& shaderLib,
		const std::string& name,
		const std::filesystem::path& shaderPath,
		uint32_t constantBufferSize)
	{
		m_name = name;

		if (!shaderPath.empty())
		{
			auto shaderRes = shaderLib.loadComputeShader(name, shaderPath);
			if (shaderRes.isOk())
			{
				m_shader = *shaderRes;
			}
			else
			{
				DF_LOG_ERROR("ComputeKernel", "Failed to load compute shader '{}' from '{}': {}", name, shaderPath.string(), shaderRes.error());
				m_shader = nullptr;
				return false;
			}
		}
		else
		{
			m_shader = shaderLib.getComputeShader(name);
			if (!m_shader)
			{
				DF_LOG_ERROR("ComputeKernel", "Compute shader '{}' not found in ShaderLibrary", name);
				return false;
			}
		}

		if (constantBufferSize > 0)
		{
			// Constant buffer size must be 16-byte aligned in D3D11
			const uint32_t alignedSize = (constantBufferSize + 15u) & ~15u;
			RHI::GpuBufferDesc cbDesc{};
			cbDesc.size = alignedSize;
			cbDesc.usage = RHI::BufferUsage::Default;
			cbDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
			auto cbRes = device.createBuffer(cbDesc);
			if (cbRes.isOk())
			{
				m_constantBuffer = *cbRes;
			}
			else
			{
				DF_LOG_ERROR("ComputeKernel", "Failed to create constant buffer for compute kernel '{}': {}", name, cbRes.error());
				m_constantBuffer.reset();
				return false;
			}
		}
		else
		{
			m_constantBuffer.reset();
		}

		return true;
	}

	bool ComputeKernel::isValid() const noexcept
	{
		return m_shader != nullptr;
	}

	void ComputeKernel::updateConstants(RHI::IGpuContext& ctx, const void* data, size_t size)
	{
		if (m_constantBuffer && data && size > 0)
		{
			ctx.updateSubresource(m_constantBuffer, data, size);
		}
	}

	void ComputeKernel::setInput(RHI::IGpuContext& ctx, uint32_t slot, RHI::IGpuTexture* tex)
	{
		ctx.setShaderResourceCS(slot, tex);
	}

	void ComputeKernel::setInputBuffer(RHI::IGpuContext& ctx, uint32_t slot, RHI::IGpuBuffer* buf)
	{
		ctx.setShaderResourceBufferCS(slot, buf);
	}

	void ComputeKernel::setOutput(RHI::IGpuContext& ctx, uint32_t slot, RHI::IGpuTexture* uavTex, uint32_t mipSlice)
	{
		ctx.setUnorderedAccessViewCS(slot, uavTex, mipSlice);
	}

	void ComputeKernel::setOutputBuffer(RHI::IGpuContext& ctx, uint32_t slot, RHI::IGpuBuffer* uavBuf)
	{
		ctx.setUnorderedAccessBufferCS(slot, uavBuf);
	}

	void ComputeKernel::setSampler(RHI::IGpuContext& ctx, uint32_t slot, RHI::IGpuSampler* sampler)
	{
		ctx.setSamplerCS(slot, sampler);
	}

	void ComputeKernel::dispatchGroups(RHI::IGpuContext& ctx, uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ)
	{
		if (!m_shader)
		{
			return;
		}

		ctx.setComputeShader(m_shader.get());
		if (m_constantBuffer)
		{
			ctx.setConstantBufferCS(0, m_constantBuffer.get());
		}

		ctx.dispatch(groupsX, groupsY, groupsZ);

		// Unbind UAV slots 0..3 with nullptr so output textures/buffers can be immediately bound as SRVs
		for (uint32_t slot = 0; slot < 4; ++slot)
		{
			ctx.setUnorderedAccessViewCS(slot, nullptr, 0);
		}
	}

	void ComputeKernel::dispatchThreads(
		RHI::IGpuContext& ctx,
		uint32_t threadsX,
		uint32_t threadsY,
		uint32_t threadsZ,
		uint32_t groupSizeX,
		uint32_t groupSizeY,
		uint32_t groupSizeZ)
	{
		if (groupSizeX == 0 || groupSizeY == 0 || groupSizeZ == 0)
		{
			return;
		}

		const uint32_t groupsX = (threadsX + groupSizeX - 1) / groupSizeX;
		const uint32_t groupsY = (threadsY + groupSizeY - 1) / groupSizeY;
		const uint32_t groupsZ = (threadsZ + groupSizeZ - 1) / groupSizeZ;

		dispatchGroups(ctx, groupsX, groupsY, groupsZ);
	}
}
