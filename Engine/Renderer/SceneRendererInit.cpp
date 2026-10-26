#include "SceneRenderer.h"
#include "Engine/Renderer/DebugRenderer.h"
#include "Engine/Core/Log/Log.h"
#include <algorithm>
#include <array>
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

		// 5. Directional Shadow Constant Buffer
		RHI::GpuBufferDesc shadowBufDesc{};
		shadowBufDesc.size = sizeof(CBShadow);
		shadowBufDesc.usage = RHI::BufferUsage::Default;
		shadowBufDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto shadowBufRes = m_device.createBuffer(shadowBufDesc);
		if (shadowBufRes.isOk()) m_cbShadow = *shadowBufRes;

		// 6. PostProcess & Feature Constant Buffers
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
		fxaaDesc.size = sizeof(CBFXAA);
		fxaaDesc.usage = RHI::BufferUsage::Default;
		fxaaDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto fxaaRes = m_device.createBuffer(fxaaDesc);
		if (fxaaRes.isOk()) m_cbFXAA = *fxaaRes;

		// Nature Constant Buffers (Phases 5-7)
		RHI::GpuBufferDesc terrainDesc{};
		terrainDesc.size = sizeof(CBTerrain);
		terrainDesc.usage = RHI::BufferUsage::Default;
		terrainDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto terrainRes = m_device.createBuffer(terrainDesc);
		if (terrainRes.isOk()) m_cbTerrain = *terrainRes;

		RHI::GpuBufferDesc foliageDesc{};
		foliageDesc.size = sizeof(CBFoliage);
		foliageDesc.usage = RHI::BufferUsage::Default;
		foliageDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto foliageRes = m_device.createBuffer(foliageDesc);
		if (foliageRes.isOk()) m_cbFoliage = *foliageRes;

		RHI::GpuBufferDesc waterDesc{};
		waterDesc.size = sizeof(CBWater);
		waterDesc.usage = RHI::BufferUsage::Default;
		waterDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto waterRes = m_device.createBuffer(waterDesc);
		if (waterRes.isOk()) m_cbWater = *waterRes;

		// Milestone 1 (Tier 2/3): Hi-Z SSR, DDGI & Subsurface Scattering Constant Buffers
		RHI::GpuBufferDesc ssrCbDesc{};
		ssrCbDesc.size = sizeof(CBSSR);
		ssrCbDesc.usage = RHI::BufferUsage::Default;
		ssrCbDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto ssrCbRes = m_device.createBuffer(ssrCbDesc);
		if (ssrCbRes.isOk()) m_cbSSR = *ssrCbRes;

		RHI::GpuBufferDesc ddgiCbDesc{};
		ddgiCbDesc.size = sizeof(CBDDGI);
		ddgiCbDesc.usage = RHI::BufferUsage::Default;
		ddgiCbDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto ddgiCbRes = m_device.createBuffer(ddgiCbDesc);
		if (ddgiCbRes.isOk()) m_cbDDGI = *ddgiCbRes;

		RHI::GpuBufferDesc sssCbDesc{};
		sssCbDesc.size = sizeof(CBSSS);
		sssCbDesc.usage = RHI::BufferUsage::Default;
		sssCbDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto sssCbRes = m_device.createBuffer(sssCbDesc);
		if (sssCbRes.isOk()) m_cbSSS = *sssCbRes;

		// Milestone 2: Atmospheric Lighting, Volumetrics & Camera VFX Constant & Structured Buffers
		RHI::GpuBufferDesc mbCbDesc{};
		mbCbDesc.size = sizeof(CBMotionBlur);
		mbCbDesc.usage = RHI::BufferUsage::Default;
		mbCbDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto mbCbRes = m_device.createBuffer(mbCbDesc);
		if (mbCbRes.isOk()) m_cbMotionBlur = *mbCbRes;

		RHI::GpuBufferDesc vfCbDesc{};
		vfCbDesc.size = sizeof(CBVolumetricFog);
		vfCbDesc.usage = RHI::BufferUsage::Default;
		vfCbDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto vfCbRes = m_device.createBuffer(vfCbDesc);
		if (vfCbRes.isOk()) m_cbVolumetricFog = *vfCbRes;

		RHI::GpuBufferDesc vfiCbDesc{};
		vfiCbDesc.size = sizeof(CBVolumetricFogIntegrate);
		vfiCbDesc.usage = RHI::BufferUsage::Default;
		vfiCbDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto vfiCbRes = m_device.createBuffer(vfiCbDesc);
		if (vfiCbRes.isOk()) m_cbVolumetricFogIntegrate = *vfiCbRes;

		RHI::GpuBufferDesc histCbDesc{};
		histCbDesc.size = sizeof(CBLuminanceHistogram);
		histCbDesc.usage = RHI::BufferUsage::Default;
		histCbDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto histCbRes = m_device.createBuffer(histCbDesc);
		if (histCbRes.isOk()) m_cbHistogram = *histCbRes;

		RHI::GpuBufferDesc avgCbDesc{};
		avgCbDesc.size = sizeof(CBLuminanceAverage);
		avgCbDesc.usage = RHI::BufferUsage::Default;
		avgCbDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto avgCbRes = m_device.createBuffer(avgCbDesc);
		if (avgCbRes.isOk()) m_cbAverage = *avgCbRes;

		RHI::GpuBufferDesc psCbDesc{};
		psCbDesc.size = sizeof(CBParaboloidShadow);
		psCbDesc.usage = RHI::BufferUsage::Default;
		psCbDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto psCbRes = m_device.createBuffer(psCbDesc);
		if (psCbRes.isOk()) m_cbParaboloidShadow = *psCbRes;

		RHI::GpuBufferDesc lumaHistDesc{};
		lumaHistDesc.size = sizeof(uint32_t) * 64;
		lumaHistDesc.usage = RHI::BufferUsage::Default;
		lumaHistDesc.bindFlags = RHI::BufferBindFlags::ShaderResource | RHI::BufferBindFlags::UnorderedAccess;
		lumaHistDesc.structureByteStride = sizeof(uint32_t);
		auto lumaHistRes = m_device.createBuffer(lumaHistDesc);
		if (lumaHistRes.isOk()) m_bufLuminanceHistogram = *lumaHistRes;

		RHI::GpuBufferDesc lumaAvgDesc{};
		lumaAvgDesc.size = sizeof(float) * 4;
		lumaAvgDesc.usage = RHI::BufferUsage::Default;
		lumaAvgDesc.bindFlags = RHI::BufferBindFlags::ShaderResource | RHI::BufferBindFlags::UnorderedAccess;
		lumaAvgDesc.structureByteStride = sizeof(float);
		auto lumaAvgRes = m_device.createBuffer(lumaAvgDesc);
		if (lumaAvgRes.isOk()) m_bufAdaptedLuminance = *lumaAvgRes;

		// Tier 1: TAA, Clustered Lighting & GPU Culling Constant Buffers
		RHI::GpuBufferDesc taaCbDesc{};
		taaCbDesc.size = sizeof(CBTAA);
		taaCbDesc.usage = RHI::BufferUsage::Default;
		taaCbDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto taaCbRes = m_device.createBuffer(taaCbDesc);
		if (taaCbRes.isOk()) m_cbTAA = *taaCbRes;

		RHI::GpuBufferDesc cbClusterBuildDesc{};
		cbClusterBuildDesc.size = sizeof(CBClusterBuild);
		cbClusterBuildDesc.usage = RHI::BufferUsage::Default;
		cbClusterBuildDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto cbClusterBuildRes = m_device.createBuffer(cbClusterBuildDesc);
		if (cbClusterBuildRes.isOk()) m_cbClusterBuild = *cbClusterBuildRes;

		RHI::GpuBufferDesc cbClusterAssignDesc{};
		cbClusterAssignDesc.size = sizeof(CBClusterAssign);
		cbClusterAssignDesc.usage = RHI::BufferUsage::Default;
		cbClusterAssignDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto cbClusterAssignRes = m_device.createBuffer(cbClusterAssignDesc);
		if (cbClusterAssignRes.isOk()) m_cbClusterAssign = *cbClusterAssignRes;

		RHI::GpuBufferDesc cbCullDesc{};
		cbCullDesc.size = sizeof(CBCullData);
		cbCullDesc.usage = RHI::BufferUsage::Default;
		cbCullDesc.bindFlags = RHI::BufferBindFlags::ConstantBuffer;
		auto cbCullRes = m_device.createBuffer(cbCullDesc);
		if (cbCullRes.isOk()) m_cbCull = *cbCullRes;

		// Clustered Dynamic Lighting 16x16x24 Buffers (6,144 clusters)
		RHI::GpuBufferDesc aabbBufDesc{};
		aabbBufDesc.size = 6144 * sizeof(ClusterAABB);
		aabbBufDesc.usage = RHI::BufferUsage::Default;
		aabbBufDesc.bindFlags = RHI::BufferBindFlags::ShaderResource | RHI::BufferBindFlags::UnorderedAccess;
		aabbBufDesc.structureByteStride = sizeof(ClusterAABB);
		auto aabbRes = m_device.createBuffer(aabbBufDesc);
		if (aabbRes.isOk()) m_bufClusterAABBs = *aabbRes;

		RHI::GpuBufferDesc gridBufDesc{};
		gridBufDesc.size = 6144 * sizeof(uint32_t) * 2; // uint2
		gridBufDesc.usage = RHI::BufferUsage::Default;
		gridBufDesc.bindFlags = RHI::BufferBindFlags::ShaderResource | RHI::BufferBindFlags::UnorderedAccess;
		gridBufDesc.structureByteStride = sizeof(uint32_t) * 2;
		auto gridRes = m_device.createBuffer(gridBufDesc);
		if (gridRes.isOk()) m_bufClusterGrid = *gridRes;

		RHI::GpuBufferDesc lightIdxBufDesc{};
		lightIdxBufDesc.size = 6144 * 64 * sizeof(uint32_t); // 393,216 indices
		lightIdxBufDesc.usage = RHI::BufferUsage::Default;
		lightIdxBufDesc.bindFlags = RHI::BufferBindFlags::ShaderResource | RHI::BufferBindFlags::UnorderedAccess;
		lightIdxBufDesc.structureByteStride = sizeof(uint32_t);
		auto lightIdxRes = m_device.createBuffer(lightIdxBufDesc);
		if (lightIdxRes.isOk()) m_bufClusterLightIndices = *lightIdxRes;

		RHI::GpuBufferDesc counterBufDesc{};
		counterBufDesc.size = 16;
		counterBufDesc.usage = RHI::BufferUsage::Default;
		counterBufDesc.bindFlags = RHI::BufferBindFlags::UnorderedAccess;
		counterBufDesc.structureByteStride = sizeof(uint32_t);
		auto counterRes = m_device.createBuffer(counterBufDesc);
		if (counterRes.isOk()) m_bufClusterCounter = *counterRes;

		// Initialize Compute Kernels
		m_csClusterBuild.init(m_device, m_shaderLibrary, "ClusterBuildAABBsCS", "ClusterBuildAABBsCS.hlsl", sizeof(CBClusterBuild));
		m_csClusterAssign.init(m_device, m_shaderLibrary, "ClusterLightAssignCS", "ClusterLightAssignCS.hlsl", sizeof(CBClusterAssign));
		m_csCullInstances.init(m_device, m_shaderLibrary, "CullInstancesCS", "CullInstancesCS.hlsl", sizeof(CBCullData));
		m_csHiZBuild.init(m_device, m_shaderLibrary, "HiZBuildCS", "HiZBuildCS.hlsl", sizeof(Math::Vector4));
		m_csDDGIRadianceGather.init(m_device, m_shaderLibrary, "DDGIRadianceGatherCS", "DDGIRadianceGatherCS.hlsl", sizeof(CBDDGI));
		m_csDDGIDepthGather.init(m_device, m_shaderLibrary, "DDGIDepthGatherCS", "DDGIDepthGatherCS.hlsl", sizeof(CBDDGI));

		// Milestone 2 Compute Kernels
		m_csTileMax.init(m_device, m_shaderLibrary, "TileMaxCS", "TileMaxCS.hlsl", sizeof(uint32_t) * 4);
		m_csNeighborMax.init(m_device, m_shaderLibrary, "NeighborMaxCS", "NeighborMaxCS.hlsl", sizeof(uint32_t) * 4);
		m_csVolumetricFog.init(m_device, m_shaderLibrary, "VolumetricFogCS", "VolumetricFogCS.hlsl", sizeof(CBVolumetricFog));
		m_csVolumetricFogIntegrate.init(m_device, m_shaderLibrary, "VolumetricFogIntegrateCS", "VolumetricFogIntegrateCS.hlsl", sizeof(CBVolumetricFogIntegrate));
		m_csLuminanceHistogram.init(m_device, m_shaderLibrary, "LuminanceHistogramCS", "LuminanceHistogramCS.hlsl", sizeof(CBLuminanceHistogram));
		m_csLuminanceAverage.init(m_device, m_shaderLibrary, "LuminanceAverageCS", "LuminanceAverageCS.hlsl", sizeof(CBLuminanceAverage));

		generateSSAOKernel();

		// 7. Lights Structured Buffer (Support up to 64 lights per pass)
		RHI::GpuBufferDesc lightDesc{};
		lightDesc.size = sizeof(LightData) * 64;
		lightDesc.usage = RHI::BufferUsage::Default;
		lightDesc.bindFlags = RHI::BufferBindFlags::ShaderResource;
		lightDesc.structureByteStride = sizeof(LightData);
		auto lightRes = m_device.createBuffer(lightDesc);
		if (lightRes.isOk()) m_bufLights = *lightRes;

		// 8. Pipeline States
		RHI::RasterizerStateDesc rsDesc{};
		rsDesc.fillMode = RHI::FillMode::Solid;
		rsDesc.cullMode = RHI::CullMode::Back;
		rsDesc.frontCounterClockwise = false;
		m_rsSolid = m_device.getOrCreateRasterizerState(rsDesc);

		RHI::RasterizerStateDesc rsNoneDesc{};
		rsNoneDesc.fillMode = RHI::FillMode::Solid;
		rsNoneDesc.cullMode = RHI::CullMode::None;
		rsNoneDesc.frontCounterClockwise = false;
		m_rsCullNone = m_device.getOrCreateRasterizerState(rsNoneDesc);

		// Shadow pass: Standard depth test (LessEqual), write enabled
		RHI::DepthStencilStateDesc dsShadowDesc{};
		dsShadowDesc.depthEnable = true;
		dsShadowDesc.depthWrite = true;
		dsShadowDesc.depthFunc = RHI::ComparisonFunc::LessEqual;
		m_dssShadow = m_device.getOrCreateDepthStencilState(dsShadowDesc);

		// Depth Prepass: test & write GreaterEqual (Reversed-Z)
		RHI::DepthStencilStateDesc dsPrepassDesc{};
		dsPrepassDesc.depthEnable = true;
		dsPrepassDesc.depthWrite = true;
		dsPrepassDesc.depthFunc = RHI::ComparisonFunc::GreaterEqual;
		m_dssDepthPrepass = m_device.getOrCreateDepthStencilState(dsPrepassDesc);

		// Opaque Pass: test & write GreaterEqual (Reversed-Z)
		RHI::DepthStencilStateDesc dsDesc{};
		dsDesc.depthEnable = true;
		dsDesc.depthWrite = true;
		dsDesc.depthFunc = RHI::ComparisonFunc::GreaterEqual;
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

		RHI::BlendStateDesc bsPma{};
		bsPma.blendEnable = true;
		bsPma.srcBlend = RHI::BlendFactor::One;
		bsPma.destBlend = RHI::BlendFactor::InvSrcAlpha;
		bsPma.blendOp = RHI::BlendOp::Add;
		bsPma.srcBlendAlpha = RHI::BlendFactor::One;
		bsPma.destBlendAlpha = RHI::BlendFactor::InvSrcAlpha;
		bsPma.blendOpAlpha = RHI::BlendOp::Add;
		m_bsPremultipliedAlpha = m_device.getOrCreateBlendState(bsPma);

		// Samplers
		RHI::SamplerStateDesc smpLinearWrap{};
		smpLinearWrap.filter = RHI::FilterMode::Linear;
		smpLinearWrap.addressU = RHI::AddressMode::Wrap;
		smpLinearWrap.addressV = RHI::AddressMode::Wrap;
		smpLinearWrap.addressW = RHI::AddressMode::Wrap;
		m_samplerLinearWrap = m_device.getOrCreateSamplerState(smpLinearWrap);

		RHI::SamplerStateDesc smpAnisoWrap{};
		smpAnisoWrap.filter = RHI::FilterMode::Anisotropic;
		smpAnisoWrap.maxAnisotropy = 16;
		smpAnisoWrap.addressU = RHI::AddressMode::Wrap;
		smpAnisoWrap.addressV = RHI::AddressMode::Wrap;
		smpAnisoWrap.addressW = RHI::AddressMode::Wrap;
		m_samplerAnisoWrap = m_device.getOrCreateSamplerState(smpAnisoWrap);

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

		// Directional Shadow Comparison Sampler (LessEqual, Border 1.0)
		RHI::SamplerStateDesc smpShadow{};
		smpShadow.filter = RHI::FilterMode::Linear;
		smpShadow.addressU = RHI::AddressMode::Border;
		smpShadow.addressV = RHI::AddressMode::Border;
		smpShadow.addressW = RHI::AddressMode::Border;
		smpShadow.comparisonFunc = RHI::ComparisonFunc::LessEqual;
		smpShadow.borderColor[0] = 1.0f;
		smpShadow.borderColor[1] = 1.0f;
		smpShadow.borderColor[2] = 1.0f;
		smpShadow.borderColor[3] = 1.0f;
		m_samplerShadow = m_device.getOrCreateSamplerState(smpShadow);

		// 9. Shaders & Input Layouts
		auto vsRes = m_shaderLibrary.getOrCreateVertexShader("StandardVS.hlsl");
		if (vsRes.isOk()) m_vsStandard = *vsRes;

		auto psRes = m_shaderLibrary.getOrCreatePixelShader("StandardPS.hlsl");
		if (psRes.isOk()) m_psStandard = *psRes;

		const std::vector<RHI::InputElementDesc> inputElements = {
			{ "POSITION", 0, RHI::PixelFormat::R32G32B32_FLOAT, 0, 0, false },
			{ "NORMAL", 0, RHI::PixelFormat::R32G32B32_FLOAT, 0, 12, false },
			{ "TEXCOORD", 0, RHI::PixelFormat::R32G32_FLOAT, 0, 24, false },
			{ "TANGENT", 0, RHI::PixelFormat::R32G32B32A32_FLOAT, 0, 32, false },
			{ "COLOR", 0, RHI::PixelFormat::R32G32B32A32_FLOAT, 0, 48, false }
		};

		if (m_vsStandard)
		{
			auto layoutRes = m_device.createInputLayout(inputElements, m_vsStandard->getBytecode());
			if (layoutRes.isOk()) m_inputLayout = *layoutRes;
		}

		auto vsShadowRes = m_shaderLibrary.getOrCreateVertexShader("ShadowVS.hlsl");
		if (vsShadowRes.isOk())
		{
			m_vsShadow = *vsShadowRes;
			auto shadowLayoutRes = m_device.createInputLayout(inputElements, m_vsShadow->getBytecode());
			if (shadowLayoutRes.isOk()) m_inputLayoutShadow = *shadowLayoutRes;
		}

		auto vsFsRes = m_shaderLibrary.getOrCreateVertexShader("FullscreenVS.hlsl");
		if (vsFsRes.isOk()) m_vsFullscreen = *vsFsRes;

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

		auto psDepthCutoutRes = m_shaderLibrary.getOrCreatePixelShader("DepthCutoutPS.hlsl");
		if (psDepthCutoutRes.isOk()) m_psDepthCutout = *psDepthCutoutRes;

		auto psShadowCutoutRes = m_shaderLibrary.getOrCreatePixelShader("ShadowCutoutPS.hlsl");
		if (psShadowCutoutRes.isOk()) m_psShadowCutout = *psShadowCutoutRes;

		auto psGBufferRes = m_shaderLibrary.getOrCreatePixelShader("GBufferPS.hlsl");
		if (psGBufferRes.isOk()) m_psGBuffer = *psGBufferRes;

		auto psDeferredRes = m_shaderLibrary.getOrCreatePixelShader("DeferredLightingPS.hlsl");
		if (psDeferredRes.isOk()) m_psDeferredLighting = *psDeferredRes;

		auto vsDecalRes = m_shaderLibrary.getOrCreateVertexShader("DecalVS.hlsl");
		if (vsDecalRes.isOk()) m_vsDecal = *vsDecalRes;

		auto psDecalRes = m_shaderLibrary.getOrCreatePixelShader("DecalPS.hlsl");
		if (psDecalRes.isOk()) m_psDecal = *psDecalRes;

		auto hsStdRes = m_shaderLibrary.getOrCreateHullShader("StandardHS.hlsl");
		if (hsStdRes.isOk()) m_hsStandard = *hsStdRes;

		auto dsStdRes = m_shaderLibrary.getOrCreateDomainShader("StandardDS.hlsl");
		if (dsStdRes.isOk()) m_dsStandard = *dsStdRes;

		// Nature Shaders (Phases 5-7)
		auto vsTerrainRes = m_shaderLibrary.getOrCreateVertexShader("TerrainVS.hlsl");
		if (vsTerrainRes.isOk()) m_vsTerrain = *vsTerrainRes;
		auto psTerrainRes = m_shaderLibrary.getOrCreatePixelShader("TerrainPS.hlsl");
		if (psTerrainRes.isOk()) m_psTerrain = *psTerrainRes;

		auto vsGrassRes = m_shaderLibrary.getOrCreateVertexShader("GrassVS.hlsl");
		if (vsGrassRes.isOk()) m_vsGrass = *vsGrassRes;
		auto psGrassRes = m_shaderLibrary.getOrCreatePixelShader("GrassPS.hlsl");
		if (psGrassRes.isOk()) m_psGrass = *psGrassRes;

		auto vsWaterRes = m_shaderLibrary.getOrCreateVertexShader("WaterVS.hlsl");
		if (vsWaterRes.isOk()) m_vsWater = *vsWaterRes;
		auto psWaterRes = m_shaderLibrary.getOrCreatePixelShader("WaterPS.hlsl");
		if (psWaterRes.isOk()) m_psWater = *psWaterRes;

		// TAA Pixel Shader (Tier 1)
		auto psTAARes = m_shaderLibrary.getOrCreatePixelShader("TAAPS.hlsl");
		if (psTAARes.isOk()) m_psTAA = *psTAARes;

		// Milestone 1 (Tier 2/3): Hi-Z SSR & Subsurface Shaders
		auto psSSRRes = m_shaderLibrary.getOrCreatePixelShader("SSRPS.hlsl");
		if (psSSRRes.isOk()) m_psSSR = *psSSRRes;

		auto psSubsurfRes = m_shaderLibrary.getOrCreatePixelShader("SubsurfacePS.hlsl");
		if (psSubsurfRes.isOk()) m_psSubsurface = *psSubsurfRes;

		// Milestone 2: Motion Blur & Paraboloid Shadow Shaders
		auto psMBRes = m_shaderLibrary.getOrCreatePixelShader("MotionBlurPS.hlsl");
		if (psMBRes.isOk()) m_psMotionBlur = *psMBRes;

		auto vsParaRes = m_shaderLibrary.getOrCreateVertexShader("ShadowParaboloidVS.hlsl");
		if (vsParaRes.isOk())
		{
			m_vsShadowParaboloid = *vsParaRes;
			auto paraLayoutRes = m_device.createInputLayout(inputElements, m_vsShadowParaboloid->getBytecode());
			if (paraLayoutRes.isOk()) m_inputLayoutShadowParaboloid = *paraLayoutRes;
		}

		// Milestone 2: 3D Froxel Volumetric Fog Textures (160x90x64 R16G16B16A16_FLOAT)
		RHI::GpuTextureDesc froxelDesc{};
		froxelDesc.width = 160;
		froxelDesc.height = 90;
		froxelDesc.depth = 64;
		froxelDesc.format = RHI::PixelFormat::R16G16B16A16_FLOAT;
		froxelDesc.bindFlags = RHI::BufferBindFlags::ShaderResource | RHI::BufferBindFlags::UnorderedAccess;
		auto fRawRes = m_device.createTexture(froxelDesc);
		if (fRawRes.isOk()) m_volumetricFogRaw = *fRawRes;
		auto fHist0Res = m_device.createTexture(froxelDesc);
		if (fHist0Res.isOk()) m_volumetricFogHistory[0] = *fHist0Res;
		auto fHist1Res = m_device.createTexture(froxelDesc);
		if (fHist1Res.isOk()) m_volumetricFogHistory[1] = *fHist1Res;
		auto fIntRes = m_device.createTexture(froxelDesc);
		if (fIntRes.isOk()) m_volumetricFogIntegrated = *fIntRes;

		// Milestone 1 (Tier 2/3): DDGI Radiance & Depth Atlases
		RHI::GpuTextureDesc ddgiRadDesc{};
		ddgiRadDesc.width = 512;
		ddgiRadDesc.height = 512;
		ddgiRadDesc.format = RHI::PixelFormat::R16G16B16A16_FLOAT;
		ddgiRadDesc.bindFlags = RHI::BufferBindFlags::ShaderResource | RHI::BufferBindFlags::UnorderedAccess;
		auto ddgiRadRes = m_device.createTexture(ddgiRadDesc);
		if (ddgiRadRes.isOk()) m_ddgiRadianceAtlas = *ddgiRadRes;

		RHI::GpuTextureDesc ddgiDepthDesc{};
		ddgiDepthDesc.width = 512;
		ddgiDepthDesc.height = 1024;
		ddgiDepthDesc.format = RHI::PixelFormat::R16G16_FLOAT;
		ddgiDepthDesc.bindFlags = RHI::BufferBindFlags::ShaderResource | RHI::BufferBindFlags::UnorderedAccess;
		auto ddgiDepthRes = m_device.createTexture(ddgiDepthDesc);
		if (ddgiDepthRes.isOk()) m_ddgiDepthAtlas = *ddgiDepthRes;

		// Debug Line Shaders & Dynamic Vertex Buffer (Phase 8)
		auto vsDebugLineRes = m_shaderLibrary.getOrCreateVertexShader("DebugLineVS.hlsl");
		if (vsDebugLineRes.isOk())
		{
			m_vsDebugLine = *vsDebugLineRes;
			const std::vector<RHI::InputElementDesc> debugLineElements = {
				{ "POSITION", 0, RHI::PixelFormat::R32G32B32_FLOAT, 0, 0, false },
				{ "COLOR", 0, RHI::PixelFormat::R32G32B32A32_FLOAT, 0, 12, false }
			};
			auto dlLayoutRes = m_device.createInputLayout(debugLineElements, m_vsDebugLine->getBytecode());
			if (dlLayoutRes.isOk()) m_inputLayoutDebugLine = *dlLayoutRes;
		}
		auto psDebugLineRes = m_shaderLibrary.getOrCreatePixelShader("DebugLinePS.hlsl");
		if (psDebugLineRes.isOk()) m_psDebugLine = *psDebugLineRes;

		m_vbDebugLinesCapacity = 4096;
		RHI::GpuBufferDesc vbDebugDesc{};
		vbDebugDesc.size = m_vbDebugLinesCapacity * sizeof(DebugVertex);
		vbDebugDesc.usage = RHI::BufferUsage::Dynamic;
		vbDebugDesc.bindFlags = RHI::BufferBindFlags::VertexBuffer;
		auto vbDebugRes = m_device.createBuffer(vbDebugDesc);
		if (vbDebugRes.isOk()) m_vbDebugLines = *vbDebugRes;

		m_grassBladeMesh = createGpuMesh(CreateGrassBladeMesh(5));

		// 10. Default 1x1 Textures (Fix B4: defaultMRPix has R=1, G=1, B=1 so untextured factors scale cleanly)
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

		const uint32_t defaultMRPix = 0xFFFFFFFF; // RGBA = 255, 255, 255, 255 (AO=1, Rough=1, Metal=1)
		auto mrTex = m_device.createTexture(tex1x1, &defaultMRPix);
		if (mrTex.isOk()) m_defaultMRTexture = *mrTex;

		const uint32_t blackPix = 0xFF000000;
		auto bTex = m_device.createTexture(tex1x1, &blackPix);
		if (bTex.isOk()) m_blackTexture = *bTex;

		// 10b. Generate 2D Split-Sum GGX BRDF LUT
		generateBRDFLUT();

		// 11. Directional Shadow Map Texture (2048x2048 D32_FLOAT)
		RHI::GpuTextureDesc shadowDesc{};
		shadowDesc.width = kShadowMapResolution;
		shadowDesc.height = kShadowMapResolution;
		shadowDesc.format = RHI::PixelFormat::D32_FLOAT;
		shadowDesc.bindFlags = RHI::BufferBindFlags::DepthStencil | RHI::BufferBindFlags::ShaderResource;
		auto shadowRes = m_device.createTexture(shadowDesc);
		if (shadowRes.isOk()) m_shadowMapTexture = *shadowRes;
		m_shadowAtlasAllocator.initialize(kShadowMapResolution, kShadowMapResolution);

		// 12. Seed Builtin Meshes
		auto cubeData = CreateCubeMesh(1.0f);
		auto cubeGpu = createGpuMesh(cubeData);
		m_meshCache[""] = cubeGpu;
		m_meshCache["Cube"] = cubeGpu;
		m_meshCache["cube"] = cubeGpu;
		m_meshCache["cube.obj"] = cubeGpu;
		m_meshCache["builtin:cube"] = cubeGpu;
		m_meshCache["/engine/Content/cube.obj"] = cubeGpu;

		auto sphereData = CreateSphereMesh(0.5f, 16, 32);
		auto sphereGpu = createGpuMesh(sphereData);
		m_meshCache["Sphere"] = sphereGpu;
		m_meshCache["sphere"] = sphereGpu;
		m_meshCache["sphere.obj"] = sphereGpu;
		m_meshCache["builtin:sphere"] = sphereGpu;
		m_meshCache["/engine/Content/sphere.obj"] = sphereGpu;

		auto planeData = CreatePlaneMesh(1.0f, 1.0f);
		auto planeGpu = createGpuMesh(planeData);
		m_meshCache["Plane"] = planeGpu;
		m_meshCache["plane"] = planeGpu;
		m_meshCache["plane.obj"] = planeGpu;
		m_meshCache["builtin:plane"] = planeGpu;
		m_meshCache["/engine/Content/plane.obj"] = planeGpu;

		// Unit billboard quad for particles: XY [-0.5, 0.5] facing -Z
		MeshData quadData;
		quadData.vertices.push_back({ { -0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		quadData.vertices.push_back({ {  0.5f, -0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		quadData.vertices.push_back({ {  0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 1.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		quadData.vertices.push_back({ { -0.5f,  0.5f, 0.0f }, { 0.0f, 0.0f, -1.0f }, { 0.0f, 0.0f }, { 1.0f, 0.0f, 0.0f, 1.0f } });
		quadData.indices = { 0, 1, 2, 0, 2, 3 };
		m_billboardQuadMesh = createGpuMesh(quadData);

		buildDefaultPipelines();
		resize(m_width, m_height);
	}

	RenderPipeline& SceneRenderer::getPipeline(RenderPipelineType type) noexcept
	{
		switch (type)
		{
		case RenderPipelineType::Deferred:
			return m_deferredPipeline;
		case RenderPipelineType::Custom:
			return m_customPipeline;
		case RenderPipelineType::ForwardPlus:
		default:
			return m_forwardPipeline;
		}
	}

	const RenderPipeline& SceneRenderer::getPipeline(RenderPipelineType type) const noexcept
	{
		switch (type)
		{
		case RenderPipelineType::Deferred:
			return m_deferredPipeline;
		case RenderPipelineType::Custom:
			return m_customPipeline;
		case RenderPipelineType::ForwardPlus:
		default:
			return m_forwardPipeline;
		}
	}

	void SceneRenderer::buildDefaultPipelines()
	{
		m_forwardPipeline.clear();
		m_forwardPipeline.addLambdaPass("ShadowPass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			passShadows(s, v);
		});
		m_forwardPipeline.addLambdaPass("VolumetricFogPass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			passVolumetricFog(s, v);
		});
		m_forwardPipeline.addLambdaPass("DDGIPass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passDDGI(s);
		});
		m_forwardPipeline.addLambdaPass("DepthPrepass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			RHI::Viewport vp{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
			m_context->setViewport(vp);
			bool ssaoEnabled = s.environment.enableSSAO;
			if (v.overrides.overrideSSAO) ssaoEnabled = v.overrides.enableSSAO;
			if (ssaoEnabled || s.environment.enableContactShadows)
			{
				passDepthPrepass(s);
			}
		});
		m_forwardPipeline.addLambdaPass("SSAOPass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			passSSAO(s, v);
		});
		m_forwardPipeline.addLambdaPass("ForwardOpaquePass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passOpaque(s);
		});
		m_forwardPipeline.addLambdaPass("HiZBuildPass", [this](SceneRenderer&, const RenderScene&, const RenderView&) {
			passHiZBuild();
		});
		m_forwardPipeline.addLambdaPass("SSRPass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			passSSR(s, v);
		});
		m_forwardPipeline.addLambdaPass("DecalPass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passDecals(s, false);
		});
		m_forwardPipeline.addLambdaPass("TerrainPass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passTerrain(s);
		});
		m_forwardPipeline.addLambdaPass("FoliagePass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passFoliage(s);
		});
		m_forwardPipeline.addLambdaPass("SkyboxPass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passSkybox(s);
		});
		m_forwardPipeline.addLambdaPass("WaterPass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passWater(s);
		});
		m_forwardPipeline.addLambdaPass("TransparentPass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passTransparent(s);
		});
		m_forwardPipeline.addLambdaPass("SubsurfacePass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passSubsurface(s);
		});
		m_forwardPipeline.addLambdaPass("DebugLinesPass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			passDebugLines(s, v);
		});
		m_forwardPipeline.addLambdaPass("TAAPass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			passTAA(s, v);
		});
		m_forwardPipeline.addLambdaPass("MotionBlurPass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			passMotionBlur(s, v);
		});
		m_forwardPipeline.addLambdaPass("AutoExposurePass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			passAutoExposure(s, v);
		});
		m_forwardPipeline.addLambdaPass("BloomPass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passBloom(s);
		});
		m_forwardPipeline.addLambdaPass("PostProcessPass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			passPostProcess(s, v);
		});

		m_deferredPipeline.clear();
		m_deferredPipeline.addLambdaPass("ShadowPass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			passShadows(s, v);
		});
		m_deferredPipeline.addLambdaPass("VolumetricFogPass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			passVolumetricFog(s, v);
		});
		m_deferredPipeline.addLambdaPass("DDGIPass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passDDGI(s);
		});
		m_deferredPipeline.addLambdaPass("GBufferPass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			RHI::Viewport vp{ 0.0f, 0.0f, static_cast<float>(m_width), static_cast<float>(m_height), 0.0f, 1.0f };
			m_context->setViewport(vp);
			passGBuffer(s);
		});
		m_deferredPipeline.addLambdaPass("HiZBuildPass", [this](SceneRenderer&, const RenderScene&, const RenderView&) {
			passHiZBuild();
		});
		m_deferredPipeline.addLambdaPass("SSRPass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			passSSR(s, v);
		});
		m_deferredPipeline.addLambdaPass("DecalPass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passDecals(s, true);
		});
		m_deferredPipeline.addLambdaPass("SSAOPass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			passSSAO(s, v);
		});
		m_deferredPipeline.addLambdaPass("DeferredLightingPass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passDeferredLighting(s);
		});
		m_deferredPipeline.addLambdaPass("SubsurfacePass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passSubsurface(s);
		});
		m_deferredPipeline.addLambdaPass("TerrainPass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passTerrain(s);
		});
		m_deferredPipeline.addLambdaPass("FoliagePass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passFoliage(s);
		});
		m_deferredPipeline.addLambdaPass("SkyboxPass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passSkybox(s);
		});
		m_deferredPipeline.addLambdaPass("WaterPass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passWater(s);
		});
		m_deferredPipeline.addLambdaPass("TransparentPass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passTransparent(s);
		});
		m_deferredPipeline.addLambdaPass("DebugLinesPass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			passDebugLines(s, v);
		});
		m_deferredPipeline.addLambdaPass("TAAPass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			passTAA(s, v);
		});
		m_deferredPipeline.addLambdaPass("MotionBlurPass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			passMotionBlur(s, v);
		});
		m_deferredPipeline.addLambdaPass("AutoExposurePass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			passAutoExposure(s, v);
		});
		m_deferredPipeline.addLambdaPass("BloomPass", [this](SceneRenderer&, const RenderScene& s, const RenderView&) {
			passBloom(s);
		});
		m_deferredPipeline.addLambdaPass("PostProcessPass", [this](SceneRenderer&, const RenderScene& s, const RenderView& v) {
			passPostProcess(s, v);
		});
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

		RHI::GpuTextureDesc prevHdrDesc = hdrDesc;
		prevHdrDesc.bindFlags = RHI::BufferBindFlags::ShaderResource;
		auto prevHdrRes = m_device.createTexture(prevHdrDesc);
		if (prevHdrRes.isOk()) m_prevHdrTexture = *prevHdrRes;

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

		// 6. SSAO Full-Resolution Targets (R8_UNORM)
		RHI::GpuTextureDesc ssaoDesc{};
		ssaoDesc.width = width;
		ssaoDesc.height = height;
		ssaoDesc.format = RHI::PixelFormat::R8_UNORM;
		ssaoDesc.bindFlags = RHI::BufferBindFlags::RenderTarget | RHI::BufferBindFlags::ShaderResource;

		auto ssaoRawRes = m_device.createTexture(ssaoDesc);
		if (ssaoRawRes.isOk()) m_ssaoRawTexture = *ssaoRawRes;

		auto ssaoBlurRes = m_device.createTexture(ssaoDesc);
		if (ssaoBlurRes.isOk()) m_ssaoBlurredTexture = *ssaoBlurRes;

		// 7. Bloom Pyramid Targets (5 levels down and up, R16G16B16A16_FLOAT)
		uint32_t halfW = std::max((width + 1) / 2, 1u);
		uint32_t halfH = std::max((height + 1) / 2, 1u);
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

		// 8. 4-MRT G-Buffer Targets (NormalRoughness: R16G16B16A16_FLOAT, AlbedoMetallic: R8G8B8A8_UNORM)
		RHI::GpuTextureDesc nrDesc{};
		nrDesc.width = width;
		nrDesc.height = height;
		nrDesc.format = RHI::PixelFormat::R16G16B16A16_FLOAT;
		nrDesc.bindFlags = RHI::BufferBindFlags::RenderTarget | RHI::BufferBindFlags::ShaderResource;
		auto nrRes = m_device.createTexture(nrDesc);
		if (nrRes.isOk()) m_normalRoughnessTexture = *nrRes;

		RHI::GpuTextureDesc amDesc{};
		amDesc.width = width;
		amDesc.height = height;
		amDesc.format = RHI::PixelFormat::R8G8B8A8_UNORM;
		amDesc.bindFlags = RHI::BufferBindFlags::RenderTarget | RHI::BufferBindFlags::ShaderResource;
		auto amRes = m_device.createTexture(amDesc);
		if (amRes.isOk()) m_albedoMetallicTexture = *amRes;

		// 9. G-Buffer Emissive + MaterialAO Target (R16G16B16A16_FLOAT, full-res)
		RHI::GpuTextureDesc emAoDesc{};
		emAoDesc.width = width;
		emAoDesc.height = height;
		emAoDesc.format = RHI::PixelFormat::R16G16B16A16_FLOAT;
		emAoDesc.bindFlags = RHI::BufferBindFlags::RenderTarget | RHI::BufferBindFlags::ShaderResource;
		auto emAoRes = m_device.createTexture(emAoDesc);
		if (emAoRes.isOk()) m_gbufferEmissiveAOTexture = *emAoRes;

		// 10. Depth SRV Copy Target for Forward+ Contact Shadows (D32_FLOAT full-res)
		RHI::GpuTextureDesc depthCopyDesc{};
		depthCopyDesc.width = width;
		depthCopyDesc.height = height;
		depthCopyDesc.format = RHI::PixelFormat::D32_FLOAT;
		depthCopyDesc.bindFlags = RHI::BufferBindFlags::ShaderResource;
		auto depthCopyRes = m_device.createTexture(depthCopyDesc);
		if (depthCopyRes.isOk()) m_depthCopyTexture = *depthCopyRes;

		// 11. Refraction HDR Copy Target (R16G16B16A16_FLOAT full-res)
		RHI::GpuTextureDesc refrDesc{};
		refrDesc.width = width;
		refrDesc.height = height;
		refrDesc.format = RHI::PixelFormat::R16G16B16A16_FLOAT;
		refrDesc.bindFlags = RHI::BufferBindFlags::ShaderResource;
		auto refrRes = m_device.createTexture(refrDesc);
		if (refrRes.isOk()) m_refractionHdrCopyTexture = *refrRes;

		// 12. Velocity Target (R16G16_FLOAT full-res)
		RHI::GpuTextureDesc velDesc{};
		velDesc.width = width;
		velDesc.height = height;
		velDesc.format = RHI::PixelFormat::R16G16_FLOAT;
		velDesc.bindFlags = RHI::BufferBindFlags::RenderTarget | RHI::BufferBindFlags::ShaderResource;
		auto velRes = m_device.createTexture(velDesc);
		if (velRes.isOk()) m_velocityTexture = *velRes;

		// 13. TAA History & Resolved Targets (R16G16B16A16_FLOAT full-res)
		RHI::GpuTextureDesc taaDesc{};
		taaDesc.width = width;
		taaDesc.height = height;
		taaDesc.format = RHI::PixelFormat::R16G16B16A16_FLOAT;
		taaDesc.bindFlags = RHI::BufferBindFlags::RenderTarget | RHI::BufferBindFlags::ShaderResource;
		auto taaHist0Res = m_device.createTexture(taaDesc);
		if (taaHist0Res.isOk()) m_taaHistory[0] = *taaHist0Res;
		auto taaHist1Res = m_device.createTexture(taaDesc);
		if (taaHist1Res.isOk()) m_taaHistory[1] = *taaHist1Res;
		auto taaResolveRes = m_device.createTexture(taaDesc);
		if (taaResolveRes.isOk()) m_taaResolvedTexture = *taaResolveRes;
		m_taaHistoryIndex = 0;

		// 14. SSR Target (R16G16B16A16_FLOAT full-res)
		RHI::GpuTextureDesc ssrDesc{};
		ssrDesc.width = width;
		ssrDesc.height = height;
		ssrDesc.format = RHI::PixelFormat::R16G16B16A16_FLOAT;
		ssrDesc.bindFlags = RHI::BufferBindFlags::RenderTarget | RHI::BufferBindFlags::ShaderResource;
		auto ssrRes = m_device.createTexture(ssrDesc);
		if (ssrRes.isOk()) m_ssrTexture = *ssrRes;

		// 15. SSS Intermediate Target (R16G16B16A16_FLOAT full-res)
		RHI::GpuTextureDesc sssDesc = ssrDesc;
		auto sssRes = m_device.createTexture(sssDesc);
		if (sssRes.isOk()) m_sssIntermediateTexture = *sssRes;

		// 16. Hi-Z Reversed-Z Depth Pyramid (R32_FLOAT half-res)
		RHI::GpuTextureDesc hiZDesc{};
		hiZDesc.width = std::max((width + 1) / 2, 1u);
		hiZDesc.height = std::max((height + 1) / 2, 1u);
		hiZDesc.format = RHI::PixelFormat::R32_FLOAT;
		hiZDesc.bindFlags = RHI::BufferBindFlags::ShaderResource | RHI::BufferBindFlags::UnorderedAccess;
		auto hiZRes = m_device.createTexture(hiZDesc);
		if (hiZRes.isOk()) m_hiZTexture = *hiZRes;

		// 17. Motion Blur Velocity TileMax & NeighborMax (1/20th resolution)
		uint32_t tileW = std::max((width + 19) / 20, 1u);
		uint32_t tileH = std::max((height + 19) / 20, 1u);
		RHI::GpuTextureDesc tileDesc{};
		tileDesc.width = tileW;
		tileDesc.height = tileH;
		tileDesc.format = RHI::PixelFormat::R16G16_FLOAT;
		tileDesc.bindFlags = RHI::BufferBindFlags::ShaderResource | RHI::BufferBindFlags::UnorderedAccess;
		auto tileMaxRes = m_device.createTexture(tileDesc);
		if (tileMaxRes.isOk()) m_tileMaxTexture = *tileMaxRes;

		auto neighMaxRes = m_device.createTexture(tileDesc);
		if (neighMaxRes.isOk()) m_neighborMaxTexture = *neighMaxRes;

		// 18. Motion Blur Fullscreen Target (R16G16B16A16_FLOAT)
		RHI::GpuTextureDesc mbDesc{};
		mbDesc.width = width;
		mbDesc.height = height;
		mbDesc.format = RHI::PixelFormat::R16G16B16A16_FLOAT;
		mbDesc.bindFlags = RHI::BufferBindFlags::RenderTarget | RHI::BufferBindFlags::ShaderResource;
		auto mbRes = m_device.createTexture(mbDesc);
		if (mbRes.isOk()) m_motionBlurTexture = *mbRes;

		m_clustersBuilt = false;
	}
}
