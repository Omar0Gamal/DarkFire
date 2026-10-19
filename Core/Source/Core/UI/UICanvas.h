#pragma once
#include "UIElement.h"
#include "Core/Graphics/Prerequisites.h"
#include "Core/Graphics/RenderPass.h"
#include <vector>
#include <memory>

namespace Core
{
	class InputManager;

	class UICanvas
	{
	public:
		UICanvas(int width = 1280, int height = 720);
		~UICanvas() = default;

		void setScreenSize(int width, int height)
		{
			m_width = width;
			m_height = height;
		}

		int getWidth() const { return m_width; }
		int getHeight() const { return m_height; }

		template <typename T, typename... Args>
		std::shared_ptr<T> addElement(Args&&... args)
		{
			auto elem = std::make_shared<T>(std::forward<Args>(args)...);
			m_elements.push_back(elem);
			return elem;
		}

		void addElement(std::shared_ptr<UIElement> element)
		{
			m_elements.push_back(element);
		}

		void removeElement(std::shared_ptr<UIElement> element)
		{
			m_elements.erase(
				std::remove(m_elements.begin(), m_elements.end(), element),
				m_elements.end()
			);
		}

		void clear()
		{
			m_elements.clear();
		}

		void setDefaultFont(FontPtr font) { m_defaultFont = font; }
		FontPtr getDefaultFont() const { return m_defaultFont; }

		void update(float deltaTime, InputManager* input);
		void render(GraphicsEngine* gfx, const RenderContext& ctx);

		bool isVisible() const { return m_visible; }
		void setVisible(bool visible) { m_visible = visible; }

	private:
		int m_width = 1280;
		int m_height = 720;
		bool m_visible = true;
		FontPtr m_defaultFont;
		std::vector<std::shared_ptr<UIElement>> m_elements;
	};
}
