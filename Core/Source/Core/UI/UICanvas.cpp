#include "UICanvas.h"
#include "Core/Input/InputManager.h"
#include "Core/Graphics/GraphicsEngine.h"
#include "Core/Graphics/RenderStateManager.h"

namespace Core
{
	UICanvas::UICanvas(int width, int height)
		: m_width(width), m_height(height)
	{
	}

	void UICanvas::update(float deltaTime, InputManager* input)
	{
		if (!m_visible || !input)
			return;

		Vector2d mousePos = input->getMousePosition();
		bool mouseClicked = input->isMousePressed(MouseButton::Left);

		for (auto& element : m_elements)
		{
			if (element && element->isVisible())
			{
				element->update(deltaTime, mousePos, mouseClicked);
			}
		}
	}

	void UICanvas::render(GraphicsEngine* gfx, const RenderContext& ctx)
	{
		if (!m_visible || !gfx)
			return;

		m_width = ctx.viewportWidth > 0 ? ctx.viewportWidth : m_width;
		m_height = ctx.viewportHeight > 0 ? ctx.viewportHeight : m_height;

		RenderStateManager* stateMgr = gfx->getRenderStateManager();
		if (stateMgr)
		{
			stateMgr->applyRasterizerState(RasterizerStateType::CullNone);
			stateMgr->applyBlendState(BlendStateType::AlphaBlend);
			stateMgr->applyDepthStencilState(DepthStencilStateType::Disabled);
		}

		for (auto& element : m_elements)
		{
			if (element && element->isVisible())
			{
				element->render(gfx, this, m_defaultFont);
			}
		}
	}
}
