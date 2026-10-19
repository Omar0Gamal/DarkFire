#pragma once
#include "Prerequisites.h"
#include "SpriteFont.h"
#include "Core/Math/Box.h"

namespace Core
{
	class Font2D
	{
	public:
		Font2D(const wchar_t* full_path, GraphicsEngine* system);
	public:
		Box getBounds(const wchar_t* text);
		DirectX::DX11::SpriteFont* getSpriteFont() const { return m_font.get(); }
		DirectX::DX11::SpriteBatch* getSpriteBatch() const { return m_batch.get(); }

	public:
		std::unique_ptr<DirectX::DX11::SpriteFont> m_font;
		std::unique_ptr<DirectX::DX11::SpriteBatch> m_batch;

	private:
		GraphicsEngine* m_system = nullptr;

		friend class Game;
		friend class UIRenderPass;
		friend class UILabel;
		friend class UIButton;
		friend class UIWorldBadge;
		friend class UICanvas;
	};
}
