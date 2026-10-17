#include "UIRender.h"
#include "Core/Graphics/GraphicsEngine.h"
#include "Core/log/log.h"
#include "Core/Graphics/Texture2D.h"
#include "Core/Resource/Material.h"

namespace Core
{
	UIRender::UIRender(GraphicsEngine* system)
		: m_system(system)
	{
		m_ImageBatch = std::make_unique<DirectX::SpriteBatch>(m_system->m_immediateContext.Get());
	}
	void UIRender::drawImage(const Texture2DPtr& texture, const Box& size)
	{
		m_ImageBatch->Begin(DirectX::DX11::SpriteSortMode::SpriteSortMode_Deferred, m_system->m_blend_state.Get());
		RECT rc = { static_cast<LONG>(size.left), static_cast<LONG>(size.top), static_cast<LONG>(size.left + size.width), static_cast<LONG>(size.top + size.height) };
		m_ImageBatch->Draw(texture->m_shader_res_view.Get(), rc);
		m_ImageBatch->End();
	}
	void UIRender::drawFilledRectangle(const Box& size)
	{
	}
}
