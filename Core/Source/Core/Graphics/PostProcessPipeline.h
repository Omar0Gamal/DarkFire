#pragma once
#include "Prerequisites.h"
#include "Core/Math/Vector4d.h"
#include "Texture2D.h"
#include <d3d11.h>
#include <wrl.h>
#include <memory>

namespace Core
{
	struct PostProcessSettings
	{
		// Tonemapping & Color Grading
		bool tonemappingEnabled = true;
		float exposure = 1.0f;
		float contrast = 1.05f;
		float saturation = 1.1f;

		// Speed Radial Blur (for kart speed / boost effects)
		bool radialBlurEnabled = false;
		float radialBlurIntensity = 0.0f; // 0.0 = off, up to 1.0
		float radialBlurCenterX = 0.5f;
		float radialBlurCenterY = 0.5f;

		// Bloom
		bool bloomEnabled = true;
		float bloomThreshold = 0.85f;
		float bloomIntensity = 0.5f;

		// Vignette
		bool vignetteEnabled = true;
		float vignetteIntensity = 0.35f;
		float vignetteSmoothness = 0.5f;
	};

	__declspec(align(16))
	struct CBPostProcess
	{
		Vector4d params0; // x: exposure, y: contrast, z: saturation, w: tonemappingEnabled
		Vector4d params1; // x: radialBlurIntensity, y: radialBlurCenterX, z: radialBlurCenterY, w: radialBlurEnabled
		Vector4d params2; // x: bloomThreshold, y: bloomIntensity, z: bloomEnabled, w: unused
		Vector4d params3; // x: vignetteIntensity, y: vignetteSmoothness, z: vignetteEnabled, w: unused
	};

	class PostProcessPipeline
	{
	public:
		PostProcessPipeline(GraphicsEngine* gfx, int width, int height);
		~PostProcessPipeline() = default;

		void resize(int width, int height);

		// Bind HDR scene buffer before rendering 3D geometry
		void beginScene(const Vector4d& clearColor = Vector4d(0.05f, 0.05f, 0.08f, 1.0f));

		// Execute post-process pipeline and composite to destination render target (e.g. SwapChain backbuffer)
		void renderPostProcess(ID3D11RenderTargetView* targetRtv, int targetWidth, int targetHeight);

		PostProcessSettings& getSettings() { return m_settings; }
		const PostProcessSettings& getSettings() const { return m_settings; }
		void setSettings(const PostProcessSettings& settings) { m_settings = settings; }

		Texture2DPtr getHDRSceneTarget() const { return m_hdrSceneTarget; }
		Texture2DPtr getDepthStencilTarget() const { return m_depthStencilTarget; }

		bool isEnabled() const { return m_enabled; }
		void setEnabled(bool enabled) { m_enabled = enabled; }

	private:
		void initShaders();
		void createRenderTargets(int width, int height);

	private:
		GraphicsEngine* m_gfx = nullptr;
		int m_width = 0;
		int m_height = 0;
		bool m_enabled = true;
		PostProcessSettings m_settings;

		Texture2DPtr m_hdrSceneTarget;
		Texture2DPtr m_depthStencilTarget;
		Texture2DPtr m_bloomTarget;

		Microsoft::WRL::ComPtr<ID3D11VertexShader> m_fullscreenVS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_postProcessPS;
		Microsoft::WRL::ComPtr<ID3D11PixelShader> m_bloomExtractPS;
		Microsoft::WRL::ComPtr<ID3D11Buffer> m_cbPostProcess;
	};
}
