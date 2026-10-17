#pragma once
#include "Core/Game/IComponent.h"
#include "Core/Math/Box.h"
#include "Core/Graphics/Prerequisites.h"
#include "Core/Resource/Font.h"
#include "Core/Graphics/Font2D.h"
#include <DirectXColors.h>
#include <string>

namespace Core
{
	class TextComponent : public IComponent
	{
    public:
		TextComponent(const std::wstring& text, FontPtr& font)
            : m_text(text), m_font(font)
		{
		}

		TextComponent& setText(const std::wstring& text)
		{
			m_text = text;
			return *this;
		}

		const wchar_t* getText() const
		{
			return m_text.c_str();
		}

		TextComponent& setFont(FontPtr& font)
		{
			m_font = font;
			return *this;
		}

		const FontPtr& getFont()
		{
			return m_font;
		}

		const Box getBounds()
		{
			if (!m_font) return Box();
			return m_font->getFont()->getBounds(m_text.c_str());
		}

		TextComponent& setScale(DirectX::XMFLOAT2 scale)
		{
			this->scale = scale;
			return *this;
		}

		DirectX::XMFLOAT2 getScale()
		{
			return scale;
		}

		TextComponent& setRotation(float rotation)
		{
			this->rotation = rotation;
			return *this;
		}

		float getRotation()
		{
			return rotation;
		}

		TextComponent& setColor(DirectX::XMVECTORF32 color)
		{
			this->r = color.f[0];
			this->g = color.f[1];
			this->b = color.f[2];
			this->a = color.f[3];
			return *this;
		}

		DirectX::FXMVECTOR getColor()
		{
			DirectX::FXMVECTOR color = { r, g, b, a };
			return color;
		}

		TextComponent& setPosition(DirectX::XMFLOAT2 position)
		{
			this->position = position;
			return *this;
		}

		DirectX::XMFLOAT2 getPosition()
		{
			return position;
		}
	private:
		std::wstring m_text = L"";
		float rotation = 0.0f;
		DirectX::XMFLOAT2 scale = DirectX::XMFLOAT2(1.0f, 1.0f);
		float r = 1.0f, g = 1.0f, b = 1.0f, a = 1.0f;
		DirectX::XMFLOAT2 position = { 0.0f, 0.0f };
		FontPtr m_font;
	};
}