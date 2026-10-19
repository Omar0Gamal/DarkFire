#include "Engine/Runtime/LayerStack.h"

namespace Runtime
{
	LayerStack::LayerStack() = default;

	LayerStack::~LayerStack()
	{
		clear();
	}

	void LayerStack::pushLayer(std::shared_ptr<Layer> layer)
	{
		if (layer)
		{
			m_layers.emplace(m_layers.begin() + m_layerInsertIndex, layer);
			++m_layerInsertIndex;
			layer->onAttach();
		}
	}

	void LayerStack::pushOverlay(std::shared_ptr<Layer> overlay)
	{
		if (overlay)
		{
			m_layers.emplace_back(overlay);
			overlay->onAttach();
		}
	}

	void LayerStack::popLayer(const std::shared_ptr<Layer>& layer)
	{
		auto it = std::find(m_layers.begin(), m_layers.begin() + m_layerInsertIndex, layer);
		if (it != m_layers.begin() + m_layerInsertIndex)
		{
			layer->onDetach();
			m_layers.erase(it);
			--m_layerInsertIndex;
		}
	}

	void LayerStack::popOverlay(const std::shared_ptr<Layer>& overlay)
	{
		auto it = std::find(m_layers.begin() + m_layerInsertIndex, m_layers.end(), overlay);
		if (it != m_layers.end())
		{
			overlay->onDetach();
			m_layers.erase(it);
		}
	}

	void LayerStack::clear()
	{
		for (auto& layer : m_layers)
		{
			if (layer)
			{
				layer->onDetach();
			}
		}
		m_layers.clear();
		m_layerInsertIndex = 0;
	}
}
