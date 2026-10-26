#include "Engine/Renderer/SceneRenderer.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/Core/VFS/AssetPathResolver.h"
#include <algorithm>
#include <cmath>
#include <thread>
#include <vector>

namespace Core::Renderer
{
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

	static float RadicalInverse_VdC(uint32_t bits)
	{
		bits = (bits << 16u) | (bits >> 16u);
		bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
		bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
		bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
		bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
		return static_cast<float>(bits) * 2.3283064365386963e-10f;
	}

	static Math::Vector2 Hammersley(uint32_t i, uint32_t N)
	{
		return Math::Vector2(static_cast<float>(i) / static_cast<float>(N), RadicalInverse_VdC(i));
	}

	static Math::Vector3 ImportanceSampleGGX(Math::Vector2 Xi, Math::Vector3 N, float roughness)
	{
		float a = roughness * roughness;
		float phi = 2.0f * 3.14159265359f * Xi.x;
		float cosTheta = std::sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
		float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));

		Math::Vector3 H(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta);
		Math::Vector3 up = std::abs(N.z) < 0.999f ? Math::Vector3(0.0f, 0.0f, 1.0f) : Math::Vector3(1.0f, 0.0f, 0.0f);
		Math::Vector3 tangent = up.Cross(N);
		tangent.Normalize();
		Math::Vector3 bitangent = N.Cross(tangent);

		return tangent * H.x + bitangent * H.y + N * H.z;
	}

	static Math::Vector2 IntegrateBRDF(float NoV, float roughness)
	{
		Math::Vector3 V(std::sqrt(std::max(0.0f, 1.0f - NoV * NoV)), 0.0f, NoV);
		Math::Vector3 N(0.0f, 0.0f, 1.0f);

		float A = 0.0f;
		float B = 0.0f;
		const uint32_t kNumSamples = 128;

		for (uint32_t i = 0; i < kNumSamples; ++i)
		{
			Math::Vector2 Xi = Hammersley(i, kNumSamples);
			Math::Vector3 H = ImportanceSampleGGX(Xi, N, roughness);
			Math::Vector3 L = H * (2.0f * V.Dot(H)) - V;

			float NoL = std::max(L.z, 0.0f);
			float NoH = std::max(H.z, 0.0f);
			float VoH = std::max(V.Dot(H), 0.0f);

			if (NoL > 0.0f)
			{
				float k = (roughness * roughness) / 2.0f;
				float G_V = NoV / (NoV * (1.0f - k) + k);
				float G_L = NoL / (NoL * (1.0f - k) + k);
				float G = G_V * G_L;

				float G_Vis = (G * VoH) / (NoH * NoV);
				float Fc = std::pow(1.0f - VoH, 5.0f);

				A += (1.0f - Fc) * G_Vis;
				B += Fc * G_Vis;
			}
		}

		return Math::Vector2(A / static_cast<float>(kNumSamples), B / static_cast<float>(kNumSamples));
	}

	static uint16_t FloatToHalf(float f)
	{
		uint32_t x = *reinterpret_cast<uint32_t*>(&f);
		uint32_t sign = (x >> 16) & 0x8000;
		int32_t exp = ((x >> 23) & 0xFF) - 127 + 15;
		uint32_t mant = x & 0x7FFFFF;

		if (exp <= 0)
		{
			return static_cast<uint16_t>(sign);
		}
		else if (exp >= 31)
		{
			return static_cast<uint16_t>(sign | 0x7C00);
		}
		return static_cast<uint16_t>(sign | (exp << 10) | (mant >> 13));
	}

	void SceneRenderer::generateBRDFLUT()
	{
		// 1. Fast-path: try loading pre-baked BRDF LUT in < 1 ms if present in VFS/disk
		if (Core::AssetPathResolver::readBinary("Content/Baked/BrdfLut.dds").isOk())
		{
			auto bakedLut = getOrCreateTexture("Content/Baked/BrdfLut.dds", false);
			if (bakedLut)
			{
				m_brdfLutTexture = bakedLut;
				DF_LOG_INFO("Renderer", "Loaded pre-baked BRDF LUT from Content/Baked/BrdfLut.dds (< 1 ms)");
				return;
			}
		}

		// 2. High-quality CPU fallback: 256x256 Split-Sum GGX BRDF LUT
		const uint32_t size = 256;
		std::vector<uint16_t> lutData(size * size * 4);

		uint32_t numThreads = std::max(1u, std::thread::hardware_concurrency());
		std::vector<std::thread> workers;
		uint32_t rowsPerThread = (size + numThreads - 1) / numThreads;

		for (uint32_t t = 0; t < numThreads; ++t)
		{
			uint32_t startY = t * rowsPerThread;
			uint32_t endY = std::min(size, startY + rowsPerThread);
			if (startY >= size) break;

			workers.emplace_back([&lutData, startY, endY, size]() {
				for (uint32_t y = startY; y < endY; ++y)
				{
					float roughness = std::clamp((static_cast<float>(y) + 0.5f) / static_cast<float>(size), 0.001f, 1.0f);
					for (uint32_t x = 0; x < size; ++x)
					{
						float NoV = std::clamp((static_cast<float>(x) + 0.5f) / static_cast<float>(size), 0.001f, 1.0f);
						Math::Vector2 res = IntegrateBRDF(NoV, roughness);

						size_t idx = (y * size + x) * 4;
						lutData[idx + 0] = FloatToHalf(res.x);
						lutData[idx + 1] = FloatToHalf(res.y);
						lutData[idx + 2] = FloatToHalf(0.0f);
						lutData[idx + 3] = FloatToHalf(1.0f);
					}
				}
			});
		}

		for (auto& w : workers)
		{
			if (w.joinable()) w.join();
		}

		RHI::SubresourceData subresource{};
		subresource.data = lutData.data();
		subresource.rowPitch = size * 4 * sizeof(uint16_t);
		subresource.slicePitch = static_cast<uint32_t>(lutData.size() * sizeof(uint16_t));

		RHI::GpuTextureDesc desc{};
		desc.width = size;
		desc.height = size;
		desc.mipLevels = 1;
		desc.arraySize = 1;
		desc.format = RHI::PixelFormat::R16G16B16A16_FLOAT;
		desc.usage = RHI::BufferUsage::Default;
		desc.bindFlags = RHI::BufferBindFlags::ShaderResource;

		auto res = m_device.createTexture(desc, lutData.data());
		if (res.isOk())
		{
			m_brdfLutTexture = *res;
			DF_LOG_INFO("Renderer", "Generated 2D Split-Sum BRDF LUT (256x256 R16G16B16A16_FLOAT)");
		}
	}

	void SceneRenderer::generateColorLUT()
	{
		if (m_colorLutTexture) return;

		// 1. Try loading prebaked 3D LUT
		if (Core::AssetPathResolver::readBinary("Assets/Textures/ColorLut.dds").isOk())
		{
			auto bakedLut = getOrCreateTexture("Assets/Textures/ColorLut.dds", false);
			if (bakedLut)
			{
				m_colorLutTexture = bakedLut;
				return;
			}
		}

		// 2. Generate neutral filmic 32x32x32 3D LUT
		constexpr uint32_t kLutSize = 32;
		std::vector<uint32_t> lutData(kLutSize * kLutSize * kLutSize);
		for (uint32_t b = 0; b < kLutSize; ++b)
		{
			float bf = static_cast<float>(b) / static_cast<float>(kLutSize - 1);
			for (uint32_t g = 0; g < kLutSize; ++g)
			{
				float gf = static_cast<float>(g) / static_cast<float>(kLutSize - 1);
				for (uint32_t r = 0; r < kLutSize; ++r)
				{
					float rf = static_cast<float>(r) / static_cast<float>(kLutSize - 1);
					uint8_t R = static_cast<uint8_t>(std::clamp(rf * 255.0f, 0.0f, 255.0f));
					uint8_t G = static_cast<uint8_t>(std::clamp(gf * 255.0f, 0.0f, 255.0f));
					uint8_t B = static_cast<uint8_t>(std::clamp(bf * 255.0f, 0.0f, 255.0f));
					uint8_t A = 255;
					lutData[b * kLutSize * kLutSize + g * kLutSize + r] = (A << 24) | (B << 16) | (G << 8) | R;
				}
			}
		}

		RHI::SubresourceData sub{};
		sub.data = lutData.data();
		sub.rowPitch = kLutSize * sizeof(uint32_t);
		sub.slicePitch = kLutSize * kLutSize * sizeof(uint32_t);

		RHI::GpuTextureDesc desc{};
		desc.width = kLutSize;
		desc.height = kLutSize;
		desc.depth = kLutSize;
		desc.mipLevels = 1;
		desc.arraySize = 1;
		desc.format = RHI::PixelFormat::R8G8B8A8_UNORM;
		desc.bindFlags = RHI::BufferBindFlags::ShaderResource;

		auto res = m_device.createTextureSubresources(desc, std::span<const RHI::SubresourceData>(&sub, 1));
		if (res.isOk())
		{
			m_colorLutTexture = *res;
			DF_LOG_INFO("Renderer", "Generated Neutral 32x32x32 3D Color LUT");
		}
	}

	void SceneRenderer::updateProceduralEnvIBL(const RenderScene& scene)
	{
		// Ensure 3D Color LUT is initialized
		generateColorLUT();

		// 1. Try loading scene-specified baked probe cubemap if provided
		const std::string& bakedPath = scene.environment.bakedProbeCubemapPath;
		if (!bakedPath.empty())
		{
			if (m_cachedBakedProbePath != bakedPath)
			{
				m_cachedBakedProbePath = bakedPath;
				m_specularIBLTexture = nullptr;
				m_irradianceIBLTexture = nullptr;

				std::string ddsPath = bakedPath;
				size_t dotPos = ddsPath.rfind('.');
				if (dotPos != std::string::npos)
				{
					ddsPath = ddsPath.substr(0, dotPos) + ".dds";
				}
				else
				{
					ddsPath += ".dds";
				}

				if (Core::AssetPathResolver::readBinary(ddsPath).isOk())
				{
					auto bakedTex = getOrCreateTexture(ddsPath, false);
					if (bakedTex)
					{
						m_specularIBLTexture = bakedTex;
					}
				}
				else if (Core::AssetPathResolver::readBinary(bakedPath).isOk())
				{
					auto bakedTex = getOrCreateTexture(bakedPath, false);
					if (bakedTex)
					{
						m_specularIBLTexture = bakedTex;
					}
				}

				std::string irradPath = bakedPath;
				if (dotPos != std::string::npos)
				{
					irradPath = bakedPath.substr(0, dotPos) + "_Irradiance.dds";
				}
				if (Core::AssetPathResolver::readBinary(irradPath).isOk())
				{
					auto irradTex = getOrCreateTexture(irradPath, false);
					if (irradTex)
					{
						m_irradianceIBLTexture = irradTex;
					}
				}

				if (m_specularIBLTexture && !m_irradianceIBLTexture)
				{
					m_irradianceIBLTexture = m_specularIBLTexture;
				}
			}

			if (m_specularIBLTexture)
			{
				return;
			}
		}
		else
		{
			// Probe disabled or empty - immediately clear local probe slots so scene reverts to outdoor sky
			if (!m_cachedBakedProbePath.empty())
			{
				m_cachedBakedProbePath.clear();
				m_specularIBLTexture = nullptr;
				m_irradianceIBLTexture = nullptr;
				m_cachedEnvSkyZenith = Math::Vector4(-1.0f, -1.0f, -1.0f, -1.0f); // Force regenerate procedural sky
			}
		}

		Math::Vector4 skyZenith = scene.environment.skyColor;
		Math::Vector4 skyHorizon = scene.environment.horizonColor;
		Math::Vector4 skyGround = scene.environment.groundColor;

		Math::Vector4 sunDir(0.577f, 0.577f, -0.577f, 0.0f);
		Math::Vector4 sunColor(1.0f, 0.95f, 0.85f, 0.0f);
		for (const auto& l : scene.lights)
		{
			if (l.type == 0)
			{
				Math::Vector3 d = -l.direction;
				if (d.LengthSquared() > 1e-4f) d.Normalize();
				sunDir = Math::Vector4(d.x, d.y, d.z, 1.0f);
				sunColor = Math::Vector4(l.color.x, l.color.y, l.color.z, l.intensity);
				break;
			}
		}

		auto vecDiff = [](const Math::Vector4& a, const Math::Vector4& b) -> float {
			return std::abs(a.x - b.x) + std::abs(a.y - b.y) + std::abs(a.z - b.z) + std::abs(a.w - b.w);
		};

		if (m_proceduralEnvIBLTexture &&
			vecDiff(skyZenith, m_cachedEnvSkyZenith) < 1e-3f &&
			vecDiff(skyHorizon, m_cachedEnvSkyHorizon) < 1e-3f &&
			vecDiff(skyGround, m_cachedEnvSkyGround) < 1e-3f &&
			vecDiff(sunDir, m_cachedEnvSunDir) < 1e-3f &&
			vecDiff(sunColor, m_cachedEnvSunColor) < 1e-3f)
		{
			return;
		}

		m_cachedEnvSkyZenith = skyZenith;
		m_cachedEnvSkyHorizon = skyHorizon;
		m_cachedEnvSkyGround = skyGround;
		m_cachedEnvSunDir = sunDir;
		m_cachedEnvSunColor = sunColor;

		constexpr uint32_t kBaseW = 128;
		constexpr uint32_t kBaseH = 64;
		constexpr uint32_t kMipCount = 6;

		std::vector<std::vector<uint16_t>> mipBuffers(kMipCount);
		std::vector<RHI::SubresourceData> subresources(kMipCount);

		Math::Vector3 zenithRGB(skyZenith.x, skyZenith.y, skyZenith.z);
		Math::Vector3 horizonRGB(skyHorizon.x, skyHorizon.y, skyHorizon.z);
		Math::Vector3 groundRGB(skyGround.x, skyGround.y, skyGround.z);
		Math::Vector3 sunDirVec(sunDir.x, sunDir.y, sunDir.z);
		Math::Vector3 sunColRGB = Math::Vector3(sunColor.x, sunColor.y, sunColor.z) * std::clamp(sunColor.w * 0.25f, 0.0f, 1.5f);

		// Analytical hemisphere average for rough diffuse convergence at Mip 5
		Math::Vector3 hemiAvgUpper = zenithRGB * 0.55f + horizonRGB * 0.45f + sunColRGB * 0.08f;
		Math::Vector3 hemiAvgLower = groundRGB * 0.65f + horizonRGB * 0.35f;

		for (uint32_t mip = 0; mip < kMipCount; ++mip)
		{
			uint32_t w = std::max(1u, kBaseW >> mip);
			uint32_t h = std::max(1u, kBaseH >> mip);
			float roughness = static_cast<float>(mip) / static_cast<float>(kMipCount - 1);

			auto& buf = mipBuffers[mip];
			buf.resize(static_cast<size_t>(w) * h * 4);

			float sunExp = std::lerp(96.0f, 2.0f, roughness * roughness);
			float sunWeight = std::lerp(0.65f, 0.06f, roughness);

			for (uint32_t y = 0; y < h; ++y)
			{
				float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(h);
				float theta = (0.5f - v) * 3.14159265359f; // [+pi/2 .. -pi/2]
				float sinTheta = std::sin(theta);
				float cosTheta = std::cos(theta);

				for (uint32_t x = 0; x < w; ++x)
				{
					float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(w);
					float phi = (u - 0.5f) * 2.0f * 3.14159265359f;

					Math::Vector3 dir(cosTheta * std::cos(phi), sinTheta, cosTheta * std::sin(phi));

					// Sharp sky gradient for low mips, smoothly convolving toward cosine hemisphere integral at high mips
					float upFactor = std::clamp(dir.y, 0.0f, 1.0f);
					float downFactor = std::clamp(-dir.y, 0.0f, 1.0f);
					Math::Vector3 sharpSky = (dir.y >= 0.0f)
						? Math::Vector3::Lerp(horizonRGB, zenithRGB, std::pow(upFactor, 0.65f))
						: Math::Vector3::Lerp(horizonRGB, groundRGB, std::pow(downFactor, 0.75f));

					float smoothWeight = std::clamp(dir.y * 0.5f + 0.5f, 0.0f, 1.0f);
					Math::Vector3 convolvedHemi = Math::Vector3::Lerp(hemiAvgLower, hemiAvgUpper, smoothWeight);

					Math::Vector3 envColor = Math::Vector3::Lerp(sharpSky, convolvedHemi, roughness * roughness);

					if (sunDir.w > 0.5f)
					{
						float sunDot = std::max(dir.Dot(sunDirVec), 0.0f);
						float sunLobe = std::pow(sunDot, sunExp) * sunWeight;
						envColor += sunColRGB * sunLobe;
					}

					// Warm sunlit limestone bounce light for horizontal directions matching Classic-Sponza atrium walls
					if (std::abs(dir.y) < 0.35f)
					{
						float horizWeight = 1.0f - (std::abs(dir.y) / 0.35f);
						float sunAlign = (sunDir.w > 0.5f) ? std::max(dir.Dot(sunDirVec), 0.0f) : 0.5f;
						float bounceFactor = horizWeight * (0.35f + 0.65f * sunAlign) * std::lerp(0.4f, 0.9f, roughness);
						Math::Vector3 warmLimestoneBounce(0.88f, 0.74f, 0.56f);
						float bounceIntensity = std::clamp(sunColor.w * 0.20f, 0.15f, 0.65f);
						envColor += warmLimestoneBounce * (bounceFactor * bounceIntensity);
					}

					size_t idx = (static_cast<size_t>(y) * w + x) * 4;
					buf[idx + 0] = FloatToHalf(envColor.x);
					buf[idx + 1] = FloatToHalf(envColor.y);
					buf[idx + 2] = FloatToHalf(envColor.z);
					buf[idx + 3] = FloatToHalf(1.0f);
				}
			}

			subresources[mip].data = buf.data();
			subresources[mip].rowPitch = w * 4 * sizeof(uint16_t);
			subresources[mip].slicePitch = w * h * 4 * sizeof(uint16_t);
		}

		RHI::GpuTextureDesc desc{};
		desc.width = kBaseW;
		desc.height = kBaseH;
		desc.mipLevels = kMipCount;
		desc.arraySize = 1;
		desc.format = RHI::PixelFormat::R16G16B16A16_FLOAT;
		desc.usage = RHI::BufferUsage::Default;
		desc.bindFlags = RHI::BufferBindFlags::ShaderResource;

		auto texRes = m_device.createTextureSubresources(desc, subresources);
		if (texRes.isOk())
		{
			m_proceduralEnvIBLTexture = *texRes;
			if (!m_specularIBLTexture) m_specularIBLTexture = m_proceduralEnvIBLTexture;
			if (!m_irradianceIBLTexture) m_irradianceIBLTexture = m_proceduralEnvIBLTexture;
		}
	}
}
