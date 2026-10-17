#pragma once
#include "Prerequisites.h"
#include <SpriteBatch.h>
#include <d3d11.h>
#include <wrl.h>
#include <d3dcompiler.h>
#include "Core/Graphics/GraphicsEngine.h"
#include "Core/Math/Box.h"
#include "Core/Math/Vector2d.h"

namespace Core
{
	class UIRender
	{
	public:
		UIRender(GraphicsEngine* system);
	public:
		void drawImage(const Texture2DPtr& texture, const Box& size);
		void drawFilledRectangle(const Box& size);
	private:
		//void compileUIShaders();
	private:
		GraphicsEngine* m_system = nullptr;
		std::unique_ptr<DirectX::SpriteBatch> m_ImageBatch;
		unsigned char m_uiLayoutByteCode[1024];
		size_t m_uiLayoutSize = 0;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_uiVertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_uiPixelShader;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_buffer;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_layout;
	};

	class VertexData
	{
	public:
		VertexData() :m_position(), m_texcoord()
		{
		}
		VertexData(const Vector3d& position, const Vector2d& texcoord) :
			m_position(position),
			m_texcoord(texcoord)
		{
		}
		VertexData(const VertexData& vertex) :
			m_position(vertex.m_position),
			m_texcoord(vertex.m_texcoord)
		{
		}
	public:
		Vector3d m_position;
		Vector2d m_texcoord;
	};
}

