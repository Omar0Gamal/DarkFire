#pragma once
#include "Prerequisites.h"
#include "Core/Math/Mat4x4.h"
#include "Core/Math/Vector4d.h"
#include "RenderStateManager.h"
#include <vector>
#include <memory>

namespace Core
{
	__declspec(align(16))
	struct CBPerObject
	{
		Mat4x4 world;
		Mat4x4 worldInvTranspose;
		Vector4d colorTint;
		Vector4d customParams;
	};

	struct RenderItem
	{
		MeshPtr mesh;
		size_t materialSlotIndex = 0;
		MaterialPtr material;
		Mat4x4 worldMatrix;
		float depth = 0.0f;
		Vector4d colorTint = Vector4d(1, 1, 1, 1);
		Vector4d customParams = Vector4d(0, 0, 0, 0);
	};

	class RenderQueue
	{
	public:
		RenderQueue() = default;
		~RenderQueue() = default;

		void clear();

		void submitOpaque(const RenderItem& item);
		void submitTransparent(const RenderItem& item);
		void submitDebug(const RenderItem& item);

		void sort();

		void execute(GraphicsEngine* gfx, RenderStateManager* stateMgr);

		size_t getOpaqueCount() const { return m_opaqueItems.size(); }
		size_t getTransparentCount() const { return m_transparentItems.size(); }
		size_t getDebugCount() const { return m_debugItems.size(); }

	private:
		std::vector<RenderItem> m_opaqueItems;
		std::vector<RenderItem> m_transparentItems;
		std::vector<RenderItem> m_debugItems;
	};
}
