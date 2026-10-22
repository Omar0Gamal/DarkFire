#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/RHI/IRHI.h"
#include "Editor/SelectionContext.h"
#include <imgui.h>
#include <memory>

namespace Editor
{
	namespace ViewportOverlays
	{
		bool ProjectClippedSegment(
			const Core::Vector3& p0, const Core::Vector3& p1,
			const Core::Matrix& viewProj,
			float startX, float startY, float vpW, float vpH,
			ImVec2& out0, ImVec2& out1
		);

		void DrawGrid3D(
			const Core::Matrix& viewProj,
			const Core::Vector3& cameraPos,
			float startX, float startY, float vpW, float vpH
		);

		void DrawViewportIcons(
			Core::Scene* scene,
			SelectionContext& selection,
			const Core::Matrix& viewProj,
			float startX, float startY, float vpW, float vpH,
			bool viewportHovered
		);

		void DrawSelectionWireframes(
			const SelectionContext& selection,
			const Core::Matrix& viewProj,
			float startX, float startY, float vpW, float vpH
		);

		void DrawOrientationGizmo(
			const Core::Matrix& viewMat,
			const Core::Vector2& viewportPos,
			const Core::Vector2& viewportSize
		);

		void DrawCameraPiP(
			std::shared_ptr<Core::RHI::IGpuTexture> gameViewportTexture,
			const Core::Vector2& viewportPos,
			const Core::Vector2& viewportSize
		);
	}
}
