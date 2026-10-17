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
	private:
		std::unique_ptr<DirectX::DX11::SpriteFont> m_font;
		std::unique_ptr<DirectX::DX11::SpriteBatch> m_batch;

		GraphicsEngine* m_system = nullptr;

		friend class Game;
		friend class UIRenderPass;
	};
}


