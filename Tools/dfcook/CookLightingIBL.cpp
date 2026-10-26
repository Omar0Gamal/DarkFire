#include "CookCommon.h"

#include <iostream>
#include <string>
#include <filesystem>
#include <algorithm>
#include <vector>
#include <cmath>
#include <thread>
#include <cstdint>
#include <cstring>

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <DirectXTex.h>

namespace dfcook
{
	namespace
	{
		constexpr float PI = 3.14159265358979323846f;

		struct Vec2
		{
			float x = 0.0f;
			float y = 0.0f;
		};

		struct Vec3
		{
			float x = 0.0f;
			float y = 0.0f;
			float z = 0.0f;

			constexpr Vec3() = default;
			constexpr Vec3(float inX, float inY, float inZ) : x(inX), y(inY), z(inZ) {}

			Vec3 operator+(const Vec3& o) const { return { x + o.x, y + o.y, z + o.z }; }
			Vec3 operator-(const Vec3& o) const { return { x - o.x, y - o.y, z - o.z }; }
			Vec3 operator*(float s) const { return { x * s, y * s, z * s }; }
			Vec3 operator*(const Vec3& o) const { return { x * o.x, y * o.y, z * o.z }; }
			Vec3& operator+=(const Vec3& o) { x += o.x; y += o.y; z += o.z; return *this; }
			Vec3& operator*=(float s) { x *= s; y *= s; z *= s; return *this; }

			float Dot(const Vec3& o) const { return x * o.x + y * o.y + z * o.z; }
			Vec3 Cross(const Vec3& o) const
			{
				return { y * o.z - z * o.y, z * o.x - x * o.z, x * o.y - y * o.x };
			}
			float LengthSquared() const { return x * x + y * y + z * z; }
			float Length() const { return std::sqrt(LengthSquared()); }
			void Normalize()
			{
				float len = Length();
				if (len > 1e-6f)
				{
					float inv = 1.0f / len;
					x *= inv; y *= inv; z *= inv;
				}
			}

			static Vec3 Lerp(const Vec3& a, const Vec3& b, float t)
			{
				return { a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t };
			}
		};

		// Van der Corput radical inverse
		float RadicalInverse_VdC(uint32_t bits)
		{
			bits = (bits << 16u) | (bits >> 16u);
			bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
			bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
			bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
			bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
			return static_cast<float>(bits) * 2.3283064365386963e-10f;
		}

		Vec2 Hammersley(uint32_t i, uint32_t N)
		{
			return { static_cast<float>(i) / static_cast<float>(N), RadicalInverse_VdC(i) };
		}

		Vec3 ImportanceSampleGGX(Vec2 Xi, Vec3 N, float roughness)
		{
			float a = roughness * roughness;
			float phi = 2.0f * PI * Xi.x;
			float cosTheta = std::sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
			float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cosTheta * cosTheta));

			Vec3 H(std::cos(phi) * sinTheta, std::sin(phi) * sinTheta, cosTheta);
			Vec3 up = std::abs(N.z) < 0.999f ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(1.0f, 0.0f, 0.0f);
			Vec3 tangent = up.Cross(N);
			tangent.Normalize();
			Vec3 bitangent = N.Cross(tangent);

			return tangent * H.x + bitangent * H.y + N * H.z;
		}

		// Standard IEEE 754 float-to-half helper
		uint16_t FloatToHalf(float f)
		{
			uint32_t x = 0;
			std::memcpy(&x, &f, sizeof(float));
			uint32_t sign = (x >> 16) & 0x8000;
			int32_t exp = static_cast<int32_t>((x >> 23) & 0xFF) - 127 + 15;
			uint32_t mant = x & 0x7FFFFF;

			if (exp <= 0)
			{
				return static_cast<uint16_t>(sign);
			}
			else if (exp >= 31)
			{
				return static_cast<uint16_t>(sign | 0x7C00);
			}
			return static_cast<uint16_t>(sign | (static_cast<uint32_t>(exp) << 10) | (mant >> 13));
		}

		Vec2 IntegrateBRDF(float NoV, float roughness, uint32_t numSamples = 64)
		{
			Vec3 V(std::sqrt(std::max(0.0f, 1.0f - NoV * NoV)), 0.0f, NoV);
			Vec3 N(0.0f, 0.0f, 1.0f);

			float A = 0.0f;
			float B = 0.0f;

			for (uint32_t i = 0; i < numSamples; ++i)
			{
				Vec2 Xi = Hammersley(i, numSamples);
				Vec3 H = ImportanceSampleGGX(Xi, N, roughness);
				Vec3 L = H * (2.0f * V.Dot(H)) - V;

				float NoL = std::max(L.z, 0.0f);
				float NoH = std::max(H.z, 0.0f);
				float VoH = std::max(V.Dot(H), 0.0f);

				if (NoL > 0.0f)
				{
					float k = (roughness * roughness) / 2.0f;
					float G_V = NoV / (NoV * (1.0f - k) + k);
					float G_L = NoL / (NoL * (1.0f - k) + k);
					float G = G_V * G_L;

					float G_Vis = (NoH > 1e-5f) ? ((G * VoH) / (NoH * NoV)) : 0.0f;
					float Fc = std::pow(1.0f - VoH, 5.0f);

					A += (1.0f - Fc) * G_Vis;
					B += Fc * G_Vis;
				}
			}

			return { A / static_cast<float>(numSamples), B / static_cast<float>(numSamples) };
		}

		Vec3 EvaluateClassicSponzaEnv(const Vec3& dir)
		{
			// Sun Pitch 66 deg, Yaw -37.5 deg
			static const float pitchRad = 66.0f * (PI / 180.0f);
			static const float yawRad = -37.5f * (PI / 180.0f);
			static const Vec3 sunDir = []() {
				Vec3 d(std::cos(pitchRad) * std::cos(yawRad), std::sin(pitchRad), std::cos(pitchRad) * std::sin(yawRad));
				d.Normalize();
				return d;
			}();

			// 5500K direct sunlight & warm Mediterranean limestone bounce palette
			static const Vec3 sunColor5500K(1.0f, 0.915f, 0.82f);
			static const float sunIntensity = 4.5f;
			static const Vec3 skyZenith(0.28f, 0.48f, 0.78f);
			static const Vec3 skyHorizon(0.78f, 0.78f, 0.74f);
			static const Vec3 groundLimestone(0.48f, 0.38f, 0.26f);

			Vec3 envColor;
			if (dir.y >= 0.0f)
			{
				float upFactor = std::clamp(dir.y, 0.0f, 1.0f);
				envColor = Vec3::Lerp(skyHorizon, skyZenith, std::pow(upFactor, 0.65f));
			}
			else
			{
				float downFactor = std::clamp(-dir.y, 0.0f, 1.0f);
				envColor = Vec3::Lerp(skyHorizon, groundLimestone, std::pow(downFactor, 0.75f));
				// Warm diffuse sun bounce on limestone floor
				envColor += groundLimestone * (sunColor5500K * 0.65f) * downFactor;
			}

			// Sun disk and corona
			float sunDot = std::max(dir.Dot(sunDir), 0.0f);
			if (sunDot > 0.0f)
			{
				float sunDisk = std::pow(sunDot, 256.0f) * 4.0f;
				float sunAureole = std::pow(sunDot, 16.0f) * 0.35f;
				envColor += (sunColor5500K * sunIntensity) * (sunDisk + sunAureole);
			}

			return envColor;
		}

		Vec3 PrefilterEnvMapGGX(const Vec3& N, float roughness, uint32_t numSamples = 64)
		{
			if (roughness <= 0.001f)
			{
				return EvaluateClassicSponzaEnv(N);
			}

			Vec3 V = N;
			Vec3 prefilteredColor(0.0f, 0.0f, 0.0f);
			float totalWeight = 0.0f;

			for (uint32_t i = 0; i < numSamples; ++i)
			{
				Vec2 Xi = Hammersley(i, numSamples);
				Vec3 H = ImportanceSampleGGX(Xi, N, roughness);
				Vec3 L = H * (2.0f * V.Dot(H)) - V;

				float NoL = std::max(N.Dot(L), 0.0f);
				if (NoL > 0.0f)
				{
					prefilteredColor += EvaluateClassicSponzaEnv(L) * NoL;
					totalWeight += NoL;
				}
			}

			return totalWeight > 0.0f ? (prefilteredColor * (1.0f / totalWeight)) : EvaluateClassicSponzaEnv(N);
		}
	}

	bool CookLightingIBL(const std::filesystem::path& outBakedDir, BakeQuality quality)
	{
		std::filesystem::create_directories(outBakedDir);

		HRESULT hrCo = CoInitializeEx(nullptr, COINIT_MULTITHREADED);
		(void)hrCo;

		// -------------------------------------------------------------
		// 1. Offline Split-Sum BRDF LUT Baker (BrdfLut.dds, 256x256 R16G16B16A16_FLOAT)
		// -------------------------------------------------------------
		const uint32_t lutSize = 256;
		DirectX::ScratchImage lutScratch;
		HRESULT hr = lutScratch.Initialize2D(DXGI_FORMAT_R16G16B16A16_FLOAT, lutSize, lutSize, 1, 1);
		if (FAILED(hr))
		{
			std::cerr << "[dfcook] Failed to allocate ScratchImage for BRDF LUT." << std::endl;
			CoUninitialize();
			return false;
		}

		const DirectX::Image* lutImg = lutScratch.GetImage(0, 0, 0);
		if (!lutImg)
		{
			std::cerr << "[dfcook] Failed to acquire image pointer for BRDF LUT." << std::endl;
			CoUninitialize();
			return false;
		}

		uint32_t numThreads = std::max(1u, std::thread::hardware_concurrency());
		std::vector<std::thread> workers;
		uint32_t rowsPerThread = (lutSize + numThreads - 1) / numThreads;

		for (uint32_t t = 0; t < numThreads; ++t)
		{
			uint32_t startY = t * rowsPerThread;
			uint32_t endY = std::min(lutSize, startY + rowsPerThread);
			if (startY >= lutSize) break;

			workers.emplace_back([lutImg, startY, endY, lutSize]() {
				for (uint32_t y = startY; y < endY; ++y)
				{
					float roughness = std::clamp((static_cast<float>(y) + 0.5f) / static_cast<float>(lutSize), 0.001f, 1.0f);
					uint16_t* row = reinterpret_cast<uint16_t*>(lutImg->pixels + y * lutImg->rowPitch);

					for (uint32_t x = 0; x < lutSize; ++x)
					{
						float NoV = std::clamp((static_cast<float>(x) + 0.5f) / static_cast<float>(lutSize), 0.001f, 1.0f);
						Vec2 res = IntegrateBRDF(NoV, roughness, 64);

						row[x * 4 + 0] = FloatToHalf(res.x);
						row[x * 4 + 1] = FloatToHalf(res.y);
						row[x * 4 + 2] = FloatToHalf(0.0f);
						row[x * 4 + 3] = FloatToHalf(1.0f);
					}
				}
			});
		}

		for (auto& w : workers)
		{
			if (w.joinable()) w.join();
		}
		workers.clear();

		std::filesystem::path brdfPath = outBakedDir / "BrdfLut.dds";
		hr = DirectX::SaveToDDSFile(
			lutScratch.GetImages(),
			lutScratch.GetImageCount(),
			lutScratch.GetMetadata(),
			DirectX::DDS_FLAGS_NONE,
			brdfPath.c_str());

		if (FAILED(hr))
		{
			std::cerr << "[dfcook] Failed to save BRDF LUT DDS: " << brdfPath.string() << std::endl;
		}
		else
		{
			std::cout << "[dfcook] Baked Split-Sum BRDF LUT -> BrdfLut.dds" << std::endl;
		}

		// -------------------------------------------------------------
		// 1b. Offline Filmic 3D Color LUT Baker (ColorLut.dds, 32x32x32 R8G8B8A8_UNORM)
		// -------------------------------------------------------------
		const uint32_t lut3dSize = 32;
		DirectX::ScratchImage lut3dScratch;
		hr = lut3dScratch.Initialize3D(DXGI_FORMAT_R8G8B8A8_UNORM, lut3dSize, lut3dSize, lut3dSize, 1);
		if (SUCCEEDED(hr))
		{
			const DirectX::Image* img3D = lut3dScratch.GetImage(0, 0, 0);
			if (img3D)
			{
				for (uint32_t z = 0; z < lut3dSize; ++z)
				{
					float b = static_cast<float>(z) / static_cast<float>(lut3dSize - 1);
					uint8_t* slice = img3D->pixels + z * img3D->slicePitch;
					for (uint32_t y = 0; y < lut3dSize; ++y)
					{
						float g = static_cast<float>(y) / static_cast<float>(lut3dSize - 1);
						uint8_t* row = slice + y * img3D->rowPitch;
						for (uint32_t x = 0; x < lut3dSize; ++x)
						{
							float r = static_cast<float>(x) / static_cast<float>(lut3dSize - 1);
							row[x * 4 + 0] = static_cast<uint8_t>(std::clamp(r * 255.0f, 0.0f, 255.0f));
							row[x * 4 + 1] = static_cast<uint8_t>(std::clamp(g * 255.0f, 0.0f, 255.0f));
							row[x * 4 + 2] = static_cast<uint8_t>(std::clamp(b * 255.0f, 0.0f, 255.0f));
							row[x * 4 + 3] = 255;
						}
					}
				}
				std::filesystem::path lut3dPath = outBakedDir / "ColorLut.dds";
				DirectX::SaveToDDSFile(lut3dScratch.GetImages(), lut3dScratch.GetImageCount(), lut3dScratch.GetMetadata(), DirectX::DDS_FLAGS_NONE, lut3dPath.c_str());
				std::cout << "[dfcook] Baked 3D Color LUT -> ColorLut.dds" << std::endl;
			}
		}

		// -------------------------------------------------------------
		// 2. Offline Pre-Convolved Environment IBL Baker (DefaultEnvIBL.dds)
		// -------------------------------------------------------------
		uint32_t envWidth = 256;
		uint32_t envHeight = 128;
		uint32_t mipCount = 6;
		uint32_t ggxSamples = 64;

		if (quality == BakeQuality::Balanced)
		{
			envWidth = 512;
			envHeight = 256;
			ggxSamples = 128;
		}
		else if (quality == BakeQuality::Production)
		{
			envWidth = 512;
			envHeight = 256;
			ggxSamples = 256;
		}

		DirectX::ScratchImage envScratch;
		hr = envScratch.Initialize2D(DXGI_FORMAT_R16G16B16A16_FLOAT, envWidth, envHeight, 1, mipCount);
		if (FAILED(hr))
		{
			std::cerr << "[dfcook] Failed to allocate ScratchImage for Environment IBL." << std::endl;
			CoUninitialize();
			return false;
		}

		for (uint32_t mip = 0; mip < mipCount; ++mip)
		{
			const DirectX::Image* img = envScratch.GetImage(mip, 0, 0);
			if (!img) continue;

			uint32_t w = static_cast<uint32_t>(img->width);
			uint32_t h = static_cast<uint32_t>(img->height);
			float roughness = static_cast<float>(mip) / static_cast<float>(mipCount - 1);

			uint32_t mipThreads = std::min(h, std::max(1u, std::thread::hardware_concurrency()));
			uint32_t mipRowsPerThread = (h + mipThreads - 1) / mipThreads;

			for (uint32_t t = 0; t < mipThreads; ++t)
			{
				uint32_t startY = t * mipRowsPerThread;
				uint32_t endY = std::min(h, startY + mipRowsPerThread);
				if (startY >= h) break;

				workers.emplace_back([img, startY, endY, w, h, roughness, ggxSamples]() {
					for (uint32_t y = startY; y < endY; ++y)
					{
						float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(h);
						float theta = (0.5f - v) * PI;
						float sinTheta = std::sin(theta);
						float cosTheta = std::cos(theta);

						uint16_t* row = reinterpret_cast<uint16_t*>(img->pixels + y * img->rowPitch);

						for (uint32_t x = 0; x < w; ++x)
						{
							float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(w);
							float phi = (u - 0.5f) * 2.0f * PI;

							Vec3 N(cosTheta * std::cos(phi), sinTheta, cosTheta * std::sin(phi));
							N.Normalize();

							Vec3 col = PrefilterEnvMapGGX(N, roughness, ggxSamples);

							row[x * 4 + 0] = FloatToHalf(col.x);
							row[x * 4 + 1] = FloatToHalf(col.y);
							row[x * 4 + 2] = FloatToHalf(col.z);
							row[x * 4 + 3] = FloatToHalf(1.0f);
						}
					}
				});
			}

			for (auto& worker : workers)
			{
				if (worker.joinable()) worker.join();
			}
			workers.clear();
		}

		std::filesystem::path envPath = outBakedDir / "DefaultEnvIBL.dds";
		hr = DirectX::SaveToDDSFile(
			envScratch.GetImages(),
			envScratch.GetImageCount(),
			envScratch.GetMetadata(),
			DirectX::DDS_FLAGS_NONE,
			envPath.c_str());

		if (FAILED(hr))
		{
			std::cerr << "[dfcook] Failed to save Environment IBL DDS: " << envPath.string() << std::endl;
			CoUninitialize();
			return false;
		}

		std::cout << "[dfcook] Baked " << mipCount << "-Mip Environment IBL (" << envWidth << "x" << envHeight << ") -> DefaultEnvIBL.dds" << std::endl;

		CoUninitialize();
		return true;
	}

	bool ConvolveProbeHDR(const std::filesystem::path& inHdrPath, const std::filesystem::path& outDdsPath, BakeQuality quality)
	{
		if (!std::filesystem::exists(inHdrPath))
		{
			return false;
		}

		DirectX::ScratchImage hdrScratch;
		HRESULT hr = DirectX::LoadFromHDRFile(inHdrPath.c_str(), nullptr, hdrScratch);
		if (FAILED(hr) || !hdrScratch.GetImage(0, 0, 0))
		{
			std::cerr << "[dfcook] Failed to load HDR file: " << inHdrPath.string() << std::endl;
			return false;
		}

		uint32_t outWidth = (quality == BakeQuality::Production) ? 1024 : 512;
		uint32_t outHeight = outWidth / 2;
		uint32_t mipCount = 6;
		uint32_t ggxSamples = (quality == BakeQuality::Production) ? 512 : (quality == BakeQuality::Balanced ? 128 : 64);

		DirectX::ScratchImage specScratch;
		hr = specScratch.Initialize2D(DXGI_FORMAT_R16G16B16A16_FLOAT, outWidth, outHeight, 1, mipCount);
		if (FAILED(hr)) return false;

		const DirectX::Image* srcImg = hdrScratch.GetImage(0, 0, 0);
		auto sampleSrcHDR = [srcImg](const Vec3& dir) -> Vec3 {
			float phi = std::atan2(dir.z, dir.x);
			float theta = std::asin(std::clamp(dir.y, -1.0f, 1.0f));
			float u = phi * (0.5f / PI) + 0.5f;
			float v = 0.5f - theta * (1.0f / PI);
			uint32_t px = std::clamp(static_cast<uint32_t>(u * srcImg->width), 0u, static_cast<uint32_t>(srcImg->width - 1));
			uint32_t py = std::clamp(static_cast<uint32_t>(v * srcImg->height), 0u, static_cast<uint32_t>(srcImg->height - 1));
			const float* pix = reinterpret_cast<const float*>(srcImg->pixels + py * srcImg->rowPitch + px * sizeof(float) * 4);
			return Vec3(pix[0], pix[1], pix[2]);
		};

		std::vector<std::thread> workers;
		for (uint32_t mip = 0; mip < mipCount; ++mip)
		{
			const DirectX::Image* img = specScratch.GetImage(mip, 0, 0);
			if (!img) continue;

			uint32_t w = static_cast<uint32_t>(img->width);
			uint32_t h = static_cast<uint32_t>(img->height);
			float roughness = static_cast<float>(mip) / static_cast<float>(mipCount - 1);

			uint32_t mipThreads = std::min(h, std::max(1u, std::thread::hardware_concurrency()));
			uint32_t mipRows = (h + mipThreads - 1) / mipThreads;

			for (uint32_t t = 0; t < mipThreads; ++t)
			{
				uint32_t startY = t * mipRows;
				uint32_t endY = std::min(h, startY + mipRows);
				if (startY >= h) break;

				workers.emplace_back([img, startY, endY, w, h, roughness, ggxSamples, &sampleSrcHDR]() {
					for (uint32_t y = startY; y < endY; ++y)
					{
						float v = (static_cast<float>(y) + 0.5f) / static_cast<float>(h);
						float theta = (0.5f - v) * PI;
						float sinTheta = std::sin(theta);
						float cosTheta = std::cos(theta);

						uint16_t* row = reinterpret_cast<uint16_t*>(img->pixels + y * img->rowPitch);

						for (uint32_t x = 0; x < w; ++x)
						{
							float u = (static_cast<float>(x) + 0.5f) / static_cast<float>(w);
							float phi = (u - 0.5f) * 2.0f * PI;

							Vec3 N(cosTheta * std::cos(phi), sinTheta, cosTheta * std::sin(phi));
							N.Normalize();

							Vec3 prefilteredColor(0.0f, 0.0f, 0.0f);
							if (roughness <= 0.001f)
							{
								prefilteredColor = sampleSrcHDR(N);
							}
							else
							{
								float totalWeight = 0.0f;
								Vec3 V = N;
								for (uint32_t i = 0; i < ggxSamples; ++i)
								{
									Vec2 Xi = Hammersley(i, ggxSamples);
									Vec3 H = ImportanceSampleGGX(Xi, N, roughness);
									Vec3 L = H * (2.0f * V.Dot(H)) - V;
									float NoL = std::max(N.Dot(L), 0.0f);
									if (NoL > 0.0f)
									{
										prefilteredColor += sampleSrcHDR(L) * NoL;
										totalWeight += NoL;
									}
								}
								if (totalWeight > 0.0f) prefilteredColor *= (1.0f / totalWeight);
							}

							row[x * 4 + 0] = FloatToHalf(prefilteredColor.x);
							row[x * 4 + 1] = FloatToHalf(prefilteredColor.y);
							row[x * 4 + 2] = FloatToHalf(prefilteredColor.z);
							row[x * 4 + 3] = FloatToHalf(1.0f);
						}
					}
				});
			}

			for (auto& worker : workers)
			{
				if (worker.joinable()) worker.join();
			}
			workers.clear();
		}

		// Save Specular IBL DDS
		hr = DirectX::SaveToDDSFile(specScratch.GetImages(), specScratch.GetImageCount(), specScratch.GetMetadata(), DirectX::DDS_FLAGS_NONE, outDdsPath.c_str());
		if (SUCCEEDED(hr))
		{
			std::cout << "[dfcook] Convolved HDR probe -> " << outDdsPath.filename().string() << std::endl;
		}

		// Save Cosine Diffuse Irradiance DDS
		std::string irradStr = outDdsPath.string();
		size_t dotPos = irradStr.rfind('.');
		if (dotPos != std::string::npos) irradStr = irradStr.substr(0, dotPos) + "_Irradiance.dds";
		else irradStr += "_Irradiance.dds";

		DirectX::ScratchImage irradScratch;
		hr = irradScratch.Initialize2D(DXGI_FORMAT_R16G16B16A16_FLOAT, 128, 64, 1, 1);
		if (SUCCEEDED(hr))
		{
			const DirectX::Image* irradImg = irradScratch.GetImage(0, 0, 0);
			uint32_t diffSamples = (quality == BakeQuality::Production) ? 512 : 128;
			for (uint32_t y = 0; y < 64; ++y)
			{
				float v = (static_cast<float>(y) + 0.5f) / 64.0f;
				float theta = (0.5f - v) * PI;
				float sinTheta = std::sin(theta);
				float cosTheta = std::cos(theta);
				uint16_t* row = reinterpret_cast<uint16_t*>(irradImg->pixels + y * irradImg->rowPitch);

				for (uint32_t x = 0; x < 128; ++x)
				{
					float u = (static_cast<float>(x) + 0.5f) / 128.0f;
					float phi = (u - 0.5f) * 2.0f * PI;
					Vec3 N(cosTheta * std::cos(phi), sinTheta, cosTheta * std::sin(phi));
					N.Normalize();

					Vec3 irradiance(0.0f, 0.0f, 0.0f);
					Vec3 up = std::abs(N.z) < 0.999f ? Vec3(0.0f, 0.0f, 1.0f) : Vec3(1.0f, 0.0f, 0.0f);
					Vec3 right = up.Cross(N); right.Normalize();
					Vec3 forward = N.Cross(right);

					for (uint32_t s = 0; s < diffSamples; ++s)
					{
						Vec2 xi = Hammersley(s, diffSamples);
						float rTheta = std::asin(std::sqrt(xi.x));
						float rPhi = 2.0f * PI * xi.y;
						Vec3 localRay(std::sin(rTheta) * std::cos(rPhi), std::cos(rTheta), std::sin(rTheta) * std::sin(rPhi));
						Vec3 sampleRay = right * localRay.x + N * localRay.y + forward * localRay.z;
						sampleRay.Normalize();
						irradiance += sampleSrcHDR(sampleRay);
					}
					irradiance *= (1.0f / static_cast<float>(diffSamples));

					row[x * 4 + 0] = FloatToHalf(irradiance.x);
					row[x * 4 + 1] = FloatToHalf(irradiance.y);
					row[x * 4 + 2] = FloatToHalf(irradiance.z);
					row[x * 4 + 3] = FloatToHalf(1.0f);
				}
			}
			std::filesystem::path irradPath(irradStr);
			DirectX::SaveToDDSFile(irradScratch.GetImages(), irradScratch.GetImageCount(), irradScratch.GetMetadata(), DirectX::DDS_FLAGS_NONE, irradPath.c_str());
			std::cout << "[dfcook] Convolved Diffuse Irradiance -> " << irradPath.filename().string() << std::endl;
		}

		return true;
	}
}
