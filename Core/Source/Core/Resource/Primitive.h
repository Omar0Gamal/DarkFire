#pragma once
#include "Mesh.h"
#include "Core/Graphics/Prerequisites.h"

namespace Core
{
	class Primitive : public Mesh
	{
	public:
		Primitive(PrimitiveType type, ResourceManager* manager);
		Primitive(PrimitiveType type, MaterialPtr material, ResourceManager* manager);

		MaterialPtr getMaterial() const { return m_material; }

	private:
		static void generateCube(std::vector<VertexMesh>& vertices, std::vector<unsigned int>& indices);
		static void generateSphere(std::vector<VertexMesh>& vertices, std::vector<unsigned int>& indices, 
			unsigned int slices = 64, unsigned int stacks = 64);
		static void generatePlane(std::vector<VertexMesh>& vertices, std::vector<unsigned int>& indices,
			unsigned int subdivisions = 32);

		static void generatePrimitive(PrimitiveType type, 
			std::vector<VertexMesh>& vertices, std::vector<unsigned int>& indices);

	private:
		MaterialPtr m_material;

	private:
		friend class GraphicsEngine;
	};
}

