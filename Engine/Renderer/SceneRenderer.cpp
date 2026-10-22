#include "SceneRenderer.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/Runtime/EngineContext.h"
#include "Engine/Core/VFS/AssetPathResolver.h"
#include <DirectXTex.h>
#include <nlohmann/json.hpp>
#include <fstream>
#include <sstream>
#include <algorithm>
#include <array>
#include <cstring>
#include <d3d11.h>

namespace Core::Renderer
{
	SceneRenderer::SceneRenderer(RHI::IRHIDevice& device)
		: m_device(device)
		, m_context(device.getImmediate())
		, m_shaderLibrary(device)
	{
		initializeResources();
	}

	SceneRenderer::~SceneRenderer()
	{
		if (m_stagingEntityId)
		{
			static_cast<ID3D11Texture2D*>(m_stagingEntityId)->Release();
			m_stagingEntityId = nullptr;
		}
	}

	void SceneRenderer::initializeResources()
	{
		m_uploadRing.initialize(m_device, 16 * 1024 * 1024);

		// 1. PerFrame Constant Buffer
		RHI::GpuBufferDesc frameDesc{};
		frameDesc.size = sizeof(CBPerFrame);
		frameDesc.usage = RHI::BufferUsage::Default;
		frameDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto frameRes = m_device.createBuffer(frameDesc);
		if (frameRes.isOk()) m_cbFrame = *frameRes;

		// 2. PerView Constant Buffer
		RHI::GpuBufferDesc viewDesc{};
		viewDesc.size = sizeof(CBPerView);
		viewDesc.usage = RHI::BufferUsage::Default;
		viewDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto viewRes = m_device.createBuffer(viewDesc);
		if (viewRes.isOk()) m_cbView = *viewRes;

		// 3. PerPass Constant Buffer
		RHI::GpuBufferDesc passDesc{};
		passDesc.size = sizeof(CBPerPass);
		passDesc.usage = RHI::BufferUsage::Default;
		passDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto passRes = m_device.createBuffer(passDesc);
		if (passRes.isOk()) m_cbPass = *passRes;

		// 4. PerMaterial Constant Buffer
		RHI::GpuBufferDesc matDesc{};
		matDesc.size = sizeof(CBPerMaterial);
		matDesc.usage = RHI::BufferUsage::Default;
		matDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto matRes = m_device.createBuffer(matDesc);
		if (matRes.isOk()) m_cbMaterial = *matRes;

		// 5. PostProcess & Frame Feature Constant Buffers
		RHI::GpuBufferDesc ppDesc{};
		ppDesc.size = sizeof(CBPostProcess);
		ppDesc.usage = RHI::BufferUsage::Default;
		ppDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto ppRes = m_device.createBuffer(ppDesc);
		if (ppRes.isOk()) m_cbPostProcess = *ppRes;

		RHI::GpuBufferDesc envDesc{};
		envDesc.size = sizeof(CBEnvironment);
		envDesc.usage = RHI::BufferUsage::Default;
		envDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto envRes = m_device.createBuffer(envDesc);
		if (envRes.isOk()) m_cbEnvironment = *envRes;

		RHI::GpuBufferDesc ssaoDesc{};
		ssaoDesc.size = sizeof(CBSSAO);
		ssaoDesc.usage = RHI::BufferUsage::Default;
		ssaoDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto ssaoRes = m_device.createBuffer(ssaoDesc);
		if (ssaoRes.isOk()) m_cbSSAO = *ssaoRes;

		RHI::GpuBufferDesc blurDesc{};
		blurDesc.size = 16;
		blurDesc.usage = RHI::BufferUsage::Default;
		blurDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto blurRes = m_device.createBuffer(blurDesc);
		if (blurRes.isOk()) m_cbSSAOBlur = *blurRes;

		RHI::GpuBufferDesc bloomDesc{};
		bloomDesc.size = sizeof(CBBloom);
		bloomDesc.usage = RHI::BufferUsage::Default;
		bloomDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto bloomRes = m_device.createBuffer(bloomDesc);
		if (bloomRes.isOk()) m_cbBloom = *bloomRes;

		RHI::GpuBufferDesc fxaaDesc{};
		fxaaDesc.size = 16;
		fxaaDesc.usage = RHI::BufferUsage::Default;
		fxaaDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto fxaaRes = m_device.createBuffer(fxaaDesc);
		if (fxaaRes.isOk()) m_cbFXAA = *fxaaRes;

		generateSSAOKernel();

		// 6. Lights Structured Buffer
		RHI::GpuBufferDesc lightDesc{};
		lightDesc.size = sizeof(LightData) * 64; // Support up to 64 lights per pass
		lightDesc.usage = RHI::BufferUsage::Default;
		lightDesc.bindFlags = RHI::BufferBindFlags::ShaderResource;
		lightDesc.structureByteStride = sizeof(LightData);
		auto lightRes = m_device.createBuffer(lightDesc);
		if (lightRes.isOk()) m_bufLights = *lightRes;

		// 7. Pipeline States
		RHI::RasterizerStateDesc rsDesc{};
		rsDesc.fillMode = RHI::FillMode::Solid;
		rsDesc.cullMode = RHI::CullMode::None; // None or Back; None is forgiving for primitives
		rsDesc.frontCounterClockwise = false;
		m_rsSolid = m_device.getOrCreateRasterizerState(rsDesc);

		// Depth Prepass: test & write GreaterEqual (Reversed-Z)
		RHI::DepthStencilStateDesc dsPrepassDesc{};
		dsPrepassDesc.depthEnable = true;
		dsPrepassDesc.depthWrite = true;
		dsPrepassDesc.depthFunc = RHI::ComparisonFunc::GreaterEqual;
		m_dssDepthPrepass = m_device.getOrCreateDepthStencilState(dsPrepassDesc);

		// Opaque Pass
		RHI::DepthStencilStateDesc dsDesc{};
		dsDesc.depthEnable = true;
		dsDesc.depthWrite = true;
		dsDesc.depthFunc = RHI::ComparisonFunc::GreaterEqual; // Reversed-Z
		m_dssOpaque = m_device.getOrCreateDepthStencilState(dsDesc);

		// Skybox: depth == 0.0 (far plane in reversed-Z), no depth write
		RHI::DepthStencilStateDesc dsSkyDesc{};
		dsSkyDesc.depthEnable = true;
		dsSkyDesc.depthWrite = false;
		dsSkyDesc.depthFunc = RHI::ComparisonFunc::Equal;
		m_dssSky = m_device.getOrCreateDepthStencilState(dsSkyDesc);

		// Transparent: depth test GreaterEqual, no depth write
		RHI::DepthStencilStateDesc dsTransDesc{};
		dsTransDesc.depthEnable = true;
		dsTransDesc.depthWrite = false;
		dsTransDesc.depthFunc = RHI::ComparisonFunc::GreaterEqual;
		m_dssTransparent = m_device.getOrCreateDepthStencilState(dsTransDesc);

		// Blend States
		RHI::BlendStateDesc bsAlpha{};
		bsAlpha.blendEnable = true;
		bsAlpha.srcBlend = RHI::BlendFactor::SrcAlpha;
		bsAlpha.destBlend = RHI::BlendFactor::InvSrcAlpha;
		bsAlpha.blendOp = RHI::BlendOp::Add;
		bsAlpha.srcBlendAlpha = RHI::BlendFactor::One;
		bsAlpha.destBlendAlpha = RHI::BlendFactor::Zero;
		bsAlpha.blendOpAlpha = RHI::BlendOp::Add;
		m_bsAlphaBlend = m_device.getOrCreateBlendState(bsAlpha);

		RHI::BlendStateDesc bsAdd{};
		bsAdd.blendEnable = true;
		bsAdd.srcBlend = RHI::BlendFactor::One;
		bsAdd.destBlend = RHI::BlendFactor::One;
		bsAdd.blendOp = RHI::BlendOp::Add;
		bsAdd.srcBlendAlpha = RHI::BlendFactor::One;
		bsAdd.destBlendAlpha = RHI::BlendFactor::One;
		bsAdd.blendOpAlpha = RHI::BlendOp::Add;
		m_bsAdditive = m_device.getOrCreateBlendState(bsAdd);

		RHI::SamplerStateDesc smpLinearWrap{};
		smpLinearWrap.filter = RHI::FilterMode::Linear;
		smpLinearWrap.addressU = RHI::AddressMode::Wrap;
		smpLinearWrap.addressV = RHI::AddressMode::Wrap;
		smpLinearWrap.addressW = RHI::AddressMode::Wrap;
		m_samplerLinearWrap = m_device.getOrCreateSamplerState(smpLinearWrap);

		RHI::SamplerStateDesc smpLinearClamp{};
		smpLinearClamp.filter = RHI::FilterMode::Linear;
		smpLinearClamp.addressU = RHI::AddressMode::Clamp;
		smpLinearClamp.addressV = RHI::AddressMode::Clamp;
		smpLinearClamp.addressW = RHI::AddressMode::Clamp;
		m_samplerLinearClamp = m_device.getOrCreateSamplerState(smpLinearClamp);

		RHI::SamplerStateDesc smpPointClamp{};
		smpPointClamp.filter = RHI::FilterMode::Point;
		smpPointClamp.addressU = RHI::AddressMode::Clamp;
		smpPointClamp.addressV = RHI::AddressMode::Clamp;
		smpPointClamp.addressW = RHI::AddressMode::Clamp;
		m_samplerPointClamp = m_device.getOrCreateSamplerState(smpPointClamp);

		// 8. Shaders & Input Layout
		auto vsRes = m_shaderLibrary.getOrCreateVertexShader("StandardVS.hlsl");
		if (vsRes.isOk()) m_vsStandard = *vsRes;

		auto psRes = m_shaderLibrary.getOrCreatePixelShader("StandardPS.hlsl");
		if (psRes.isOk()) m_psStandard = *psRes;

		if (m_vsStandard)
		{
			const std::vector<RHI::InputElementDesc> inputElements = {
				{ "POSITION", 0, RHI::PixelFormat::R32G32B32_FLOAT, 0, 0, false },
				{ "NORMAL", 0, RHI::PixelFormat::R32G32B32_FLOAT, 0, 12, false },
				{ "TEXCOORD", 0, RHI::PixelFormat::R32G32_FLOAT, 0, 24, false },
				{ "TANGENT", 0, RHI::PixelFormat::R32G32B32A32_FLOAT, 0, 32, false },
				{ "COLOR", 0, RHI::PixelFormat::R32G32B32A32_FLOAT, 0, 48, false }
			};
			auto layoutRes = m_device.createInputLayout(inputElements, m_vsStandard->getBytecode());
			if (layoutRes.isOk()) m_inputLayout = *layoutRes;
		}

		auto vsFsRes = m_shaderLibrary.getOrCreateVertexShader("FullscreenVS.hlsl");
		if (vsFsRes.isOk()) m_vsFullscreen = *vsFsRes;

		auto psPpRes = m_shaderLibrary.getOrCreatePixelShader("PostProcessPS.hlsl");
		if (psPpRes.isOk()) m_psPostProcess = *psPpRes;

		auto psSkyRes = m_shaderLibrary.getOrCreatePixelShader("SkyPS.hlsl");
		if (psSkyRes.isOk()) m_psSky = *psSkyRes;

		auto psSSAORes = m_shaderLibrary.getOrCreatePixelShader("SSAOPS.hlsl");
		if (psSSAORes.isOk()) m_psSSAO = *psSSAORes;

		auto psSSAOBlurRes = m_shaderLibrary.getOrCreatePixelShader("SSAOBlurPS.hlsl");
		if (psSSAOBlurRes.isOk()) m_psSSAOBlur = *psSSAOBlurRes;

		auto psBloomPreRes = m_shaderLibrary.getOrCreatePixelShader("BloomPrefilterPS.hlsl");
		if (psBloomPreRes.isOk()) m_psBloomPrefilter = *psBloomPreRes;

		auto psBloomDownRes = m_shaderLibrary.getOrCreatePixelShader("BloomDownsamplePS.hlsl");
		if (psBloomDownRes.isOk()) m_psBloomDownsample = *psBloomDownRes;

		auto psBloomUpRes = m_shaderLibrary.getOrCreatePixelShader("BloomUpsamplePS.hlsl");
		if (psBloomUpRes.isOk()) m_psBloomUpsample = *psBloomUpRes;

		auto psTonemapRes = m_shaderLibrary.getOrCreatePixelShader("TonemapPS.hlsl");
		if (psTonemapRes.isOk()) m_psTonemap = *psTonemapRes;

		auto psFXAARes = m_shaderLibrary.getOrCreatePixelShader("FXAAPS.hlsl");
		if (psFXAARes.isOk()) m_psFXAA = *psFXAARes;

		// 9. Default 1x1 Textures
		RHI::GpuTextureDesc tex1x1{};
		tex1x1.width = 1;
		tex1x1.height = 1;
		tex1x1.format = RHI::PixelFormat::R8G8B8A8_UNORM;
		tex1x1.bindFlags = RHI::BufferBindFlags::ShaderResource;

		const uint32_t whitePix = 0xFFFFFFFF;
		auto wTex = m_device.createTexture(tex1x1, &whitePix);
		if (wTex.isOk()) m_whiteTexture = *wTex;

		const uint32_t flatNormPix = 0xFFFF8080; // RGBA = 128, 128, 255, 255
		auto nTex = m_device.createTexture(tex1x1, &flatNormPix);
		if (nTex.isOk()) m_flatNormalTexture = *nTex;

		const uint32_t defaultMRPix = 0xFF008000; // RGBA = 0, 128, 0, 255 (metal 0, rough 0.5)
		auto mrTex = m_device.createTexture(tex1x1, &defaultMRPix);
		if (mrTex.isOk()) m_defaultMRTexture = *mrTex;

		const uint32_t blackPix = 0xFF000000;
		auto bTex = m_device.createTexture(tex1x1, &blackPix);
		if (bTex.isOk()) m_blackTexture = *bTex;

		// 10. Seed Builtin Meshes
		auto cubeData = CreateCubeMesh(1.0f);
		auto cubeGpu = createGpuMesh(cubeData);
		m_meshCache[""] = cubeGpu;
		m_meshCache["Cube"] = cubeGpu;
		m_meshCache["cube"] = cubeGpu;
		m_meshCache["cube.obj"] = cubeGpu;

		auto sphereData = CreateSphereMesh(0.5f, 16, 32);
		auto sphereGpu = createGpuMesh(sphereData);
		m_meshCache["Sphere"] = sphereGpu;
		m_meshCache["sphere"] = sphereGpu;
		m_meshCache["sphere.obj"] = sphereGpu;

		auto planeData = CreatePlaneMesh(1.0f, 1.0f);
		auto planeGpu = createGpuMesh(planeData);
		m_meshCache["Plane"] = planeGpu;
		m_meshCache["plane"] = planeGpu;
		m_meshCache["plane.obj"] = planeGpu;

		// Unit billboard quad for particles: XY [-0.5, 0.5] facing -Z
		MeshData quadData;
		quadData.vertices.push_back({ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		quadData.vertices.push_back({ {  0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		quadData.vertices.push_back({ {  0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		quadData.vertices.push_back({ { -0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		quadData.indices = { 0, 1, 2, 0, 2, 3 };
		m_billboardQuadMesh = createGpuMesh(quadData);

		resize(m_width, m_height);
	}

	GpuMesh SceneRenderer::createGpuMesh(const MeshData& meshData)
	{
		GpuMesh mesh;
		if (meshData.vertices.empty() || meshData.indices.empty())
		{
			return mesh;
		}

		RHI::GpuBufferDesc vbDesc{};
		vbDesc.size = meshData.vertices.size() * sizeof(VertexMesh);
		vbDesc.usage = RHI::BufferUsage::Immutable;
		vbDesc.bindFlags = RHI::BufferBindFlags::VertexBuffer;
		auto vbRes = m_device.createBuffer(vbDesc, meshData.vertices.data());
		if (vbRes.isOk()) mesh.vertexBuffer = *vbRes;

		RHI::GpuBufferDesc ibDesc{};
		ibDesc.size = meshData.indices.size() * sizeof(uint32_t);
		ibDesc.usage = RHI::BufferUsage::Immutable;
		ibDesc.bindFlags = RHI::BufferBindFlags::IndexBuffer;
		auto ibRes = m_device.createBuffer(ibDesc, meshData.indices.data());
		if (ibRes.isOk()) mesh.indexBuffer = *ibRes;

		mesh.indexCount = static_cast<uint32_t>(meshData.indices.size());
		return mesh;
	}

	GpuMesh SceneRenderer::getOrCreateMesh(const std::string& meshPath)
	{
		auto it = m_meshCache.find(meshPath);
		if (it != m_meshCache.end())
		{
			return it->second;
		}

		MeshData meshData = LoadObjMesh(meshPath);
		GpuMesh gpu = createGpuMesh(meshData);
		m_meshCache[meshPath] = gpu;
		return gpu;
	}

	std::shared_ptr<RHI::IGpuTexture> SceneRenderer::getOrCreateTexture(const std::string& texturePath, bool sRGB)
	{
		if (texturePath.empty())
		{
			return nullptr;
		}

		std::string cacheKey = texturePath + (sRGB ? "#srgb" : "#linear");
		auto it = m_textureCache.find(cacheKey);
		if (it != m_textureCache.end())
		{
			return it->second;
		}

		auto binRes = Core::AssetPathResolver::readBinary(texturePath);
		if (!binRes.isOk() || binRes.value().empty())
		{
			DF_LOG_WARN("Renderer", "Could not load texture file: {}", texturePath);
			return nullptr;
		}

		const auto& fileBytes = binRes.value();

		DirectX::TexMetadata metadata{};
		DirectX::ScratchImage scratchImage;
		HRESULT hr = E_FAIL;

		// Check file extension
		std::string ext;
		auto dotPos = texturePath.find_last_of('.');
		if (dotPos != std::string::npos)
		{
			ext = texturePath.substr(dotPos);
			std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		}

		if (ext == ".dds")
		{
			hr = DirectX::LoadFromDDSMemory(fileBytes.data(), fileBytes.size(), DirectX::DDS_FLAGS_NONE, &metadata, scratchImage);
		}
		else if (ext == ".tga")
		{
			hr = DirectX::LoadFromTGAMemory(fileBytes.data(), fileBytes.size(), &metadata, scratchImage);
		}
		else
		{
			hr = DirectX::LoadFromWICMemory(fileBytes.data(), fileBytes.size(), DirectX::WIC_FLAGS_NONE, &metadata, scratchImage);
		}

		if (FAILED(hr))
		{
			DF_LOG_WARN("Renderer", "DirectXTex failed to parse texture {}: HRESULT 0x{:08X}", texturePath, static_cast<uint32_t>(hr));
			return nullptr;
		}

		DXGI_FORMAT targetFormat = sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM;

		// Ensure 32-bit RGBA in target format
		const DirectX::Image* baseImage = scratchImage.GetImage(0, 0, 0);
		DirectX::ScratchImage convertedImage;
		if (metadata.format != targetFormat)
		{
			hr = DirectX::Convert(
				*scratchImage.GetImage(0, 0, 0),
				targetFormat,
				DirectX::TEX_FILTER_DEFAULT,
				DirectX::TEX_THRESHOLD_DEFAULT,
				convertedImage
			);
			if (SUCCEEDED(hr))
			{
				baseImage = convertedImage.GetImage(0, 0, 0);
				metadata.format = targetFormat;
			}
		}

		if (!baseImage)
		{
			return nullptr;
		}

		// Generate full mipmap chain for textures
		DirectX::ScratchImage mipChain;
		if (metadata.mipLevels <= 1 && metadata.width > 1 && metadata.height > 1)
		{
			hr = DirectX::GenerateMipMaps(*baseImage, DirectX::TEX_FILTER_DEFAULT, 0, mipChain);
			if (SUCCEEDED(hr))
			{
				metadata = mipChain.GetMetadata();
			}
		}

		const DirectX::ScratchImage& finalScratch = (mipChain.GetImageCount() > 0) ? mipChain :
			(convertedImage.GetImageCount() > 0 ? convertedImage : scratchImage);

		std::vector<RHI::SubresourceData> subresources(metadata.mipLevels);
		for (size_t i = 0; i < metadata.mipLevels; ++i)
		{
			const DirectX::Image* img = finalScratch.GetImage(i, 0, 0);
			if (img)
			{
				subresources[i].data = img->pixels;
				subresources[i].rowPitch = static_cast<uint32_t>(img->rowPitch);
				subresources[i].slicePitch = static_cast<uint32_t>(img->slicePitch);
			}
		}

		RHI::GpuTextureDesc texDesc{};
		texDesc.width = static_cast<uint32_t>(metadata.width);
		texDesc.height = static_cast<uint32_t>(metadata.height);
		texDesc.mipLevels = static_cast<uint32_t>(metadata.mipLevels);
		texDesc.arraySize = 1;
		texDesc.format = sRGB ? RHI::PixelFormat::R8G8B8A8_UNORM_SRGB : RHI::PixelFormat::R8G8B8A8_UNORM;
		texDesc.usage = RHI::BufferUsage::Default;
		texDesc.bindFlags = RHI::BufferBindFlags::ShaderResource;

		auto texRes = m_device.createTextureSubresources(texDesc, subresources);
		if (texRes.isOk())
		{
			m_textureCache[cacheKey] = *texRes;
			DF_LOG_INFO("Renderer", "Loaded texture '{}' ({}x{}, {} mips, {})",
				texturePath, texDesc.width, texDesc.height, texDesc.mipLevels, sRGB ? "sRGB" : "Linear");
			return *texRes;
		}

		DF_LOG_WARN("Renderer", "Failed to create GPU texture for '{}': {}", texturePath, texRes.error());
		return nullptr;
	}

	std::shared_ptr<Material> SceneRenderer::getOrCreateMaterial(const std::string& materialPath)
	{
		if (materialPath.empty())
		{
			static auto s_defaultMat = std::make_shared<Material>("DefaultMaterial");
			return s_defaultMat;
		}

		auto it = m_materialCache.find(materialPath);
		if (it != m_materialCache.end())
		{
			return it->second;
		}

		auto mat = std::make_shared<Material>(materialPath);
		std::string jsonContent;
		auto textRes = Core::AssetPathResolver::readText(materialPath);
		if (textRes.isOk())
		{
			jsonContent = textRes.value();
		}

		if (!jsonContent.empty())
		{
			try
			{
				auto j = nlohmann::json::parse(jsonContent);
				if (j.contains("name")) mat->setName(j["name"].get<std::string>());
				if (j.contains("baseColor") && j["baseColor"].is_array() && j["baseColor"].size() >= 3)
				{
					float r = j["baseColor"][0].get<float>();
					float g = j["baseColor"][1].get<float>();
					float b = j["baseColor"][2].get<float>();
					float a = (j["baseColor"].size() >= 4) ? j["baseColor"][3].get<float>() : 1.0f;
					mat->setBaseColor(Math::Vector4(r, g, b, a));
				}
				if (j.contains("roughness")) mat->setRoughness(j["roughness"].get<float>());
				if (j.contains("metallic")) mat->setMetallic(j["metallic"].get<float>());
				if (j.contains("emissive") && j["emissive"].is_array() && j["emissive"].size() >= 3)
				{
					float r = j["emissive"][0].get<float>();
					float g = j["emissive"][1].get<float>();
					float b = j["emissive"][2].get<float>();
					float a = (j["emissive"].size() >= 4) ? j["emissive"][3].get<float>() : 0.0f;
					mat->setEmissive(Math::Vector4(r, g, b, a));
				}
				if (j.contains("tiling") && j["tiling"].is_array() && j["tiling"].size() >= 2)
				{
					mat->setTilingOffset(Math::Vector2(j["tiling"][0].get<float>(), j["tiling"][1].get<float>()));
				}

				// Texture Maps
				if (j.contains("albedoTexture") && j["albedoTexture"].is_string() && !j["albedoTexture"].get<std::string>().empty())
				{
					mat->setAlbedoTexture(getOrCreateTexture(j["albedoTexture"].get<std::string>(), true)); // sRGB = true
				}
				if (j.contains("normalTexture") && j["normalTexture"].is_string() && !j["normalTexture"].get<std::string>().empty())
				{
					mat->setNormalTexture(getOrCreateTexture(j["normalTexture"].get<std::string>(), false)); // Linear
				}
				if (j.contains("metallicRoughnessTexture") && j["metallicRoughnessTexture"].is_string() && !j["metallicRoughnessTexture"].get<std::string>().empty())
				{
					mat->setMetallicRoughnessTexture(getOrCreateTexture(j["metallicRoughnessTexture"].get<std::string>(), false)); // Linear
				}
				if (j.contains("emissiveTexture") && j["emissiveTexture"].is_string() && !j["emissiveTexture"].get<std::string>().empty())
				{
					mat->setEmissiveTexture(getOrCreateTexture(j["emissiveTexture"].get<std::string>(), true)); // sRGB = true
				}
				if (j.contains("occlusionTexture") && j["occlusionTexture"].is_string() && !j["occlusionTexture"].get<std::string>().empty())
				{
					mat->setOcclusionTexture(getOrCreateTexture(j["occlusionTexture"].get<std::string>(), false)); // Linear
				}

				// Render states
				if (j.contains("blendMode") && j["blendMode"].is_number_integer())
				{
					mat->setBlendMode(static_cast<BlendMode>(j["blendMode"].get<uint8_t>()));
				}
				if (j.contains("depthMode") && j["depthMode"].is_number_integer())
				{
					mat->setDepthMode(static_cast<DepthMode>(j["depthMode"].get<uint8_t>()));
				}
				if (j.contains("cullMode") && j["cullMode"].is_number_integer())
				{
					mat->setCullMode(static_cast<RHI::CullMode>(j["cullMode"].get<uint8_t>()));
				}
			}
			catch (const std::exception& e)
			{
				DF_LOG_WARN("Renderer", "Failed to parse material {}: {}", materialPath, e.what());
			}
		}

		m_materialCache[materialPath] = mat;
		return mat;
	}

	void SceneRenderer::generateSSAOKernel()
	{
		for (uint32_t i = 0; i < 16; ++i)
		{
			float u = static_cast<float>(i + 1) / 16.0f;
			float v = static_cast<float>((i * 7) % 16) / 16.0f;
			float phi = v * 2.0f * 3.14159265f;
			float cosTheta = 1.0f - u * 0.9f;
			float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));

			Math::Vector3 sample(
				sinTheta * std::cos(phi),
				sinTheta * std::sin(phi),
				std::max(cosTheta, 0.1f)
			);
			sample.Normalize();

			float scale = static_cast<float>(i) / 16.0f;
			scale = 0.1f + (scale * scale) * (1.0f - 0.1f);
			sample *= scale;

			m_ssaoKernel[i] = Math::Vector4(sample.x, sample.y, sample.z, 0.0f);
		}
	}

	void SceneRenderer::resize(uint32_t width, uint32_t height)
	{
		if (width == 0 || height == 0) return;
		m_width = width;
		m_height = height;

		// 1. HDR Render Target (R16G16B16A16_FLOAT)
		RHI::GpuTextureDesc hdrDesc{};
		hdrDesc.width = width;
		hdrDesc.height = height;
		hdrDesc.format = RHI::PixelFormat::R16G16B16A16_FLOAT;
		hdrDesc.bindFlags = RHI::BufferBindFlags::RenderTarget | RHI::BufferBindFlags::ShaderResource;
		auto hdrRes = m_device.createTexture(hdrDesc);
		if (hdrRes.isOk()) m_hdrTexture = *hdrRes;

		// 2. Depth Stencil Target (D32_FLOAT Reversed-Z / R32_TYPELESS)
		RHI::GpuTextureDesc depthDesc{};
		depthDesc.width = width;
		depthDesc.height = height;
		depthDesc.format = RHI::PixelFormat::D32_FLOAT;
		depthDesc.bindFlags = RHI::BufferBindFlags::DepthStencil | RHI::BufferBindFlags::ShaderResource;
		auto depthRes = m_device.createTexture(depthDesc);
		if (depthRes.isOk()) m_depthTexture = *depthRes;

		// 3. Entity-ID Target (R32_UINT)
		RHI::GpuTextureDesc idDesc{};
		idDesc.width = width;
		idDesc.height = height;
		idDesc.format = RHI::PixelFormat::R32_UINT;
		idDesc.bindFlags = RHI::BufferBindFlags::RenderTarget | RHI::BufferBindFlags::ShaderResource;
		auto idRes = m_device.createTexture(idDesc);
		if (idRes.isOk()) m_entityIdTexture = *idRes;

		// 4. Final LDR Target (R8G8B8A8_UNORM)
		RHI::GpuTextureDesc ldrDesc{};
		ldrDesc.width = width;
		ldrDesc.height = height;
		ldrDesc.format = RHI::PixelFormat::R8G8B8A8_UNORM;
		ldrDesc.bindFlags = RHI::BufferBindFlags::RenderTarget | RHI::BufferBindFlags::ShaderResource;
		auto ldrRes = m_device.createTexture(ldrDesc);
		if (ldrRes.isOk()) m_finalLdrTexture = *ldrRes;

		// 5. Intermediate LDR Target for Post-Processing & FXAA
		RHI::GpuTextureDesc ldrIntermDesc = ldrDesc;
		auto ldrIntermRes = m_device.createTexture(ldrIntermDesc);
		if (ldrIntermRes.isOk()) m_ldrIntermediateTexture = *ldrIntermRes;

		// 6. SSAO Half-Resolution Targets (R8_UNORM)
		uint32_t halfW = std::max((width + 1) / 2, 1u);
		uint32_t halfH = std::max((height + 1) / 2, 1u);

		RHI::GpuTextureDesc ssaoDesc{};
		ssaoDesc.width = halfW;
		ssaoDesc.height = halfH;
		ssaoDesc.format = RHI::PixelFormat::R8_UNORM;
		ssaoDesc.bindFlags = RHI::BufferBindFlags::RenderTarget | RHI::BufferBindFlags::ShaderResource;

		auto ssaoRawRes = m_device.createTexture(ssaoDesc);
		if (ssaoRawRes.isOk()) m_ssaoRawTexture = *ssaoRawRes;

		auto ssaoBlurRes = m_device.createTexture(ssaoDesc);
		if (ssaoBlurRes.isOk()) m_ssaoBlurredTexture = *ssaoBlurRes;

		// 7. Bloom Pyramid Targets (5 levels down and up, R16G16B16A16_FLOAT)
		uint32_t bw = halfW;
		uint32_t bh = halfH;
		for (uint32_t i = 0; i < 5; ++i)
		{
			RHI::GpuTextureDesc bloomDesc{};
			bloomDesc.width = std::max(bw, 1u);
			bloomDesc.height = std::max(bh, 1u);
			bloomDesc.format = RHI::PixelFormat::R16G16B16A16_FLOAT;
			bloomDesc.bindFlags = RHI::BufferBindFlags::RenderTarget | RHI::BufferBindFlags::ShaderResource;

			auto downRes = m_device.createTexture(bloomDesc);
			if (downRes.isOk()) m_bloomDown[i] = *downRes;

			auto upRes = m_device.createTexture(bloomDesc);
			if (upRes.isOk()) m_bloomUp[i] = *upRes;

			bw = std::max(bw / 2, 1u);
			bh = std::max(bh / 2, 1u);
		}
	}

	void SceneRenderer::render(const RenderScene& scene, const RenderView& view)
	{
		if (view.width != m_width || view.height != m_height)
		{
			resize(view.width, view.height);
		}

		m_drawCallCount = 0;
		m_batchCount = 0;
		m_instanceCount = 0;
		m_uploadRing.reset();

		if (!m_context) return;

		// 1. Update PerFrame Constant Buffer with real elapsed time and frame index
		float dt = 0.016666f;
		m_totalTime += dt;
		m_frameIndex++;

		CBPerFrame frameData{};
		frameData.time = m_totalTime;
		frameData.deltaTime = dt;
		frameData.frameIndex = m_frameIndex;
		m_context->updateSubresource(m_cbFrame, &frameData, sizeof(frameData));
		m_context->setConstantBufferVS(DF_SLOT_CB_PER_FRAME, m_cbFrame);
		m_context->setConstantBufferPS(DF_SLOT_CB_PER_FRAME, m_cbFrame);

		// 2. Update PerView Constant Buffer
		CBPerView viewData{};
		viewData.viewMatrix = scene.camera.viewMatrix;
		viewData.projMatrix = scene.camera.projMatrix;
		viewData.viewProjMatrix = scene.camera.viewProjMatrix;
		viewData.invViewMatrix = scene.camera.invViewMatrix;
		viewData.invProjMatrix = scene.camera.invProjMatrix;
		viewData.invViewProjMatrix = scene.camera.invViewProjMatrix;
		viewData.cameraPosition = scene.camera.position;
		viewData.nearZ = scene.camera.nearZ;
		viewData.cameraForward = scene.camera.forward;
		viewData.farZ = scene.camera.farZ;
		viewData.viewportSize = Math::Vector2(static_cast<float>(view.width), static_cast<float>(view.height));
		viewData.invViewportSize = Math::Vector2(1.0f / view.width, 1.0f / view.height);
		m_context->updateSubresource(m_cbView, &viewData, sizeof(viewData));
		m_context->setConstantBufferVS(DF_SLOT_CB_PER_VIEW, m_cbView);
		m_context->setConstantBufferPS(DF_SLOT_CB_PER_VIEW, m_cbView);

		// 3. Update PerPass Constant Buffer
		CBPerPass passData{};
		passData.ambientLightColor = Math::Vector4(
			scene.environment.ambientColor.x,
			scene.environment.ambientColor.y,
			scene.environment.ambientColor.z,
			scene.environment.ambientIntensity
		);
		passData.lightCount = static_cast<uint32_t>(std::min<size_t>(scene.lights.size(), 64));
		m_context->updateSubresource(m_cbPass, &passData, sizeof(passData));
		m_context->setConstantBufferPS(DF_SLOT_CB_PER_PASS, m_cbPass);

		// 4. Update Lights Buffer
		if (!scene.lights.empty() && m_bufLights)
		{
			std::array<LightData, 64> lightArray{};
			for (uint32_t i = 0; i < passData.lightCount; ++i)
			{
				const auto& l = scene.lights[i];
				LightData ld{};
				ld.positionType = Math::Vector4(l.position.x, l.position.y, l.position.z, static_cast<float>(l.type));
				ld.directionRange = Math::Vector4(l.direction.x, l.direction.y, l.direction.z, l.range);
				ld.colorIntensity = Math::Vector4(l.color.x, l.color.y, l.color.z, l.intensity);
				ld.spotAnglesPad = Math::Vector4(std::cos(l.innerConeAngle), std::cos(l.outerConeAngle), 0.0f, 0.0f);
				lightArray[i] = ld;
			}
			m_context->updateSubresource(m_bufLights, lightArray.data(), sizeof(LightData) * 64);
		}

		// 5. Execute Passes in Strict Order
		executePasses(scene, view);
	}

	void SceneRenderer::executePasses(const RenderScene& scene, const RenderView& view)
	{
		RHI::Viewport vp{};
		vp.x = 0.0f;
		vp.y = 0.0f;
		vp.width = static_cast<float>(m_width);
		vp.height = static_cast<float>(m_height);
		vp.minDepth = 0.0f;
		vp.maxDepth = 1.0f;
		m_context->setViewport(vp);

		// Pass 1: Depth Prepass (Reversed-Z, fast depth-only)
		passDepthPrepass(scene);

		// Pass 2: Screen-Space Ambient Occlusion (half-res)
		passSSAO(scene, view);

		// Pass 3: Forward Opaque PBR Shading (HDR + Reversed-Z + Entity ID)
		passOpaque(scene);

		// Pass 4: Procedural Skybox Dome (at depth == 0.0)
		passSkybox(scene);

		// Pass 5: Transparent Meshes (back-to-front sorting + alpha/additive blending)
		passTransparent(scene);

		// Pass 6: Bloom (13-tap prefilter + 5-level down/upsample pyramid)
		passBloom(scene);

		// Pass 7: Post-Process & Tonemapping (ACES/sRGB + single-pass FXAA)
		passPostProcess(scene, view);
	}

	void SceneRenderer::passDepthPrepass(const RenderScene& scene)
	{
		if (!m_depthTexture) return;

		// Reversed-Z: clear depth to 0.0f
		m_context->clearDepthStencil(m_depthTexture, 0.0f, 0);

		// Bind depth-only render target
		m_context->setRenderTargets({}, m_depthTexture);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		if (m_inputLayout) m_context->setInputLayout(m_inputLayout);
		if (m_vsStandard) m_context->setVertexShader(m_vsStandard);
		m_context->setPixelShader(nullptr); // Hardware fast depth-only
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		if (m_dssDepthPrepass) m_context->setDepthStencilState(m_dssDepthPrepass, 0);
		m_context->setBlendState(nullptr);

		for (const auto& mesh : scene.meshes)
		{
			if (mesh.layer != RenderLayer::Opaque) continue;

			const auto gpuMesh = getOrCreateMesh(mesh.meshPath);
			if (!gpuMesh.vertexBuffer || !gpuMesh.indexBuffer || gpuMesh.indexCount == 0) continue;

			m_context->setVertexBuffer(0, gpuMesh.vertexBuffer, sizeof(VertexMesh), 0);
			m_context->setIndexBuffer(gpuMesh.indexBuffer, RHI::PixelFormat::R32_UINT, 0);

			CBPerObject obj{};
			obj.worldMatrix = mesh.world;
			obj.invWorldMatrix = mesh.world.Invert();
			obj.entityId = mesh.entityId;

			auto alloc = m_uploadRing.allocate(sizeof(CBPerObject), &obj);
			if (alloc.isValid())
			{
				m_context->setConstantBufferSubrangeVS(DF_SLOT_CB_PER_OBJECT, alloc.buffer, alloc.firstConstant16, alloc.numConstants16);
			}

			m_context->drawIndexed(gpuMesh.indexCount, 0, 0);
			m_drawCallCount++;
		}

		// Unbind DSV so depth can be read as SRV in SSAO
		m_context->setRenderTargets({}, nullptr);
	}

	void SceneRenderer::passSSAO(const RenderScene& scene, const RenderView& view)
	{
		bool ssaoEnabled = scene.environment.enableSSAO;
		if (view.overrides.overrideSSAO)
		{
			ssaoEnabled = view.overrides.enableSSAO;
		}
		if (!ssaoEnabled || !m_psSSAO || !m_psSSAOBlur || !m_depthTexture || !m_ssaoRawTexture || !m_ssaoBlurredTexture)
		{
			return;
		}

		uint32_t halfW = m_ssaoRawTexture->getDesc().width;
		uint32_t halfH = m_ssaoRawTexture->getDesc().height;

		RHI::Viewport halfVp{};
		halfVp.x = 0.0f;
		halfVp.y = 0.0f;
		halfVp.width = static_cast<float>(halfW);
		halfVp.height = static_cast<float>(halfH);
		halfVp.minDepth = 0.0f;
		halfVp.maxDepth = 1.0f;
		m_context->setViewport(halfVp);

		// Step A: Generate raw SSAO
		m_context->setRenderTargets({ m_ssaoRawTexture }, nullptr);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		m_context->setInputLayout(nullptr);
		m_context->setVertexShader(m_vsFullscreen);
		m_context->setPixelShader(m_psSSAO);
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		m_context->setDepthStencilState(nullptr, 0);
		m_context->setBlendState(nullptr);

		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, m_depthTexture);
		if (m_samplerPointClamp) m_context->setSamplerPS(DF_SLOT_SMP_POINT_CLAMP, m_samplerPointClamp);

		CBSSAO cbSSAO{};
		cbSSAO.radius = scene.environment.ssaoRadius > 0.001f ? scene.environment.ssaoRadius : 0.5f;
		cbSSAO.bias = scene.environment.ssaoBias > 0.0001f ? scene.environment.ssaoBias : 0.025f;
		cbSSAO.intensity = scene.environment.ssaoIntensity > 0.001f ? scene.environment.ssaoIntensity : 1.2f;
		cbSSAO.power = 1.5f;
		cbSSAO.sampleCount = 16;
		cbSSAO.invResolution = Math::Vector2(1.0f / halfW, 1.0f / halfH);
		for (size_t i = 0; i < 16; ++i) cbSSAO.kernel[i] = m_ssaoKernel[i];

		if (m_cbSSAO)
		{
			m_context->updateSubresource(m_cbSSAO, &cbSSAO, sizeof(cbSSAO));
			m_context->setConstantBufferPS(DF_SLOT_CB_POST_PROCESS, m_cbSSAO);
		}

		m_context->draw(3, 0);
		m_drawCallCount++;
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, nullptr);

		// Step B: Bilateral Blur
		m_context->setRenderTargets({ m_ssaoBlurredTexture }, nullptr);
		m_context->setPixelShader(m_psSSAOBlur);
		m_context->setShaderResourcePS(0, m_ssaoRawTexture);
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, m_depthTexture);
		if (m_samplerLinearClamp) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_CLAMP, m_samplerLinearClamp);

		struct { Math::Vector2 texelSize; Math::Vector2 pad; } cbBlur{
			Math::Vector2(1.0f / halfW, 1.0f / halfH),
			Math::Vector2(0.0f, 0.0f)
		};
		if (m_cbSSAOBlur)
		{
			m_context->updateSubresource(m_cbSSAOBlur, &cbBlur, sizeof(cbBlur));
			m_context->setConstantBufferPS(DF_SLOT_CB_POST_PROCESS, m_cbSSAOBlur);
		}

		m_context->draw(3, 0);
		m_drawCallCount++;

		m_context->setShaderResourcePS(0, nullptr);
		m_context->setShaderResourcePS(DF_SLOT_TEX_DEPTH, nullptr);
		m_context->setRenderTargets({}, nullptr);

		// Restore full viewport
		RHI::Viewport fullVp{};
		fullVp.x = 0.0f;
		fullVp.y = 0.0f;
		fullVp.width = static_cast<float>(m_width);
		fullVp.height = static_cast<float>(m_height);
		fullVp.minDepth = 0.0f;
		fullVp.maxDepth = 1.0f;
		m_context->setViewport(fullVp);
	}

	void SceneRenderer::passOpaque(const RenderScene& scene)
	{
		std::vector<std::shared_ptr<RHI::IGpuTexture>> rtvs = { m_hdrTexture, m_entityIdTexture };
		m_context->setRenderTargets(rtvs, m_depthTexture);

		// Clear HDR target and ID target using scene environment ambient lighting
		Color envClearColor(
			scene.environment.ambientColor.x * scene.environment.ambientIntensity * 0.35f,
			scene.environment.ambientColor.y * scene.environment.ambientIntensity * 0.38f,
			scene.environment.ambientColor.z * scene.environment.ambientIntensity * 0.44f,
			1.0f
		);
		m_context->clearRenderTarget(m_hdrTexture, envClearColor);
		m_context->clearRenderTarget(m_entityIdTexture, Color(0.0f, 0.0f, 0.0f, 0.0f));

		// Collect and sort Opaque and AlphaTest meshes
		m_cachedDrawItems.clear();
		m_cachedMeshPaths.clear();
		m_cachedMaterialPaths.clear();

		for (const auto& mesh : scene.meshes)
		{
			if (mesh.layer == RenderLayer::Opaque || mesh.layer == RenderLayer::AlphaTest)
			{
				const auto gpu = getOrCreateMesh(mesh.meshPath);
				const auto mat = mesh.material ? mesh.material : getOrCreateMaterial(mesh.materialPath);

				DrawItem item{};
				item.shaderVariantId = 0;
				item.materialId = mat ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(mat.get()) & 0xFFFF) : 0;
				item.meshId = gpu.vertexBuffer ? static_cast<uint32_t>(reinterpret_cast<uintptr_t>(gpu.vertexBuffer.get()) & 0xFFFF) : 0;
				item.indexCount = gpu.indexCount;
				item.startIndexLocation = 0;
				item.baseVertexLocation = 0;
				item.instanceData.world = mesh.world;
				item.instanceData.entityId = mesh.entityId;
				item.sortKey = DrawItem::makeSortKey(mesh.layer, item.shaderVariantId, item.materialId, item.meshId, mesh.distanceToCamera);

				m_cachedDrawItems.push_back(item);
				m_cachedMeshPaths.push_back(mesh.meshPath);
				m_cachedMaterialPaths.push_back(mesh.materialPath);
			}
		}

		if (m_cachedDrawItems.empty()) return;

		// Bind Pipeline State for Opaque pass
		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		if (m_inputLayout) m_context->setInputLayout(m_inputLayout);
		if (m_vsStandard) m_context->setVertexShader(m_vsStandard);
		if (m_psStandard) m_context->setPixelShader(m_psStandard);
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		if (m_dssOpaque) m_context->setDepthStencilState(m_dssOpaque, 0);
		m_context->setBlendState(nullptr);

		// Bind Samplers
		if (m_samplerLinearWrap) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_WRAP, m_samplerLinearWrap);
		if (m_samplerLinearClamp) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_CLAMP, m_samplerLinearClamp);
		if (m_samplerPointClamp) m_context->setSamplerPS(DF_SLOT_SMP_POINT_CLAMP, m_samplerPointClamp);

		// Bind Lights
		if (m_bufLights)
		{
			m_context->setShaderResourceBufferPS(DF_SLOT_BUF_LIGHTS, m_bufLights);
		}

		// Bind SSAO Texture (slot 11)
		bool ssaoActive = scene.environment.enableSSAO && m_ssaoBlurredTexture;
		auto ssaoTex = ssaoActive ? m_ssaoBlurredTexture : m_whiteTexture;
		if (ssaoTex)
		{
			m_context->setShaderResourcePS(DF_SLOT_TEX_SSAO, ssaoTex);
		}

		// Bind Default Textures
		if (m_whiteTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO, m_whiteTexture);
		if (m_flatNormalTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_NORMAL, m_flatNormalTexture);
		if (m_defaultMRTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_METALLIC_ROUGHNESS, m_defaultMRTexture);
		if (m_blackTexture) m_context->setShaderResourcePS(DF_SLOT_TEX_EMISSIVE, m_blackTexture);

		// Update default material constant buffer
		CBPerMaterial defaultMat{};
		defaultMat.baseColorFactor = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
		defaultMat.roughnessFactor = 0.5f;
		defaultMat.metallicFactor = 0.0f;
		defaultMat.tilingOffset = Math::Vector2(1.0f, 1.0f);
		defaultMat.emissiveFactor = Math::Vector4(0.0f, 0.0f, 0.0f, 0.0f);
		defaultMat.materialFlags = 0;
		if (m_cbMaterial)
		{
			m_context->updateSubresource(m_cbMaterial, &defaultMat, sizeof(defaultMat));
			m_context->setConstantBufferPS(DF_SLOT_CB_PER_MATERIAL, m_cbMaterial);
		}

		// Sort and batch
		std::vector<size_t> indices(m_cachedDrawItems.size());
		for (size_t i = 0; i < indices.size(); ++i) indices[i] = i;
		std::sort(indices.begin(), indices.end(), [this](size_t a, size_t b) {
			return m_cachedDrawItems[a] < m_cachedDrawItems[b];
		});

		std::vector<DrawItem> sortedItems;
		sortedItems.reserve(indices.size());
		std::vector<std::string> sortedPaths;
		sortedPaths.reserve(indices.size());
		std::vector<std::string> sortedMatPaths;
		sortedMatPaths.reserve(indices.size());
		for (size_t idx : indices)
		{
			sortedItems.push_back(m_cachedDrawItems[idx]);
			sortedPaths.push_back(m_cachedMeshPaths[idx]);
			sortedMatPaths.push_back(m_cachedMaterialPaths[idx]);
		}
		m_cachedDrawItems = std::move(sortedItems);
		m_cachedMeshPaths = std::move(sortedPaths);
		m_cachedMaterialPaths = std::move(sortedMatPaths);

		auto batches = BatchDrawItems(m_cachedDrawItems, m_cachedInstances);
		m_batchCount += static_cast<uint32_t>(batches.size());
		m_instanceCount += static_cast<uint32_t>(m_cachedInstances.size());

		for (const auto& batch : batches)
		{
			const std::string& meshPath = m_cachedMeshPaths[batch.firstInstanceIndex];
			const std::string& matPath = m_cachedMaterialPaths[batch.firstInstanceIndex];
			const auto gpuMesh = getOrCreateMesh(meshPath);
			if (!gpuMesh.vertexBuffer || !gpuMesh.indexBuffer || gpuMesh.indexCount == 0) continue;

			// Update material constant buffer and bind textures for this batch
			const auto mat = getOrCreateMaterial(matPath);
			if (mat)
			{
				if (m_cbMaterial)
				{
					m_context->updateSubresource(m_cbMaterial, &mat->getBufferData(), sizeof(CBPerMaterial));
					m_context->setConstantBufferPS(DF_SLOT_CB_PER_MATERIAL, m_cbMaterial);
				}

				auto albedoTex = mat->getAlbedoTexture() ? mat->getAlbedoTexture() : m_whiteTexture;
				auto normalTex = mat->getNormalTexture() ? mat->getNormalTexture() : m_flatNormalTexture;
				auto mrTex = mat->getMetallicRoughnessTexture() ? mat->getMetallicRoughnessTexture() : m_defaultMRTexture;
				auto emissiveTex = mat->getEmissiveTexture() ? mat->getEmissiveTexture() : m_blackTexture;

				if (albedoTex) m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO, albedoTex);
				if (normalTex) m_context->setShaderResourcePS(DF_SLOT_TEX_NORMAL, normalTex);
				if (mrTex) m_context->setShaderResourcePS(DF_SLOT_TEX_METALLIC_ROUGHNESS, mrTex);
				if (emissiveTex) m_context->setShaderResourcePS(DF_SLOT_TEX_EMISSIVE, emissiveTex);
			}

			m_context->setVertexBuffer(0, gpuMesh.vertexBuffer, sizeof(VertexMesh), 0);
			m_context->setIndexBuffer(gpuMesh.indexBuffer, RHI::PixelFormat::R32_UINT, 0);

			for (uint32_t i = 0; i < batch.instanceCount; ++i)
			{
				const auto& inst = m_cachedInstances[batch.firstInstanceIndex + i];

				CBPerObject obj{};
				obj.worldMatrix = inst.world;
				obj.invWorldMatrix = inst.world.Invert();
				obj.entityId = inst.entityId;

				auto alloc = m_uploadRing.allocate(sizeof(CBPerObject), &obj);
				if (alloc.isValid())
				{
					m_context->setConstantBufferSubrangeVS(DF_SLOT_CB_PER_OBJECT, alloc.buffer, alloc.firstConstant16, alloc.numConstants16);
				}

				m_context->drawIndexed(gpuMesh.indexCount, 0, 0);
				m_drawCallCount++;
			}
		}

		// Unbind SSAO texture from PS
		m_context->setShaderResourcePS(DF_SLOT_TEX_SSAO, nullptr);
	}

	void SceneRenderer::passSkybox(const RenderScene& scene)
	{
		if (!m_vsFullscreen || !m_psSky || !m_hdrTexture || !m_depthTexture) return;

		std::vector<std::shared_ptr<RHI::IGpuTexture>> rtvs = { m_hdrTexture, m_entityIdTexture };
		m_context->setRenderTargets(rtvs, m_depthTexture);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		m_context->setInputLayout(nullptr);
		m_context->setVertexShader(m_vsFullscreen);
		m_context->setPixelShader(m_psSky);
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		if (m_dssSky) m_context->setDepthStencilState(m_dssSky, 0);
		m_context->setBlendState(nullptr);

		CBEnvironment envData{};
		envData.skyZenith = scene.environment.skyColor;
		envData.skyHorizon = scene.environment.horizonColor;
		envData.groundColor = scene.environment.groundColor;
		envData.ambientColor = scene.environment.ambientColor;
		envData.skyIntensity = 1.0f;
		envData.ambientIntensity = scene.environment.ambientIntensity;

		// Find first directional light for sun disk
		bool foundDir = false;
		for (const auto& l : scene.lights)
		{
			if (l.type == 0) // Directional
			{
				envData.sunDirection = Math::Vector4(-l.direction.x, -l.direction.y, -l.direction.z, 1.0f);
				envData.sunColor = Math::Vector4(l.color.x, l.color.y, l.color.z, 0.9995f);
				foundDir = true;
				break;
			}
		}
		if (!foundDir)
		{
			envData.sunDirection = Math::Vector4(0.0f, 1.0f, 0.0f, 0.0f);
			envData.sunColor = Math::Vector4(1.0f, 1.0f, 1.0f, 0.9995f);
		}

		if (m_cbEnvironment)
		{
			m_context->updateSubresource(m_cbEnvironment, &envData, sizeof(envData));
			m_context->setConstantBufferPS(DF_SLOT_CB_POST_PROCESS, m_cbEnvironment);
		}

		m_context->draw(3, 0);
		m_drawCallCount++;
	}

	void SceneRenderer::passTransparent(const RenderScene& scene)
	{
		std::vector<RenderMeshInstance> transparentMeshes;
		for (const auto& mesh : scene.meshes)
		{
			const auto mat = mesh.material ? mesh.material : getOrCreateMaterial(mesh.materialPath);
			bool isTransparent = (mesh.layer == RenderLayer::Transparent) ||
				(mat && mat->getBlendMode() != BlendMode::Opaque);
			if (isTransparent)
			{
				transparentMeshes.push_back(mesh);
			}
		}

		if (transparentMeshes.empty()) return;

		// Sort back-to-front (farthest first)
		std::sort(transparentMeshes.begin(), transparentMeshes.end(), [](const RenderMeshInstance& a, const RenderMeshInstance& b) {
			return a.distanceToCamera > b.distanceToCamera;
		});

		std::vector<std::shared_ptr<RHI::IGpuTexture>> rtvs = { m_hdrTexture, m_entityIdTexture };
		m_context->setRenderTargets(rtvs, m_depthTexture);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		if (m_inputLayout) m_context->setInputLayout(m_inputLayout);
		if (m_vsStandard) m_context->setVertexShader(m_vsStandard);
		if (m_psStandard) m_context->setPixelShader(m_psStandard);
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		if (m_dssTransparent) m_context->setDepthStencilState(m_dssTransparent, 0);

		for (const auto& mesh : transparentMeshes)
		{
			const auto gpuMesh = getOrCreateMesh(mesh.meshPath);
			if (!gpuMesh.vertexBuffer || !gpuMesh.indexBuffer || gpuMesh.indexCount == 0) continue;

			const auto mat = mesh.material ? mesh.material : getOrCreateMaterial(mesh.materialPath);
			if (mat)
			{
				if (mat->getBlendMode() == BlendMode::Additive)
				{
					m_context->setBlendState(m_bsAdditive);
				}
				else
				{
					m_context->setBlendState(m_bsAlphaBlend);
				}

				if (m_cbMaterial)
				{
					m_context->updateSubresource(m_cbMaterial, &mat->getBufferData(), sizeof(CBPerMaterial));
					m_context->setConstantBufferPS(DF_SLOT_CB_PER_MATERIAL, m_cbMaterial);
				}

				auto albedoTex = mat->getAlbedoTexture() ? mat->getAlbedoTexture() : m_whiteTexture;
				auto normalTex = mat->getNormalTexture() ? mat->getNormalTexture() : m_flatNormalTexture;
				auto mrTex = mat->getMetallicRoughnessTexture() ? mat->getMetallicRoughnessTexture() : m_defaultMRTexture;
				auto emissiveTex = mat->getEmissiveTexture() ? mat->getEmissiveTexture() : m_blackTexture;

				if (albedoTex) m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO, albedoTex);
				if (normalTex) m_context->setShaderResourcePS(DF_SLOT_TEX_NORMAL, normalTex);
				if (mrTex) m_context->setShaderResourcePS(DF_SLOT_TEX_METALLIC_ROUGHNESS, mrTex);
				if (emissiveTex) m_context->setShaderResourcePS(DF_SLOT_TEX_EMISSIVE, emissiveTex);
			}
			else
			{
				m_context->setBlendState(m_bsAlphaBlend);
			}

			m_context->setVertexBuffer(0, gpuMesh.vertexBuffer, sizeof(VertexMesh), 0);
			m_context->setIndexBuffer(gpuMesh.indexBuffer, RHI::PixelFormat::R32_UINT, 0);

			CBPerObject obj{};
			obj.worldMatrix = mesh.world;
			obj.invWorldMatrix = mesh.world.Invert();
			obj.entityId = mesh.entityId;

			auto alloc = m_uploadRing.allocate(sizeof(CBPerObject), &obj);
			if (alloc.isValid())
			{
				m_context->setConstantBufferSubrangeVS(DF_SLOT_CB_PER_OBJECT, alloc.buffer, alloc.firstConstant16, alloc.numConstants16);
			}

			m_context->drawIndexed(gpuMesh.indexCount, 0, 0);
			m_drawCallCount++;
		}

		// Render Camera-Facing Billboard Particles
		if (!scene.particles.empty() && m_billboardQuadMesh.vertexBuffer)
		{
			Math::Vector3 camR(scene.camera.invViewMatrix._11, scene.camera.invViewMatrix._12, scene.camera.invViewMatrix._13);
			Math::Vector3 camU(scene.camera.invViewMatrix._21, scene.camera.invViewMatrix._22, scene.camera.invViewMatrix._23);
			Math::Vector3 camF(scene.camera.invViewMatrix._31, scene.camera.invViewMatrix._32, scene.camera.invViewMatrix._33);
			if (camR.LengthSquared() > 1e-6f) camR.Normalize();
			if (camU.LengthSquared() > 1e-6f) camU.Normalize();
			if (camF.LengthSquared() > 1e-6f) camF.Normalize();

			m_context->setVertexBuffer(0, m_billboardQuadMesh.vertexBuffer, sizeof(VertexMesh), 0);
			m_context->setIndexBuffer(m_billboardQuadMesh.indexBuffer, RHI::PixelFormat::R32_UINT, 0);

			for (const auto& p : scene.particles)
			{
				if (p.blendMode == BlendMode::Additive)
				{
					m_context->setBlendState(m_bsAdditive);
				}
				else
				{
					m_context->setBlendState(m_bsAlphaBlend);
				}

				auto tex = p.texturePath.empty() ? m_whiteTexture : getOrCreateTexture(p.texturePath, true);
				m_context->setShaderResourcePS(DF_SLOT_TEX_ALBEDO, tex ? tex : m_whiteTexture);

				CBPerMaterial pMat{};
				pMat.baseColorFactor = p.color;
				pMat.roughnessFactor = 1.0f;
				pMat.metallicFactor = 0.0f;
				pMat.tilingOffset = Math::Vector2(1.0f, 1.0f);
				pMat.emissiveFactor = Math::Vector4(p.color.x * 2.0f, p.color.y * 2.0f, p.color.z * 2.0f, 0.0f);
				if (m_cbMaterial)
				{
					m_context->updateSubresource(m_cbMaterial, &pMat, sizeof(pMat));
					m_context->setConstantBufferPS(DF_SLOT_CB_PER_MATERIAL, m_cbMaterial);
				}

				Math::Matrix world = Math::Matrix::Identity;
				world._11 = camR.x * p.size; world._12 = camR.y * p.size; world._13 = camR.z * p.size;
				world._21 = camU.x * p.size; world._22 = camU.y * p.size; world._23 = camU.z * p.size;
				world._31 = camF.x * p.size; world._32 = camF.y * p.size; world._33 = camF.z * p.size;
				world._41 = p.position.x;    world._42 = p.position.y;    world._43 = p.position.z;

				CBPerObject obj{};
				obj.worldMatrix = world;
				obj.invWorldMatrix = world.Invert();
				obj.entityId = 0;

				auto alloc = m_uploadRing.allocate(sizeof(CBPerObject), &obj);
				if (alloc.isValid())
				{
					m_context->setConstantBufferSubrangeVS(DF_SLOT_CB_PER_OBJECT, alloc.buffer, alloc.firstConstant16, alloc.numConstants16);
				}

				m_context->drawIndexed(m_billboardQuadMesh.indexCount, 0, 0);
				m_drawCallCount++;
			}
		}

		m_context->setBlendState(nullptr);
		m_context->setRenderTargets({}, nullptr);
	}

	void SceneRenderer::passBloom(const RenderScene& scene)
	{
		if (!scene.environment.enableBloom || !m_psBloomPrefilter || !m_psBloomDownsample || !m_psBloomUpsample || !m_bloomDown[0])
		{
			return;
		}

		m_context->setRenderTargets({}, nullptr);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		m_context->setInputLayout(nullptr);
		m_context->setVertexShader(m_vsFullscreen);
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		m_context->setDepthStencilState(nullptr, 0);
		m_context->setBlendState(nullptr);
		if (m_samplerLinearClamp) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_CLAMP, m_samplerLinearClamp);

		CBBloom cbBloom{};
		cbBloom.threshold = scene.environment.bloomThreshold > 0.001f ? scene.environment.bloomThreshold : 1.0f;
		cbBloom.knee = 0.5f;
		cbBloom.scatter = 0.7f;
		cbBloom.intensity = scene.environment.bloomIntensity > 0.0001f ? scene.environment.bloomIntensity : 0.04f;
		cbBloom.tint = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);

		// Step A: Prefilter & Downsample Mip 0 from HDR Texture
		{
			uint32_t w = m_bloomDown[0]->getDesc().width;
			uint32_t h = m_bloomDown[0]->getDesc().height;
			RHI::Viewport vp{ 0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f };
			m_context->setViewport(vp);

			m_context->setRenderTargets({ m_bloomDown[0] }, nullptr);
			m_context->setPixelShader(m_psBloomPrefilter);
			m_context->setShaderResourcePS(0, m_hdrTexture);

			cbBloom.texelSize = Math::Vector2(1.0f / w, 1.0f / h);
			cbBloom.mipLevel = 0.0f;
			if (m_cbBloom)
			{
				m_context->updateSubresource(m_cbBloom, &cbBloom, sizeof(cbBloom));
				m_context->setConstantBufferPS(DF_SLOT_CB_POST_PROCESS, m_cbBloom);
			}

			m_context->draw(3, 0);
			m_drawCallCount++;
			m_context->setShaderResourcePS(0, nullptr);
		}

		// Step B: Downsample Pyramid Mips 1..4
		for (uint32_t i = 1; i < 5; ++i)
		{
			uint32_t w = m_bloomDown[i]->getDesc().width;
			uint32_t h = m_bloomDown[i]->getDesc().height;
			RHI::Viewport vp{ 0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f };
			m_context->setViewport(vp);

			m_context->setRenderTargets({ m_bloomDown[i] }, nullptr);
			m_context->setPixelShader(m_psBloomDownsample);
			m_context->setShaderResourcePS(0, m_bloomDown[i - 1]);

			cbBloom.texelSize = Math::Vector2(1.0f / w, 1.0f / h);
			cbBloom.mipLevel = static_cast<float>(i);
			if (m_cbBloom)
			{
				m_context->updateSubresource(m_cbBloom, &cbBloom, sizeof(cbBloom));
				m_context->setConstantBufferPS(DF_SLOT_CB_POST_PROCESS, m_cbBloom);
			}

			m_context->draw(3, 0);
			m_drawCallCount++;
			m_context->setShaderResourcePS(0, nullptr);
		}

		// Step C: Upsample Pyramid Mips 3..0
		for (int32_t i = 3; i >= 0; --i)
		{
			uint32_t w = m_bloomUp[i]->getDesc().width;
			uint32_t h = m_bloomUp[i]->getDesc().height;
			RHI::Viewport vp{ 0.0f, 0.0f, static_cast<float>(w), static_cast<float>(h), 0.0f, 1.0f };
			m_context->setViewport(vp);

			m_context->setRenderTargets({ m_bloomUp[i] }, nullptr);
			m_context->setPixelShader(m_psBloomUpsample);

			auto lowerMip = (i == 3) ? m_bloomDown[4] : m_bloomUp[i + 1];
			auto currentMip = m_bloomDown[i];
			m_context->setShaderResourcePS(0, lowerMip);
			m_context->setShaderResourcePS(1, currentMip);

			cbBloom.texelSize = Math::Vector2(1.0f / w, 1.0f / h);
			cbBloom.mipLevel = static_cast<float>(i);
			if (m_cbBloom)
			{
				m_context->updateSubresource(m_cbBloom, &cbBloom, sizeof(cbBloom));
				m_context->setConstantBufferPS(DF_SLOT_CB_POST_PROCESS, m_cbBloom);
			}

			m_context->draw(3, 0);
			m_drawCallCount++;
			m_context->setShaderResourcePS(0, nullptr);
			m_context->setShaderResourcePS(1, nullptr);
		}

		m_context->setRenderTargets({}, nullptr);

		// Restore full viewport
		RHI::Viewport fullVp{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
		m_context->setViewport(fullVp);
	}

	void SceneRenderer::passTonemap(const RenderScene& scene, const RenderView& view, const std::shared_ptr<RHI::IGpuTexture>& destination)
	{
		if (!destination) return;

		std::vector<std::shared_ptr<RHI::IGpuTexture>> rtvs = { destination };
		m_context->setRenderTargets(rtvs, nullptr);

		RHI::Viewport vp{ 0.0f, 0.0f, static_cast<float>(view.width), static_cast<float>(view.height), 0.0f, 1.0f };
		m_context->setViewport(vp);

		auto psToUse = m_psTonemap ? m_psTonemap : m_psPostProcess;
		if (m_vsFullscreen && psToUse)
		{
			m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
			m_context->setInputLayout(nullptr);
			m_context->setVertexShader(m_vsFullscreen);
			m_context->setPixelShader(psToUse);
			if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
			m_context->setDepthStencilState(nullptr, 0);
			m_context->setBlendState(nullptr);

			// Bind HDR Texture as SRV on slot 0
			m_context->setShaderResourcePS(0, m_hdrTexture);

			// Bind Bloom Texture on slot 10
			auto bloomTex = (scene.environment.enableBloom && m_bloomUp[0]) ? m_bloomUp[0] : m_blackTexture;
			if (bloomTex) m_context->setShaderResourcePS(DF_SLOT_TEX_BLOOM, bloomTex);

			// Bind SSAO Texture on slot 11
			auto ssaoTex = (scene.environment.enableSSAO && m_ssaoBlurredTexture) ? m_ssaoBlurredTexture : m_whiteTexture;
			if (ssaoTex) m_context->setShaderResourcePS(DF_SLOT_TEX_SSAO, ssaoTex);

			if (m_samplerLinearClamp) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_CLAMP, m_samplerLinearClamp);
			if (m_samplerPointClamp) m_context->setSamplerPS(DF_SLOT_SMP_POINT_CLAMP, m_samplerPointClamp);

			CBPostProcess cbPP{};
			cbPP.exposure = scene.environment.exposure > 0.001f ? scene.environment.exposure : 1.0f;
			cbPP.tonemapper = static_cast<uint32_t>(scene.environment.tonemapper);
			cbPP.contrast = 1.0f;
			cbPP.saturation = 1.0f;
			cbPP.vignetteIntensity = 0.0f;
			cbPP.vignetteSmoothness = 0.4f;
			cbPP.bloomIntensity = scene.environment.enableBloom ? scene.environment.bloomIntensity : 0.0f;
			cbPP.debugView = 0; // 0 = Lit
			cbPP.colorFilter = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			cbPP.bloomTint = Math::Vector4(1.0f, 1.0f, 1.0f, 1.0f);
			cbPP.texelSize = Math::Vector2(
				1.0f / (view.width > 0 ? static_cast<float>(view.width) : 1.0f),
				1.0f / (view.height > 0 ? static_cast<float>(view.height) : 1.0f)
			);
			cbPP.gamma = scene.environment.gamma > 0.1f ? scene.environment.gamma : 2.2f;

			if (m_cbPostProcess)
			{
				m_context->updateSubresource(m_cbPostProcess, &cbPP, sizeof(cbPP));
				m_context->setConstantBufferPS(0, m_cbPostProcess);
			}

			m_context->draw(3, 0);
			m_drawCallCount++;

			m_context->setShaderResourcePS(0, nullptr);
			m_context->setShaderResourcePS(DF_SLOT_TEX_BLOOM, nullptr);
			m_context->setShaderResourcePS(DF_SLOT_TEX_SSAO, nullptr);
		}
		else
		{
			m_context->clearRenderTarget(destination, Color(0.05f, 0.05f, 0.08f, 1.0f));
		}

		m_context->setRenderTargets({}, nullptr);
	}

	void SceneRenderer::passFXAA(const std::shared_ptr<RHI::IGpuTexture>& source, const std::shared_ptr<RHI::IGpuTexture>& destination)
	{
		if (!source || !destination || !m_vsFullscreen || !m_psFXAA) return;

		std::vector<std::shared_ptr<RHI::IGpuTexture>> rtvs = { destination };
		m_context->setRenderTargets(rtvs, nullptr);

		RHI::Viewport vp{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
		m_context->setViewport(vp);

		m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
		m_context->setInputLayout(nullptr);
		m_context->setVertexShader(m_vsFullscreen);
		m_context->setPixelShader(m_psFXAA);
		if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
		m_context->setDepthStencilState(nullptr, 0);
		m_context->setBlendState(nullptr);

		m_context->setShaderResourcePS(0, source);
		if (m_samplerLinearClamp) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_CLAMP, m_samplerLinearClamp);

		struct { Math::Vector2 texelSize; Math::Vector2 pad; } cbFXAA{
			Math::Vector2(1.0f / m_width, 1.0f / m_height),
			Math::Vector2(0.0f, 0.0f)
		};
		if (m_cbFXAA)
		{
			m_context->updateSubresource(m_cbFXAA, &cbFXAA, sizeof(cbFXAA));
			m_context->setConstantBufferPS(0, m_cbFXAA);
		}

		m_context->draw(3, 0);
		m_drawCallCount++;

		m_context->setShaderResourcePS(0, nullptr);
		m_context->setRenderTargets({}, nullptr);
	}

	void SceneRenderer::passPostProcess(const RenderScene& scene, const RenderView& view)
	{
		std::shared_ptr<RHI::IGpuTexture> finalTarget = m_finalLdrTexture;
		if (view.targetTexture)
		{
			finalTarget = view.targetTexture;
		}
		else if (view.swapChain)
		{
			finalTarget = view.swapChain->getBackBufferTexture();
		}

		if (!finalTarget) return;

		m_context->clearRenderTarget(finalTarget, Color(0.05f, 0.05f, 0.08f, 1.0f));

		bool fxaaEnabled = scene.environment.enableFXAA;
		if (view.overrides.overrideFXAA)
		{
			fxaaEnabled = view.overrides.enableFXAA;
		}
		if (!m_psFXAA || !m_ldrIntermediateTexture)
		{
			fxaaEnabled = false;
		}

		auto tonemapDest = fxaaEnabled ? m_ldrIntermediateTexture : finalTarget;
		passTonemap(scene, view, tonemapDest);

		if (fxaaEnabled)
		{
			passFXAA(m_ldrIntermediateTexture, finalTarget);
		}
	}

	uint32_t SceneRenderer::readEntityId(uint32_t x, uint32_t y) const
	{
		if (!m_entityIdTexture || x >= m_width || y >= m_height)
		{
			return 0;
		}

		auto* nativeDev = static_cast<ID3D11Device*>(m_device.getNativeDevice());
		auto* nativeCtx = static_cast<ID3D11DeviceContext*>(m_device.getNativeContext());
		if (!nativeDev || !nativeCtx)
		{
			return 0;
		}

		auto* staging = static_cast<ID3D11Texture2D*>(m_stagingEntityId);
		if (!staging)
		{
			D3D11_TEXTURE2D_DESC desc{};
			desc.Width = 1;
			desc.Height = 1;
			desc.MipLevels = 1;
			desc.ArraySize = 1;
			desc.Format = DXGI_FORMAT_R32_UINT;
			desc.SampleDesc.Count = 1;
			desc.Usage = D3D11_USAGE_STAGING;
			desc.CPUAccessFlags = D3D11_CPU_ACCESS_READ;
			ID3D11Texture2D* newTex = nullptr;
			HRESULT hr = nativeDev->CreateTexture2D(&desc, nullptr, &newTex);
			if (SUCCEEDED(hr) && newTex)
			{
				m_stagingEntityId = newTex;
				staging = newTex;
			}
		}

		if (!staging) return 0;

		D3D11_BOX srcBox{};
		srcBox.left = x;
		srcBox.right = x + 1;
		srcBox.top = y;
		srcBox.bottom = y + 1;
		srcBox.front = 0;
		srcBox.back = 1;

		auto* srcTex = static_cast<ID3D11Resource*>(m_entityIdTexture->getNativeHandle());
		if (!srcTex) return 0;

		nativeCtx->CopySubresourceRegion(staging, 0, 0, 0, 0, srcTex, 0, &srcBox);

		D3D11_MAPPED_SUBRESOURCE mapped{};
		HRESULT hr = nativeCtx->Map(staging, 0, D3D11_MAP_READ, 0, &mapped);
		if (FAILED(hr)) return 0;

		uint32_t id = *reinterpret_cast<const uint32_t*>(mapped.pData);
		nativeCtx->Unmap(staging, 0);
		return id;
	}

	Math::Ray SceneRenderer::unprojectRayReversedZ(
		float screenX,
		float screenY,
		float viewportWidth,
		float viewportHeight,
		const Math::Matrix& invViewProj) noexcept
	{
		// Reversed-Z NDC:
		// Near plane is at Z = 1.0f
		// Far plane is at Z = 0.0f
		const float ndcX = (2.0f * screenX) / viewportWidth - 1.0f;
		const float ndcY = 1.0f - (2.0f * screenY) / viewportHeight;

		Math::Vector4 nearCS(ndcX, ndcY, 1.0f, 1.0f);
		Math::Vector4 farCS(ndcX, ndcY, 0.0f, 1.0f);

		Math::Vector4 nearWS = Math::Vector4::Transform(nearCS, invViewProj);
		Math::Vector4 farWS = Math::Vector4::Transform(farCS, invViewProj);

		if (std::abs(nearWS.w) > 0.00001f) nearWS /= nearWS.w;
		if (std::abs(farWS.w) > 0.00001f) farWS /= farWS.w;

		Math::Vector3 rayOrigin(nearWS.x, nearWS.y, nearWS.z);
		Math::Vector3 rayDir = Math::Vector3(farWS.x - nearWS.x, farWS.y - nearWS.y, farWS.z - nearWS.z);
		rayDir.Normalize();

		return Math::Ray(rayOrigin, rayDir);
	}
}
