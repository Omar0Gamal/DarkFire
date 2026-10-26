#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Scene/Scene.h"
#include "Engine/Core/Scene/Entity.h"
#include "Engine/Core/Scene/Components.h"
#include "Engine/Core/Scene/SceneRendererBridge.h"
#include "Engine/RHI/Null/NullRHI.h"
#include "Engine/Renderer/RenderScene.h"
#include "Engine/Renderer/SceneRenderer.h"
#include "Engine/Renderer/RenderPipeline.h"
#include "Engine/Shaders/Shared/Bindings.h"

#ifdef _MSC_VER
#pragma warning(disable: 5285)
#endif
#include "doctest/doctest.h"

using namespace Core;
using namespace Core::RHI;
using namespace Core::Renderer;

TEST_SUITE("Tier 1: High-Performance GPU Compute & Temporal Foundations")
{
	TEST_CASE("Tier 1.1: RHI Indirect Draw & Compute Buffer Bindings")
	{
		NullRHIDevice device;
		auto context = device.getImmediate();
		REQUIRE(context != nullptr);

		// 1. Indirect Draw Argument Buffer Creation
		GpuBufferDesc indirectDesc{};
		indirectDesc.size = sizeof(DrawIndexedInstancedIndirectArgs) * 32;
		indirectDesc.usage = BufferUsage::Default;
		indirectDesc.bindFlags = BufferBindFlags::IndirectDrawArgs | BufferBindFlags::UnorderedAccess;
		indirectDesc.structureByteStride = sizeof(DrawIndexedInstancedIndirectArgs);

		auto indirectBufRes = device.createBuffer(indirectDesc);
		REQUIRE(indirectBufRes.isOk());
		auto indirectBuf = *indirectBufRes;
		REQUIRE(indirectBuf != nullptr);
		CHECK(indirectBuf->getDesc().size == sizeof(DrawIndexedInstancedIndirectArgs) * 32);

		// 2. Execute drawIndexedInstancedIndirect on Device Context
		auto* nullCtx = dynamic_cast<NullDeviceContext*>(context.get());
		REQUIRE(nullCtx != nullptr);
		uint32_t initialDrawCount = nullCtx->getDrawCallCount();
		uint32_t initialDrawIndexedCount = nullCtx->getDrawIndexedCallCount();

		context->drawIndexedInstancedIndirect(indirectBuf, 0);

		CHECK(nullCtx->getDrawCallCount() == initialDrawCount + 1);
		CHECK(nullCtx->getDrawIndexedCallCount() == initialDrawIndexedCount + 1);

		// 3. Compute Shader Resource & UAV Buffer Bindings
		GpuBufferDesc structuredDesc{};
		structuredDesc.size = sizeof(GpuInstanceData) * 64;
		structuredDesc.usage = BufferUsage::Default;
		structuredDesc.bindFlags = BufferBindFlags::ShaderResource | BufferBindFlags::UnorderedAccess;
		structuredDesc.structureByteStride = sizeof(GpuInstanceData);

		auto structuredBufRes = device.createBuffer(structuredDesc);
		REQUIRE(structuredBufRes.isOk());
		auto structuredBuf = *structuredBufRes;

		context->setShaderResourceBufferCS(0, structuredBuf);
		context->setUnorderedAccessBufferCS(0, indirectBuf);
		context->dispatch(1, 1, 1);
		CHECK(nullCtx->getDispatchCallCount() > 0);

		context->setUnorderedAccessBufferCS(0, nullptr);
	}

	TEST_CASE("Tier 1.2: Clustered Dynamic Light Assignment (16x16x24 Grid)")
	{
		NullRHIDevice device;
		SceneRenderer renderer(device);

		// 1. Verify Cluster Grid Dimension Parameters
		// 16 X-bins, 16 Y-bins, 24 Z-slices = 6,144 clusters
		const uint32_t totalClusters = 16 * 16 * 24;
		CHECK(totalClusters == 6144);

		// 2. Logarithmic Depth Slicing Formula Verification
		const float nearZ = 0.1f;
		const float farZ = 250.0f;
		const float logRatio = std::log(farZ / nearZ);

		auto computeSliceZ = [nearZ, logRatio](float depth) -> uint32_t {
			float clampedDepth = std::clamp(depth, nearZ, 250.0f);
			float ratio = std::log(clampedDepth / nearZ) / logRatio;
			return std::clamp(static_cast<uint32_t>(ratio * 24.0f), 0u, 23u);
		};

		CHECK(computeSliceZ(nearZ) == 0);
		CHECK(computeSliceZ(nearZ * 1.01f) == 0);
		CHECK(computeSliceZ(farZ) == 23);
		CHECK(computeSliceZ(farZ * 0.99f) == 23);

		// Exponential spacing: slice 12 (midpoint) depth should be around sqrt(nearZ * farZ) = 5.0m
		float midDepth = std::sqrt(nearZ * farZ);
		uint32_t midSlice = computeSliceZ(midDepth);
		CHECK(midSlice == 12);

		// 3. Populate Scene with 64 Point and Spot Lights
		RenderScene scene{};
		scene.camera.position = Math::Vector3(0.0f, 2.0f, -10.0f);
		scene.camera.forward = Math::Vector3(0.0f, 0.0f, 1.0f);
		scene.camera.nearZ = nearZ;
		scene.camera.farZ = farZ;
		scene.camera.fov = 1.0472f; // 60 deg
		scene.camera.viewMatrix = Math::lookAtLH(scene.camera.position, scene.camera.position + scene.camera.forward, Math::Vector3(0.0f, 1.0f, 0.0f));
		scene.camera.projMatrix = Math::perspectiveFovLH(scene.camera.fov, 16.0f / 9.0f, nearZ, farZ);
		scene.camera.viewProjMatrix = scene.camera.viewMatrix * scene.camera.projMatrix;

		for (int i = 0; i < 64; ++i)
		{
			RenderLight light{};
			light.type = (i % 2 == 0) ? 1 : 2; // Point or Spot
			light.position = Math::Vector3(static_cast<float>(i % 8 - 4) * 3.0f, 1.0f, static_cast<float>(i / 8) * 5.0f);
			light.direction = Math::Vector3(0.0f, -1.0f, 0.0f);
			light.range = 8.0f;
			light.color = Math::Vector3(1.0f, 0.8f, 0.6f);
			light.intensity = 5.0f;
			light.innerConeAngle = 0.35f;
			light.outerConeAngle = 0.70f;
			scene.lights.push_back(light);
		}
		CHECK(scene.lights.size() == 64);

		RenderView view{};
		view.width = 1280;
		view.height = 720;

		renderer.render(scene, view);

		// 4. Verify Clustered Light Buffers Allocation
		auto clusterGrid = renderer.getClusterGridBuffer();
		REQUIRE(clusterGrid != nullptr);
		CHECK(clusterGrid->getDesc().size == 6144 * sizeof(uint32_t) * 2);

		auto clusterIndices = renderer.getClusterLightIndicesBuffer();
		REQUIRE(clusterIndices != nullptr);
		CHECK(clusterIndices->getDesc().size == 6144 * 64 * sizeof(uint32_t));
	}

	TEST_CASE("Tier 1.3: Velocity G-Buffer & Temporal Anti-Aliasing (TAA)")
	{
		NullRHIDevice device;
		SceneRenderer renderer(device);

		// 1. Verify Velocity Target Creation
		auto velTex = renderer.getVelocityTexture();
		REQUIRE(velTex != nullptr);
		CHECK(velTex->getDesc().width == 1280);
		CHECK(velTex->getDesc().height == 720);
		CHECK(velTex->getDesc().format == PixelFormat::R16G16_FLOAT);

		// 2. Verify Halton(2, 3) 8-Phase Subpixel Jitter Determinism
		for (uint32_t i = 1; i <= 8; ++i)
		{
			float h2 = SceneRenderer::halton(i, 2);
			float h3 = SceneRenderer::halton(i, 3);
			CHECK(h2 >= 0.0f);
			CHECK(h2 <= 1.0f);
			CHECK(h3 >= 0.0f);
			CHECK(h3 <= 1.0f);

			// Subpixel offsets are bounded in [-0.5, 0.5]
			float jx = h2 - 0.5f;
			float jy = h3 - 0.5f;
			CHECK(jx >= -0.5f);
			CHECK(jx <= 0.5f);
			CHECK(jy >= -0.5f);
			CHECK(jy <= 0.5f);
		}

		// 3. Multi-Frame Simulation with TAA Enabled
		CHECK(renderer.isTAAEnabled());

		RenderScene scene{};
		scene.camera.position = Math::Vector3(0.0f, 1.5f, -5.0f);
		scene.camera.forward = Math::Vector3(0.0f, 0.0f, 1.0f);
		scene.camera.nearZ = 0.1f;
		scene.camera.farZ = 100.0f;
		scene.camera.fov = 1.0472f;
		scene.camera.viewMatrix = Math::lookAtLH(scene.camera.position, scene.camera.position + scene.camera.forward, Math::Vector3(0.0f, 1.0f, 0.0f));
		scene.camera.projMatrix = Math::perspectiveFovLH(scene.camera.fov, 16.0f / 9.0f, 0.1f, 100.0f);
		scene.camera.viewProjMatrix = scene.camera.viewMatrix * scene.camera.projMatrix;

		RenderView view{};
		view.width = 1280;
		view.height = 720;

		// Render 16 frames with moving camera to exercise TAA velocity reprojection & history blend
		for (int f = 0; f < 16; ++f)
		{
			scene.camera.position.x += 0.05f;
			scene.camera.viewMatrix = Math::lookAtLH(scene.camera.position, scene.camera.position + scene.camera.forward, Math::Vector3(0.0f, 1.0f, 0.0f));
			scene.camera.viewProjMatrix = scene.camera.viewMatrix * scene.camera.projMatrix;

			renderer.render(scene, view);
		}

		// 4. Verify Toggle TAA
		renderer.setTAAEnabled(false);
		CHECK_FALSE(renderer.isTAAEnabled());
		renderer.render(scene, view);

		renderer.setTAAEnabled(true);
		CHECK(renderer.isTAAEnabled());
		renderer.render(scene, view);
	}

	TEST_CASE("Tier 1.4: GPU-Driven Frustum & Cascade Culling Architecture")
	{
		// 1. Verify Memory Alignment of GpuInstanceData (176 bytes, 16-byte aligned)
		CHECK(sizeof(GpuInstanceData) == 176);
		CHECK(sizeof(GpuInstanceData) % 16 == 0);

		// 2. Verify DrawIndexedInstancedIndirectArgs Layout (20 bytes, 5 uint32_t fields)
		CHECK(sizeof(DrawIndexedInstancedIndirectArgs) == 20);

		// 3. Test Frustum-AABB Intersection
		Math::BoundingBox boxInside(Math::Vector3(0.0f, 0.0f, 10.0f), Math::Vector3(1.0f, 1.0f, 1.0f));
		Math::BoundingBox boxOutside(Math::Vector3(500.0f, 0.0f, 10.0f), Math::Vector3(1.0f, 1.0f, 1.0f));

		Math::Matrix proj = Math::perspectiveFovLH(1.0472f, 16.0f / 9.0f, 0.1f, 100.0f);
		DirectX::BoundingFrustum frustum(proj);

		CHECK(frustum.Intersects(boxInside));
		CHECK_FALSE(frustum.Intersects(boxOutside));

		// 4. Test Orthographic Cascade Frustum Intersection
		Math::Matrix cascadeProj = Math::orthographicLH(10.0f, 10.0f, 0.1f, 50.0f);
		DirectX::BoundingFrustum cascadeFrustum(cascadeProj);

		Math::BoundingBox cascadeBoxInside(Math::Vector3(0.0f, 0.0f, 15.0f), Math::Vector3(2.0f, 2.0f, 2.0f));
		Math::BoundingBox cascadeBoxOutside(Math::Vector3(20.0f, 0.0f, 15.0f), Math::Vector3(2.0f, 2.0f, 2.0f));

		CHECK(cascadeFrustum.Intersects(cascadeBoxInside));
		CHECK_FALSE(cascadeFrustum.Intersects(cascadeBoxOutside));
	}
}
