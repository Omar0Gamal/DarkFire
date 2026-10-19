#pragma once
#include "RenderPass.h"
#include "GraphicsEngine.h"
#include "Core/Math/Vector2d.h"
#include "Core/Math/Vector3d.h"
#include "Core/Math/Vector4d.h"
#include "Core/Math/Mat4x4.h"
#include <wrl.h>
#include <d3d11.h>
#include <vector>

namespace Core
{
	struct ParticleVertex
	{
		Vector3d position;
		Vector2d uv;
		Vector4d color;
	};

	class ParticleRenderPass : public IRenderPass
	{
	public:
		ParticleRenderPass();
		virtual ~ParticleRenderPass() = default;

		void execute(GraphicsEngine* gfx, ECS* ecs, const RenderContext& ctx) override;

		int priority() const override { return 280; }
		const char* name() const override { return "ParticleRenderPass"; }

	private:
		void init(ID3D11Device* device);
		void createDefaultTexture(ID3D11Device* device);
		void ensureBuffers(ID3D11Device* device, size_t particleCount);

	private:
		bool m_initialized = false;

		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_indexBuffer;
		Microsoft::WRL::ComPtr<ID3D11ShaderResourceView> m_defaultTextureSRV;

		size_t m_vertexCapacity = 0;
		size_t m_indexCapacity = 0;
	};
}
