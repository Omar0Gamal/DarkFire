#include "Font2D.h"
#include "GraphicsEngine.h"
#include "Core/log/Log.h"

namespace Core
{
	Font2D::Font2D(const wchar_t* full_path, GraphicsEngine* system)
	{
		m_system = system;
		m_font = std::make_unique<DirectX::DX11::SpriteFont>(m_system->m_d3dDevice.Get(), full_path);
		if (!m_font)
			darklog.error(L"Font2D not created successfully");
		m_batch = std::make_unique<DirectX::DX11::SpriteBatch>(m_system->m_immediateContext.Get());
		if (!m_batch)
			darklog.error(L"Font2D not created successfully");
	}
	Box Font2D::getBounds(const wchar_t* text)
	{
		if (!m_font) return Box();
		auto rc = m_font->MeasureDrawBounds(text, DirectX::XMFLOAT2(0.0, 0.0));
		return Box(rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top);
	}
}