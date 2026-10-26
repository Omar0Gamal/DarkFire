#include "TerrainGenerator.h"
#include <algorithm>
#include <cmath>

namespace Core::Renderer
{
	float TerrainGenerator::hash21(float x, float z, uint32_t seed) noexcept
	{
		int32_t ix = static_cast<int32_t>(std::floor(x));
		int32_t iz = static_cast<int32_t>(std::floor(z));
		uint32_t n = static_cast<uint32_t>(ix * 374761393 + iz * 668265263) ^ seed;
		n = (n ^ (n >> 13)) * 1274126177u;
		return static_cast<float>(n & 0x007FFFFFu) / static_cast<float>(0x007FFFFFu);
	}

	float TerrainGenerator::perlinNoise2D(float x, float z, uint32_t seed) noexcept
	{
		int32_t x0 = static_cast<int32_t>(std::floor(x));
		int32_t z0 = static_cast<int32_t>(std::floor(z));
		int32_t x1 = x0 + 1;
		int32_t z1 = z0 + 1;

		float sx = x - static_cast<float>(x0);
		float sz = z - static_cast<float>(z0);

		// Quintic smoothstep for C2 continuity
		float u = sx * sx * sx * (sx * (sx * 6.0f - 15.0f) + 10.0f);
		float v = sz * sz * sz * (sz * (sz * 6.0f - 15.0f) + 10.0f);

		auto gradDot = [&](int32_t ix, int32_t iz, float dx, float dz) {
			float h = hash21(static_cast<float>(ix), static_cast<float>(iz), seed) * 6.283185307f;
			return dx * std::cos(h) + dz * std::sin(h);
		};

		float n00 = gradDot(x0, z0, sx, sz);
		float n10 = gradDot(x1, z0, sx - 1.0f, sz);
		float n01 = gradDot(x0, z1, sx, sz - 1.0f);
		float n11 = gradDot(x1, z1, sx - 1.0f, sz - 1.0f);

		float nx0 = std::lerp(n00, n10, u);
		float nx1 = std::lerp(n01, n11, u);
		float val = std::lerp(nx0, nx1, v);

		return std::clamp(val * 0.7071f + 0.5f, 0.0f, 1.0f);
	}

	float TerrainGenerator::fbm(float x, float z, uint32_t seed, uint32_t octaves, float persistence, float lacunarity) noexcept
	{
		float total = 0.0f;
		float frequency = 1.0f;
		float amplitude = 1.0f;
		float maxValue = 0.0f;

		for (uint32_t i = 0; i < octaves; ++i)
		{
			total += perlinNoise2D(x * frequency, z * frequency, seed + i * 31u) * amplitude;
			maxValue += amplitude;
			amplitude *= persistence;
			frequency *= lacunarity;
		}

		return maxValue > 0.0f ? (total / maxValue) : 0.5f;
	}

	float TerrainGenerator::ridgedMultifractal(float x, float z, uint32_t seed, uint32_t octaves, float persistence, float lacunarity) noexcept
	{
		float total = 0.0f;
		float frequency = 1.0f;
		float amplitude = 1.0f;
		float weight = 1.0f;
		float maxValue = 0.0f;

		for (uint32_t i = 0; i < octaves; ++i)
		{
			float val = perlinNoise2D(x * frequency, z * frequency, seed + i * 47u);
			val = 1.0f - std::abs(val * 2.0f - 1.0f);
			val *= val;
			val *= weight;
			weight = std::clamp(val * 2.0f, 0.0f, 1.0f);

			total += val * amplitude;
			maxValue += amplitude;
			amplitude *= persistence;
			frequency *= lacunarity;
		}

		return maxValue > 0.0f ? (total / maxValue) : 0.5f;
	}

	float TerrainGenerator::sampleHeight(const TerrainComponent& tc, float localX, float localZ) noexcept
	{
		// Domain warping seeded by tc.seed
		float warpFreq = tc.frequency * 0.6f;
		float qx = fbm(localX * warpFreq, localZ * warpFreq, tc.seed + 101u, 3u, 0.5f, 2.0f);
		float qz = fbm(localX * warpFreq + 5.2f, localZ * warpFreq + 1.3f, tc.seed + 203u, 3u, 0.5f, 2.0f);

		float warpAmp = 12.0f;
		float warpedX = localX + (qx * 2.0f - 1.0f) * warpAmp;
		float warpedZ = localZ + (qz * 2.0f - 1.0f) * warpAmp;

		// Warped multi-octave fBm + ridged multifractal
		float baseNoise = fbm(warpedX * tc.frequency, warpedZ * tc.frequency, tc.seed, tc.octaves, tc.persistence, tc.lacunarity);
		float ridgeNoise = ridgedMultifractal(warpedX * tc.frequency * 1.5f, warpedZ * tc.frequency * 1.5f, tc.seed + 500u, std::min(tc.octaves, 4u), tc.persistence, tc.lacunarity);
		float combinedNoise = std::lerp(baseNoise, ridgeNoise, std::clamp(tc.ridgeWeight, 0.0f, 1.0f));
		combinedNoise = std::clamp(combinedNoise, 0.0f, 1.0f);

		if (tc.isIsland)
		{
			float dist = std::sqrt(localX * localX + localZ * localZ);
			float r = dist / std::max(tc.islandRadius, 1.0f);
			float falloff = 0.0f;
			if (r < 1.0f)
			{
				float inner = std::clamp(1.0f - r * r, 0.0f, 1.0f);
				falloff = std::pow(inner, tc.coastlineFalloff);
			}

			float peakHeight = combinedNoise * tc.maxHeight;
			return std::lerp(tc.seaFloorDepth, peakHeight, falloff);
		}

		return combinedNoise * tc.maxHeight;
	}

	Math::Vector3 TerrainGenerator::sampleNormal(const TerrainComponent& tc, float localX, float localZ) noexcept
	{
		float eps = std::max(tc.worldSize / static_cast<float>(std::max(tc.resolution, 1u)), 0.2f);
		float hL = sampleHeight(tc, localX - eps, localZ);
		float hR = sampleHeight(tc, localX + eps, localZ);
		float hD = sampleHeight(tc, localX, localZ - eps);
		float hU = sampleHeight(tc, localX, localZ + eps);

		Math::Vector3 normal(hL - hR, 2.0f * eps, hD - hU);
		normal.Normalize();
		return normal;
	}

	MeshData TerrainGenerator::generateTerrainMesh(const TerrainComponent& tc)
	{
		MeshData mesh;
		uint32_t res = std::max(tc.resolution, 2u);
		float size = tc.worldSize;
		float halfSize = size * 0.5f;
		float step = size / static_cast<float>(res);

		mesh.vertices.reserve((res + 1) * (res + 1));
		mesh.indices.reserve(res * res * 6);

		for (uint32_t iz = 0; iz <= res; ++iz)
		{
			float localZ = -halfSize + static_cast<float>(iz) * step;
			float v = static_cast<float>(iz) / static_cast<float>(res);

			for (uint32_t ix = 0; ix <= res; ++ix)
			{
				float localX = -halfSize + static_cast<float>(ix) * step;
				float u = static_cast<float>(ix) / static_cast<float>(res);

				float y = sampleHeight(tc, localX, localZ);
				Math::Vector3 normal = sampleNormal(tc, localX, localZ);

				VertexMesh vert{};
				vert.position = Math::Vector3(localX, y, localZ);
				vert.normal = normal;
				vert.texcoord = Math::Vector2(u, v);
				vert.tangent = Math::Vector4(1.0f, 0.0f, 0.0f, 1.0f);
				vert.color = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

				mesh.vertices.push_back(vert);
			}
		}

		for (uint32_t iz = 0; iz < res; ++iz)
		{
			for (uint32_t ix = 0; ix < res; ++ix)
			{
				uint32_t i0 = iz * (res + 1) + ix;
				uint32_t i1 = (iz + 1) * (res + 1) + ix;
				uint32_t i2 = (iz + 1) * (res + 1) + (ix + 1);
				uint32_t i3 = iz * (res + 1) + (ix + 1);

				// Quad triangulated as 2 triangles
				mesh.indices.push_back(i0);
				mesh.indices.push_back(i1);
				mesh.indices.push_back(i2);

				mesh.indices.push_back(i0);
				mesh.indices.push_back(i2);
				mesh.indices.push_back(i3);
			}
		}

		Submesh submesh{};
		submesh.startIndexLocation = 0;
		submesh.indexCount = static_cast<uint32_t>(mesh.indices.size());
		submesh.baseVertexLocation = 0;
		submesh.name = "Terrain";
		mesh.submeshes.push_back(submesh);

		return mesh;
	}
}
