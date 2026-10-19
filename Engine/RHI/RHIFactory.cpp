#include "RHIFactory.h"
#include "Null/NullRHI.h"
#include "D3D11/D3D11RHI.h"

namespace Core::RHI
{
	std::shared_ptr<IRHIDevice> CreateRHIDevice(BackendType backend, bool enableDebug)
	{
		if (backend == BackendType::Null)
		{
			return std::make_shared<NullRHIDevice>();
		}
		else if (backend == BackendType::D3D11)
		{
			auto device = std::make_shared<D3D11RHIDevice>(enableDebug);
			if (device->initialize())
			{
				return device;
			}
			return nullptr;
		}

		return nullptr;
	}
}
