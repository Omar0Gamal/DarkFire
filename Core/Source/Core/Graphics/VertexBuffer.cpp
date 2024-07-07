#include "VertexBuffer.h"
#include "GraphicsEngine.h"
#include "Core/log/log.h"

namespace Core
{
	VertexBuffer::VertexBuffer(const VertexBufferDesc& desc, GraphicsEngine* system)
		: m_system(system)
	{
		D3D11_BUFFER_DESC buff_desc = {};
		buff_desc.Usage = D3D11_USAGE_DEFAULT;
		buff_desc.ByteWidth = desc.vertexSize * desc.listSize;
		buff_desc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		buff_desc.CPUAccessFlags = 0;
		buff_desc.MiscFlags = 0;

		D3D11_SUBRESOURCE_DATA init_data = {};
		init_data.pSysMem = desc.verticesList;

		m_desc = desc;

		if (FAILED(m_system->m_d3dDevice->CreateBuffer(&buff_desc, &init_data, &m_buffer)))
		{
			throw std::runtime_error("VertexBuffer not created successfully");
		}

		D3D11_INPUT_ELEMENT_DESC layout[] =
		{
			//SEMANTIC NAME - SEMANTIC INDEX - FORMAT - INPUT SLOT - ALIGNED BYTE OFFSET - INPUT SLOT CLASS - INSTANCE CXATA STEP RATE
			{"POSITION", 0,  DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,D3D11_INPUT_PER_VERTEX_DATA ,0},
			{"TEXCOORD", 0,  DXGI_FORMAT_R32G32_FLOAT, 0, 12,D3D11_INPUT_PER_VERTEX_DATA ,0 },
			{"NORMAL", 0,  DXGI_FORMAT_R32G32B32_FLOAT, 0, 20,D3D11_INPUT_PER_VERTEX_DATA ,0 },
			{"TANGENT", 0,  DXGI_FORMAT_R32G32B32_FLOAT, 0, 32,D3D11_INPUT_PER_VERTEX_DATA ,0 },
			{"BINORMAL", 0,  DXGI_FORMAT_R32G32B32_FLOAT, 0, 44,D3D11_INPUT_PER_VERTEX_DATA ,0 },
		};

		UINT size_layout = ARRAYSIZE(layout);

		if (FAILED(m_system->m_d3dDevice->CreateInputLayout(layout, size_layout, m_system->m_meshLayoutByteCode, m_system->m_meshLayoutSize, &m_layout)))
		{
			throw std::runtime_error("InputLayout not created successfully");
		}
	}

	unsigned int VertexBuffer::getVerticesListSize()
	{
		return m_desc.listSize;
	}

	unsigned int VertexBuffer::getVertexSize()
	{
		return m_desc.vertexSize;
	}

	void* VertexBuffer::getBuffer()
	{
		return m_buffer.Get();
	}

	void* VertexBuffer::getLayout()
	{
		return m_layout.Get();
	}
}