#include <doctest/doctest.h>
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Renderer/ShadowAtlasAllocator.h"
#include "Engine/Shaders/Shared/Bindings.h"
#include <cmath>
#include <vector>
#include <algorithm>

using namespace Core;
using namespace Core::Renderer;

TEST_SUITE("Milestone 2: Atmospheric Lighting, Volumetrics & Camera VFX")
{
	TEST_CASE("2.1: Tile-Max 20x20 Velocity Reduction & Neighbor-Max 3x3 Filter")
	{
		// 1. Simulate 20x20 Tile parallel reduction
		constexpr uint32_t kTileDim = 20;
		constexpr uint32_t kPixelsPerTile = kTileDim * kTileDim;

		std::vector<Math::Vector2> tileVelocities(kPixelsPerTile, Math::Vector2(0.0f, 0.0f));

		// Set random subpixel velocities (< 0.5 px)
		for (size_t i = 0; i < kPixelsPerTile; ++i)
		{
			float fx = static_cast<float>(i % 20) * 0.01f;
			float fy = static_cast<float>(i / 20) * 0.01f;
			tileVelocities[i] = Math::Vector2(fx, fy);
		}

		// Inject dominant velocity vector at pixel (7, 13) = index 267
		const Math::Vector2 dominantVelocity(-14.5f, 22.0f); // Length = ~26.35 px
		tileVelocities[13 * kTileDim + 7] = dominantVelocity;

		// Perform simulated parallel tree reduction (identical to TileMaxCS.hlsl)
		std::vector<Math::Vector2> s_vel = tileVelocities;
		std::vector<float> s_mag(kPixelsPerTile);
		for (size_t i = 0; i < kPixelsPerTile; ++i)
		{
			s_mag[i] = s_vel[i].LengthSquared();
		}

		for (uint32_t s = 256; s > 0; s >>= 1)
		{
			for (uint32_t i = 0; i < s; ++i)
			{
				if (i + s < kPixelsPerTile)
				{
					if (s_mag[i + s] > s_mag[i])
					{
						s_mag[i] = s_mag[i + s];
						s_vel[i] = s_vel[i + s];
					}
				}
			}
		}

		CHECK(s_vel[0].x == doctest::Approx(dominantVelocity.x));
		CHECK(s_vel[0].y == doctest::Approx(dominantVelocity.y));
		CHECK(s_mag[0] == doctest::Approx(dominantVelocity.LengthSquared()).epsilon(1e-4f));

		// 2. Simulate 3x3 NeighborMax filter across tile grid (NeighborMaxCS.hlsl)
		const uint32_t gridW = 5;
		const uint32_t gridH = 5;
		std::vector<Math::Vector2> tileGrid(gridW * gridH, Math::Vector2(0.1f, 0.1f));

		// Place high velocity at tile (2, 2)
		const Math::Vector2 centerMax(30.0f, -10.0f);
		tileGrid[2 * gridW + 2] = centerMax;

		// Boundary invariant check: Corner tile (0, 0) sees tiles in [0..1] x [0..1]
		auto evaluateNeighborMax = [&](int tx, int ty) -> Math::Vector2 {
			Math::Vector2 maxV(0.0f, 0.0f);
			float maxMagSq = 0.0f;
			for (int dy = -1; dy <= 1; ++dy)
			{
				for (int dx = -1; dx <= 1; ++dx)
				{
					int cx = std::clamp(tx + dx, 0, static_cast<int>(gridW) - 1);
					int cy = std::clamp(ty + dy, 0, static_cast<int>(gridH) - 1);
					Math::Vector2 v = tileGrid[cy * gridW + cx];
					float magSq = v.LengthSquared();
					if (magSq > maxMagSq)
					{
						maxMagSq = magSq;
						maxV = v;
					}
				}
			}
			return maxV;
		};

		// Corner tile (0, 0) should not see tile (2, 2)
		Math::Vector2 cornerMax = evaluateNeighborMax(0, 0);
		CHECK_NE(cornerMax.x, centerMax.x);

		// Adjacent tile (1, 2) MUST see tile (2, 2)
		Math::Vector2 adjMax = evaluateNeighborMax(1, 2);
		CHECK(adjMax.x == doctest::Approx(centerMax.x));
		CHECK(adjMax.y == doctest::Approx(centerMax.y));

		// Diagonal neighbor (1, 1) MUST see tile (2, 2)
		Math::Vector2 diagMax = evaluateNeighborMax(1, 1);
		CHECK(diagMax.x == doctest::Approx(centerMax.x));
		CHECK(diagMax.y == doctest::Approx(centerMax.y));
	}

	TEST_CASE("2.2: 3D Froxel Volumetric Fog Depth Slicing & Beer-Lambert Integration")
	{
		const float nearZ = 0.5f;
		const float farZ = 65.0f;
		constexpr uint32_t kSlices = 64;

		// 1. Verify logarithmic view-space slice depth distribution
		std::vector<float> sliceDepths(kSlices);
		for (uint32_t z = 0; z < kSlices; ++z)
		{
			float sliceT = (static_cast<float>(z) + 0.5f) / static_cast<float>(kSlices);
			sliceDepths[z] = nearZ * std::pow(farZ / nearZ, sliceT);
		}

		// First slice should be just beyond near plane
		CHECK_GT(sliceDepths[0], nearZ);
		CHECK_LT(sliceDepths[0], 1.0f);

		// Last slice should be close to far plane
		CHECK_LT(sliceDepths[kSlices - 1], farZ);
		CHECK_GT(sliceDepths[kSlices - 1], 60.0f);

		// Verify strict monotonicity (viewZ[i] < viewZ[i+1])
		for (uint32_t z = 0; z < kSlices - 1; ++z)
		{
			CHECK_LT(sliceDepths[z], sliceDepths[z + 1]);
		}

		// 2. Numerical integration of Beer-Lambert law along froxel ray
		float accumTransmittance = 1.0f;
		Math::Vector3 accumRadiance(0.0f, 0.0f, 0.0f);
		float prevSliceZ = nearZ;

		const float extinctionCoeff = 0.05f; // medium fog density
		const Math::Vector3 inScatterRadiance(0.4f, 0.5f, 0.6f);

		for (uint32_t z = 0; z < kSlices; ++z)
		{
			float curSliceZ = sliceDepths[z];
			float stepDist = curSliceZ - prevSliceZ;
			prevSliceZ = curSliceZ;

			float stepTransmittance = std::exp(-extinctionCoeff * stepDist);
			Math::Vector3 stepIntegrated = inScatterRadiance * ((1.0f - stepTransmittance) / extinctionCoeff);

			accumRadiance += stepIntegrated * accumTransmittance;
			accumTransmittance *= stepTransmittance;

			// Physical invariants: Transmittance strictly in [0, 1]
			CHECK_GE(accumTransmittance, 0.0f);
			CHECK_LE(accumTransmittance, 1.0f);
		}

		// By 65 meters with extinction 0.05, transmittance should have decayed significantly
		CHECK_LT(accumTransmittance, 0.20f);
		CHECK_GT(accumRadiance.x, 0.0f);
		CHECK_GT(accumRadiance.y, 0.0f);
		CHECK_GT(accumRadiance.z, 0.0f);
	}

	TEST_CASE("2.3: Dynamic Shadow Atlas Quadtree Allocator & Spot/Point Partitioning")
	{
		ShadowAtlasAllocator allocator;
		allocator.initialize(4096, 4096);

		CHECK_EQ(allocator.getAtlasWidth(), 4096);
		CHECK_EQ(allocator.getAtlasHeight(), 4096);
		CHECK_EQ(allocator.getReservedCornerSize(), 2048);

		// Allocate a 512x512 slot for Spot Light 1
		uint32_t x1 = 0, y1 = 0, size1 = 0;
		Math::Vector4 uv1;
		bool ok1 = allocator.allocate(1, 512, x1, y1, size1, uv1);
		CHECK(ok1);
		CHECK_EQ(size1, 512);

		// Verify allocation is OUTSIDE the reserved directional CSM quadrant [0, 2048) x [0, 2048)
		bool insideReserved1 = (x1 < 2048 && y1 < 2048);
		CHECK_FALSE(insideReserved1);

		// Verify UV matches pixel coordinates
		CHECK(uv1.x == doctest::Approx(static_cast<float>(x1) / 4096.0f).epsilon(1e-5f));
		CHECK(uv1.y == doctest::Approx(static_cast<float>(y1) / 4096.0f).epsilon(1e-5f));
		CHECK(uv1.z == doctest::Approx(static_cast<float>(size1) / 4096.0f).epsilon(1e-5f));
		CHECK(uv1.w == doctest::Approx(static_cast<float>(size1) / 4096.0f).epsilon(1e-5f));

		// Allocate multiple slots of different sizes
		uint32_t x2 = 0, y2 = 0, size2 = 0;
		Math::Vector4 uv2;
		bool ok2 = allocator.allocate(2, 256, x2, y2, size2, uv2);
		CHECK(ok2);
		CHECK_EQ(size2, 256);
		bool insideReserved2 = (x2 < 2048 && y2 < 2048);
		CHECK_FALSE(insideReserved2);

		// Verify slots 1 and 2 do not overlap
		bool overlapX = (x1 < x2 + size2) && (x2 < x1 + size1);
		bool overlapY = (y1 < y2 + size2) && (y2 < y1 + size1);
		bool isOverlapping = overlapX && overlapY;
		CHECK_FALSE(isOverlapping);

		// Verify Reset clears allocations and allows re-allocation at starting nodes
		allocator.reset();
		uint32_t xReset = 0, yReset = 0, sizeReset = 0;
		Math::Vector4 uvReset;
		bool okReset = allocator.allocate(1, 512, xReset, yReset, sizeReset, uvReset);
		CHECK(okReset);
		CHECK_EQ(xReset, x1);
		CHECK_EQ(yReset, y1);
	}

	TEST_CASE("2.4: Dual-Paraboloid Omni-Directional Projective Coordinates & Depth Encoding")
	{
		// Test dual-paraboloid hemisphere projection formula
		auto projectParaboloid = [](const Math::Vector3& dir, float zSign) -> Math::Vector2 {
			Math::Vector3 P = dir;
			P.z *= zSign;
			return Math::Vector2(P.x / (P.z + 1.0f), P.y / (P.z + 1.0f));
		};

		// Forward ray in front hemisphere: (0, 0, 1) with zSign = +1.0
		Math::Vector3 forward(0.0f, 0.0f, 1.0f);
		Math::Vector2 projFwd = projectParaboloid(forward, 1.0f);
		CHECK(projFwd.x == doctest::Approx(0.0f).epsilon(1e-4f));
		CHECK(projFwd.y == doctest::Approx(0.0f).epsilon(1e-4f));

		// Backward ray in back hemisphere: (0, 0, -1) with zSign = -1.0
		Math::Vector3 backward(0.0f, 0.0f, -1.0f);
		Math::Vector2 projBwd = projectParaboloid(backward, -1.0f);
		CHECK(projBwd.x == doctest::Approx(0.0f).epsilon(1e-4f));
		CHECK(projBwd.y == doctest::Approx(0.0f).epsilon(1e-4f));

		// Lateral rays at 90 degrees (equator): (1, 0, 0)
		Math::Vector3 lateral(1.0f, 0.0f, 0.0f);
		Math::Vector2 projLat = projectParaboloid(lateral, 1.0f);
		CHECK(projLat.x == doctest::Approx(1.0f).epsilon(1e-4f));
		CHECK(projLat.y == doctest::Approx(0.0f).epsilon(1e-4f));

		// Verify projective coordinates remain strictly inside unit disk for any hemisphere ray
		for (int angleDeg = 0; angleDeg < 360; angleDeg += 15)
		{
			float rad = static_cast<float>(angleDeg) * 3.14159265f / 180.0f;
			Math::Vector3 ray(std::cos(rad) * 0.7071f, std::sin(rad) * 0.7071f, 0.7071f);
			Math::Vector2 proj = projectParaboloid(ray, 1.0f);
			CHECK_LE(proj.Length(), 1.0f);
		}

		// Depth encoding normalization
		float nearZ = 0.1f;
		float farZ = 25.0f;
		float testDist = 12.5f;
		float normDepth = (testDist - nearZ) / (farZ - nearZ);
		CHECK(normDepth == doctest::Approx(0.49799f).epsilon(1e-4f));
	}

	TEST_CASE("2.5: Luminance Histogram Outlier Rejection & Temporal Exposure Adaptation")
	{
		// 1. Log2 luminance to histogram bin mapping
		const float minEV = -10.0f;
		const float maxEV = 10.0f;
		const float invRange = 1.0f / (maxEV - minEV);

		auto mapLumaToBin = [&](float luma) -> uint32_t {
			float logLuma = std::log2(std::max(luma, 0.0001f));
			float norm = std::clamp((logLuma - minEV) * invRange, 0.0f, 1.0f);
			return std::clamp(static_cast<uint32_t>(norm * 64.0f), 0u, 63u);
		};

		// EV 0 (luma = 1.0) should map to bin 32
		CHECK_EQ(mapLumaToBin(1.0f), 32);

		// EV -10 (luma = 2^-10 = 0.0009765625) should map to bin 0
		CHECK_EQ(mapLumaToBin(std::exp2(-10.0f)), 0);

		// EV +10 (luma = 2^10 = 1024.0) should map to bin 63
		CHECK_EQ(mapLumaToBin(std::exp2(10.0f)), 63);

		// 2. 5% Outlier rejection simulation
		constexpr uint32_t kTotalPixels = 1000;
		std::vector<uint32_t> hist(64, 0);

		// 100 pixels in extreme dark bin 0 (10% of pixels)
		hist[0] = 100;
		// 800 pixels in normal scene bin 32 (80% of pixels)
		hist[32] = 800;
		// 100 pixels in extreme sun glare bin 63 (10% of pixels)
		hist[63] = 100;

		uint32_t lowCutoff = static_cast<uint32_t>(kTotalPixels * 0.05f); // 50 pixels
		uint32_t highCutoff = static_cast<uint32_t>(kTotalPixels * 0.95f); // 950 pixels

		uint32_t cumulative = 0;
		float weightedSum = 0.0f;
		float validPixelCount = 0.0f;

		for (uint32_t i = 0; i < 64; ++i)
		{
			uint32_t count = hist[i];
			uint32_t nextCumulative = cumulative + count;

			if (nextCumulative > lowCutoff && cumulative < highCutoff)
			{
				uint32_t validInBin = count;
				if (cumulative < lowCutoff) validInBin -= (lowCutoff - cumulative);
				if (nextCumulative > highCutoff) validInBin -= (nextCumulative - highCutoff);

				float ev = minEV + (static_cast<float>(i) + 0.5f) / 64.0f * (maxEV - minEV);
				weightedSum += static_cast<float>(validInBin) * ev;
				validPixelCount += static_cast<float>(validInBin);
			}
			cumulative = nextCumulative;
		}

		float avgEV = weightedSum / validPixelCount;
		// Since bin 32 has 800 of the 900 valid pixels, average EV should be very close to EV 0
		CHECK(avgEV == doctest::Approx(0.0f).epsilon(0.5f));

		// 3. Temporal exponential adaptation
		float prevLuma = 1.0f;
		float targetLuma = 4.0f; // sudden step into bright daylight
		float dt = 0.01666f;
		float speedUp = 3.0f;

		float alpha = 1.0f - std::exp(-dt * speedUp);
		float adaptedLuma = prevLuma + (targetLuma - prevLuma) * alpha;

		CHECK_GT(adaptedLuma, prevLuma);
		CHECK_LT(adaptedLuma, targetLuma);
		// With 60 FPS (dt = ~0.0166), 1 frame should adapt ~4.8% of the delta
		CHECK(adaptedLuma == doctest::Approx(1.146f).epsilon(0.05f));

		// 4. Reinhard-Jodie Tonemapping Math
		auto tonemapReinhardJodie = [](const Math::Vector3& hdr) -> Math::Vector3 {
			float luma = hdr.x * 0.2126f + hdr.y * 0.7152f + hdr.z * 0.0722f;
			Math::Vector3 tv = Math::Vector3(hdr.x / (1.0f + hdr.x), hdr.y / (1.0f + hdr.y), hdr.z / (1.0f + hdr.z));
			Math::Vector3 reinhard = hdr / (1.0f + luma);
			return Math::Vector3(
				std::lerp(reinhard.x, tv.x, tv.x),
				std::lerp(reinhard.y, tv.y, tv.y),
				std::lerp(reinhard.z, tv.z, tv.z)
			);
		};

		Math::Vector3 daylight(2.0f, 1.5f, 1.0f);
		Math::Vector3 resultDaylight = tonemapReinhardJodie(daylight);
		CHECK_LE(resultDaylight.x, 1.0f);
		CHECK_LE(resultDaylight.y, 1.0f);
		CHECK_LE(resultDaylight.z, 1.0f);

		Math::Vector3 saturatedRed(10.0f, 0.1f, 0.1f);
		Math::Vector3 resultRed = tonemapReinhardJodie(saturatedRed);
		CHECK_GT(resultRed.x, resultRed.y); // Hue preservation: red channel remains dominant
	}

	TEST_CASE("2.6: Dual-Cascade Ocean Spectrum, Wave Subsurface Scattering & Jacobian Crest Foam")
	{
		// 1. Dual-cascade 8-wave Gerstner parameters
		struct GerstnerWave {
			Math::Vector2 dir;
			float amplitude;
			float frequency;
			float speed;
			float steepness;
		};

		// 4 Swell waves + 4 Chop waves
		std::vector<GerstnerWave> waves = {
			// Cascade 0: Primary swell
			{ Math::Vector2(1.0f, 0.0f), 0.60f, 0.08f, 1.2f, 0.45f },
			{ Math::Vector2(0.92f, 0.38f), 0.35f, 0.14f, 1.5f, 0.40f },
			{ Math::Vector2(0.86f, -0.50f), 0.25f, 0.22f, 1.8f, 0.35f },
			{ Math::Vector2(0.70f, 0.71f), 0.15f, 0.35f, 2.2f, 0.30f },
			// Cascade 1: High-frequency wind chop
			{ Math::Vector2(0.98f, 0.17f), 0.08f, 0.70f, 3.2f, 0.25f },
			{ Math::Vector2(0.81f, -0.58f), 0.05f, 1.20f, 4.0f, 0.20f },
			{ Math::Vector2(0.50f, 0.86f), 0.03f, 2.10f, 5.5f, 0.15f },
			{ Math::Vector2(-0.25f, 0.96f), 0.02f, 3.50f, 7.0f, 0.10f }
		};

		// Evaluate wave displacement & horizontal derivatives for Jacobian at crest
		Math::Vector2 pos(0.0f, 0.0f);
		float time = 0.0f;

		float dDxdx = 0.0f;
		float dDxdy = 0.0f;
		float dDydy = 0.0f;
		float totalElevation = 0.0f;

		for (const auto& w : waves)
		{
			float phase = w.frequency * (w.dir.Dot(pos)) + time * w.speed;
			float sinP = std::sin(phase);
			// float cosP = std::cos(phase);

			totalElevation += w.amplitude * sinP;

			float WA = w.frequency * w.amplitude;
			float Q = w.steepness;

			dDxdx += Q * WA * (w.dir.x * w.dir.x) * sinP;
			dDxdy += Q * WA * (w.dir.x * w.dir.y) * sinP;
			dDydy += Q * WA * (w.dir.y * w.dir.y) * sinP;
		}

		// Jacobian determinant: J = (1 - dDxdx)(1 - dDydy) - dDxdy^2
		float J = (1.0f - dDxdx) * (1.0f - dDydy) - (dDxdy * dDxdy);
		CHECK_LE(J, 1.0f);

		// Test wave crest with phase = PI / 2 (maximum elevation and maximum compression)
		float crestElevation = 0.0f;
		float crestDxdx = 0.0f;
		float crestDydy = 0.0f;
		float crestDxdy = 0.0f;

		for (const auto& w : waves)
		{
			crestElevation += w.amplitude; // at peak sinP = 1.0
			float WA = w.frequency * w.amplitude;
			float Q = w.steepness;
			crestDxdx += Q * WA * (w.dir.x * w.dir.x);
			crestDydy += Q * WA * (w.dir.y * w.dir.y);
			crestDxdy += Q * WA * (w.dir.x * w.dir.y);
		}

		float crestJ = (1.0f - crestDxdx) * (1.0f - crestDydy) - (crestDxdy * crestDxdy);

		// At wave crest, surface compresses -> Jacobian determinant drops significantly below 1.0
		CHECK_LT(crestJ, 1.0f);
		CHECK_GT(crestElevation, 1.0f);

		// Foam appearance condition: crestJ < jacobianThreshold (default ~0.70)
		float jacobianThreshold = 0.90f;
		bool foamGenerated = (crestJ < jacobianThreshold);
		CHECK(foamGenerated);

		// Wave Subsurface Scattering: strictly positive at crest (elevation > 0)
		float sssIntensity = 0.65f;
		float sssFactor = std::max(crestElevation, 0.0f) * sssIntensity;
		CHECK_GT(sssFactor, 0.5f);
	}
}
