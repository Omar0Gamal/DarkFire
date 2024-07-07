#pragma once
#include "Prerequisites.h"
#include <d3d11.h>
#include <wrl.h>

namespace Core
{
	class ConstantBuffer
	{
	public:
		ConstantBuffer(const ConstantBufferDesc& desc, GraphicsEngine* system);
	public:
		void update(void* buffer);
		void* getBuffer();
	private:
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_buffer;
		GraphicsEngine* m_system = nullptr;
	};
}

