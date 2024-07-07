#pragma once
#include "Core/Graphics/Prerequisites.h"
#include "Resource.h"
#include "Core/Math/Vector2d.h"
#include "Core/Math/Vector3d.h"
#include "Core/Math/VertexMesh.h"

#include <vector>
namespace Core
{
	class Mesh : public Resource
	{
	public:
		Mesh(const wchar_t* full_path, ResourceManager* manager);
		Mesh(VertexMesh* vertex_list_data, unsigned int vertex_list_size,
			unsigned int* index_list_data, unsigned int index_list_size,
			MaterialSlot* material_slot_list, unsigned int material_slot_list_size, ResourceManager* manager);
		const  VertexBufferPtr& getVertexBuffer();
		const  IndexBufferPtr& getIndexBuffer();

		const  MaterialSlot& getMaterialSlot(unsigned int slot);
		size_t getNumMaterialSlots();

		Vector3d getMaxCorner();
		Vector3d getMinCorner();
	private:
		void computeTangents(
			const  Vector3d& v0, const  Vector3d& v1, const  Vector3d& v2,
			const  Vector2d& t0, const  Vector2d& t1, const  Vector2d& t2,
			Vector3d& tangent, Vector3d& binormal);
	private:
		VertexBufferPtr m_vertex_buffer;
		IndexBufferPtr m_index_buffer;
		std::vector<MaterialSlot> m_mat_slots;

		Vector3d m_maxCorner;
		Vector3d m_minCorner = Vector3d(10000, 10000, 10000);

	private:
		friend class  GraphicsEngine;
	};
}

