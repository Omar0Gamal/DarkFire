#include "CookMeshes.h"
#include <iostream>
#include <fstream>
#include <cmath>
#include <set>
#include <queue>
#include <map>

namespace dfcook
{
	struct SymMatrix4
	{
		double m[10]{}; // Symmetric 4x4 matrix stored as 10 doubles:
		                // 00, 01, 02, 03, 11, 12, 13, 22, 23, 33

		SymMatrix4() = default;
		SymMatrix4(double a, double b, double c, double d)
		{
			m[0] = a * a; m[1] = a * b; m[2] = a * c; m[3] = a * d;
			              m[4] = b * b; m[5] = b * c; m[6] = b * d;
			                            m[7] = c * c; m[8] = c * d;
			                                          m[9] = d * d;
		}

		SymMatrix4& operator+=(const SymMatrix4& o)
		{
			for (int i = 0; i < 10; ++i) m[i] += o.m[i];
			return *this;
		}

		double evaluate(double x, double y, double z) const
		{
			return m[0] * x * x + 2.0 * m[1] * x * y + 2.0 * m[2] * x * z + 2.0 * m[3] * x
			     + m[4] * y * y + 2.0 * m[5] * y * z + 2.0 * m[6] * y
			     + m[7] * z * z + 2.0 * m[8] * z
			     + m[9];
		}
	};

	bool SimplifyMeshQEM(
		const std::vector<float>& positions,
		const std::vector<uint32_t>& inputIndices,
		float targetRatio,
		std::vector<uint32_t>& outputIndices
	)
	{
		size_t vertexCount = positions.size() / 3;
		size_t triCount = inputIndices.size() / 3;
		if (vertexCount < 4 || triCount < 4)
		{
			outputIndices = inputIndices;
			return true;
		}

		size_t targetTriCount = static_cast<size_t>(triCount * std::clamp(targetRatio, 0.05f, 1.0f));
		if (targetTriCount >= triCount)
		{
			outputIndices = inputIndices;
			return true;
		}

		struct Vertex
		{
			double p[3];
			SymMatrix4 q;
			int parent = -1;
		};

		std::vector<Vertex> vertices(vertexCount);
		for (size_t i = 0; i < vertexCount; ++i)
		{
			vertices[i].p[0] = positions[i * 3 + 0];
			vertices[i].p[1] = positions[i * 3 + 1];
			vertices[i].p[2] = positions[i * 3 + 2];
			vertices[i].parent = static_cast<int>(i);
		}

		auto findRoot = [&](auto& self, int i) -> int {
			if (vertices[i].parent == i) return i;
			return vertices[i].parent = self(self, vertices[i].parent);
		};

		struct Triangle
		{
			int v[3];
			bool deleted = false;
		};

		std::vector<Triangle> triangles(triCount);
		for (size_t i = 0; i < triCount; ++i)
		{
			int v0 = static_cast<int>(inputIndices[i * 3 + 0]);
			int v1 = static_cast<int>(inputIndices[i * 3 + 1]);
			int v2 = static_cast<int>(inputIndices[i * 3 + 2]);
			triangles[i].v[0] = v0;
			triangles[i].v[1] = v1;
			triangles[i].v[2] = v2;

			// Triangle plane equation
			double p0[3] = { vertices[v0].p[0], vertices[v0].p[1], vertices[v0].p[2] };
			double p1[3] = { vertices[v1].p[0], vertices[v1].p[1], vertices[v1].p[2] };
			double p2[3] = { vertices[v2].p[0], vertices[v2].p[1], vertices[v2].p[2] };

			double e1[3] = { p1[0] - p0[0], p1[1] - p0[1], p1[2] - p0[2] };
			double e2[3] = { p2[0] - p0[0], p2[1] - p0[1], p2[2] - p0[2] };

			double n[3] = {
				e1[1] * e2[2] - e1[2] * e2[1],
				e1[2] * e2[0] - e1[0] * e2[2],
				e1[0] * e2[1] - e1[1] * e2[0]
			};

			double len = std::sqrt(n[0] * n[0] + n[1] * n[1] + n[2] * n[2]);
			if (len > 1e-12)
			{
				n[0] /= len; n[1] /= len; n[2] /= len;
				double d = -(n[0] * p0[0] + n[1] * p0[1] + n[2] * p0[2]);
				SymMatrix4 K(n[0], n[1], n[2], d);
				vertices[v0].q += K;
				vertices[v1].q += K;
				vertices[v2].q += K;
			}
		}

		// Collect unique candidate edges
		struct Edge
		{
			int u;
			int v;
			double cost;
			double targetPos[3];

			bool operator>(const Edge& o) const noexcept { return cost > o.cost; }
		};

		std::priority_queue<Edge, std::vector<Edge>, std::greater<Edge>> edgeQueue;

		auto calculateEdgeCost = [&](int u, int v) -> Edge {
			SymMatrix4 q = vertices[u].q;
			q += vertices[v].q;

			Edge e;
			e.u = u;
			e.v = v;
			// Midpoint
			e.targetPos[0] = 0.5 * (vertices[u].p[0] + vertices[v].p[0]);
			e.targetPos[1] = 0.5 * (vertices[u].p[1] + vertices[v].p[1]);
			e.targetPos[2] = 0.5 * (vertices[u].p[2] + vertices[v].p[2]);
			e.cost = q.evaluate(e.targetPos[0], e.targetPos[1], e.targetPos[2]);
			return e;
		};

		std::set<std::pair<int, int>> seenEdges;
		for (const auto& tri : triangles)
		{
			for (int j = 0; j < 3; ++j)
			{
				int u = tri.v[j];
				int v = tri.v[(j + 1) % 3];
				if (u > v) std::swap(u, v);
				if (seenEdges.insert({ u, v }).second)
				{
					edgeQueue.push(calculateEdgeCost(u, v));
				}
			}
		}

		size_t currentTriangles = triCount;

		// Iteratively collapse lowest error edges
		while (currentTriangles > targetTriCount && !edgeQueue.empty())
		{
			Edge top = edgeQueue.top();
			edgeQueue.pop();

			int u = findRoot(findRoot, top.u);
			int v = findRoot(findRoot, top.v);
			if (u == v) continue;

			// Collapse v into u
			vertices[v].parent = u;
			vertices[u].p[0] = top.targetPos[0];
			vertices[u].p[1] = top.targetPos[1];
			vertices[u].p[2] = top.targetPos[2];
			vertices[u].q += vertices[v].q;

			// Update triangles and count degenerated triangles
			for (auto& tri : triangles)
			{
				if (tri.deleted) continue;

				int t0 = findRoot(findRoot, tri.v[0]);
				int t1 = findRoot(findRoot, tri.v[1]);
				int t2 = findRoot(findRoot, tri.v[2]);

				if (t0 == t1 || t1 == t2 || t0 == t2)
				{
					tri.deleted = true;
					if (currentTriangles > 0) currentTriangles--;
				}
				else
				{
					tri.v[0] = t0;
					tri.v[1] = t1;
					tri.v[2] = t2;
				}
			}
		}

		outputIndices.clear();
		outputIndices.reserve(currentTriangles * 3);
		for (const auto& tri : triangles)
		{
			if (!tri.deleted)
			{
				outputIndices.push_back(static_cast<uint32_t>(findRoot(findRoot, tri.v[0])));
				outputIndices.push_back(static_cast<uint32_t>(findRoot(findRoot, tri.v[1])));
				outputIndices.push_back(static_cast<uint32_t>(findRoot(findRoot, tri.v[2])));
			}
		}

		return true;
	}

	bool CookMeshes(const std::filesystem::path& inDir, const std::filesystem::path& outDir, CookCache* cache)
	{
		(void)cache;
		if (!std::filesystem::exists(inDir)) return false;
		std::filesystem::create_directories(outDir);
		std::cout << "[dfcook] Mesh LOD decimation ready for directory: " << inDir.string() << std::endl;
		return true;
	}
}
