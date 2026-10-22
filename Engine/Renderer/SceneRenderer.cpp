#include "SceneRenderer.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/Runtime/EngineContext.h"
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

		// 5. PostProcess Constant Buffer (16 bytes aligned)
		RHI::GpuBufferDesc ppDesc{};
		ppDesc.size = 16;
		ppDesc.usage = RHI::BufferUsage::Default;
		ppDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto ppRes = m_device.createBuffer(ppDesc);
		if (ppRes.isOk()) m_cbPostProcess = *ppRes;

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

		RHI::DepthStencilStateDesc dsDesc{};
		dsDesc.depthEnable = true;
		dsDesc.depthWrite = true;
		dsDesc.depthFunc = RHI::ComparisonFunc::GreaterEqual; // Reversed-Z
		m_dssOpaque = m_device.getOrCreateDepthStencilState(dsDesc);

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

	std::shared_ptr<RHI::IGpuTexture> SceneRenderer::getOrCreateTexture(const std::string& texturePath)
	{
		if (texturePath.empty())
		{
			return nullptr;
		}

		auto it = m_textureCache.find(texturePath);
		if (it != m_textureCache.end())
		{
			return it->second;
		}

		std::vector<uint8_t> fileBytes;

		// 1. Try VFS
		auto& vfs = Core::EngineContext::Get().vfs;
		std::string vpath = texturePath;
		std::replace(vpath.begin(), vpath.end(), '\\', '/');
		if (!vpath.empty() && vpath.front() != '/')
		{
			vpath = "/" + vpath;
		}

		auto vfsRes = vfs.readBinaryFile(vpath);
		if (vfsRes.isOk() && !vfsRes.value().empty())
		{
			fileBytes = std::move(vfsRes.value());
		}
		else
		{
			// 2. Try physical file paths
			std::string resolvedPath;
			if (std::filesystem::exists(texturePath))
			{
				resolvedPath = texturePath;
			}
			else if (std::filesystem::exists("Engine/Content/" + std::filesystem::path(texturePath).filename().string()))
			{
				resolvedPath = "Engine/Content/" + std::filesystem::path(texturePath).filename().string();
			}
			else if (std::filesystem::exists("Assets/" + texturePath))
			{
				resolvedPath = "Assets/" + texturePath;
			}

			if (!resolvedPath.empty())
			{
				std::ifstream f(resolvedPath, std::ios::binary | std::ios::ate);
				if (f.is_open())
				{
					std::streamsize size = f.tellg();
					f.seekg(0, std::ios::beg);
					fileBytes.resize(static_cast<size_t>(size));
					f.read(reinterpret_cast<char*>(fileBytes.data()), size);
				}
			}
		}

		if (fileBytes.empty())
		{
			DF_LOG_WARN("Renderer", "Could not load texture file: {}", texturePath);
			return nullptr;
		}

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

		// Ensure 32-bit RGBA UNORM
		const DirectX::Image* finalImage = scratchImage.GetImage(0, 0, 0);
		DirectX::ScratchImage convertedImage;
		if (metadata.format != DXGI_FORMAT_R8G8B8A8_UNORM)
		{
			hr = DirectX::Convert(
				*scratchImage.GetImage(0, 0, 0),
				DXGI_FORMAT_R8G8B8A8_UNORM,
				DirectX::TEX_FILTER_DEFAULT,
				DirectX::TEX_THRESHOLD_DEFAULT,
				convertedImage
			);
			if (SUCCEEDED(hr))
			{
				finalImage = convertedImage.GetImage(0, 0, 0);
				metadata.format = DXGI_FORMAT_R8G8B8A8_UNORM;
			}
		}

		if (!finalImage)
		{
			return nullptr;
		}

		RHI::GpuTextureDesc texDesc{};
		texDesc.width = static_cast<uint32_t>(metadata.width);
		texDesc.height = static_cast<uint32_t>(metadata.height);
		texDesc.mipLevels = 1;
		texDesc.arraySize = 1;
		texDesc.format = RHI::PixelFormat::R8G8B8A8_UNORM;
		texDesc.usage = RHI::BufferUsage::Default;
		texDesc.bindFlags = RHI::BufferBindFlags::ShaderResource;

		auto texRes = m_device.createTexture(texDesc, finalImage->pixels);
		if (texRes.isOk())
		{
			m_textureCache[texturePath] = *texRes;
			DF_LOG_INFO("Renderer", "Loaded texture '{}' ({}x{})", texturePath, texDesc.width, texDesc.height);
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

		// 1. Try VFS
		auto& vfs = Core::EngineContext::Get().vfs;
		std::string vpath = materialPath;
		std::replace(vpath.begin(), vpath.end(), '\\', '/');
		if (!vpath.empty() && vpath.front() != '/')
		{
			vpath = "/" + vpath;
		}

		auto vfsRes = vfs.readTextFile(vpath);
		if (vfsRes.isOk())
		{
			jsonContent = vfsRes.value();
		}
		else
		{
			// 2. Try physical file
			if (std::filesystem::exists(materialPath))
			{
				std::ifstream f(materialPath);
				if (f.is_open())
				{
					std::stringstream ss;
					ss << f.rdbuf();
					jsonContent = ss.str();
				}
			}
			else if (std::filesystem::exists("Engine/Content/" + std::filesystem::path(materialPath).filename().string()))
			{
				std::ifstream f("Engine/Content/" + std::filesystem::path(materialPath).filename().string());
				if (f.is_open())
				{
					std::stringstream ss;
					ss << f.rdbuf();
					jsonContent = ss.str();
				}
			}
			else if (std::filesystem::exists("Assets/" + materialPath))
			{
				std::ifstream f("Assets/" + materialPath);
				if (f.is_open())
				{
					std::stringstream ss;
					ss << f.rdbuf();
					jsonContent = ss.str();
				}
			}
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
					mat->setAlbedoTexture(getOrCreateTexture(j["albedoTexture"].get<std::string>()));
				}
				if (j.contains("normalTexture") && j["normalTexture"].is_string() && !j["normalTexture"].get<std::string>().empty())
				{
					mat->setNormalTexture(getOrCreateTexture(j["normalTexture"].get<std::string>()));
				}
				if (j.contains("metallicRoughnessTexture") && j["metallicRoughnessTexture"].is_string() && !j["metallicRoughnessTexture"].get<std::string>().empty())
				{
					mat->setMetallicRoughnessTexture(getOrCreateTexture(j["metallicRoughnessTexture"].get<std::string>()));
				}
				if (j.contains("emissiveTexture") && j["emissiveTexture"].is_string() && !j["emissiveTexture"].get<std::string>().empty())
				{
					mat->setEmissiveTexture(getOrCreateTexture(j["emissiveTexture"].get<std::string>()));
				}
				if (j.contains("occlusionTexture") && j["occlusionTexture"].is_string() && !j["occlusionTexture"].get<std::string>().empty())
				{
					mat->setOcclusionTexture(getOrCreateTexture(j["occlusionTexture"].get<std::string>()));
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

		// 1. Update PerFrame Constant Buffer
		CBPerFrame frameData{};
		frameData.time = 0.0f;
		frameData.deltaTime = 0.016f;
		frameData.frameIndex = 0;
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

		// Pass 1: Forward Opaque & Alpha-Test (HDR + Reversed-Z + Entity ID)
		passOpaque(scene);

		// Pass 2: Skybox
		passSkybox(scene);

		// Pass 3: Transparent
		passTransparent(scene);

		// Pass 4: Post-Process & Tonemap (Outputs to targetTexture, backbuffer, or m_finalLdrTexture)
		passPostProcess(view);
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

		// Reversed-Z: depth cleared to 0.0f!
		m_context->clearDepthStencil(m_depthTexture, 0.0f, 0);

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
	}

	void SceneRenderer::passSkybox(const RenderScene& scene)
	{
		(void)scene;
	}

	void SceneRenderer::passTransparent(const RenderScene& scene)
	{
		(void)scene;
	}

	void SceneRenderer::passPostProcess(const RenderView& view)
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

		std::vector<std::shared_ptr<RHI::IGpuTexture>> rtvs = { finalTarget };
		m_context->setRenderTargets(rtvs, nullptr);
		m_context->clearRenderTarget(finalTarget, Color(0.05f, 0.05f, 0.08f, 1.0f));

		RHI::Viewport vp{};
		vp.x = 0.0f;
		vp.y = 0.0f;
		vp.width = static_cast<float>(view.width);
		vp.height = static_cast<float>(view.height);
		vp.minDepth = 0.0f;
		vp.maxDepth = 1.0f;
		m_context->setViewport(vp);

		if (m_vsFullscreen && m_psPostProcess)
		{
			m_context->setPrimitiveTopology(RHI::PrimitiveTopology::TriangleList);
			m_context->setInputLayout(nullptr);
			m_context->setVertexShader(m_vsFullscreen);
			m_context->setPixelShader(m_psPostProcess);
			if (m_rsSolid) m_context->setRasterizerState(m_rsSolid);
			m_context->setDepthStencilState(nullptr, 0);
			m_context->setBlendState(nullptr);

			// Bind HDR Texture as SRV on slot 0
			m_context->setShaderResourcePS(0, m_hdrTexture);
			if (m_samplerLinearClamp) m_context->setSamplerPS(DF_SLOT_SMP_LINEAR_CLAMP, m_samplerLinearClamp);
			if (m_samplerPointClamp) m_context->setSamplerPS(DF_SLOT_SMP_POINT_CLAMP, m_samplerPointClamp);

			// PostProcess CB (exposure, enableFXAA, texelSize)
			struct CBPostProcess
			{
				float exposure;
				float enableFXAA;
				Math::Vector2 texelSize;
			} cbPP{
				m_config.exposure > 0.001f ? m_config.exposure : 1.0f,
				m_config.enableFXAA ? 1.0f : 0.0f,
				Math::Vector2(
					1.0f / (view.width > 0 ? static_cast<float>(view.width) : 1.0f),
					1.0f / (view.height > 0 ? static_cast<float>(view.height) : 1.0f)
				)
			};

			if (m_cbPostProcess)
			{
				m_context->updateSubresource(m_cbPostProcess, &cbPP, sizeof(cbPP));
				m_context->setConstantBufferPS(0, m_cbPostProcess);
			}

			m_context->draw(3, 0);
			m_drawCallCount++;

			// CRITICAL: Unbind HDR texture so next frame can bind it as RTV!
			m_context->setShaderResourcePS(0, nullptr);
		}
		else
		{
			// Fallback clear if shaders missing
			m_context->clearRenderTarget(finalTarget, Color(0.05f, 0.05f, 0.08f, 1.0f));
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
