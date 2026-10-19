#pragma once
#include "Core/Math/Vector3d.h"
#include "Core/Math/Vector4d.h"
#include "Core/Math/Mat4x4.h"
#include "RenderPass.h"
#include <vector>
#include <d3d11.h>
#include <wrl.h>

namespace Core
{
	struct DebugVertex
	{
		Vector3d position;
		Vector4d color;
	};

	class DebugRenderer
	{
	public:
		static DebugRenderer& get();

		void init(ID3D11Device* device);
		void clear();

		void drawLine(const Vector3d& start, const Vector3d& end, const Vector4d& color = Vector4d(0, 1, 0, 1));
		void drawRay(const Vector3d& origin, const Vector3d& dir, float length, const Vector4d& color = Vector4d(1, 0, 0, 1));
		void drawWireBox(const Vector3d& center, const Vector3d& halfExtents, const Vector4d& color = Vector4d(0, 1, 1, 1));
		void drawWireSphere(const Vector3d& center, float radius, const Vector4d& color = Vector4d(1, 1, 0, 1), int segments = 16);
		void drawSpline(const std::vector<Vector3d>& points, const Vector4d& color = Vector4d(1, 0, 1, 1), bool loop = false);

		void render(GraphicsEngine* gfx, const RenderContext& ctx);

		bool isEnabled() const { return m_enabled; }
		void setEnabled(bool enabled) { m_enabled = enabled; }
		void toggle() { m_enabled = !m_enabled; }

	private:
		DebugRenderer() = default;
		~DebugRenderer() = default;

	private:
		bool m_enabled = false;
		bool m_initialized = false;
		std::vector<DebugVertex> m_lines;

		Microsoft::WRL::ComPtr<ID3D11Buffer> m_vertexBuffer;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_constantBuffer;
		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_vertexShader;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_pixelShader;
		Microsoft::WRL::ComPtr<ID3D11InputLayout> m_inputLayout;
		size_t m_bufferCapacity = 0;
	};
}
