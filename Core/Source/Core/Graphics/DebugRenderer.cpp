#include "DebugRenderer.h"
#include "GraphicsEngine.h"
#include "RenderStateManager.h"
#include "Core/Log/Log.h"

#include <d3dcompiler.h>
#include <cmath>

namespace Core
{
	static const char* s_debugLineShader = R"(
		cbuffer cbDebug : register(b0)
		{
			float4x4 viewProj;
		};

		struct VSInput
		{
			float3 position : POSITION;
			float4 color : COLOR;
		};

		struct VSOutput
		{
			float4 position : SV_POSITION;
			float4 color : COLOR;
		};

		VSOutput VSMain(VSInput input)
		{
			VSOutput output;
			output.position = mul(viewProj, float4(input.position, 1.0));
			output.color = input.color;
			return output;
		}

		float4 PSMain(VSOutput input) : SV_TARGET
		{
			return input.color;
		}
	)";

	DebugRenderer& DebugRenderer::get()
	{
		static DebugRenderer instance;
		return instance;
	}

	void DebugRenderer::init(ID3D11Device* device)
	{
		if (m_initialized || !device)
			return;

		ID3DBlob* vsBlob = nullptr;
		ID3DBlob* psBlob = nullptr;
		ID3DBlob* errorBlob = nullptr;

		size_t codeLen = strlen(s_debugLineShader);
		HRESULT hr = D3DCompile(s_debugLineShader, codeLen, "DebugVS", nullptr, nullptr, "VSMain", "vs_5_0", 0, 0, &vsBlob, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob) { darklog.error(L"Failed to compile DebugRenderer VS"); errorBlob->Release(); }
			return;
		}
		device->CreateVertexShader(vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), nullptr, m_vertexShader.ReleaseAndGetAddressOf());

		hr = D3DCompile(s_debugLineShader, codeLen, "DebugPS", nullptr, nullptr, "PSMain", "ps_5_0", 0, 0, &psBlob, &errorBlob);
		if (FAILED(hr))
		{
			if (errorBlob) { darklog.error(L"Failed to compile DebugRenderer PS"); errorBlob->Release(); }
			if (vsBlob) vsBlob->Release();
			return;
		}
		device->CreatePixelShader(psBlob->GetBufferPointer(), psBlob->GetBufferSize(), nullptr, m_pixelShader.ReleaseAndGetAddressOf());
		psBlob->Release();

		// Input Layout
		D3D11_INPUT_ELEMENT_DESC layoutDesc[] = {
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D11_INPUT_PER_VERTEX_DATA, 0 },
			{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D11_INPUT_PER_VERTEX_DATA, 0 },
		};
		device->CreateInputLayout(layoutDesc, 2, vsBlob->GetBufferPointer(), vsBlob->GetBufferSize(), m_inputLayout.ReleaseAndGetAddressOf());
		vsBlob->Release();

		// Constant Buffer
		D3D11_BUFFER_DESC cbDesc = {};
		cbDesc.ByteWidth = sizeof(Mat4x4);
		cbDesc.Usage = D3D11_USAGE_DYNAMIC;
		cbDesc.BindFlags = D3D11_BIND_CONSTANT_BUFFER;
		cbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
		device->CreateBuffer(&cbDesc, nullptr, m_constantBuffer.ReleaseAndGetAddressOf());

		m_initialized = true;
	}

	void DebugRenderer::clear()
	{
		m_lines.clear();
	}

	void DebugRenderer::drawLine(const Vector3d& start, const Vector3d& end, const Vector4d& color)
	{
		m_lines.push_back({ start, color });
		m_lines.push_back({ end, color });
	}

	void DebugRenderer::drawRay(const Vector3d& origin, const Vector3d& dir, float length, const Vector4d& color)
	{
		drawLine(origin, origin + (Vector3d::normalize(dir) * length), color);
	}

	void DebugRenderer::drawWireBox(const Vector3d& center, const Vector3d& halfExtents, const Vector4d& color)
	{
		Vector3d min = center - halfExtents;
		Vector3d max = center + halfExtents;

		// 8 vertices
		Vector3d p0(min.x, min.y, min.z);
		Vector3d p1(max.x, min.y, min.z);
		Vector3d p2(max.x, max.y, min.z);
		Vector3d p3(min.x, max.y, min.z);
		Vector3d p4(min.x, min.y, max.z);
		Vector3d p5(max.x, min.y, max.z);
		Vector3d p6(max.x, max.y, max.z);
		Vector3d p7(min.x, max.y, max.z);

		// Bottom face
		drawLine(p0, p1, color); drawLine(p1, p2, color); drawLine(p2, p3, color); drawLine(p3, p0, color);
		// Top face
		drawLine(p4, p5, color); drawLine(p5, p6, color); drawLine(p6, p7, color); drawLine(p7, p4, color);
		// Vertical edges
		drawLine(p0, p4, color); drawLine(p1, p5, color); drawLine(p2, p6, color); drawLine(p3, p7, color);
	}

	void DebugRenderer::drawWireSphere(const Vector3d& center, float radius, const Vector4d& color, int segments)
	{
		if (segments < 4) segments = 4;
		const float step = 6.2831853f / static_cast<float>(segments);

		// XY, XZ, YZ circles
		for (int i = 0; i < segments; ++i)
		{
			float a0 = i * step;
			float a1 = (i + 1) * step;

			// XY
			drawLine(
				center + Vector3d(std::cos(a0) * radius, std::sin(a0) * radius, 0),
				center + Vector3d(std::cos(a1) * radius, std::sin(a1) * radius, 0), color);

			// XZ
			drawLine(
				center + Vector3d(std::cos(a0) * radius, 0, std::sin(a0) * radius),
				center + Vector3d(std::cos(a1) * radius, 0, std::sin(a1) * radius), color);

			// YZ
			drawLine(
				center + Vector3d(0, std::cos(a0) * radius, std::sin(a0) * radius),
				center + Vector3d(0, std::cos(a1) * radius, std::sin(a1) * radius), color);
		}
	}

	void DebugRenderer::drawSpline(const std::vector<Vector3d>& points, const Vector4d& color, bool loop)
	{
		if (points.size() < 2) return;
		for (size_t i = 0; i < points.size() - 1; ++i)
		{
			drawLine(points[i], points[i + 1], color);
		}
		if (loop && points.size() > 2)
		{
			drawLine(points.back(), points.front(), color);
		}
	}

	void DebugRenderer::render(GraphicsEngine* gfx, const RenderContext& ctx)
	{
		if (!m_enabled || m_lines.empty() || !gfx)
			return;

		ID3D11Device* device = gfx->getD3DDevice();
		ID3D11DeviceContext* context = gfx->getImmediateContext();
		RenderStateManager* stateMgr = gfx->getRenderStateManager();

		if (!m_initialized)
		{
			init(device);
		}

		if (!m_vertexShader || !m_pixelShader || !m_inputLayout)
			return;

		// Expand vertex buffer if needed
		size_t requiredSize = m_lines.size() * sizeof(DebugVertex);
		if (!m_vertexBuffer || m_bufferCapacity < requiredSize)
		{
			m_bufferCapacity = (std::max)(requiredSize * 2, size_t(1024 * sizeof(DebugVertex)));
			D3D11_BUFFER_DESC vbDesc = {};
			vbDesc.ByteWidth = static_cast<UINT>(m_bufferCapacity);
			vbDesc.Usage = D3D11_USAGE_DYNAMIC;
			vbDesc.BindFlags = D3D11_BIND_VERTEX_BUFFER;
			vbDesc.CPUAccessFlags = D3D11_CPU_ACCESS_WRITE;
			device->CreateBuffer(&vbDesc, nullptr, m_vertexBuffer.ReleaseAndGetAddressOf());
		}

		// Upload vertices
		D3D11_MAPPED_SUBRESOURCE mapped = {};
		if (SUCCEEDED(context->Map(m_vertexBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, m_lines.data(), requiredSize);
			context->Unmap(m_vertexBuffer.Get(), 0);
		}

		// Calculate viewProj
		Mat4x4 viewProj = ctx.view;
		viewProj *= ctx.projection;

		if (SUCCEEDED(context->Map(m_constantBuffer.Get(), 0, D3D11_MAP_WRITE_DISCARD, 0, &mapped)))
		{
			memcpy(mapped.pData, &viewProj, sizeof(Mat4x4));
			context->Unmap(m_constantBuffer.Get(), 0);
		}

		// Setup pipeline
		UINT stride = sizeof(DebugVertex);
		UINT offset = 0;
		ID3D11Buffer* vb = m_vertexBuffer.Get();
		context->IASetVertexBuffers(0, 1, &vb, &stride, &offset);
		context->IASetInputLayout(m_inputLayout.Get());
		context->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_LINELIST);

		ID3D11Buffer* cb = m_constantBuffer.Get();
		context->VSSetConstantBuffers(0, 1, &cb);
		context->VSSetShader(m_vertexShader.Get(), nullptr, 0);
		context->PSSetShader(m_pixelShader.Get(), nullptr, 0);

		stateMgr->applyRasterizerState(RasterizerStateType::CullNone);
		stateMgr->applyBlendState(BlendStateType::Opaque);
		stateMgr->applyDepthStencilState(DepthStencilStateType::Default);

		context->Draw(static_cast<UINT>(m_lines.size()), 0);

		clear();
	}
}
