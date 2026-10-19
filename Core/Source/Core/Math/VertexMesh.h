#pragma once
#include "Vector3d.h"
#include "Vector2d.h"
namespace Core
{
	class VertexMesh
	{
	public:
		VertexMesh() :m_position(), m_texcoord(), m_normal()
		{
		}
		VertexMesh(const  Vector3d& position, const  Vector2d& texcoord, const  Vector3d& normal,
			const  Vector3d& tangent, const  Vector3d& binormal) :
			m_position(position),
			m_texcoord(texcoord),
			m_normal(normal),
			m_tangent(tangent),
			m_binormal(binormal)
		{
		}
		VertexMesh(const  VertexMesh& vertex) :
			m_position(vertex.m_position),
			m_texcoord(vertex.m_texcoord),
			m_normal(vertex.m_normal),
			m_tangent(vertex.m_tangent),
			m_binormal(vertex.m_binormal)
		{
		}

		bool operator==(const VertexMesh& other) const
		{
			return m_position == other.m_position &&
				   m_texcoord == other.m_texcoord &&
				   m_normal == other.m_normal &&
				   m_tangent == other.m_tangent &&
				   m_binormal == other.m_binormal;
		}

	public:
		Vector3d m_position;
		Vector2d m_texcoord;
		Vector3d m_normal;
		Vector3d m_tangent;
		Vector3d m_binormal;
	};

	struct VertexMeshHash
	{
		size_t operator()(const VertexMesh& v) const noexcept
		{
			size_t h1 = std::hash<float>{}(v.m_position.x) ^ (std::hash<float>{}(v.m_position.y) << 1) ^ (std::hash<float>{}(v.m_position.z) << 2);
			size_t h2 = std::hash<float>{}(v.m_texcoord.x) ^ (std::hash<float>{}(v.m_texcoord.y) << 1);
			size_t h3 = std::hash<float>{}(v.m_normal.x) ^ (std::hash<float>{}(v.m_normal.y) << 1) ^ (std::hash<float>{}(v.m_normal.z) << 2);
			return h1 ^ (h2 << 1) ^ (h3 << 2);
		}
	};
}