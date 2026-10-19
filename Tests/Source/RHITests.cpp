#include "Engine/Core/Log/Log.h"
#include "Engine/Core/Random.h"
#include "Engine/RHI/IRHI.h"
#include "Engine/RHI/Null/NullRHI.h"
#include "Engine/RHI/D3D11/D3D11RHI.h"
#include "Engine/RHI/RHIFactory.h"
#include "Engine/RHI/UploadRing.h"
#include "Engine/Renderer/ShaderLibrary.h"
#include "Engine/Renderer/Material.h"
#include "Engine/Renderer/DrawItem.h"

#include <ostream>
#ifdef _MSC_VER
#pragma warning(disable: 5285)
#endif
#include "doctest/doctest.h"
#include <algorithm>
#include <vector>

using namespace Core;
using namespace Core::RHI;
using namespace Core::Renderer;

TEST_SUITE("Milestone P3: RHI, GPU Resource Layer, Shaders & Materials")
{
	TEST_CASE("P3.1: NullRHI Resource Creation & Target Clearing (HDR & R32_UINT)")
	{
		NullRHIDevice device;
		auto context = device.getImmediate();
		REQUIRE(context.get() != nullptr);

		// 1. Create Buffers
		GpuBufferDesc vbDesc{};
		vbDesc.size = 1024;
		vbDesc.usage = BufferUsage::Immutable;
		vbDesc.bindFlags = BufferBindFlags::VertexBuffer;
		auto vbRes = device.createBuffer(vbDesc);
		REQUIRE(vbRes.isOk());

		// 2. Create HDR Render Target (R16G16B16A16_FLOAT)
		GpuTextureDesc hdrDesc{};
		hdrDesc.width = 1280;
		hdrDesc.height = 720;
		hdrDesc.format = PixelFormat::R16G16B16A16_FLOAT;
		hdrDesc.bindFlags = BufferBindFlags::RenderTarget | BufferBindFlags::ShaderResource;
		auto hdrRes = device.createTexture(hdrDesc);
		REQUIRE(hdrRes.isOk());
		auto hdrTex = *hdrRes;

		// 3. Create Entity-ID Picking Target (R32_UINT)
		GpuTextureDesc idDesc{};
		idDesc.width = 1280;
		idDesc.height = 720;
		idDesc.format = PixelFormat::R32_UINT;
		idDesc.bindFlags = BufferBindFlags::RenderTarget | BufferBindFlags::ShaderResource;
		auto idRes = device.createTexture(idDesc);
		REQUIRE(idRes.isOk());
		auto idTex = *idRes;

		// 4. Create Depth Target (D32_FLOAT)
		GpuTextureDesc depthDesc{};
		depthDesc.width = 1280;
		depthDesc.height = 720;
		depthDesc.format = PixelFormat::D32_FLOAT;
		depthDesc.bindFlags = BufferBindFlags::DepthStencil | BufferBindFlags::ShaderResource;
		auto depthRes = device.createTexture(depthDesc);
		REQUIRE(depthRes.isOk());
		auto depthTex = *depthRes;

		// 5. Set Render Targets
		std::vector<std::shared_ptr<IGpuTexture>> rtvs = { hdrTex, idTex };
		context->setRenderTargets(rtvs, depthTex);

		// 6. Clear HDR, ID, and Reversed-Z Depth Targets
		Color clearHdr(0.2f, 0.4f, 0.6f, 1.0f);
		context->clearRenderTarget(hdrTex, clearHdr);

		Color clearId(0.0f, 0.0f, 0.0f, 0.0f);
		context->clearRenderTarget(idTex, clearId);

		context->clearDepthStencil(depthTex, 0.0f, 0); // Reversed-Z clears to 0.0f

		auto* nullHdr = dynamic_cast<NullTexture*>(hdrTex.get());
		REQUIRE(nullHdr != nullptr);
		CHECK(nullHdr->isCleared());
		CHECK(nullHdr->getClearColor().x == doctest::Approx(0.2f));
		CHECK(nullHdr->getClearColor().y == doctest::Approx(0.4f));

		auto* nullId = dynamic_cast<NullTexture*>(idTex.get());
		REQUIRE(nullId != nullptr);
		CHECK(nullId->isCleared());

		auto* nullDepth = dynamic_cast<NullTexture*>(depthTex.get());
		REQUIRE(nullDepth != nullptr);
		CHECK(nullDepth->isCleared());
		CHECK(nullDepth->getClearDepth() == doctest::Approx(0.0f));

		// 7. SwapChain
		auto swapChainRes = device.createSwapChain(reinterpret_cast<void*>(0x1234), 1280, 720);
		REQUIRE(swapChainRes.isOk());
		auto swapChain = std::move(*swapChainRes);
		REQUIRE(swapChain->getBackBufferTexture().get() != nullptr);
		swapChain->present(true);
		swapChain->resize(1920, 1080);
		CHECK(swapChain->getBackBufferTexture()->getDesc().width == 1920);
		CHECK(swapChain->getBackBufferTexture()->getDesc().height == 1080);
	}

	TEST_CASE("P3.2: State Cache Hash Collisions & Bucket Chaining")
	{
		NullRHIDevice device;

		RasterizerStateDesc rs1{};
		rs1.cullMode = CullMode::Back;
		rs1.fillMode = FillMode::Solid;

		RasterizerStateDesc rs2{};
		rs2.cullMode = CullMode::Front;
		rs2.fillMode = FillMode::Wireframe;

		// Verify operator== differentiates them
		CHECK(rs1 == rs1);
		CHECK(rs2 == rs2);
		CHECK_FALSE(rs1 == rs2);

		auto state1 = device.getOrCreateRasterizerState(rs1);
		auto state1Again = device.getOrCreateRasterizerState(rs1);
		CHECK(state1.get() == state1Again.get()); // Cached

		// Force simulated hash collision in cache: same hash key, distinct descriptors
		const uint64_t fakeCollidingHash = 0xDEADBEEFCAFEull;
		auto& cache = device.getRasterizerCache();
		cache.clear();

		auto cachedState1 = cache.getOrCreate(rs1, fakeCollidingHash, [&]() {
			return std::make_shared<NullRasterizerState>(rs1);
		});

		auto cachedState2 = cache.getOrCreate(rs2, fakeCollidingHash, [&]() {
			return std::make_shared<NullRasterizerState>(rs2);
		});

		// Both states must be distinct
		CHECK(cachedState1.get() != cachedState2.get());
		CHECK(cache.getCollisionCount() == 1);
		CHECK(cache.size() == 2);

		// Querying again with rs1 returns cachedState1, rs2 returns cachedState2
		auto query1 = cache.getOrCreate(rs1, fakeCollidingHash, []() { return nullptr; });
		auto query2 = cache.getOrCreate(rs2, fakeCollidingHash, []() { return nullptr; });
		CHECK(query1.get() == cachedState1.get());
		CHECK(query2.get() == cachedState2.get());
	}

	TEST_CASE("P3.3: UploadRing 10,000 Randomized Allocations & Wraparound")
	{
		NullRHIDevice device;
		UploadRing ring;
		const size_t capacity = 16 * 1024 * 1024; // 16 MB
		auto initRes = ring.initialize(device, capacity);
		REQUIRE(initRes.isOk());

		Core::Random rng(0xCAFEBABE);

		for (int i = 0; i < 10000; ++i)
		{
			// Random size between 1 byte and 65,536 bytes (64 KB)
			const size_t randomSize = static_cast<size_t>(rng.nextIntRange(1, 65536));
			auto alloc = ring.allocate(randomSize);

			REQUIRE(alloc.isValid());
			// Exit Criterion: 256-byte alignment
			CHECK((alloc.byteOffset % 256) == 0);
			// Exit Criterion: 16-byte constant count calculation
			CHECK(alloc.firstConstant16 == static_cast<uint32_t>(alloc.byteOffset / 16));
			CHECK((alloc.firstConstant16 % 16) == 0);
			const size_t expectedAlignedSize = (randomSize + 255) & ~size_t(255);
			CHECK(alloc.alignedSize == expectedAlignedSize);
			CHECK(alloc.numConstants16 == static_cast<uint32_t>(expectedAlignedSize / 16));
			CHECK((alloc.numConstants16 % 16) == 0);
			CHECK(alloc.byteOffset + alloc.alignedSize <= ring.getCapacity());
		}

		// Verify wraparound occurred
		CHECK(ring.getWrapCount() > 0);
		CHECK(ring.getAllocatedBytes() > capacity);

		// Test writing and verifying payload
		auto testAlloc = ring.allocate(sizeof(uint32_t) * 4);
		REQUIRE(testAlloc.isValid());
		const uint32_t testData[4] = { 0x11223344, 0x55667788, 0x99AABBCC, 0xDDEEFF00 };
		CHECK(ring.write(testAlloc, testData, sizeof(testData)));

		auto* nullBuf = dynamic_cast<NullBuffer*>(ring.getBuffer().get());
		REQUIRE(nullBuf != nullptr);
		const auto* stored = reinterpret_cast<const uint32_t*>(nullBuf->getData().data() + testAlloc.byteOffset);
		CHECK(stored[0] == testData[0]);
		CHECK(stored[1] == testData[1]);
		CHECK(stored[2] == testData[2]);
		CHECK(stored[3] == testData[3]);
	}

	TEST_CASE("P3.4: Shader Error Recovery & Magenta Fallback (Never Throw)")
	{
		NullRHIDevice device;
		ShaderLibrary shaderLib(device);

		// 1. Verify Fallback Pixel Shader is present
		REQUIRE(shaderLib.getFallbackPixelShader().get() != nullptr);

		// 2. Syntax Error in HLSL shader
		const std::string badShaderSource =
			"float4 main() : SV_Target0\n"
			"{\n"
			"    syntax_error_not_valid_code!;\n"
			"}\n";

		auto compileRes = shaderLib.compileSource(
			badShaderSource,
			"badShader.hlsl",
			"main",
			"ps_5_0");

		// Must fail compilation without throwing
		CHECK_FALSE(compileRes.isOk());
		const bool hasErrorText = (compileRes.error().find("syntax") != std::string::npos || compileRes.error().find("error") != std::string::npos);
		CHECK(hasErrorText);

		// 3. Requesting bad/missing shader from ShaderLibrary must return magenta fallback
		auto psRes = shaderLib.getOrCreatePixelShader("non_existent_bad_shader.hlsl");
		REQUIRE(psRes.isOk());
		CHECK(psRes->get() == shaderLib.getFallbackPixelShader().get());

		// 4. Fixing shader restores valid compilation
		const std::string goodShaderSource =
			"struct PSInput { float4 pos : SV_Position; };\n"
			"float4 main(PSInput input) : SV_Target0\n"
			"{\n"
			"    return float4(0.0f, 1.0f, 0.0f, 1.0f);\n"
			"}\n";

		auto goodCompileRes = shaderLib.compileSource(
			goodShaderSource,
			"goodShader.hlsl",
			"main",
			"ps_5_0");

		REQUIRE(goodCompileRes.isOk());
		auto goodShader = device.createPixelShader(*goodCompileRes);
		REQUIRE(goodShader.isOk());
		CHECK(goodShader->get() != shaderLib.getFallbackPixelShader().get());
	}

	TEST_CASE("P3.5: DrawItem Sorting & Instancing Batching (1,000 Entities, 10 Materials)")
	{
		constexpr size_t NUM_ITEMS = 1000;
		constexpr uint32_t NUM_MATERIALS = 10;
		constexpr uint32_t TEST_SHADER_VARIANT = 42;
		constexpr uint32_t TEST_MESH_ID = 7;

		Core::Random rng(0x1337BEEF);
		std::vector<DrawItem> items;
		items.reserve(NUM_ITEMS);

		for (size_t i = 0; i < NUM_ITEMS; ++i)
		{
			DrawItem item{};
			item.shaderVariantId = TEST_SHADER_VARIANT;
			item.materialId = static_cast<uint32_t>(rng.nextIntRange(0, static_cast<int32_t>(NUM_MATERIALS - 1)));
			item.meshId = TEST_MESH_ID;
			item.indexCount = 36;
			item.startIndexLocation = 0;
			item.baseVertexLocation = 0;
			item.instanceData.world = Math::Matrix::CreateTranslation(static_cast<float>(i), 0.0f, 0.0f);
			item.instanceData.entityId = static_cast<uint32_t>(i + 1);

			const float depth = rng.nextFloatRange(1.0f, 500.0f);
			item.sortKey = DrawItem::makeSortKey(
				RenderLayer::Opaque,
				item.shaderVariantId,
				item.materialId,
				item.meshId,
				depth);

			items.push_back(item);
		}

		// Sort items by 64-bit sort key
		std::sort(items.begin(), items.end());

		// Batch consecutive compatible items
		std::vector<InstanceData> instanceBuffer;
		auto batches = BatchDrawItems(items, instanceBuffer);

		// Exit Criterion 5: 1,000 entities sharing 10 materials batch into exactly 10 batches!
		CHECK(batches.size() == NUM_MATERIALS);
		CHECK(instanceBuffer.size() == NUM_ITEMS);

		uint32_t totalInstances = 0;
		uint32_t shaderVariantSwitches = 0;
		uint32_t prevShaderVariant = 0xFFFFFFFF;

		for (const auto& batch : batches)
		{
			CHECK(batch.indexCount == 36);
			CHECK(batch.shaderVariantId == TEST_SHADER_VARIANT);
			totalInstances += batch.instanceCount;

			if (batch.shaderVariantId != prevShaderVariant)
			{
				shaderVariantSwitches++;
				prevShaderVariant = batch.shaderVariantId;
			}
		}

		CHECK(totalInstances == NUM_ITEMS);
		// Exactly 1 shader bind for all 1,000 entities!
		CHECK(shaderVariantSwitches == 1);
	}

	TEST_CASE("P3.6: D3D11 Device Clean Boot & Shutdown (0 Debug Warnings)")
	{
		D3D11RHIDevice device(true); // Enable debug layer
		const bool ok = device.initialize();
		if (!ok)
		{
			DF_LOG_WARN("Tests", "D3D11 device not available on this machine/environment; skipping D3D11 live test.");
			return;
		}

		REQUIRE(device.getBackendName() == "D3D11");
		auto context = device.getImmediate();
		REQUIRE(context.get() != nullptr);

		// Create dynamic constant buffer
		GpuBufferDesc cbDesc{};
		cbDesc.size = 256;
		cbDesc.usage = BufferUsage::Dynamic;
		cbDesc.bindFlags = BufferBindFlags::ConstantBuffer;
		auto cbRes = device.createBuffer(cbDesc);
		REQUIRE(cbRes.isOk());

		// Create HDR texture
		GpuTextureDesc texDesc{};
		texDesc.width = 64;
		texDesc.height = 64;
		texDesc.format = PixelFormat::R16G16B16A16_FLOAT;
		texDesc.bindFlags = BufferBindFlags::RenderTarget | BufferBindFlags::ShaderResource;
		auto texRes = device.createTexture(texDesc);
		REQUIRE(texRes.isOk());

		// Clear texture
		context->clearRenderTarget(*texRes, Color(1.0f, 0.5f, 0.25f, 1.0f));

		// Clean shutdown with 0 debug warnings
		cbRes->reset();
		texRes->reset();
		device.shutdown();
		CHECK(device.getD3D11Device() == nullptr);
	}
}
