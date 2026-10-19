#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/RHI/IRHI.h"
#include <memory>

namespace Core::RHI
{
	enum class BackendType
	{
		Null = 0,
		D3D11
	};

	DF_API std::shared_ptr<IRHIDevice> CreateRHIDevice(BackendType backend, bool enableDebug = false);
}
