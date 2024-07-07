#pragma once
#include "Prerequisites.h"
#include <d3d11.h>
#include <wrl.h>

namespace Core
{
	class IndexBuffer
	{
	public:
		IndexBuffer(const IndexBufferDesc& desc, GraphicsEngine* system);
	public:
		unsigned int getIndicesListSize();
		void* getBuffer();
	private:
		IndexBufferDesc m_desc;
	private:
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_buffer;
		GraphicsEngine* m_system = nullptr;
	};
}



