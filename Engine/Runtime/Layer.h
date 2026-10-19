#pragma once

#include "Engine/Core/Defines.h"
#include <string>
#include <cstdint>

namespace Runtime
{
	class DF_API Layer
	{
	public:
		explicit Layer(std::string name = "Layer")
			: m_debugName(std::move(name))
		{
		}

		virtual ~Layer() = default;

		virtual void onAttach() {}
		virtual void onDetach() {}
		virtual void onUpdate(float deltaTime) { (void)deltaTime; }
		virtual void onFixedUpdate(float fixedDeltaTime) { (void)fixedDeltaTime; }
		virtual void onImGuiRender() {}
		virtual void onResize(uint32_t width, uint32_t height) { (void)width; (void)height; }

		const std::string& getName() const noexcept { return m_debugName; }

	protected:
		std::string m_debugName;
	};
}
