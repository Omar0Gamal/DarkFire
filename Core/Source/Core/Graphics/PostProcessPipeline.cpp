#include "PostProcessPipeline.h"
#include "GraphicsEngine.h"
#include "RenderStateManager.h"
#include "Core/Log/Log.h"

#include <d3dcompiler.h>

namespace Core
{
	static const char* s_postProcessShaderCode = R"(
		cbuffer cbPostProcess : register(b0)
		{
			float4 params0; // x: exposure, y: contrast, z: saturation, w: tonemappingEnabled
			float4 params1; // x: radialBlurIntensity, y: radialBlurCenterX, z: radialBlurCenterY, w: radialBlurEnabled
			float4 params2; // x: bloomThreshold, y: bloomIntensity, z: bloomEnabled, w: unused
			float4 params3; // x: vignetteIntensity, y: vignetteSmoothness, z: vignetteEnabled, w: unused
		};

		Texture2D sceneTexture : register(t0);
		Texture2D bloomTexture : register(t1);
		SamplerState linearSampler : register(s0);

		struct VSOutput
		{
			float4 position : SV_POSITION;
			float2 texcoord : TEXCOORD0;
		};

		VSOutput VSMain(uint id : SV_VertexID)
		{
			VSOutput output;
			output.texcoord = float2((id << 1) & 2, id & 2);
			output.position = float4(output.texcoord * float2(2.0, -2.0) + float2(-1.0, 1.0), 0.0, 1.0);
			return output;
		}

		float3 ACESFilm(float3 x)
		{
			float a = 2.51f;
			float b = 0.03f;
			float c = 2.43f;
			float d = 0.59f;
			float e = 0.14f;
			return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
		}

		float4 PSMain(VSOutput input) : SV_TARGET
		{
			float2 uv = input.texcoord;
			float3 color = float3(0, 0, 0);

			// 1. Speed Radial Blur (e.g. for kart boost)
			if (params1.w > 0.5f && params1.x > 0.001f)
			{
				float2 center = params1.yz;
				float2 dir = uv - center;
				float dist = length(dir);
				float intensity = params1.x * saturate(dist * 1.5f);
				
				const int SAMPLES = 8;
				float3 blurCol = float3(0, 0, 0);
				for (int i = 0; i < SAMPLES; ++i)
				{
					float scale = 1.0f - intensity * (float(i) / float(SAMPLES - 1));
					blurCol += sceneTexture.Sample(linearSampler, center + dir * scale).rgb;
				}
				color = blurCol / float(SAMPLES);
			}
			else
			{
				color = sceneTexture.Sample(linearSampler, uv).rgb;
			}

			// 2. Bloom additive blend
			if (params2.z > 0.5f)
			{
				float3 bloom = bloomTexture.Sample(linearSampler, uv).rgb;
				color += bloom * params2.y;
			}

			// 3. Exposure
			color *= params0.x;

			// 4. ACES Filmic Tonemapping
			if (params0.w > 0.5f)
			{
				color = ACESFilm(color);
			}

			// 5. Contrast & Saturation (Color Grading)
			color = saturate((color - 0.5f) * params0.y + 0.5f);
			float luma = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
			color = saturate(lerp(float3(luma, luma, luma), color, params0.z));

			// 6. Vignette
			if (params3.z > 0.5f)
			{
				float2 coord = (uv - 0.5f) * 2.0f;
				float dist = dot(coord, coord);
				float vignette = saturate(1.0f - dist * params3.x);
				color *= vignette;
			}

			return float4(color, 1.0f);
		}

		float4 PSBloomExtract(VSOutput input) : SV_TARGET
		{
			float3 color = sceneTexture.Sample(linearSampler, input.texcoord).rgb;
			float brightness = dot(color, float3(0.2126f, 0.7152f, 0.0722f));
			float threshold = params2.x;
			if (brightness > threshold)
			{
				return float4((color - threshold) / (1.0f - min(threshold, 0.999f)), 1.0f);
			}
			return float4(0.0f, 0.0f, 0.0f, 1.0f);
		}
	)";

	PostProcessPipeline::PostProcessPipeline(GraphicsEngine* gfx, int width, int height)
		: m_gfx(gfx), m_width(width), m_height(height)
	{
		initShaders();
		createRenderTargets(width, height);
	}

	void PostProcessPipeline::initShaders()
	{
		ID3D11Device* device = m_gfx->getD3DDevice();
		ID3DBlob* vsBlob = nullptr;
		ID3DBlob* psBlob = nullptr;
		ID3DBlob* bloomBlob = nullptr;
		ID3DBlob* errorBlob = nullptr;

		size_t codeLen = strlen(s_postProcessShaderCode);

		// 1. Vertex Shader
		HRESULT hr = D3DCompile(s_postProcessShaderCode, codeLen, "PostProcessVS", nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob)
			{
				darklog.error(L"Failed to compile PostProcess VS");
				errorBlob->Release();
			}
			return;
		}
		device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_fullscreenVS.ReleaseAndGetAddressOf());
		vsBlob->Release();

		// 2. Main Post-Process Pixel Shader
		hr = D3DCompile(s_postProcessShaderCode, codeLen, "PostProcessPS", nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &psBlob, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob)
			{
				darklog.error(L"Failed to compile PostProcess PS");
				errorBlob->Release();
			}
			return;
		}
		device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_postProcessPS.ReleaseAndGetAddressOf());
		psBlob->Release();

		// 3. Bloom Extraction Pixel Shader
		hr = D3DCompile(s_postProcessShaderCode, codeLen, "BloomExtractPS", nullptr, nullptr, "PSBloomExtract", "ps_5_0", 0, 0, &bloomBlob, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob)
			{
				darklog.error(L"Failed to compile BloomExtract PS");
				errorBlob->Release();
			}
			return;
		}
		device->CreatePixelShader(bloomBlob->GetBufferPointer(), bloomBlob->GetBufferSize(), nullptr, m_bloomExtractPS.ReleaseAndGetAddressOf());
		bloomBlob->Release();

		// 4. Constant Buffer
		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.ByteWidth = sizeof(CBPostProcess);
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		device->CreateBuffer(&cbDesc, nullptr, m_cbPostProcess.ReleaseAndGetAddressOf());
	}

	void PostProcessPipeline::createRenderTargets(int width, int height)
	{
		m_width = (width > 0) ? width : 1280;
		m_height = (height > 0) ? height : 720;

		Box targetSize;
		targetSize.width = m_width;
		targetSize.height = m_height;

		// HDR Scene Target
		Texture2DDesc hdrDesc;
		hdrDesc.size = targetSize;
		hdrDesc.type = TextureType::RenderTargetHDR;
		m_hdrSceneTarget = std::make_shared<Texture2D>(hdrDesc, m_gfx);

		// Depth Stencil Target
		Texture2DDesc depthDesc;
		depthDesc.size = targetSize;
		depthDesc.type = TextureType::DepthStencil;
		m_depthStencilTarget = std::make_shared<Texture2D>(depthDesc, m_gfx);

		// Bloom Target (Quarter resolution for efficient glow extraction)
		Box bloomSize;
		bloomSize.width = (std::max)(1, m_width / 2);
		bloomSize.height = (std::max)(1, m_height / 2);

		Texture2DDesc bloomDesc;
		bloomDesc.size = bloomSize;
		bloomDesc.type = TextureType::RenderTargetHDR;
		m_bloomTarget = std::make_shared<Texture2D>(bloomDesc, m_gfx);
	}

	void PostProcessPipeline::resize(int width, int height)
	{
		if (width != m_width || height != m_height)
		{
			createRenderTargets(width, height);
		}
	}

	void PostProcessPipeline::beginScene(const Vector4d& clearColor)
	{
		if (!m_hdrSceneTarget || !m_depthStencilTarget)
			return;

		m_gfx->setRenderTarget(m_hdrSceneTarget, m_depthStencilTarget);
		m_gfx->clearColor(m_hdrSceneTarget, clearColor);
		m_gfx->clearDepthStencil(m_depthStencilTarget);
		m_gfx->setViewportSize(m_width, m_height);
	}

	void PostProcessPipeline::renderPostProcess(ID3D11RenderTargetView* targetRtv, int targetWidth, int targetHeight)
	{
		if (!m_enabled || !targetRtv || !m_fullscreenVS || !m_postProcessPS)
			return;

		ID3D11DeviceContext* ctx = m_gfx->getImmediateContext();
		RenderStateManager* stateMgr = m_gfx->getRenderStateManager();

		// 1. Unbind render target before reading as SRV
		ID3D11RenderTargetView* nullRTV[1] = { nullptr };
		ctx->OMSetRenderTargets(1, nullRTV, nullptr);

		// 2. Bloom Extraction Pass (if enabled)
		if (m_settings.bloomEnabled && m_bloomTarget && m_bloomExtractPS)
		{
			// Render into bloom target
			ID3D11RenderTargetView* bloomRtv = m_bloomTarget->getRTV();
			ctx->OMSetRenderTargets(1, &bloomRtv, nullptr);

			D3D11_VIEWPORT bloomVp = {};
			bloomVp.Width = static_cast<FLOAT>(m_bloomTarget->getDesc().size.width);
			bloomVp.Height = static_cast<FLOAT>(m_bloomTarget->getDesc().size.height);
			bloomVp.MinDepth = 0.0f;
			bloomVp.MaxDepth = 1.0f;
			ctx->RSSetViewports(1, &bloomVp);

			// Update bloom parameters
			CBPostProcess cbData = {};
			cbData.params2 = Vector4d(m_settings.bloomThreshold, m_settings.bloomIntensity, 1.0f, 0.0f);

			D3D11_MAPPED_SUBRESOURCE mapped = {};
			if (SUCCEEDED(ctx->Map(m_cbPostProcess.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
			{
				memcpy(mapped.pData, &cbData, sizeof(CBPostProcess));
				ctx->Unmap(m_cbPostProcess.Get(), 0);
			}

			ID3D11Buffer* cbBuf = m_cbPostProcess.Get();
			ctx->PSSetConstantBuffers(0, 1, &cbBuf);

			ID3D11ShaderResourceView* sceneSrv = m_hdrSceneTarget->getSRV();
			ctx->PSSetShaderResources(0, 1, &sceneSrv);
			stateMgr->applySamplerState(SamplerStateType::LinearClamp, 0, false, true);

			ctx->IASetInputLayout(nullptr);
			ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
			ctx->VSSetShader(m_fullscreenVS.Get(), nullptr, 0);
			ctx->PSSetShader(m_bloomExtractPS.Get(), nullptr, 0);

			stateMgr->applyRasterizerState(RasterizerStateType::CullNone);
			stateMgr->applyBlendState(BlendStateType::Opaque);
			stateMgr->applyDepthStencilState(DepthStencilStateType::Disabled);

			ctx->Draw(3, 0);

			// Unbind SRV
			ID3D11ShaderResourceView* nullSRV[1] = { nullptr };
			ctx->PSSetShaderResources(0, 1, nullSRV);
		}

		// 3. Final Composite Pass to destination RTV (e.g. SwapChain backbuffer)
		ctx->OMSetRenderTargets(1, &targetRtv, nullptr);

		D3D11_VIEWPORT outVp = {};
		outVp.Width = static_cast<FLOAT>(targetWidth);
		outVp.Height = static_cast<FLOAT>(targetHeight);
		outVp.MinDepth = 0.0f;
		outVp.MaxDepth = 1.0f;
		ctx->RSSetViewports(1, &outVp);

		CBPostProcess cbData = {};
		cbData.params0 = Vector4d(m_settings.exposure, m_settings.contrast, m_settings.saturation, m_settings.tonemappingEnabled ? 1.0f : 0.0f);
		cbData.params1 = Vector4d(m_settings.radialBlurIntensity, m_settings.radialBlurCenterX, m_settings.radialBlurCenterY, m_settings.radialBlurEnabled ? 1.0f : 0.0f);
		cbData.params2 = Vector4d(m_settings.bloomThreshold, m_settings.bloomIntensity, m_settings.bloomEnabled ? 1.0f : 0.0f, 0.0f);
		cbData.params3 = Vector4d(m_settings.vignetteIntensity, m_settings.vignetteSmoothness, m_settings.vignetteEnabled ? 1.0f : 0.0f, 0.0f);

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		if (SUCCEEDED(ctx->Map(m_cbPostProcess.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &cbData, sizeof(CBPostProcess));
			ctx->Unmap(m_cbPostProcess.Get(), 0);
		}

		ID3D11Buffer* cbBuf = m_cbPostProcess.Get();
		ctx->PSSetConstantBuffers(0, 1, &cbBuf);

		ID3D11ShaderResourceView* srvs[2] = {
			m_hdrSceneTarget->getSRV(),
			m_bloomTarget ? m_bloomTarget->getSRV() : nullptr
		};
		ctx->PSSetShaderResources(0, 2, srvs);
		stateMgr->applySamplerState(SamplerStateType::LinearClamp, 0, false, true);

		ctx->IASetInputLayout(nullptr);
		ctx->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
		ctx->VSSetShader(m_fullscreenVS.Get(), nullptr, 0);
		ctx->PSSetShader(m_postProcessPS.Get(), nullptr, 0);

		stateMgr->applyRasterizerState(RasterizerStateType::CullNone);
		stateMgr->applyBlendState(BlendStateType::Opaque);
		stateMgr->applyDepthStencilState(DepthStencilStateType::Disabled);

		ctx->Draw(3, 0);

		// Unbind textures from slots so future passes can write to them
		ID3D11ShaderResourceView* nullSRVs[2] = { nullptr, nullptr };
		ctx->PSSetShaderResources(0, 2, nullSRVs);
	}
}
