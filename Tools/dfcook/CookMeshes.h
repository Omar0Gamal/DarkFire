#pragma once

#include "CookCommon.h"
#include <vector>
#include <string>
#include <filesystem>

namespace dfcook
{
	struct MeshLODData
	{
		std::vector<float> positions;      // 3 floats per vertex (x, y, z)
		std::vector<float> normals;        // 3 floats per vertex (nx, ny, nz)
		std::vector<float> texcoords;      // 2 floats per vertex (u, v)
		std::vector<uint32_t> indicesLOD0; // 100%
		std::vector<uint32_t> indicesLOD1; // 50%
		std::vector<uint32_t> indicesLOD2; // 25%
	};

	// Quadric Error Metric (QEM) Decimation
	bool SimplifyMeshQEM(
		const std::vector<float>& positions,
		const std::vector<uint32_t>& inputIndices,
		float targetRatio,
		std::vector<uint32_t>& outputIndices
	);

	bool CookMeshes(const std::filesystem::path& inDir, const std::filesystem::path& outDir, CookCache* cache);
}
