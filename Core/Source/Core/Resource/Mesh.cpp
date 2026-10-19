#include "Mesh.h"

#include "ResourceManager.h"
#include "Core/Game/Game.h"
#include "Core/Graphics/GraphicsEngine.h"

#define TINYOBJLOADER_IMPLEMENTATION
#include <tiny_obj_loader.h>

#include <locale>
#include <codecvt>
#include <filesystem>

#include "Core/Math/VertexMesh.h"

namespace Core
{
	Mesh::Mesh(const wchar_t* full_path, ResourceManager* manager) : Resource(full_path, manager)
	{
		tinyobj::attrib_t attribs;
		std::vector<tinyobj::shape_t> shapes;
		std::vector<tinyobj::material_t> materials;

		std::string warn;
		std::string err;

		auto inputfile = std::filesystem::path(full_path).string();
		std::string mtldir = "";
		auto lastSlash = inputfile.find_last_of("\\/");
		if (lastSlash != std::string::npos)
		{
			mtldir = inputfile.substr(0, lastSlash);
		}

		bool res = tinyobj::LoadObj(&attribs, &shapes, &materials, &warn, &err, inputfile.c_str(), mtldir.c_str());

		if (!res)
		{
			std::string errorMsg = "Failed to load obj mesh: " + inputfile;
			if (!err.empty()) errorMsg += " (" + err + ")";
			throw std::runtime_error(errorMsg);
		}

		size_t num_materials = materials.empty() ? 1 : materials.size();
		m_mat_slots.resize(num_materials);

		std::vector<VertexMesh> list_vertices;
		std::vector<unsigned int> list_indices;
		std::vector<std::vector<unsigned int>> slot_indices(num_materials);

		std::unordered_map<VertexMesh, unsigned int, VertexMeshHash> unique_vertices;

		for (size_t s = 0; s < shapes.size(); s++)
		{
			size_t index_offset = 0;
			for (size_t f = 0; f < shapes[s].mesh.num_face_vertices.size(); f++)
			{
				unsigned char num_face_verts = shapes[s].mesh.num_face_vertices[f];

				int mat_id = 0;
				if (!shapes[s].mesh.material_ids.empty() && f < shapes[s].mesh.material_ids.size())
				{
					mat_id = shapes[s].mesh.material_ids[f];
				}
				if (mat_id < 0 || (size_t)mat_id >= num_materials)
				{
					mat_id = 0;
				}

				Vector3d vertices_face[3];
				Vector2d texcoords_face[3];

				for (unsigned char v = 0; v < num_face_verts && v < 3; v++)
				{
					tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

					tinyobj::real_t vx = attribs.vertices[(int)idx.vertex_index * 3 + 0];
					tinyobj::real_t vy = attribs.vertices[(int)idx.vertex_index * 3 + 1];
					tinyobj::real_t vz = -attribs.vertices[(int)idx.vertex_index * 3 + 2];

					tinyobj::real_t tx = 0;
					tinyobj::real_t ty = 0;
					if (attribs.texcoords.size() && idx.texcoord_index >= 0)
					{
						tx = attribs.texcoords[(int)idx.texcoord_index * 2 + 0];
						ty = 1.0f - attribs.texcoords[(int)idx.texcoord_index * 2 + 1];
					}

					vertices_face[v] = Vector3d(vx, vy, vz);
					texcoords_face[v] = Vector2d(tx, ty);
				}

				Vector3d tangent, binormal;
				computeTangents(
					vertices_face[0], vertices_face[1], vertices_face[2],
					texcoords_face[0], texcoords_face[1], texcoords_face[2],
					tangent, binormal);

				for (unsigned char v = 0; v < num_face_verts; v++)
				{
					tinyobj::index_t idx = shapes[s].mesh.indices[index_offset + v];

					tinyobj::real_t vx = attribs.vertices[(int)idx.vertex_index * 3 + 0];
					tinyobj::real_t vy = attribs.vertices[(int)idx.vertex_index * 3 + 1];
					tinyobj::real_t vz = -attribs.vertices[(int)idx.vertex_index * 3 + 2];

					m_maxCorner.x = (std::max)(m_maxCorner.x, vx);
					m_maxCorner.y = (std::max)(m_maxCorner.y, vy);
					m_maxCorner.z = (std::max)(m_maxCorner.z, vz);

					m_minCorner.x = (std::min)(m_minCorner.x, vx);
					m_minCorner.y = (std::min)(m_minCorner.y, vy);
					m_minCorner.z = (std::min)(m_minCorner.z, vz);

					tinyobj::real_t tx = 0;
					tinyobj::real_t ty = 0;
					if (attribs.texcoords.size() && idx.texcoord_index >= 0)
					{
						tx = attribs.texcoords[(int)idx.texcoord_index * 2 + 0];
						ty = 1.0f - attribs.texcoords[(int)idx.texcoord_index * 2 + 1];
					}

					tinyobj::real_t nx = 0;
					tinyobj::real_t ny = 0;
					tinyobj::real_t nz = 0;
					if (attribs.normals.size() && idx.normal_index >= 0)
					{
						nx = attribs.normals[(int)idx.normal_index * 3 + 0];
						ny = attribs.normals[(int)idx.normal_index * 3 + 1];
						nz = -attribs.normals[(int)idx.normal_index * 3 + 2];
					}

					Vector3d v_binormal = Vector3d::cross(Vector3d(nx, ny, nz), tangent);
					Vector3d v_tangent = Vector3d::cross(v_binormal, Vector3d(nx, ny, nz));

					VertexMesh vertex(Vector3d(vx, vy, vz), Vector2d(tx, ty), Vector3d(nx, ny, nz), v_tangent, v_binormal);

					auto it = unique_vertices.find(vertex);
					unsigned int vert_index = 0;
					if (it == unique_vertices.end())
					{
						vert_index = static_cast<unsigned int>(list_vertices.size());
						unique_vertices[vertex] = vert_index;
						list_vertices.push_back(vertex);
					}
					else
					{
						vert_index = it->second;
					}

					slot_indices[mat_id].push_back(vert_index);
				}

				index_offset += num_face_verts;
			}
		}

		// Flatten slot indices into unified index buffer with slot boundaries
		for (size_t m = 0; m < num_materials; m++)
		{
			m_mat_slots[m].material_id = static_cast<unsigned int>(m);
			m_mat_slots[m].start_index = list_indices.size();
			m_mat_slots[m].num_indices = slot_indices[m].size();
			list_indices.insert(list_indices.end(), slot_indices[m].begin(), slot_indices[m].end());
		}

		if (list_vertices.empty() || list_indices.empty())
		{
			// Fallback: 1 quad triangle pair if model was completely empty
			VertexMesh v0(Vector3d(-0.5f, -0.5f, 0.0f), Vector2d(0, 1), Vector3d(0, 0, -1), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
			VertexMesh v1(Vector3d(-0.5f,  0.5f, 0.0f), Vector2d(0, 0), Vector3d(0, 0, -1), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
			VertexMesh v2(Vector3d( 0.5f,  0.5f, 0.0f), Vector2d(1, 0), Vector3d(0, 0, -1), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
			VertexMesh v3(Vector3d( 0.5f, -0.5f, 0.0f), Vector2d(1, 1), Vector3d(0, 0, -1), Vector3d(1, 0, 0), Vector3d(0, 1, 0));
			list_vertices = { v0, v1, v2, v3 };
			list_indices = { 0, 1, 2, 0, 2, 3 };
			m_mat_slots.resize(1);
			m_mat_slots[0] = { 0, 6, 0 };
			m_minCorner = Vector3d(-0.5f, -0.5f, 0.0f);
			m_maxCorner = Vector3d(0.5f, 0.5f, 0.0f);
		}

		auto engine = m_resManager->getGame()->getGraphicsEngine();
		m_vertex_buffer = engine->CreateVertexBuffer({ list_vertices.data(), sizeof(VertexMesh), (UINT)list_vertices.size() });
		m_index_buffer = engine->CreateIndexBuffer({ list_indices.data(), (UINT)list_indices.size() });
	}

	Mesh::Mesh(VertexMesh* vertex_list_data, unsigned int vertex_list_size,
	unsigned int* index_list_data, unsigned int index_list_size,
	MaterialSlot* material_slot_list, unsigned int material_slot_list_size, ResourceManager* manager) :
	Resource(L"", manager)
	{
	// Skip buffer creation if no vertex data is provided (e.g., Primitive subclass handles its own buffers)
	if (vertex_list_data == nullptr || vertex_list_size == 0)
	return;

	auto engine = m_resManager->getGame()->getGraphicsEngine();

	void* shader_byte_code = nullptr;
	size_t size_shader = 0;
	m_vertex_buffer = engine->CreateVertexBuffer({ vertex_list_data,
	sizeof(VertexMesh),
	(UINT)vertex_list_size });
	m_index_buffer = engine->CreateIndexBuffer({ index_list_data,
	(UINT)index_list_size });

	m_mat_slots.resize(material_slot_list_size);

	for (unsigned int i = 0; i < material_slot_list_size; i++)
	{
	m_mat_slots[i] = material_slot_list[i];
	}

	}

	const VertexBufferPtr& Mesh::getVertexBuffer()
	{
		return m_vertex_buffer;
	}

	const IndexBufferPtr& Mesh::getIndexBuffer()
	{
		return m_index_buffer;
	}

	const MaterialSlot& Mesh::getMaterialSlot(unsigned int slot)
	{
		return m_mat_slots[slot];
	}

	size_t Mesh::getNumMaterialSlots()
	{
		return m_mat_slots.size();
	}

	Vector3d Mesh::getMaxCorner()
	{
		return m_maxCorner;
	}

	Vector3d Mesh::getMinCorner()
	{
		return m_minCorner;
	}

	void Mesh::computeTangents(const  Vector3d& v0,
		const  Vector3d& v1,
		const  Vector3d& v2,
		const  Vector2d& t0,
		const  Vector2d& t1,
		const  Vector2d& t2,
		Vector3d& tangent, Vector3d& binormal)
	{
		Vector3d deltaPos1 = v1 - v0;
		Vector3d deltaPos2 = v2 - v0;

		Vector2d deltaUV1 = t1 - t0;
		Vector2d deltaUV2 = t2 - t0;


		float r = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV1.y * deltaUV2.x);
		tangent = (deltaPos1 * deltaUV2.y - deltaPos2 * deltaUV1.y);
		tangent = Vector3d::normalize(tangent);
		binormal = (deltaPos2 * deltaUV1.x - deltaPos1 * deltaUV2.x);
		binormal = Vector3d::normalize(binormal);
	}
}