#include "IndexBuffer.h"
#include "GraphicsEngine.h"
#include "Core/log/Log.h"

namespace Core
{
	IndexBuffer::IndexBuffer(const IndexBufferDesc& desc, GraphicsEngine* system)
		: m_system(system)
	{
		D3D11_BUFFER_DESC buff_desc = {};
		buff_desc.Usage = D3D11_USAGE_DEFAULT;
		buff_desc.ByteWidth = 4 * desc.listSize;
		buff_desc.BindFlags = D3D11_BIND_INDEX_BUFFER;
		buff_desc.CPUAccessFlags = 0;
		buff_desc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA init_data = {};
		init_data.pSysMem = desc.indicesList;

		m_desc = desc;

		if (FAILED(m_system->m_d3dDevice->CreateBuffer(&buff_desc, &init_data, &m_buffer)))
			darklog.error(L"IndexBuffer not created successfully");
	}

	unsigned int IndexBuffer::getIndicesListSize()
	{
		return m_desc.listSize;
	}

	void* IndexBuffer::getBuffer()
	{
		return m_buffer.Get();
	}
}