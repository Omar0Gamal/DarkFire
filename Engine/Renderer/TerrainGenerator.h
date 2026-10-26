#pragma once

#include "Engine/Core/Defines.h"
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Renderer/MeshPrimitives.h"

namespace Core::Renderer
{
	class DF_API TerrainGenerator
	{
	public:
		// Sample procedural height at local (X, Z) coordinate
		[[nodiscard]] static float sampleHeight(const TerrainComponent& tc, float localX, float localZ) noexcept;

		// Sample analytical/finite-difference surface normal at local (X, Z)
		[[nodiscard]] static Math::Vector3 sampleNormal(const TerrainComponent& tc, float localX, float localZ) noexcept;

		// Generate vertex and index buffers for terrain grid
		[[nodiscard]] static MeshData generateTerrainMesh(const TerrainComponent& tc);

	private:
		// Noise primitives
		static float hash21(float x, float z, uint32_t seed) noexcept;
		static float perlinNoise2D(float x, float z, uint32_t seed) noexcept;
		static float fbm(float x, float z, uint32_t seed, uint32_t octaves, float persistence, float lacunarity) noexcept;
		static float ridgedMultifractal(float x, float z, uint32_t seed, uint32_t octaves, float persistence, float lacunarity) noexcept;
	};
}
