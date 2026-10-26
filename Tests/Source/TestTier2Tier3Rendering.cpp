#include <doctest/doctest.h>
#include "Engine/Core/Math/Math.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Serialization/SceneSerializer.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Shaders/Shared/Bindings.h"
#include <cmath>
#include <algorithm>

using namespace Core;
using namespace Core::Renderer;

namespace
{
	// Canonical Octahedral encoding & decoding test helpers
	Vector2 OctEncode(const Vector3& n)
	{
		float invL1 = 1.0f / (std::abs(n.x) + std::abs(n.y) + std::abs(n.z));
		Vector2 oct;
		if (n.z >= 0.0f)
		{
			oct = Vector2(n.x * invL1, n.y * invL1);
		}
		else
		{
			float signX = (n.x >= 0.0f) ? 1.0f : -1.0f;
			float signY = (n.y >= 0.0f) ? 1.0f : -1.0f;
			oct = Vector2((1.0f - std::abs(n.y * invL1)) * signX,
			              (1.0f - std::abs(n.x * invL1)) * signY);
		}
		return Vector2(oct.x * 0.5f + 0.5f, oct.y * 0.5f + 0.5f);
	}

	Vector3 OctDecode(const Vector2& uv)
	{
		Vector2 f(uv.x * 2.0f - 1.0f, uv.y * 2.0f - 1.0f);
		Vector3 n(f.x, f.y, 1.0f - std::abs(f.x) - std::abs(f.y));
		float t = std::clamp(-n.z, 0.0f, 1.0f);
		n.x += (n.x >= 0.0f ? -t : t);
		n.y += (n.y >= 0.0f ? -t : t);
		n.Normalize();
		return n;
	}

	// Chebyshev Upper Bound visibility test
	float ChebyshevVisibility(float mean, float meanSq, float dist)
	{
		float variance = std::max(meanSq - mean * mean, 0.0001f);
		float d = dist - mean;
		if (d <= 0.0f) return 1.0f; // Receiver is in front of blocker
		float pMax = variance / (variance + d * d);
		return pMax * pMax * pMax; // Cubic curve sharpens contact shadows
	}

	// Anisotropic GGX NDF
	float DistributionAnisotropicGGX(float NdotH, float HdotT, float HdotB, float at, float ab)
	{
		constexpr float kPI = 3.14159265359f;
		float term1 = HdotT / at;
		float term2 = HdotB / ab;
		float d = term1 * term1 + term2 * term2 + NdotH * NdotH;
		return 1.0f / (kPI * at * ab * d * d);
	}
}

TEST_SUITE("Tier2Tier3Rendering")
{
	TEST_CASE("DDGI: Octahedral Projection Roundtrip")
	{
		const Vector3 testDirections[] = {
			Vector3(0.0f, 1.0f, 0.0f),
			Vector3(0.0f, -1.0f, 0.0f),
			Vector3(1.0f, 0.0f, 0.0f),
			Vector3(-1.0f, 0.0f, 0.0f),
			Vector3(0.0f, 0.0f, 1.0f),
			Vector3(0.0f, 0.0f, -1.0f),
			Vector3(0.577f, 0.577f, 0.577f),
			Vector3(-0.707f, 0.0f, 0.707f)
		};

		for (const auto& originalDir : testDirections)
		{
			Vector3 dir = originalDir;
			dir.Normalize();

			Vector2 uv = OctEncode(dir);
			CHECK(uv.x >= 0.0f);
			CHECK(uv.x <= 1.0f);
			CHECK(uv.y >= 0.0f);
			CHECK(uv.y <= 1.0f);

			Vector3 reconstructed = OctDecode(uv);
			float dotProduct = dir.Dot(reconstructed);
			CHECK(dotProduct > 0.999f); // Excellent angular fidelity
		}
	}

	TEST_CASE("DDGI: Chebyshev Upper Bound Visibility")
	{
		// Test case A: Point is closer than blocker mean depth -> full visibility
		float visClose = ChebyshevVisibility(10.0f, 105.0f, 8.0f);
		CHECK(visClose == 1.0f);

		// Test case B: Point is behind blocker mean depth -> rapid falloff
		float visFar = ChebyshevVisibility(10.0f, 105.0f, 15.0f);
		CHECK(visFar < 0.1f);

		// Test case C: Point at exact mean depth -> boundary
		float visBoundary = ChebyshevVisibility(10.0f, 105.0f, 10.0f);
		CHECK(visBoundary == 1.0f);
	}

	TEST_CASE("DDGI: Probe Volume Component Serialization & Bridge")
	{
		Scene scene;
		auto entity = scene.createEntity("DDGIProbeVolume");

		auto& ddgi = entity.addComponent<DDGIProbeVolumeComponent>();
		ddgi.enabled = true;
		ddgi.origin = Vector3(0.0f, 5.0f, 0.0f);
		ddgi.probeCounts = Vector3(16.0f, 8.0f, 16.0f);
		ddgi.probeSpacing = 2.5f;
		ddgi.normalBias = 0.20f;
		ddgi.hysteresis = 0.96f;
		ddgi.intensity = 1.25f;
		ddgi.energyPreservation = 0.88f;

		SceneSerializer serializer(&scene);
		std::string jsonStr = serializer.serializeToString();
		CHECK(jsonStr.find("DDGIProbeVolumeComponent") != std::string::npos);
		CHECK(jsonStr.find("\"probeSpacing\": 2.5") != std::string::npos);

		Scene loadedScene;
		SceneSerializer deserializer(&loadedScene);
		CHECK(deserializer.deserializeFromString(jsonStr));

		auto loadedEntity = loadedScene.findEntityByName("DDGIProbeVolume");
		REQUIRE(loadedEntity.isValid());
		CHECK(loadedEntity.hasComponent<DDGIProbeVolumeComponent>());

		const auto& loadedDDGI = loadedEntity.getComponent<DDGIProbeVolumeComponent>();
		CHECK(loadedDDGI.enabled == true);
		CHECK(loadedDDGI.origin.y == doctest::Approx(5.0f));
		CHECK(loadedDDGI.probeSpacing == doctest::Approx(2.5f));
		CHECK(loadedDDGI.intensity == doctest::Approx(1.25f));
		CHECK(loadedDDGI.hysteresis == doctest::Approx(0.96f));
	}

	TEST_CASE("Hi-Z SSR: Settings & PostProcessing Component")
	{
		Scene scene;
		auto entity = scene.createEntity("PostProcessingSettings");

		auto& pp = entity.addComponent<PostProcessingComponent>();
		pp.enableSSR = true;
		pp.ssrMaxDistance = 35.0f;
		pp.ssrThickness = 0.75f;
		pp.ssrRoughnessCutoff = 0.60f;
		pp.ssrIntensity = 1.2f;
		pp.ssrStepCount = 48;

		SceneSerializer serializer(&scene);
		std::string jsonStr = serializer.serializeToString();
		CHECK(jsonStr.find("enableSSR") != std::string::npos);
		CHECK(jsonStr.find("\"ssrMaxDistance\": 35.0") != std::string::npos);

		Scene loadedScene;
		SceneSerializer deserializer(&loadedScene);
		CHECK(deserializer.deserializeFromString(jsonStr));

		auto loadedEntity = loadedScene.findEntityByName("PostProcessingSettings");
		REQUIRE(loadedEntity.isValid());
		const auto& loadedPP = loadedEntity.getComponent<PostProcessingComponent>();
		CHECK(loadedPP.enableSSR == true);
		CHECK(loadedPP.ssrMaxDistance == doctest::Approx(35.0f));
		CHECK(loadedPP.ssrThickness == doctest::Approx(0.75f));
		CHECK(loadedPP.ssrRoughnessCutoff == doctest::Approx(0.60f));
		CHECK(loadedPP.ssrStepCount == 48);
	}

	TEST_CASE("Material: Dual-Lobe Clearcoat & Brushed Anisotropy")
	{
		Material mat("CarPaint");
		mat.setClearcoatFactor(0.90f);
		mat.setClearcoatRoughness(0.04f);
		mat.setAnisotropyFactor(0.75f);
		mat.setAnisotropyRotation(0.35f);

		CHECK(mat.getClearcoatFactor() == doctest::Approx(0.90f));
		CHECK(mat.getClearcoatRoughness() == doctest::Approx(0.04f));
		CHECK(mat.getAnisotropyFactor() == doctest::Approx(0.75f));
		CHECK(mat.getAnisotropyRotation() == doctest::Approx(0.35f));
		CHECK(mat.hasFlag(DF_MAT_FLAG_CLEAR_COAT));
		CHECK(mat.hasFlag(DF_MAT_FLAG_ANISOTROPY));

		// Verify constant buffer alignment and values
		CBPerMaterial cbData = mat.getBufferData();
		CHECK(cbData.extraMaterialParams.x == doctest::Approx(0.90f));
		CHECK(cbData.extraMaterialParams.y == doctest::Approx(0.04f));
		CHECK(cbData.extraMaterialParams.z == doctest::Approx(0.75f));
		CHECK(cbData.extraMaterialParams.w == doctest::Approx(0.35f));
		CHECK((cbData.materialFlags & DF_MAT_FLAG_CLEAR_COAT) != 0);
		CHECK((cbData.materialFlags & DF_MAT_FLAG_ANISOTROPY) != 0);

		// Verify JSON serialization roundtrip
		std::string jsonStr = mat.toJson();
		Material loadedMat("CarPaintLoaded");
		CHECK(loadedMat.loadFromJson(jsonStr));

		CHECK(loadedMat.getClearcoatFactor() == doctest::Approx(0.90f));
		CHECK(loadedMat.getClearcoatRoughness() == doctest::Approx(0.04f));
		CHECK(loadedMat.getAnisotropyFactor() == doctest::Approx(0.75f));
		CHECK(loadedMat.getAnisotropyRotation() == doctest::Approx(0.35f));
	}

	TEST_CASE("BRDF: Anisotropic GGX NDF Evaluation")
	{
		float roughness = 0.5f;
		float aniso = 0.8f;
		float at = std::max(roughness * roughness * (1.0f + aniso), 0.001f);
		float ab = std::max(roughness * roughness * (1.0f - aniso), 0.001f);

		// Along tangent vs along bitangent
		float ndfTangent = DistributionAnisotropicGGX(0.8f, 0.6f, 0.0f, at, ab);
		float ndfBitangent = DistributionAnisotropicGGX(0.8f, 0.0f, 0.6f, at, ab);

		CHECK(ndfTangent > 0.0f);
		CHECK(ndfBitangent > 0.0f);
		// With positive anisotropy (+0.8), highlight is stretched along bitangent, so NDF along bitangent is distinct
		CHECK(ndfTangent != doctest::Approx(ndfBitangent));
	}

	TEST_CASE("SSS: Subsurface Scattering Profile & Weights")
	{
		// 7-tap Gaussian kernel weights sum to 1.0 (center + 3 symmetrical pairs)
		const float weights[4] = { 0.382928f, 0.241732f, 0.060598f, 0.005977f };
		float totalWeight = weights[0] + 2.0f * (weights[1] + weights[2] + weights[3]);
		CHECK(totalWeight == doctest::Approx(1.0f).epsilon(0.001f));

		Scene scene;
		auto entity = scene.createEntity("SubsurfaceSettings");
		auto& pp = entity.addComponent<PostProcessingComponent>();
		pp.enableSSS = true;
		pp.sssRadius = 2.0f;
		pp.sssMaxDepthDiff = 0.15f;

		SceneSerializer serializer(&scene);
		std::string jsonStr = serializer.serializeToString();
		CHECK(jsonStr.find("enableSSS") != std::string::npos);

		Scene loadedScene;
		SceneSerializer deserializer(&loadedScene);
		CHECK(deserializer.deserializeFromString(jsonStr));

		auto loadedEntity = loadedScene.findEntityByName("SubsurfaceSettings");
		REQUIRE(loadedEntity.isValid());
		const auto& loadedPP = loadedEntity.getComponent<PostProcessingComponent>();
		CHECK(loadedPP.enableSSS == true);
		CHECK(loadedPP.sssRadius == doctest::Approx(2.0f));
		CHECK(loadedPP.sssMaxDepthDiff == doctest::Approx(0.15f));
	}
}
