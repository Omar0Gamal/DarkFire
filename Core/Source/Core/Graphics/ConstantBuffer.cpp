#include "ConstantBuffer.h"
#include "GraphicsEngine.h"
#include "Core/log/Log.h"

namespace Core
{
	ConstantBuffer::ConstantBuffer(const ConstantBufferDesc& desc, GraphicsEngine* system)
		: m_system(system)
	{
		D3D11_BUFFER_DESC buff_desc = {};
		buff_desc.Usage = D3D11_USAGE_DEFAULT;
		buff_desc.ByteWidth = desc.bufferSize;
		buff_desc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		buff_desc.CPUAccessFlags = 0;
		buff_desc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA init_data = {};
		init_data.pSysMem = desc.buffer;
		auto device = m_system->m_d3dDevice;

		if (FAILED(device->CreateBuffer(&buff_desc, &init_data, &m_buffer)))
			darklog.error(L"CXConstantBuffer not created successfully");
	}

	void ConstantBuffer::update(void* buffer)
	{
		m_system->m_immediateContext->UpdateSubresource(this->m_buffer.Get(), NULL, NULL, buffer, NULL, NULL);
	}

	void* ConstantBuffer::getBuffer()
	{
		return m_buffer.Get();
	}
}