#include "AssetHelper.h"
#include "ResourceManager.h"
#include "Mesh.h"
#include "Material.h"
#include "Texture.h"
#include "Core/Game/World.h"
#include "Core/Game/IEntity.h"
#include "Core/Game/Game.h"
#include "Core/Game/Component/TransformComponent.h"
#include "Core/Game/Component/MeshComponent.h"
#include "Core/Log/Log.h"

namespace Core
{
	MaterialPtr AssetHelper::CreateMaterialWithTexture(
		ResourceManager* resourceManager,
		const std::wstring& texturePath,
		const std::wstring& shaderPath)
	{
		if (!resourceManager)
			return nullptr;

		std::wstring shader = shaderPath;
		if (shader.empty())
		{
			shader = L"Assets/Shaders/base.hlsl";
		}

		MaterialPtr material = resourceManager->createResourceFromFile<Material>(shader.c_str());
		if (!material)
		{
			material = resourceManager->getDefaultMaterial();
		}

		if (material && !texturePath.empty())
		{
			TexturePtr texture = resourceManager->createResourceFromFile<Texture>(texturePath.c_str());
			if (texture)
			{
				material->addTexture(texture);
			}
		}

		return material;
	}

	std::shared_ptr<ModelEntity> AssetHelper::SpawnModel(
		World* world,
		const std::string& name,
		const std::wstring& meshPath,
		const std::wstring& texturePath,
		const Vector3d& position,
		const Vector3d& rotation,
		const Vector3d& scale,
		const std::wstring& shaderPath)
	{
		if (!world)
		{
			darklog.error(L"AssetHelper::SpawnModel called with null World");
			return nullptr;
		}

		ResourceManager* resManager = world->getGame()->getResourceManager();
		MeshPtr mesh = resManager->createResourceFromFile<Mesh>(meshPath.c_str());
		if (!mesh)
		{
			darklog.error(L"AssetHelper::SpawnModel: Failed to load mesh: " + meshPath);
			return nullptr;
		}

		std::vector<MaterialPtr> materials;
		MaterialPtr mat = CreateMaterialWithTexture(resManager, texturePath, shaderPath);
		if (mat)
		{
			materials.push_back(mat);
		}

		auto entity = world->createEntity<ModelEntity>(name.c_str(), mesh, materials);
		if (entity && entity->hasComponent<TransformComponent>())
		{
			entity->getComponent<TransformComponent>()
				.setPosition(position)
				.setRotation(rotation)
				.setScale(scale);
		}

		return entity;
	}
}
