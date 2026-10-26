#include "Engine/Renderer/DebugRenderer.h"
#include <mutex>
#include <cmath>

namespace Core::Renderer
{
	static std::vector<DebugVertex> s_vertices;
	static std::mutex s_mutex;

	void DebugRenderer::drawLine(const Math::Vector3& p0, const Math::Vector3& p1, const Math::Vector4& color)
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		s_vertices.push_back({ p0, color });
		s_vertices.push_back({ p1, color });
	}

	void DebugRenderer::drawBox(const Math::Vector3& center, const Math::Vector3& size, const Math::Quaternion& rotation, const Math::Vector4& color)
	{
		Math::Vector3 h = size * 0.5f;

		// 8 local corners
		Math::Vector3 corners[8] = {
			Math::Vector3(-h.x, -h.y, -h.z),
			Math::Vector3( h.x, -h.y, -h.z),
			Math::Vector3( h.x, -h.y,  h.z),
			Math::Vector3(-h.x, -h.y,  h.z),
			Math::Vector3(-h.x,  h.y, -h.z),
			Math::Vector3( h.x,  h.y, -h.z),
			Math::Vector3( h.x,  h.y,  h.z),
			Math::Vector3(-h.x,  h.y,  h.z)
		};

		// Transform corners to world space
		for (int i = 0; i < 8; ++i)
		{
			corners[i] = center + Math::Vector3::Transform(corners[i], rotation);
		}

		std::lock_guard<std::mutex> lock(s_mutex);
		auto addEdge = [&](int i0, int i1) {
			s_vertices.push_back({ corners[i0], color });
			s_vertices.push_back({ corners[i1], color });
		};

		// Bottom face
		addEdge(0, 1); addEdge(1, 2); addEdge(2, 3); addEdge(3, 0);
		// Top face
		addEdge(4, 5); addEdge(5, 6); addEdge(6, 7); addEdge(7, 4);
		// Vertical pillars
		addEdge(0, 4); addEdge(1, 5); addEdge(2, 6); addEdge(3, 7);
	}

	void DebugRenderer::drawSphere(const Math::Vector3& center, float radius, const Math::Vector4& color, uint32_t segments)
	{
		if (segments < 4) segments = 4;
		const float step = (2.0f * 3.14159265358979323846f) / static_cast<float>(segments);

		std::lock_guard<std::mutex> lock(s_mutex);

		// XY circle
		for (uint32_t i = 0; i < segments; ++i)
		{
			float a0 = static_cast<float>(i) * step;
			float a1 = static_cast<float>(i + 1) * step;
			Math::Vector3 p0 = center + Math::Vector3(std::cos(a0) * radius, std::sin(a0) * radius, 0.0f);
			Math::Vector3 p1 = center + Math::Vector3(std::cos(a1) * radius, std::sin(a1) * radius, 0.0f);
			s_vertices.push_back({ p0, color });
			s_vertices.push_back({ p1, color });
		}

		// XZ circle
		for (uint32_t i = 0; i < segments; ++i)
		{
			float a0 = static_cast<float>(i) * step;
			float a1 = static_cast<float>(i + 1) * step;
			Math::Vector3 p0 = center + Math::Vector3(std::cos(a0) * radius, 0.0f, std::sin(a0) * radius);
			Math::Vector3 p1 = center + Math::Vector3(std::cos(a1) * radius, 0.0f, std::sin(a1) * radius);
			s_vertices.push_back({ p0, color });
			s_vertices.push_back({ p1, color });
		}

		// YZ circle
		for (uint32_t i = 0; i < segments; ++i)
		{
			float a0 = static_cast<float>(i) * step;
			float a1 = static_cast<float>(i + 1) * step;
			Math::Vector3 p0 = center + Math::Vector3(0.0f, std::cos(a0) * radius, std::sin(a0) * radius);
			Math::Vector3 p1 = center + Math::Vector3(0.0f, std::cos(a1) * radius, std::sin(a1) * radius);
			s_vertices.push_back({ p0, color });
			s_vertices.push_back({ p1, color });
		}
	}

	void DebugRenderer::clear()
	{
		std::lock_guard<std::mutex> lock(s_mutex);
		s_vertices.clear();
	}

	const std::vector<DebugVertex>& DebugRenderer::getVertices()
	{
		return s_vertices;
	}
}
