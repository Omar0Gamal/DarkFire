#include "Primitive.h"

#include "ResourceManager.h"
#include "Core/Game/Game.h"
#include "Core/Graphics/GraphicsEngine.h"
#include "Core/Math/VertexMesh.h"
#include "Material.h"

#include <cmath>
#include <numbers>

namespace Core
{
	Primitive::Primitive(PrimitiveType type, ResourceManager* manager) :
		Mesh(nullptr, 0, nullptr, 0, nullptr, 0, manager)
	{
		// Use default material when none is provided
		m_material = GetDefaultMaterial(manager);
		
		std::vector<VertexMesh> vertices;
		std::vector<unsigned int> indices;

		generatePrimitive(type, vertices, indices);

		MaterialSlot slot;
		slot.start_index = 0;
		slot.num_indices = static_cast<unsigned int>(indices.size());
		slot.material_id = 0;

		auto engine = m_resManager->getGame()->getGraphicsEngine();

		m_vertex_buffer = engine->CreateVertexBuffer({ &vertices[0], sizeof(VertexMesh), 
			static_cast<UINT>(vertices.size()) });
		m_index_buffer = engine->CreateIndexBuffer({ &indices[0], 
			static_cast<UINT>(indices.size()) });

		m_mat_slots.push_back(slot);
	}

	Primitive::Primitive(PrimitiveType type, MaterialPtr material, ResourceManager* manager) :
		Mesh(nullptr, 0, nullptr, 0, nullptr, 0, manager), 
		m_material(material ? material : GetDefaultMaterial(manager))
	{
		std::vector<VertexMesh> vertices;
		std::vector<unsigned int> indices;

		generatePrimitive(type, vertices, indices);

		MaterialSlot slot;
		slot.start_index = 0;
		slot.num_indices = static_cast<unsigned int>(indices.size());
		slot.material_id = 0;

		auto engine = m_resManager->getGame()->getGraphicsEngine();

		m_vertex_buffer = engine->CreateVertexBuffer({ &vertices[0], sizeof(VertexMesh), 
			static_cast<UINT>(vertices.size()) });
		m_index_buffer = engine->CreateIndexBuffer({ &indices[0], 
			static_cast<UINT>(indices.size()) });

		m_mat_slots.push_back(slot);
	}
	
	void Primitive::generatePrimitive(PrimitiveType type, 
		std::vector<VertexMesh>& vertices, std::vector<unsigned int>& indices)
	{
		switch (type)
		{
		case PrimitiveType::Cube:
			generateCube(vertices, indices);
			break;
		case PrimitiveType::Sphere:
			generateSphere(vertices, indices);
			break;
		case PrimitiveType::Plane:
			generatePlane(vertices, indices);
			break;
		}
	}

	void Primitive::generateCube(std::vector<VertexMesh>& vertices, std::vector<unsigned int>& indices)
	{
		vertices.clear();
		indices.clear();

		// Define the 6 faces of a cube with proper normals, tangents, and UVs
		// Each face has 4 vertices to allow for proper normals/UVs at corners

		const float s = 0.5f; // half-size

		// Front face (Z+)
		Vector3d frontNormal(0, 0, 1);
		Vector3d frontTangent(1, 0, 0);
		Vector3d frontBinormal(0, 1, 0);
		vertices.push_back(VertexMesh(Vector3d(-s, -s, s), Vector2d(0, 1), frontNormal, frontTangent, frontBinormal));
		vertices.push_back(VertexMesh(Vector3d(s, -s, s), Vector2d(1, 1), frontNormal, frontTangent, frontBinormal));
		vertices.push_back(VertexMesh(Vector3d(s, s, s), Vector2d(1, 0), frontNormal, frontTangent, frontBinormal));
		vertices.push_back(VertexMesh(Vector3d(-s, s, s), Vector2d(0, 0), frontNormal, frontTangent, frontBinormal));

		// Back face (Z-)
		Vector3d backNormal(0, 0, -1);
		Vector3d backTangent(-1, 0, 0);
		Vector3d backBinormal(0, 1, 0);
		vertices.push_back(VertexMesh(Vector3d(s, -s, -s), Vector2d(0, 1), backNormal, backTangent, backBinormal));
		vertices.push_back(VertexMesh(Vector3d(-s, -s, -s), Vector2d(1, 1), backNormal, backTangent, backBinormal));
		vertices.push_back(VertexMesh(Vector3d(-s, s, -s), Vector2d(1, 0), backNormal, backTangent, backBinormal));
		vertices.push_back(VertexMesh(Vector3d(s, s, -s), Vector2d(0, 0), backNormal, backTangent, backBinormal));

		// Top face (Y+)
		Vector3d topNormal(0, 1, 0);
		Vector3d topTangent(1, 0, 0);
		Vector3d topBinormal(0, 0, -1);
		vertices.push_back(VertexMesh(Vector3d(-s, s, s), Vector2d(0, 1), topNormal, topTangent, topBinormal));
		vertices.push_back(VertexMesh(Vector3d(s, s, s), Vector2d(1, 1), topNormal, topTangent, topBinormal));
		vertices.push_back(VertexMesh(Vector3d(s, s, -s), Vector2d(1, 0), topNormal, topTangent, topBinormal));
		vertices.push_back(VertexMesh(Vector3d(-s, s, -s), Vector2d(0, 0), topNormal, topTangent, topBinormal));

		// Bottom face (Y-)
		Vector3d bottomNormal(0, -1, 0);
		Vector3d bottomTangent(1, 0, 0);
		Vector3d bottomBinormal(0, 0, 1);
		vertices.push_back(VertexMesh(Vector3d(-s, -s, -s), Vector2d(0, 1), bottomNormal, bottomTangent, bottomBinormal));
		vertices.push_back(VertexMesh(Vector3d(s, -s, -s), Vector2d(1, 1), bottomNormal, bottomTangent, bottomBinormal));
		vertices.push_back(VertexMesh(Vector3d(s, -s, s), Vector2d(1, 0), bottomNormal, bottomTangent, bottomBinormal));
		vertices.push_back(VertexMesh(Vector3d(-s, -s, s), Vector2d(0, 0), bottomNormal, bottomTangent, bottomBinormal));

		// Right face (X+)
		Vector3d rightNormal(1, 0, 0);
		Vector3d rightTangent(0, 0, -1);
		Vector3d rightBinormal(0, 1, 0);
		vertices.push_back(VertexMesh(Vector3d(s, -s, s), Vector2d(0, 1), rightNormal, rightTangent, rightBinormal));
		vertices.push_back(VertexMesh(Vector3d(s, -s, -s), Vector2d(1, 1), rightNormal, rightTangent, rightBinormal));
		vertices.push_back(VertexMesh(Vector3d(s, s, -s), Vector2d(1, 0), rightNormal, rightTangent, rightBinormal));
		vertices.push_back(VertexMesh(Vector3d(s, s, s), Vector2d(0, 0), rightNormal, rightTangent, rightBinormal));

		// Left face (X-)
		Vector3d leftNormal(-1, 0, 0);
		Vector3d leftTangent(0, 0, 1);
		Vector3d leftBinormal(0, 1, 0);
		vertices.push_back(VertexMesh(Vector3d(-s, -s, -s), Vector2d(0, 1), leftNormal, leftTangent, leftBinormal));
		vertices.push_back(VertexMesh(Vector3d(-s, -s, s), Vector2d(1, 1), leftNormal, leftTangent, leftBinormal));
		vertices.push_back(VertexMesh(Vector3d(-s, s, s), Vector2d(1, 0), leftNormal, leftTangent, leftBinormal));
		vertices.push_back(VertexMesh(Vector3d(-s, s, -s), Vector2d(0, 0), leftNormal, leftTangent, leftBinormal));

		// Generate indices for 6 faces (2 triangles per face)
		// Using counter-clockwise winding order for front faces
		for (unsigned int face = 0; face < 6; ++face)
		{
		unsigned int base = face * 4;
		indices.push_back(base + 0);
		indices.push_back(base + 2);
		indices.push_back(base + 1);
		indices.push_back(base + 0);
		indices.push_back(base + 3);
		indices.push_back(base + 2);
		}
	}

	void Primitive::generateSphere(std::vector<VertexMesh>& vertices, std::vector<unsigned int>& indices,
		unsigned int slices, unsigned int stacks)
	{
		vertices.clear();
		indices.clear();

		const float radius = 0.5f;
		const float pi = std::numbers::pi_v<float>;

		// Generate vertices
		for (unsigned int stack = 0; stack <= stacks; ++stack)
		{
			float phi = pi * static_cast<float>(stack) / static_cast<float>(stacks);
			float sinPhi = std::sin(phi);
			float cosPhi = std::cos(phi);

			for (unsigned int slice = 0; slice <= slices; ++slice)
			{
				float theta = 2.0f * pi * static_cast<float>(slice) / static_cast<float>(slices);
				float sinTheta = std::sin(theta);
				float cosTheta = std::cos(theta);

				// Position
				float x = sinPhi * cosTheta;
				float y = cosPhi;
				float z = sinPhi * sinTheta;

				Vector3d position(x * radius, y * radius, z * radius);
				Vector3d normal = Vector3d::normalize(Vector3d(x, y, z));

				// UV coordinates
				float u = static_cast<float>(slice) / static_cast<float>(slices);
				float v = static_cast<float>(stack) / static_cast<float>(stacks);
				Vector2d texcoord(u, v);

				// Tangent (derivative with respect to theta)
				Vector3d tangent = Vector3d::normalize(Vector3d(-sinTheta, 0.0f, cosTheta));

				// Binormal (derivative with respect to phi, adjusted)
				Vector3d binormal = Vector3d::cross(normal, tangent);

				vertices.push_back(VertexMesh(position, texcoord, normal, tangent, binormal));
			}
		}

		// Generate indices
		// Using counter-clockwise winding order for front faces
		for (unsigned int stack = 0; stack < stacks; ++stack)
		{
		for (unsigned int slice = 0; slice < slices; ++slice)
		{
		unsigned int current = stack * (slices + 1) + slice;
		unsigned int next = current + slices + 1;

		// First triangle
		indices.push_back(current);
		indices.push_back(current + 1);
		indices.push_back(next);

		// Second triangle
		indices.push_back(current + 1);
		indices.push_back(next + 1);
		indices.push_back(next);
		}
		}
		}

	void Primitive::generatePlane(std::vector<VertexMesh>& vertices, std::vector<unsigned int>& indices,
		unsigned int subdivisions)
	{
		vertices.clear();
		indices.clear();

		const float size = 1.0f;
		const float halfSize = size * 0.5f;
		const float step = size / static_cast<float>(subdivisions);

		Vector3d normal(0, 1, 0);
		Vector3d tangent(1, 0, 0);
		Vector3d binormal(0, 0, 1);

		// Generate vertices in a grid pattern
		for (unsigned int z = 0; z <= subdivisions; ++z)
		{
			for (unsigned int x = 0; x <= subdivisions; ++x)
			{
				float posX = -halfSize + static_cast<float>(x) * step;
				float posZ = -halfSize + static_cast<float>(z) * step;

				Vector3d position(posX, 0.0f, posZ);

				// UV coordinates
				float u = static_cast<float>(x) / static_cast<float>(subdivisions);
				float v = static_cast<float>(z) / static_cast<float>(subdivisions);
				Vector2d texcoord(u, v);

				vertices.push_back(VertexMesh(position, texcoord, normal, tangent, binormal));
			}
		}

		// Generate indices
		// Using counter-clockwise winding order for front faces
		for (unsigned int z = 0; z < subdivisions; ++z)
		{
		for (unsigned int x = 0; x < subdivisions; ++x)
		{
			unsigned int topLeft = z * (subdivisions + 1) + x;
			unsigned int topRight = topLeft + 1;
			unsigned int bottomLeft = topLeft + subdivisions + 1;
			unsigned int bottomRight = bottomLeft + 1;

			// First triangle
			indices.push_back(topLeft);
			indices.push_back(topRight);
			indices.push_back(bottomLeft);

			// Second triangle
			indices.push_back(topRight);
			indices.push_back(bottomRight);
			indices.push_back(bottomLeft);
		}
		}
		}
		}