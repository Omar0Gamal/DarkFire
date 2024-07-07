#pragma once
#include "Prerequisites.h"
#include <d3d11.h>
#include <wrl.h>

namespace Core
{
	class VertexBuffer
	{
	public:
		VertexBuffer(const VertexBufferDesc& desc, GraphicsEngine* system);
	public:
		unsigned int getVerticesListSize();
		unsigned int getVertexSize();
		void* getBuffer();
		void* getLayout();
	private:
		VertexBufferDesc m_desc;
	private:
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_buffer;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_layout;
		GraphicsEngine* m_system = nullptr;
	};
}

