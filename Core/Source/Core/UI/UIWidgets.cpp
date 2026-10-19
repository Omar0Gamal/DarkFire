#include "UIWidgets.h"
#include "UICanvas.h"
#include "Core/Graphics/GraphicsEngine.h"
#include "Core/Graphics/Font2D.h"
#include "Core/Resource/Font.h"
#include <DirectXMath.h>

namespace Core
{
	// =========================================================================
	// UILabel Implementation
	// =========================================================================

	void UILabel::render(GraphicsEngine* gfx, UICanvas* canvas, FontPtr defaultFont)
	{
		if (!m_visible || m_text.empty() || !defaultFont || !canvas)
			return;

		auto font = defaultFont->getFont();
		if (!font || !font->m_batch || !font->m_font)
			return;

		Vector2d pos = computeScreenPosition(canvas->getWidth(), canvas->getHeight());

		font->m_batch->Begin();

		// Draw drop shadow
		if (m_dropShadow)
		{
			font->m_font->DrawString(
				font->m_batch.get(),
				m_text.c_str(),
				DirectX::XMFLOAT2(pos.x + 2.0f, pos.y + 2.0f),
				DirectX::XMVectorSet(m_shadowColor.x, m_shadowColor.y, m_shadowColor.z, m_shadowColor.w * m_opacity),
				0.0f,
				DirectX::XMFLOAT2(0, 0),
				m_scale
			);
		}

		// Draw main text
		font->m_font->DrawString(
			font->m_batch.get(),
			m_text.c_str(),
			DirectX::XMFLOAT2(pos.x, pos.y),
			DirectX::XMVectorSet(m_color.x, m_color.y, m_color.z, m_color.w * m_opacity),
			0.0f,
			DirectX::XMFLOAT2(0, 0),
			m_scale
		);

		font->m_batch->End();
	}

	// =========================================================================
	// UIImage Implementation
	// =========================================================================

	void UIImage::render(GraphicsEngine* gfx, UICanvas* canvas, FontPtr defaultFont)
	{
		if (!m_visible || !m_texture || !canvas || !gfx)
			return;

		Vector2d pos = computeScreenPosition(canvas->getWidth(), canvas->getHeight());
		Box rc = {
			static_cast<unsigned int>((std::max)(0.0f, pos.x)),
			static_cast<unsigned int>((std::max)(0.0f, pos.y)),
			static_cast<unsigned int>(m_size.x),
			static_cast<unsigned int>(m_size.y)
		};

		gfx->drawImage(m_texture, rc);
	}

	// =========================================================================
	// UIButton Implementation
	// =========================================================================

	void UIButton::update(float deltaTime, const Vector2d& mousePos, bool mouseClicked)
	{
		if (!m_visible)
			return;

		// Note: hover test is computed in render/update with canvas dimensions
	}

	void UIButton::render(GraphicsEngine* gfx, UICanvas* canvas, FontPtr defaultFont)
	{
		if (!m_visible || !canvas || !gfx)
			return;

		Vector2d pos = computeScreenPosition(canvas->getWidth(), canvas->getHeight());

		Box rc = {
			static_cast<unsigned int>((std::max)(0.0f, pos.x)),
			static_cast<unsigned int>((std::max)(0.0f, pos.y)),
			static_cast<unsigned int>(m_size.x),
			static_cast<unsigned int>(m_size.y)
		};

		// Draw button background
		gfx->drawFilledRectangle(rc, nullptr);

		// Draw button label
		if (!m_text.empty() && defaultFont)
		{
			auto font = defaultFont->getFont();
			if (font && font->m_batch && font->m_font)
			{
				font->m_batch->Begin();
				font->m_font->DrawString(
					font->m_batch.get(),
					m_text.c_str(),
					DirectX::XMFLOAT2(pos.x + 12.0f, pos.y + (m_size.y * 0.25f)),
					DirectX::XMVectorSet(m_textColor.x, m_textColor.y, m_textColor.z, m_textColor.w * m_opacity),
					0.0f,
					DirectX::XMFLOAT2(0, 0),
					0.85f
				);
				font->m_batch->End();
			}
		}
	}

	// =========================================================================
	// UIProgressBar Implementation
	// =========================================================================

	void UIProgressBar::render(GraphicsEngine* gfx, UICanvas* canvas, FontPtr defaultFont)
	{
		if (!m_visible || !canvas || !gfx)
			return;

		Vector2d pos = computeScreenPosition(canvas->getWidth(), canvas->getHeight());

		// 1. Background Bar
		Box bgRc = {
			static_cast<unsigned int>((std::max)(0.0f, pos.x)),
			static_cast<unsigned int>((std::max)(0.0f, pos.y)),
			static_cast<unsigned int>(m_size.x),
			static_cast<unsigned int>(m_size.y)
		};
		gfx->drawFilledRectangle(bgRc, nullptr);

		// 2. Foreground Fill Bar
		float fillWidth = m_size.x * m_value;
		if (fillWidth > 1.0f)
		{
			Box fillRc = {
				static_cast<unsigned int>((std::max)(0.0f, pos.x)),
				static_cast<unsigned int>((std::max)(0.0f, pos.y)),
				static_cast<unsigned int>(fillWidth),
				static_cast<unsigned int>(m_size.y)
			};
			gfx->drawFilledRectangle(fillRc, nullptr);
		}
	}

	// =========================================================================
	// UIWorldBadge Implementation
	// =========================================================================

	void UIWorldBadge::updateProjection(const Mat4x4& view, const Mat4x4& proj, int screenWidth, int screenHeight)
	{
		// Manual 4x4 matrix vector transform: clip = worldPos * view * proj
		float x = m_worldPosition.x;
		float y = m_worldPosition.y;
		float z = m_worldPosition.z;

		// View space
		float vx = x * view.mat[0][0] + y * view.mat[1][0] + z * view.mat[2][0] + view.mat[3][0];
		float vy = x * view.mat[0][1] + y * view.mat[1][1] + z * view.mat[2][1] + view.mat[3][1];
		float vz = x * view.mat[0][2] + y * view.mat[1][2] + z * view.mat[2][2] + view.mat[3][2];
		float vw = x * view.mat[0][3] + y * view.mat[1][3] + z * view.mat[2][3] + view.mat[3][3];

		// Clip space
		float cx = vx * proj.mat[0][0] + vy * proj.mat[1][0] + vz * proj.mat[2][0] + vw * proj.mat[3][0];
		float cy = vx * proj.mat[0][1] + vy * proj.mat[1][1] + vz * proj.mat[2][1] + vw * proj.mat[3][1];
		float cw = vx * proj.mat[0][3] + vy * proj.mat[1][3] + vz * proj.mat[2][3] + vw * proj.mat[3][3];

		if (cw > 0.1f)
		{
			m_inFrontOfCamera = true;
			float ndcX = cx / cw;
			float ndcY = cy / cw;

			m_position.x = (ndcX * 0.5f + 0.5f) * screenWidth;
			m_position.y = (-ndcY * 0.5f + 0.5f) * screenHeight - 30.0f; // float 30px above
		}
		else
		{
			m_inFrontOfCamera = false;
		}
	}

	void UIWorldBadge::render(GraphicsEngine* gfx, UICanvas* canvas, FontPtr defaultFont)
	{
		if (!m_visible || !m_inFrontOfCamera || m_text.empty() || !defaultFont)
			return;

		auto font = defaultFont->getFont();
		if (!font || !font->m_batch || !font->m_font)
			return;

		font->m_batch->Begin();

		// Badge text with drop shadow
		font->m_font->DrawString(
			font->m_batch.get(),
			m_text.c_str(),
			DirectX::XMFLOAT2(m_position.x + 2.0f, m_position.y + 2.0f),
			DirectX::XMVectorSet(0.0f, 0.0f, 0.0f, 0.9f),
			0.0f,
			DirectX::XMFLOAT2(0, 0),
			0.9f
		);

		font->m_font->DrawString(
			font->m_batch.get(),
			m_text.c_str(),
			DirectX::XMFLOAT2(m_position.x, m_position.y),
			DirectX::XMVectorSet(m_color.x, m_color.y, m_color.z, m_color.w),
			0.0f,
			DirectX::XMFLOAT2(0, 0),
			0.9f
		);

		font->m_batch->End();
	}
}
