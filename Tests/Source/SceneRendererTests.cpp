#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Entity.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Scene/SceneRendererBridge.h"
#include "Engine/RHI/Null/NullRHI.h"
#include "Engine/Renderer/RenderScene.h"
#include "Engine/Renderer/SceneRenderer.h"
#include "Engine/Core/Scripting/ScriptableEntity.h"
#include "Engine/Shaders/Shared/Bindings.h"
#include "Engine/Core/Scene/Systems/ParticleSystem.h"

#include <ostream>
#include <fstream>
#include <filesystem>
#ifdef _MSC_VER
#pragma warning(disable: 5285)
#endif
#include "doctest/doctest.h"

using namespace Core;
using namespace Core::RHI;
using namespace Core::Renderer;

TEST_SUITE("Milestone P4: Frame Pipeline, Color Space & SceneRenderer")
{
	TEST_CASE("P4.1: RenderScene Snapshot Extraction & ECS Decoupling")
	{
		Scene scene;

		// 1. Create Camera Entity
		auto camEntity = scene.createEntity("MainCamera");
		auto& camComp = camEntity.addComponent<CameraComponent>();
		camComp.isPrimary = true;
		camComp.fov = 60.0f;
		camComp.nearZ = 0.5f;
		camComp.farZ = 500.0f;

		auto& camTrans = camEntity.getComponent<TransformComponent>();
		camTrans.position = Vector3(0.0f, 2.0f, -10.0f);

		// 2. Create Mesh Entities
		auto mesh1 = scene.createEntity("Cube1");
		mesh1.addComponent<MeshRendererComponent>();
		mesh1.getComponent<TransformComponent>().position = Vector3(-2.0f, 0.0f, 0.0f);

		auto mesh2 = scene.createEntity("Cube2");
		mesh2.addComponent<MeshRendererComponent>();
		mesh2.getComponent<TransformComponent>().position = Vector3(2.0f, 0.0f, 5.0f);

		// 3. Create Light Entity
		auto lightEntity = scene.createEntity("SunLight");
		auto& lightComp = lightEntity.addComponent<LightComponent>();
		lightComp.type = LightType::Directional;
		lightComp.intensity = 2.5f;
		lightEntity.getComponent<TransformComponent>().position = Vector3(0.0f, 10.0f, 0.0f);

		// Update scene hierarchy transforms
		scene.updateTransforms();

		// Extract flat snapshot
		RenderScene renderScene;
		ExtractRenderScene(scene, renderScene);

		// Verify extracted snapshot data
		CHECK(renderScene.camera.nearZ == doctest::Approx(0.5f));
		CHECK(renderScene.camera.farZ == doctest::Approx(500.0f));
		CHECK(renderScene.camera.position.x == doctest::Approx(0.0f));
		CHECK(renderScene.camera.position.y == doctest::Approx(2.0f));
		CHECK(renderScene.camera.position.z == doctest::Approx(-10.0f));

		CHECK(renderScene.meshes.size() == 2);
		CHECK(renderScene.lights.size() == 1);
		CHECK(renderScene.lights[0].intensity == doctest::Approx(2.5f));
	}

	TEST_CASE("P4.2: Reversed-Z Ray Unprojection & Entity Picking Math")
	{
		const float width = 1920.0f;
		const float height = 1080.0f;
		const float aspect = width / height;
		const float nearZ = 0.1f;
		const float farZ = 1000.0f;

		const Vector3 eye(0.0f, 0.0f, -10.0f);
		const Vector3 target(0.0f, 0.0f, 0.0f);
		const Vector3 up(0.0f, 1.0f, 0.0f);

		const Matrix view = Math::lookAtLH(eye, target, up);
		const Matrix proj = Math::perspectiveFovLH_ReversedZ(Math::toRadians(60.0f), aspect, nearZ, farZ);
		const Matrix viewProj = view * proj;
		const Matrix invViewProj = viewProj.Invert();

		// Center of screen: (960, 540)
		auto rayCenter = SceneRenderer::unprojectRayReversedZ(
			width * 0.5f,
			height * 0.5f,
			width,
			height,
			invViewProj);

		// In Left-Handed space looking forward (+Z), the ray must point along +Z
		CHECK(rayCenter.direction.x == doctest::Approx(0.0f).epsilon(0.01f));
		CHECK(rayCenter.direction.y == doctest::Approx(0.0f).epsilon(0.01f));
		CHECK(rayCenter.direction.z == doctest::Approx(1.0f).epsilon(0.01f));

		// Ray origin must be near eye position at nearZ
		CHECK(rayCenter.position.x == doctest::Approx(eye.x).epsilon(0.01f));
		CHECK(rayCenter.position.y == doctest::Approx(eye.y).epsilon(0.01f));
		CHECK(rayCenter.position.z == doctest::Approx(eye.z + nearZ).epsilon(0.02f));

		// Intersection with target at (0, 0, 0)
		BoundingSphere sphere(Vector3(0.0f, 0.0f, 0.0f), 1.0f);
		float dist = 0.0f;
		CHECK(rayCenter.Intersects(sphere, dist));
		CHECK(dist == doctest::Approx(10.0f - nearZ - 1.0f).epsilon(0.05f));
	}

	TEST_CASE("P4.3: SceneRenderer Dual Output (Backbuffer vs Offscreen)")
	{
		NullRHIDevice device;
		SceneRenderer renderer(device);

		RenderScene scene;
		scene.camera.position = Vector3(0.0f, 0.0f, -5.0f);
		scene.camera.viewMatrix = Math::lookAtLH(scene.camera.position, Vector3(0.0f, 0.0f, 0.0f), Vector3(0.0f, 1.0f, 0.0f));
		scene.camera.projMatrix = Math::perspectiveFovLH_ReversedZ(Math::toRadians(60.0f), 16.0f / 9.0f, 0.1f, 100.0f);
		scene.camera.viewProjMatrix = scene.camera.viewMatrix * scene.camera.projMatrix;

		// Add 3 test meshes
		for (int i = 0; i < 3; ++i)
		{
			RenderMeshInstance mesh{};
			mesh.world = Matrix::CreateTranslation(static_cast<float>(i * 2), 0.0f, 0.0f);
			mesh.entityId = static_cast<uint32_t>(i + 1);
			mesh.layer = RenderLayer::Opaque;
			mesh.indexCount = 36;
			mesh.startIndex = 0;
			mesh.baseVertex = 0;
			scene.meshes.push_back(mesh);
		}

		// 1. Render to Offscreen Texture (e.g. Editor Viewport)
		GpuTextureDesc offscreenDesc{};
		offscreenDesc.width = 1280;
		offscreenDesc.height = 720;
		offscreenDesc.format = PixelFormat::R8G8B8A8_UNORM;
		offscreenDesc.bindFlags = BufferBindFlags::RenderTarget | BufferBindFlags::ShaderResource;
		auto offscreenRes = device.createTexture(offscreenDesc);
		REQUIRE(offscreenRes.isOk());

		RenderView editorView{};
		editorView.width = 1280;
		editorView.height = 720;
		editorView.targetTexture = *offscreenRes;
		editorView.isEditorView = true;

		renderer.render(scene, editorView);
		const uint32_t editorDrawCalls = renderer.getDrawCallCount();
		const uint32_t editorBatches = renderer.getBatchCount();
		CHECK(editorDrawCalls > 0);
		CHECK(editorBatches > 0);

		auto* nullOffscreen = dynamic_cast<NullTexture*>(offscreenRes->get());
		REQUIRE(nullOffscreen != nullptr);
		CHECK(nullOffscreen->isCleared());

		// 2. Render to SwapChain (e.g. Runtime Player)
		auto swapChainRes = device.createSwapChain(reinterpret_cast<void*>(0x5678), 1280, 720);
		REQUIRE(swapChainRes.isOk());
		auto swapChain = std::move(*swapChainRes);

		RenderView playerView{};
		playerView.width = 1280;
		playerView.height = 720;
		playerView.swapChain = std::move(swapChain);
		playerView.isEditorView = false;

		renderer.render(scene, playerView);
		const uint32_t playerDrawCalls = renderer.getDrawCallCount();
		const uint32_t playerBatches = renderer.getBatchCount();

		// Both paths must execute identically
		CHECK(playerDrawCalls == editorDrawCalls);
		CHECK(playerBatches == editorBatches);
	}

	TEST_CASE("P4.4: Dynamic Camera Aspect Ratio & Material Properties Parsing")
	{
		Scene scene;
		auto camEntity = scene.createEntity("DynamicCam");
		auto& camComp = camEntity.addComponent<CameraComponent>();
		camComp.isPrimary = true;
		camComp.fov = 45.0f;
		scene.updateTransforms();

		// Extract with ultrawide 21:9 aspect (~2.333)
		RenderScene renderScene;
		ExtractRenderScene(scene, renderScene, 21.0f / 9.0f);
		CHECK(renderScene.camera.aspect == doctest::Approx(21.0f / 9.0f));

		// Test Material JSON parsing with render states
		NullRHIDevice device;
		SceneRenderer renderer(device);

		std::filesystem::create_directories("Temp_MatTest");
		std::string matPath = "Temp_MatTest/CustomMaterial.mat";
		{
			std::ofstream f(matPath);
			f << "{\n"
			  << "  \"name\": \"TestMat\",\n"
			  << "  \"baseColor\": [0.2, 0.4, 0.6, 1.0],\n"
			  << "  \"roughness\": 0.25,\n"
			  << "  \"metallic\": 0.75,\n"
			  << "  \"emissive\": [0.1, 0.2, 0.3, 0.0],\n"
			  << "  \"tiling\": [2.0, 3.0],\n"
			  << "  \"blendMode\": 0,\n"
			  << "  \"depthMode\": 0,\n"
			  << "  \"cullMode\": 2\n"
			  << "}\n";
		}

		auto mat = renderer.getOrCreateMaterial(matPath);
		REQUIRE(mat != nullptr);
		CHECK(mat->getName() == "TestMat");
		CHECK(mat->getBaseColor().x == doctest::Approx(0.2f));
		CHECK(mat->getBaseColor().y == doctest::Approx(0.4f));
		CHECK(mat->getBaseColor().z == doctest::Approx(0.6f));
		CHECK(mat->getRoughness() == doctest::Approx(0.25f));
		CHECK(mat->getMetallic() == doctest::Approx(0.75f));
		CHECK(mat->getTilingOffset().x == doctest::Approx(2.0f));
		CHECK(mat->getTilingOffset().y == doctest::Approx(3.0f));
		CHECK(mat->getBlendMode() == BlendMode::Opaque);
		CHECK(mat->getCullMode() == RHI::CullMode::Back);

		std::filesystem::remove_all("Temp_MatTest");
	}

	TEST_CASE("P4.5: OBJ Vertex Welding & Lengyel Tangent Calculation")
	{
		std::filesystem::create_directories("Temp_ObjTest");
		std::string objPath = "Temp_ObjTest/WeldedCube.obj";
		{
			std::ofstream f(objPath);
			// 8 positions
			f << "v -1.0 -1.0  1.0\n"
			  << "v  1.0 -1.0  1.0\n"
			  << "v  1.0  1.0  1.0\n"
			  << "v -1.0  1.0  1.0\n"
			  << "v  1.0 -1.0 -1.0\n"
			  << "v -1.0 -1.0 -1.0\n"
			  << "v -1.0  1.0 -1.0\n"
			  << "v  1.0  1.0 -1.0\n"
			  // 6 normals
			  << "vn  0.0  0.0  1.0\n"
			  << "vn  0.0  0.0 -1.0\n"
			  << "vn  0.0  1.0  0.0\n"
			  << "vn  0.0 -1.0  0.0\n"
			  << "vn  1.0  0.0  0.0\n"
			  << "vn -1.0  0.0  0.0\n"
			  // 4 UVs
			  << "vt 0.0 0.0\n"
			  << "vt 1.0 0.0\n"
			  << "vt 1.0 1.0\n"
			  << "vt 0.0 1.0\n"
			  // 6 faces (12 triangles, 36 indices)
			  << "f 1/1/1 2/2/1 3/3/1\n f 1/1/1 3/3/1 4/4/1\n"
			  << "f 5/1/2 6/2/2 7/3/2\n f 5/1/2 7/3/2 8/4/2\n"
			  << "f 4/1/3 3/2/3 8/3/3\n f 4/1/3 8/3/3 7/4/3\n"
			  << "f 6/1/4 5/2/4 2/3/4\n f 6/1/4 2/3/4 1/4/4\n"
			  << "f 2/1/5 5/2/5 8/3/5\n f 2/1/5 8/3/5 3/4/5\n"
			  << "f 6/1/6 1/2/6 4/3/6\n f 6/1/6 4/3/6 7/4/6\n";
		}

		auto mesh = Core::Renderer::LoadObjMesh(objPath);
		// Cube has 6 faces * 4 vertices = 24 welded vertices (down from 36 without welding)
		CHECK(mesh.vertices.size() == 24);
		CHECK(mesh.indices.size() == 36);

		for (const auto& v : mesh.vertices)
		{
			// Normal must be unit length
			CHECK(v.normal.Length() == doctest::Approx(1.0f).epsilon(0.01f));

			// Tangent XYZ must be unit length
			Vector3 tan(v.tangent.x, v.tangent.y, v.tangent.z);
			CHECK(tan.Length() == doctest::Approx(1.0f).epsilon(0.01f));

			// Tangent must be orthonormal to Normal (|dot| < 1e-4)
			CHECK(std::abs(v.normal.Dot(tan)) < 0.001f);

			// Handedness must be +1.0 or -1.0
			CHECK(std::abs(v.tangent.w) == doctest::Approx(1.0f));
		}

		std::filesystem::remove_all("Temp_ObjTest");
	}

	TEST_CASE("P4.6: CPU Particle System Simulation & Recycling")
	{
		Scene scene;
		auto entity = scene.createEntity("ParticleFX");
		auto& emitter = entity.addComponent<ParticleEmitterComponent>();
		emitter.maxParticles = 100;
		emitter.emissionRate = 50.0f;
		emitter.lifetime = 0.5f;
		emitter.startSize = 1.0f;
		emitter.endSize = 0.1f;
		emitter.startVelocity = Vector3(0.0f, 10.0f, 0.0f);
		emitter.gravityScale = 1.0f;

		scene.updateTransforms();

		// Tick 0.1s -> should spawn 5 particles (50 * 0.1) at age 0
		Core::Systems::ParticleSystem::Tick(scene, 0.1f);
		auto& state = entity.getComponent<ParticleRuntimeState>();
		CHECK(state.activeCount == 5);

		for (int i = 0; i < state.activeCount; ++i)
		{
			const auto& p = state.particles[i];
			CHECK(p.age == doctest::Approx(0.0f));
			CHECK(p.size == doctest::Approx(1.0f));
		}

		// Tick another 0.1s -> existing particles advance age by 0.1s, 5 more spawn (total 10)
		Core::Systems::ParticleSystem::Tick(scene, 0.1f);
		CHECK(state.activeCount == 10);

		for (int i = 0; i < 5; ++i)
		{
			const auto& p = state.particles[i];
			CHECK(p.age == doctest::Approx(0.1f));
			CHECK(p.size < 1.0f);
			CHECK(p.size >= 0.1f);
			CHECK(p.velocity.y < 10.0f);
			CHECK(p.velocity.y > 5.0f);
		}

		// Tick 0.6s -> original particles should expire and new ones spawn
		Core::Systems::ParticleSystem::Tick(scene, 0.6f);
		CHECK(state.activeCount > 0);
		CHECK(state.activeCount <= emitter.maxParticles);

		// Reset -> should clear all active particles
		Core::Systems::ParticleSystem::Reset(scene);
		CHECK(state.activeCount == 0);
	}

	TEST_CASE("SSAO Projection & Unprojection Math")
	{
		float fovY = Math::toRadians(60.0f);
		float aspect = 16.0f / 9.0f;
		float nearZ = 0.1f;
		float farZ = 1000.0f;

		Matrix proj = Math::perspectiveFovLH_ReversedZ(fovY, aspect, nearZ, farZ);
		Matrix invProj = proj.Invert();

		// Test view-space point at (1.0, 2.0, 5.0)
		Vector4 viewPos(1.0f, 2.0f, 5.0f, 1.0f);
		Vector4 clip = Vector4::Transform(viewPos, proj);

		// Perspective divide to get NDC
		Vector3 ndc(clip.x / clip.w, clip.y / clip.w, clip.z / clip.w);
		CHECK(ndc.z >= 0.0f);
		CHECK(ndc.z <= 1.0f);

		// UV from NDC
		Vector2 uv(ndc.x * 0.5f + 0.5f, -ndc.y * 0.5f + 0.5f);

		// Reconstruct viewPos from (uv, depth = ndc.z) as in SSAOPS.hlsl:
		Vector2 reNdc = uv * Vector2(2.0f, -2.0f) + Vector2(-1.0f, 1.0f);
		Vector4 reClip(reNdc.x, reNdc.y, ndc.z, 1.0f);
		Vector4 viewH = Vector4::Transform(reClip, invProj);
		Vector3 reViewPos(viewH.x / viewH.w, viewH.y / viewH.w, viewH.z / viewH.w);

		CHECK(reViewPos.x == doctest::Approx(1.0f).epsilon(0.001f));
		CHECK(reViewPos.y == doctest::Approx(2.0f).epsilon(0.001f));
		CHECK(reViewPos.z == doctest::Approx(5.0f).epsilon(0.001f));
	}

	TEST_CASE("P4.9: Directional Shadow Mapping Bridge Extraction & Parameters")
	{
		Scene scene;

		// 1. Directional Light with Shadows
		auto dirLight = scene.createEntity("Sun");
		auto& lightComp = dirLight.addComponent<LightComponent>();
		lightComp.type = LightType::Directional;
		lightComp.castShadows = true;
		lightComp.shadowBias = 0.002f;
		lightComp.shadowNormalBias = 0.03f;
		lightComp.shadowDistance = 75.0f;
		lightComp.shadowSoftness = 1.5f;

		// 2. Mesh with Cast and Receive Shadows
		auto casterMesh = scene.createEntity("CasterBox");
		auto& mrComp = casterMesh.addComponent<MeshRendererComponent>();
		mrComp.castShadows = true;
		mrComp.receiveShadows = true;

		// 3. PostProcessing with SSAO Direct Influence & Debug View
		auto envEntity = scene.createEntity("Environment");
		auto& ppComp = envEntity.addComponent<PostProcessingComponent>();
		ppComp.ssaoDirectLightInfluence = 0.65f;
		ppComp.debugView = 3; // Raw Shadow Map

		RenderScene renderScene;
		ExtractRenderScene(scene, renderScene, 16.0f / 9.0f);

		REQUIRE(!renderScene.lights.empty());
		CHECK(renderScene.lights[0].castShadows == true);
		CHECK(renderScene.lights[0].shadowBias == doctest::Approx(0.002f));
		CHECK(renderScene.lights[0].shadowNormalBias == doctest::Approx(0.03f));
		CHECK(renderScene.lights[0].shadowDistance == doctest::Approx(75.0f));
		CHECK(renderScene.lights[0].shadowSoftness == doctest::Approx(1.5f));

		REQUIRE(!renderScene.meshes.empty());
		CHECK(renderScene.meshes[0].castShadows == true);
		CHECK(renderScene.meshes[0].receiveShadows == true);

		CHECK(renderScene.environment.ssaoDirectLightInfluence == doctest::Approx(0.65f));
		CHECK(renderScene.environment.debugView == 3);
	}

	TEST_CASE("P4.10: PBR Material JSON Serialization & ORM Parameter Roundtrip")
	{
		Material mat("TestORM");
		mat.setBaseColor(Vector4(0.8f, 0.2f, 0.1f, 1.0f));
		mat.setRoughness(0.35f);
		mat.setMetallic(0.95f);
		mat.setNormalScale(1.2f);
		mat.setFlipNormalY(true);
		mat.setORMPacked(true);
		mat.setAlphaCutoff(0.5f);
		mat.setEmissiveColor(Vector3(1.0f, 0.5f, 0.0f));
		mat.setEmissiveIntensity(3.0f);
		mat.setTiling(Vector2(2.5f, 2.5f));
		mat.setOffset(Vector2(0.1f, 0.2f));

		std::string jsonStr = mat.toJson();
		CHECK(!jsonStr.empty());

		Material loaded("LoadedMat");
		bool ok = loaded.loadFromJson(jsonStr);
		CHECK(ok == true);
		CHECK(loaded.getBaseColor().x == doctest::Approx(0.8f));
		CHECK(loaded.getRoughness() == doctest::Approx(0.35f));
		CHECK(loaded.getMetallic() == doctest::Approx(0.95f));
		CHECK(loaded.getNormalScale() == doctest::Approx(1.2f));
		CHECK(loaded.isFlipNormalY() == true);
		CHECK(loaded.isORMPacked() == true);
		CHECK(loaded.getAlphaCutoff() == doctest::Approx(0.5f));
		CHECK(loaded.getEmissiveIntensity() == doctest::Approx(3.0f));
		CHECK(loaded.getTiling().x == doctest::Approx(2.5f));
		CHECK(loaded.getOffset().y == doctest::Approx(0.2f));

		// Test CBPerMaterial conversion
		CBPerMaterial cb = loaded.getBufferData();
		CHECK(cb.roughnessFactor == doctest::Approx(0.35f));
		CHECK(cb.metallicFactor == doctest::Approx(0.95f));
		CHECK(cb.normalScale == doctest::Approx(1.2f));
		CHECK(cb.alphaCutoff == doctest::Approx(0.5f));
		CHECK((cb.materialFlags & DF_MAT_FLAG_ORM_PACKED) != 0);
		CHECK((cb.materialFlags & DF_MAT_FLAG_FLIP_NORMAL_Y) != 0);
	}

	TEST_CASE("P4.11: ScriptableEntity Field Exposure & JSON Serialization")
	{
		class DummyTestEntity : public ScriptableEntity
		{
		public:
			float speed = 7.5f;
			int health = 100;
			bool active = true;
			Vector3 target = Vector3(1.0f, 2.0f, 3.0f);

			void onCreate() override
			{
				exposeFloat("Speed", speed, 1.0f, 50.0f);
				exposeInt("Health", health, 0, 200);
				exposeBool("Active", active);
				exposeVec3("Target", target);
			}
		};

		DummyTestEntity entityA;
		entityA.onCreate();
		entityA.speed = 12.3f;
		entityA.health = 80;
		entityA.active = false;
		entityA.target = Vector3(4.0f, 5.0f, 6.0f);

		std::string serialized = entityA.serializeExposedFields();
		CHECK(!serialized.empty());

		DummyTestEntity entityB;
		entityB.onCreate();
		entityB.deserializeExposedFields(serialized);

		CHECK(entityB.speed == doctest::Approx(12.3f));
		CHECK(entityB.health == 80);
		CHECK(entityB.active == false);
		CHECK(entityB.target.x == doctest::Approx(4.0f));
		CHECK(entityB.target.y == doctest::Approx(5.0f));
		CHECK(entityB.target.z == doctest::Approx(6.0f));
	}

	TEST_CASE("PBR.CSM: 4-Cascade Practical Splits and Sphere-Fit Stability")
	{
		const float nearZ = 0.1f;
		const float farZ = 80.0f;
		float splits[5] = { nearZ, 0.0f, 0.0f, 0.0f, farZ };

		const float lambda = 0.75f;
		for (int i = 1; i <= 4; ++i)
		{
			float p = static_cast<float>(i) / 4.0f;
			float zLog = nearZ * std::pow(farZ / nearZ, p);
			float zUni = nearZ + (farZ - nearZ) * p;
			splits[i] = lambda * zLog + (1.0f - lambda) * zUni;
		}

		// Verify strict monotonicity
		CHECK(splits[0] < splits[1]);
		CHECK(splits[1] < splits[2]);
		CHECK(splits[2] < splits[3]);
		CHECK(splits[3] <= splits[4]);

		// Cascade 0 must be near camera for high resolution close-up shadows
		CHECK(splits[1] < 6.0f);
		CHECK(splits[4] == doctest::Approx(farZ));

		// Verify 10% blend band thickness is positive and finite
		for (int i = 0; i < 3; ++i)
		{
			float blendBand = (splits[i + 1] - splits[i]) * 0.10f;
			CHECK(blendBand > 0.0f);
		}
	}

	TEST_CASE("PBR.SH: Constant Color Sky Irradiance Invariance")
	{
		// Constant white sky (radiance = 1.0 in all directions)
		// Projected into L2 Spherical Harmonics
		auto evalSHBasis = [](const Vector3& dir, float outY[9]) {
			outY[0] = 0.282095f;
			outY[1] = 0.488603f * dir.y;
			outY[2] = 0.488603f * dir.z;
			outY[3] = 0.488603f * dir.x;
			outY[4] = 1.092548f * dir.x * dir.y;
			outY[5] = 1.092548f * dir.y * dir.z;
			outY[6] = 0.315392f * (3.0f * dir.z * dir.z - 1.0f);
			outY[7] = 1.092548f * dir.x * dir.z;
			outY[8] = 0.546274f * (dir.x * dir.x - dir.y * dir.y);
		};

		static const Vector3 cubeDirs[6] = {
			Vector3( 1.0f,  0.0f,  0.0f), Vector3(-1.0f,  0.0f,  0.0f),
			Vector3( 0.0f,  1.0f,  0.0f), Vector3( 0.0f, -1.0f,  0.0f),
			Vector3( 0.0f,  0.0f,  1.0f), Vector3( 0.0f,  0.0f, -1.0f)
		};
		const float cubeWeight = (4.0f * 3.14159265f) / 6.0f;

		float coeffs[9] = {};
		for (int i = 0; i < 6; ++i)
		{
			float Y[9];
			evalSHBasis(cubeDirs[i], Y);
			for (int k = 0; k < 9; ++k)
			{
				coeffs[k] += 1.0f * Y[k] * cubeWeight;
			}
		}

		// Higher order moments must be 0 for constant isotropic sky
		for (int k = 1; k < 9; ++k)
		{
			CHECK(std::abs(coeffs[k]) < 0.05f);
		}

		// L00 coefficient must integrate to 2 * sqrt(pi)
		float expectedL00 = 2.0f * std::sqrt(3.14159265f);
		CHECK(coeffs[0] == doctest::Approx(expectedL00).epsilon(0.05f));
	}

	TEST_CASE("PBR.BRDF: Split-Sum GGX LUT Bounds and Edge Behavior")
	{
		// Van der Corput + Hammersley
		auto RadicalInverse_VdC = [](uint32_t bits) {
			bits = (bits << 16u) | (bits >> 16u);
			bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
			bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
			bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
			bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
			return static_cast<float>(bits) * 2.3283064365386963e-10f;
		};

		auto IntegrateBRDF = [&](float NoV, float roughness) {
			Vector3 V(std::sqrt(std::max(0.0f, 1.0f - NoV * NoV)), 0.0f, NoV);
			Vector3 N(0.0f, 0.0f, 1.0f);
			float A = 0.0f, B = 0.0f;
			const uint32_t kNumSamples = 256;
			for (uint32_t i = 0; i < kNumSamples; ++i)
			{
				Vector2 Xi(static_cast<float>(i) / static_cast<float>(kNumSamples), RadicalInverse_VdC(i));
				float a = roughness * roughness;
				float phi = 2.0f * 3.14159265f * Xi.x;
				float cosTheta = std::sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
				float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));
				Vector3 H(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta);
				Vector3 L = H * (2.0f * V.Dot(H)) - V;

				float NoL = std::max(L.z, 0.0f);
				float NoH = std::max(H.z, 0.0f);
				float VoH = std::max(V.Dot(H), 0.0f);
				if (NoL > 0.0f)
				{
					float k = (roughness * roughness) / 2.0f;
					float G_V = NoV / (NoV * (1.0f - k) + k);
					float G_L = NoL / (NoL * (1.0f - k) + k);
					float G = G_V * G_L;
					float G_Vis = (G * VoH) / (NoH * NoV);
					float Fc = std::pow(1.0f - VoH, 5.0f);
					A += (1.0f - Fc) * G_Vis;
					B += Fc * G_Vis;
				}
			}
			return Vector2(A / static_cast<float>(kNumSamples), B / static_cast<float>(kNumSamples));
		};

		// 1. Normal incidence, zero roughness: scale approx 1.0, bias approx 0.0
		Vector2 perfectMirror = IntegrateBRDF(1.0f, 0.01f);
		CHECK(perfectMirror.x == doctest::Approx(1.0f).epsilon(0.1f));
		CHECK(perfectMirror.y < 0.1f);

		// 2. All values in [0, 1] range across entire domain
		for (float r = 0.1f; r <= 1.0f; r += 0.3f)
		{
			for (float nov = 0.1f; nov <= 1.0f; nov += 0.3f)
			{
				Vector2 val = IntegrateBRDF(nov, r);
				CHECK(val.x >= 0.0f);
				CHECK(val.x <= 1.05f);
				CHECK(val.y >= 0.0f);
				CHECK(val.y <= 1.05f);
			}
		}
	}
}
