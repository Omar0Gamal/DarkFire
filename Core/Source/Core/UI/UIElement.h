#pragma once
#include "Core/Math/Vector2d.h"
#include "Core/Math/Vector4d.h"
#include "Core/Math/Box.h"
#include "Core/Graphics/Prerequisites.h"
#include <string>
#include <memory>
#include <functional>

namespace Core
{
	enum class UIAnchor
	{
		TopLeft,
		TopCenter,
		TopRight,
		CenterLeft,
		Center,
		CenterRight,
		BottomLeft,
		BottomCenter,
		BottomRight
	};

	class UICanvas;

	class UIElement
	{
	public:
		UIElement() = default;
		virtual ~UIElement() = default;

		virtual void update(float deltaTime, const Vector2d& mousePos, bool mouseClicked) {}
		virtual void render(GraphicsEngine* gfx, UICanvas* canvas, FontPtr defaultFont) = 0;

		void setPosition(const Vector2d& pos) { m_position = pos; }
		Vector2d getPosition() const { return m_position; }

		void setSize(const Vector2d& size) { m_size = size; }
		Vector2d getSize() const { return m_size; }

		void setAnchor(UIAnchor anchor) { m_anchor = anchor; }
		UIAnchor getAnchor() const { return m_anchor; }

		void setVisible(bool visible) { m_visible = visible; }
		bool isVisible() const { return m_visible; }

		void setOpacity(float opacity) { m_opacity = opacity; }
		float getOpacity() const { return m_opacity; }

		Vector2d computeScreenPosition(int screenWidth, int screenHeight) const
		{
			Vector2d origin(0, 0);
			switch (m_anchor)
			{
			case UIAnchor::TopLeft: origin = Vector2d(0, 0); break;
			case UIAnchor::TopCenter: origin = Vector2d(screenWidth * 0.5f, 0); break;
			case UIAnchor::TopRight: origin = Vector2d(static_cast<float>(screenWidth), 0); break;
			case UIAnchor::CenterLeft: origin = Vector2d(0, screenHeight * 0.5f); break;
			case UIAnchor::Center: origin = Vector2d(screenWidth * 0.5f, screenHeight * 0.5f); break;
			case UIAnchor::CenterRight: origin = Vector2d(static_cast<float>(screenWidth), screenHeight * 0.5f); break;
			case UIAnchor::BottomLeft: origin = Vector2d(0, static_cast<float>(screenHeight)); break;
			case UIAnchor::BottomCenter: origin = Vector2d(screenWidth * 0.5f, static_cast<float>(screenHeight)); break;
			case UIAnchor::BottomRight: origin = Vector2d(static_cast<float>(screenWidth), static_cast<float>(screenHeight)); break;
			}
			return origin + m_position;
		}

	protected:
		Vector2d m_position = { 0, 0 };
		Vector2d m_size = { 100, 30 };
		UIAnchor m_anchor = UIAnchor::TopLeft;
		bool m_visible = true;
		float m_opacity = 1.0f;
	};
}
