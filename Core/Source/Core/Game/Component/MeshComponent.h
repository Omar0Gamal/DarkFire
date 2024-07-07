#pragma once
#include <vector>
#include "Core/Graphics/Prerequisites.h"
#include "Core/Game/IComponent.h"
namespace Core
{
    class MeshComponent : public IComponent
    {
    public:
		MeshComponent(MeshPtr mesh, std::vector<MaterialPtr> materials)
			: m_Mesh(mesh), m_Materials(materials)
		{
		}
    public:
		MeshPtr m_Mesh;
		std::vector<MaterialPtr> m_Materials;
    };
}