#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Runtime/Layer.h"
#include <vector>
#include <memory>

namespace Runtime
{
	class DF_API LayerStack
	{
	public:
		LayerStack();
		~LayerStack();

		LayerStack(const LayerStack&) = delete;
		LayerStack& operator=(const LayerStack&) = delete;

		void pushLayer(std::shared_ptr<Layer> layer);
		void pushOverlay(std::shared_ptr<Layer> overlay);
		void popLayer(const std::shared_ptr<Layer>& layer);
		void popOverlay(const std::shared_ptr<Layer>& overlay);
		void clear();

		std::vector<std::shared_ptr<Layer>>::iterator begin() { return m_layers.begin(); }
		std::vector<std::shared_ptr<Layer>>::iterator end() { return m_layers.end(); }
		std::vector<std::shared_ptr<Layer>>::reverse_iterator rbegin() { return m_layers.rbegin(); }
		std::vector<std::shared_ptr<Layer>>::reverse_iterator rend() { return m_layers.rend(); }

		std::vector<std::shared_ptr<Layer>>::const_iterator begin() const { return m_layers.begin(); }
		std::vector<std::shared_ptr<Layer>>::const_iterator end() const { return m_layers.end(); }

	private:
		std::vector<std::shared_ptr<Layer>> m_layers;
		uint32_t m_layerInsertIndex = 0;
	};
}
