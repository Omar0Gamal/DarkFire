#pragma once
#include "Core/Game/IComponent.h"
#include "Core/Graphics/Prerequisites.h"
#include "Core/Graphics/Texture2D.h"
#include "Core/Math/Box.h"
#include <DirectXColors.h>

namespace Core
{
	class ImageComponent : public IComponent
	{
	public:
		ImageComponent(Texture2DPtr texture, const Box& size)
			: m_texture(texture)
			, m_size(size)
		{
		}
		virtual ~ImageComponent() = default;
	public:
		Texture2DPtr GetTexture() const
		{
			return m_texture;
		}
		const Box& GetSize() const
		{
			return m_size;
		}
		void SetTexture(Texture2DPtr texture)
		{
			m_texture = texture;
			m_size = texture->m_desc.size;
		}
		void SetSize(const Box& size)
		{
			m_size = size;
		}
		void SetPosition(DirectX::XMFLOAT2 position)
		{
			this->position = position;
		}

		DirectX::XMFLOAT2 GetPosition()
		{
			return position;
		}
	private:
		Texture2DPtr m_texture;
		Box m_size;
		DirectX::XMFLOAT2 position = { 0.0f, 0.0f };
	};
}