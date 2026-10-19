#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/RHI/IRHI.h"
#include "Engine/Renderer/DrawItem.h"
#include "Engine/Renderer/Material.h"
#include <cstdint>
#include <memory>
#include <vector>

namespace Core::Renderer
{
	struct DF_API RenderMeshInstance
	{
		Math::Matrix world{};
		uint32_t entityId = 0;
		std::shared_ptr<Material> material;
		std::shared_ptr<RHI::IGpuBuffer> vertexBuffer;
		std::shared_ptr<RHI::IGpuBuffer> indexBuffer;
		uint32_t vertexStride = 0;
		uint32_t indexCount = 0;
		uint32_t startIndex = 0;
		int32_t baseVertex = 0;
		RenderLayer layer = RenderLayer::Opaque;
		float distanceToCamera = 0.0f;
	};

	struct DF_API RenderLight
	{
		Math::Vector3 position = Math::Vector3(0.0f, 0.0f, 0.0f);
		Math::Vector3 direction = Math::Vector3(0.0f, -1.0f, 0.0f);
		Math::Vector3 color = Math::Vector3(1.0f, 1.0f, 1.0f);
		float intensity = 1.0f;
		float range = 10.0f;
		float innerConeAngle = 0.785f; // ~45 deg
		float outerConeAngle = 0.872f; // ~50 deg
		uint32_t type = 0;             // 0 = Directional, 1 = Point, 2 = Spot
	};

	struct DF_API RenderCamera
	{
		Math::Matrix viewMatrix{};
		Math::Matrix projMatrix{};
		Math::Matrix viewProjMatrix{};
		Math::Matrix invViewMatrix{};
		Math::Matrix invProjMatrix{};
		Math::Matrix invViewProjMatrix{};
		Math::Vector3 position = Math::Vector3(0.0f, 0.0f, 0.0f);
		Math::Vector3 forward = Math::Vector3(0.0f, 0.0f, 1.0f);
		float nearZ = 0.1f;
		float farZ = 1000.0f;
		float fov = 1.047f; // ~60 deg
		float aspect = 1.777f;
		bool isOrthographic = false;
	};

	struct DF_API RenderEnvironment
	{
		Math::Vector4 ambientColor = Math::Vector4(0.03f, 0.03f, 0.03f, 1.0f);
		float ambientIntensity = 1.0f;
		std::shared_ptr<RHI::IGpuTexture> skyboxTexture;
	};

	struct DF_API RenderScene
	{
		RenderCamera camera;
		RenderEnvironment environment;
		std::vector<RenderMeshInstance> meshes;
		std::vector<RenderLight> lights;

		void clear()
		{
			meshes.clear();
			lights.clear();
		}
	};

	struct DF_API RenderView
	{
		uint32_t width = 1280;
		uint32_t height = 720;
		std::shared_ptr<RHI::IGpuTexture> targetTexture; // Offscreen target or null for backbuffer
		std::shared_ptr<RHI::ISwapChain> swapChain;      // Optional swapchain
		bool isEditorView = false;
	};
}
