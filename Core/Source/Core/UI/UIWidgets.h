#pragma once
#include "UIElement.h"
#include "Core/Math/Vector3d.h"
#include "Core/Math/Mat4x4.h"
#include <string>
#include <functional>

namespace Core
{
	// -------------------------------------------------------------------------
	// UILabel: Responsive 2D text with drop-shadow
	// -------------------------------------------------------------------------
	class UILabel : public UIElement
	{
	public:
		UILabel(const std::wstring& text = L"", const Vector2d& pos = { 0, 0 }, UIAnchor anchor = UIAnchor::TopLeft)
			: m_text(text) { m_position = pos; m_anchor = anchor; }

		void setText(const std::wstring& text) { m_text = text; }
		const std::wstring& getText() const { return m_text; }

		void setColor(const Vector4d& color) { m_color = color; }
		Vector4d getColor() const { return m_color; }

		void setScale(float scale) { m_scale = scale; }
		float getScale() const { return m_scale; }

		void setDropShadow(bool shadow) { m_dropShadow = shadow; }

		void render(GraphicsEngine* gfx, UICanvas* canvas, FontPtr defaultFont) override;

	private:
		std::wstring m_text;
		Vector4d m_color = Vector4d(1, 1, 1, 1);
		Vector4d m_shadowColor = Vector4d(0, 0, 0, 0.85f);
		float m_scale = 1.0f;
		bool m_dropShadow = true;
	};

	// -------------------------------------------------------------------------
	// UIImage: 2D texture display with tint
	// -------------------------------------------------------------------------
	class UIImage : public UIElement
	{
	public:
		UIImage(const Texture2DPtr& texture = nullptr, const Vector2d& pos = { 0, 0 }, const Vector2d& size = { 64, 64 }, UIAnchor anchor = UIAnchor::TopLeft)
			: m_texture(texture) { m_position = pos; m_size = size; m_anchor = anchor; }

		void setTexture(const Texture2DPtr& tex) { m_texture = tex; }
		Texture2DPtr getTexture() const { return m_texture; }

		void setTint(const Vector4d& tint) { m_tint = tint; }
		Vector4d getTint() const { return m_tint; }

		void render(GraphicsEngine* gfx, UICanvas* canvas, FontPtr defaultFont) override;

	private:
		Texture2DPtr m_texture;
		Vector4d m_tint = Vector4d(1, 1, 1, 1);
	};

	// -------------------------------------------------------------------------
	// UIButton: Interactive button with hover/click states
	// -------------------------------------------------------------------------
	class UIButton : public UIElement
	{
	public:
		UIButton(const std::wstring& text, const Vector2d& pos = { 0, 0 }, const Vector2d& size = { 160, 40 }, UIAnchor anchor = UIAnchor::TopLeft)
			: m_text(text) { m_position = pos; m_size = size; m_anchor = anchor; }

		void setText(const std::wstring& text) { m_text = text; }
		const std::wstring& getText() const { return m_text; }

		void setOnClick(std::function<void()> cb) { m_onClick = cb; }

		void update(float deltaTime, const Vector2d& mousePos, bool mouseClicked) override;
		void render(GraphicsEngine* gfx, UICanvas* canvas, FontPtr defaultFont) override;

	public:
		Vector4d m_normalColor = Vector4d(0.18f, 0.22f, 0.3f, 0.9f);
		Vector4d m_hoverColor = Vector4d(0.28f, 0.5f, 0.85f, 1.0f);
		Vector4d m_pressedColor = Vector4d(0.12f, 0.35f, 0.65f, 1.0f);
		Vector4d m_textColor = Vector4d(1, 1, 1, 1);

	private:
		std::wstring m_text;
		std::function<void()> m_onClick = nullptr;
		bool m_isHovered = false;
		bool m_isPressed = false;
	};

	// -------------------------------------------------------------------------
	// UIProgressBar: Gauge for speedometers and boost accumulators
	// -------------------------------------------------------------------------
	class UIProgressBar : public UIElement
	{
	public:
		UIProgressBar(float value = 0.0f, const Vector2d& pos = { 0, 0 }, const Vector2d& size = { 200, 20 }, UIAnchor anchor = UIAnchor::BottomRight)
			: m_value(value) { m_position = pos; m_size = size; m_anchor = anchor; }

		void setValue(float v) { m_value = (std::max)(0.0f, (std::min)(1.0f, v)); }
		float getValue() const { return m_value; }

		void setFillColor(const Vector4d& color) { m_fillColor = color; }
		void setBackgroundColor(const Vector4d& color) { m_bgColor = color; }

		void render(GraphicsEngine* gfx, UICanvas* canvas, FontPtr defaultFont) override;

	private:
		float m_value = 0.0f;
		Vector4d m_bgColor = Vector4d(0.1f, 0.12f, 0.16f, 0.85f);
		Vector4d m_fillColor = Vector4d(0.0f, 0.75f, 1.0f, 1.0f);
	};

	// -------------------------------------------------------------------------
	// UIWorldBadge: Projects 3D world position to 2D screen overlay
	// -------------------------------------------------------------------------
	class UIWorldBadge : public UIElement
	{
	public:
		UIWorldBadge(const std::wstring& text = L"", const Vector3d& worldPos = { 0, 0, 0 })
			: m_text(text), m_worldPosition(worldPos) {}

		void setWorldPosition(const Vector3d& pos) { m_worldPosition = pos; }
		Vector3d getWorldPosition() const { return m_worldPosition; }

		void setText(const std::wstring& text) { m_text = text; }
		const std::wstring& getText() const { return m_text; }

		void setColor(const Vector4d& color) { m_color = color; }

		void updateProjection(const Mat4x4& view, const Mat4x4& proj, int screenWidth, int screenHeight);
		void render(GraphicsEngine* gfx, UICanvas* canvas, FontPtr defaultFont) override;

	private:
		Vector3d m_worldPosition = { 0, 0, 0 };
		std::wstring m_text;
		Vector4d m_color = Vector4d(1, 0.9f, 0.2f, 1.0f);
		bool m_inFrontOfCamera = true;
	};
}
