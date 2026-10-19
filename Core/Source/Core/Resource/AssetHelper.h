#pragma once
#include <string>
#include <memory>
#include <vector>
#include "Core/Math/Vector3d.h"
#include "Core/Graphics/Prerequisites.h"

namespace Core
{
	class World;
	class ModelEntity;
	class ResourceManager;

	class AssetHelper
	{
	public:
		// Create a Material with an optional base texture and shader
		static MaterialPtr CreateMaterialWithTexture(
			ResourceManager* resourceManager,
			const std::wstring& texturePath = L"",
			const std::wstring& shaderPath = L"");

		// Spawn a fully configured ModelEntity into the World in a single call
		static std::shared_ptr<ModelEntity> SpawnModel(
			World* world,
			const std::string& name,
			const std::wstring& meshPath,
			const std::wstring& texturePath = L"",
			const Vector3d& position = Vector3d(0, 0, 0),
			const Vector3d& rotation = Vector3d(0, 0, 0),
			const Vector3d& scale = Vector3d(1, 1, 1),
			const std::wstring& shaderPath = L"");
	};
}
