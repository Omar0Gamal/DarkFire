#include "ParticleRenderPass.h"
#include "GraphicsEngine.h"
#include "RenderStateManager.h"
#include "Texture2D.h"
#include "Core/Game/Component/TransformComponent.h"
#include "Core/Game/Component/ParticleEmitterComponent.h"
#include "Core/Game/ECS.h"
#include "Core/Log/Log.h"

#include <d3dcompiler.h>
#include <cmath>
#include <algorithm>

namespace Core
{
	static const char* s_particleShaderCode = R"(
cbuffer CBPerFrame : register(b0)
{
    float4x4 viewProj;
};

Texture2D particleTex : register(t0);
SamplerState particleSampler : register(s0);

struct VSInput
{
    float3 position : POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

struct VSOutput
{
    float4 position : SV_POSITION;
    float2 uv : TEXCOORD0;
    float4 color : COLOR0;
};

VSOutput VSMain(VSInput input)
{
    VSOutput output;
    output.position = mul(viewProj, float4(input.position, 1.0f));
    output.uv = input.uv;
    output.color = input.color;
    return output;
}

float4 PSMain(VSOutput input) : SV_TARGET
{
    float4 texColor = particleTex.Sample(particleSampler, input.uv);
    return input.color * texColor;
}
)";

	ParticleRenderPass::ParticleRenderPass()
	{
	}

	void ParticleRenderPass::init(ID3D11Device* device)
	{
		if (m_initialized || !device)
			return;

		ID3DBlob* vsBlob = nullptr;
		ID3DBlob* psBlob = nullptr;
		ID3DBlob* errorBlob = nullptr;

		size_t codeLen = strlen(s_particleShaderCode);
		HRESULT hr = D3DCompile(s_particleShaderCode, codeLen, "ParticleVS", nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob) { darklog.error(L"Failed to compile ParticleVS"); errorBlob->Release(); }
			return;
		}
		device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_vertexShader.ReleaseAndGetAddressOf());

		hr = D3DCompile(s_particleShaderCode, codeLen, "ParticlePS", nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &psBlob, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob) { darklog.error(L"Failed to compile ParticlePS"); errorBlob->Release(); }
			if (vsBlob) vsBlob->Release();
			return;
		}
		device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_pixelShader.ReleaseAndGetAddressOf());
		psBlob->Release();

		// Input Layout
		D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		device->CreateInputLayout(layoutDesc, 3, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), m_inputLayout.ReleaseAndGetAddressOf());
		vsBlob->Release();

		// Constant Buffer
		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.ByteWidth = sizeof(Mat4x4);
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		device->CreateBuffer(&cbDesc, nullptr, m_constantBuffer.ReleaseAndGetAddressOf());

		// Procedural glowing particle texture
		createDefaultTexture(device);

		// Allocate initial buffers (1024 particles)
		ensureBuffers(device, 1024);

		m_initialized = true;
	}

	void ParticleRenderPass::createDefaultTexture(ID3D11Device* device)
	{
		const int texDim = 32;
		std::vector<uint32_t> pixels(texDim * texDim);
		float center = (texDim - 1) * 0.5f;
		float maxRadius = center;

		for (int y = 0; y < texDim; ++y)
		{
			for (int x = 0; x < texDim; ++x)
			{
				float dx = (x - center) / maxRadius;
				float dy = (y - center) / maxRadius;
				float distSq = dx * dx + dy * dy;
				float alpha = 0.0f;
				if (distSq < 1.0f)
				{
					alpha = std::exp(-distSq * 3.5f);
				}
				uint8_t a = static_cast<uint8_t>((std::clamp)(alpha * 255.0f, 0.0f, 255.0f));
				pixels[y * texDim + x] = (static_cast<uint32_t>(a) << 24) | 0x00FFFFFF;
			}
		}

		D3D11_TEXTURE2D_DESC texDesc = {};
		texDesc.Width = texDim;
		texDesc.Height = texDim;
		texDesc.MipLevels = 1;
		texDesc.ArraySize = 1;
		texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		texDesc.SampleDesc.Count = 1;
		texDesc.Usage = D3D11_USAGE_IMMUTABLE;
		texDesc.BindFlags = D3D11_BIND_SHADER_RESOURCE;

		D3D11_SUBRESOURCE_DATA subData = {};
		subData.pSysMem = pixels.data();
		subData.SysMemPitch = texDim * sizeof(uint32_t);

		Microsoft::WRL::ComPtr<ID3D11Texture2D> tex;
		if (SUCCEEDED(device->CreateTexture2D(&texDesc, &subData, tex.GetAddressOf())))
		{
			device->CreateShaderResourceView(tex.Get(), nullptr, m_defaultTextureSRV.ReleaseAndGetAddressOf());
		}
	}

	void ParticleRenderPass::ensureBuffers(ID3D11Device* device, size_t particleCount)
	{
		if (particleCount <= m_vertexCapacity / 4 && m_vertexBuffer && m_indexBuffer)
			return;

		size_t newCount = (std::max)(particleCount, size_t(1024));
		newCount = (std::max)(newCount, m_vertexCapacity / 2);

		// Dynamic Vertex Buffer (4 vertices per particle)
		m_vertexCapacity = newCount * 4;
		D3D11_BUFFER_DESC vbDesc = {};
		vbDesc.ByteWidth = static_cast<UINT>(m_vertexCapacity * sizeof(ParticleVertex));
		vbDesc.Usage = D3D11_USAGE_DYNAMIC;
		vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
		vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		device->CreateBuffer(&vbDesc, nullptr, m_vertexBuffer.ReleaseAndGetAddressOf());

		// Static Index Buffer (6 indices per particle: 0, 1, 2, 0, 2, 3)
		m_indexCapacity = newCount * 6;
		std::vector<uint32_t> indices(m_indexCapacity);
		for (size_t i = 0; i < newCount; ++i)
		{
			uint32_t vBase = static_cast<uint32_t>(i * 4);
			size_t iBase = i * 6;
			indices[iBase + 0] = vBase + 0;
			indices[iBase + 1] = vBase + 1;
			indices[iBase + 2] = vBase + 2;
			indices[iBase + 3] = vBase + 0;
			indices[iBase + 4] = vBase + 2;
			indices[iBase + 5] = vBase + 3;
		}

		D3D11_BUFFER_DESC ibDesc = {};
		ibDesc.ByteWidth = static_cast<UINT>(m_indexCapacity * sizeof(uint32_t));
		ibDesc.Usage = D3D11_USAGE_IMMUTABLE;
		ibDesc.BindFlags = D3D11_BIND_INDEX_BUFFER;

		D3D11_SUBRESOURCE_DATA ibData = {};
		ibData.pSysMem = indices.data();
		device->CreateBuffer(&ibDesc, &ibData, m_indexBuffer.ReleaseAndGetAddressOf());
	}

	void ParticleRenderPass::execute(GraphicsEngine* gfx, ECS* ecs, const RenderContext& ctx)
	{
		if (!gfx || !ecs)
			return;

		ID3D11Device* device = gfx->getD3DDevice();
		ID3D11DeviceContext* context = gfx->getImmediateContext();
		RenderStateManager* stateMgr = gfx->getRenderStateManager();

		if (!m_initialized)
		{
			init(device);
		}

		if (!m_vertexShader || !m_pixelShader || !m_inputLayout || !m_constantBuffer)
			return;

		// Extract camera basis vectors from view matrix
		Vector3d camRight(ctx.view.m[0][0], ctx.view.m[1][0], ctx.view.m[2][0]);
		Vector3d camUp(ctx.view.m[0][1], ctx.view.m[1][1], ctx.view.m[2][1]);

		// Separate emitters by blend mode
		struct BatchData
		{
			std::vector<ParticleVertex> vertices;
			ID3D11ShaderResourceView* textureSRV = nullptr;
		};

		BatchData additiveBatch;
		BatchData alphaBatch;

		ecs->ForEach<ParticleEmitterComponent>(
			[&](ParticleEmitterComponent& emitter)
			{
				if (emitter.particles.empty())
					return;

				BatchData& batch = (emitter.blendMode == ParticleBlendMode::Additive) ? additiveBatch : alphaBatch;
				ID3D11ShaderResourceView* srv = emitter.texture ? emitter.texture->getSRV() : m_defaultTextureSRV.Get();
				if (!batch.textureSRV)
					batch.textureSRV = srv;

				for (const auto& p : emitter.particles)
				{
					if (!p.isAlive())
						continue;

					float halfSize = p.size * 0.5f;
					Vector3d r = camRight * halfSize;
					Vector3d u = camUp * halfSize;

					Vector3d c0 = p.position - r - u;
					Vector3d c1 = p.position + r - u;
					Vector3d c2 = p.position + r + u;
					Vector3d c3 = p.position - r + u;

					batch.vertices.push_back({ c0, Vector2d(0, 1), p.color });
					batch.vertices.push_back({ c1, Vector2d(1, 1), p.color });
					batch.vertices.push_back({ c2, Vector2d(1, 0), p.color });
					batch.vertices.push_back({ c3, Vector2d(0, 0), p.color });
				}
			});

		size_t totalVertices = additiveBatch.vertices.size() + alphaBatch.vertices.size();
		if (totalVertices == 0)
			return;

		// Ensure buffer space
		ensureBuffers(device, totalVertices / 4);

		// Calculate ViewProj matrix
		Mat4x4 viewProj = ctx.view;
		viewProj *= ctx.projection;

		D3D11_MAPPED_SUBRESOURCE mapped = {};
		if (SUCCEEDED(context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &viewProj, sizeof(Mat4x4));
			context->Unmap(m_constantBuffer.Get(), 0);
		}

		// Setup pipeline
		UINT stride = sizeof(ParticleVertex);
		UINT offset = 0;
		ID3D11Buffer* vb = m_vertexBuffer.Get();
		context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
		context->IASetIndexBuffer(m_indexBuffer.Get(), DXGI_FORMAT_R32_UINT, 0);
		context->IASetInputLayout(m_inputLayout.Get());
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

		ID3D11Buffer* cb = m_constantBuffer.Get();
		context->VSSetConstantBuffers(0, 1, &cb);
		context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
		context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

		stateMgr->applyRasterizerState(RasterizerStateType::CullNone);
		stateMgr->applyDepthStencilState(DepthStencilStateType::ReadOnly);
		stateMgr->applySamplerState(SamplerStateType::LinearClamp, 0, false, true);

		// Helper to render a batch
		auto renderBatch = [&](BatchData& batch, BlendStateType blendState)
		{
			if (batch.vertices.empty())
				return;

			if (SUCCEEDED(context->Map(m_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
			{
				memcpy(mapped.pData, batch.vertices.data(), batch.vertices.size() * sizeof(ParticleVertex));
				context->Unmap(m_vertexBuffer.Get(), 0);
			}

			ID3D11ShaderResourceView* srv = batch.textureSRV ? batch.textureSRV : m_defaultTextureSRV.Get();
			context->PSSetShaderResources(0, 1, &srv);

			stateMgr->applyBlendState(blendState);

			UINT indexCount = static_cast<UINT>((batch.vertices.size() / 4) * 6);
			context->DrawIndexed(indexCount, 0, 0);
		};

		// 1. Draw Additive (Sparks, Flames)
		renderBatch(additiveBatch, BlendStateType::Additive);

		// 2. Draw AlphaBlend (Smoke, Dust)
		renderBatch(alphaBatch, BlendStateType::AlphaBlend);
	}
}
