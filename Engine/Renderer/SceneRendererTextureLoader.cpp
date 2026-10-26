#include "SceneRenderer.h"
#include "Engine/Core/Log/Log.h"
#include "Engine/Runtime/EngineContext.h"
#include "Engine/Core/VFS/AssetPathResolver.h"
#include <DirectXTex.h>
#include <algorithm>
#include <cmath>
#include <filesystem>
#include <thread>
#ifdef _WIN32
#include <objbase.h>
#include <d3d11.h>
#endif

namespace Core::Renderer
{
	static void EnsureWICCOMInitialized()
	{
#ifdef _WIN32
		CoInitializeEx(nullptr, COINIT_MULTITHREADED);
#endif
	}

	std::shared_ptr<RHI::IGpuTexture> SceneRenderer::getOrCreateTexture(const std::string& texturePath, bool sRGB)
	{
		if (texturePath.empty())
		{
			return nullptr;
		}

		std::string cacheKey = texturePath + (sRGB ? "#srgb" : "#linear");
		auto it = m_textureCache.find(cacheKey);
		if (it != m_textureCache.end())
		{
			return it->second;
		}

		auto binRes = Core::AssetPathResolver::readBinary(texturePath);
		if (!binRes.isOk() || binRes.value().empty())
		{
			DF_LOG_WARN("Renderer", "Could not load texture file: {}", texturePath);
			return nullptr;
		}

		const auto& fileBytes = binRes.value();

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
		else if (ext == ".hdr")
		{
			hr = DirectX::LoadFromHDRMemory(fileBytes.data(), fileBytes.size(), &metadata, scratchImage);
		}
		else if (ext == ".tga")
		{
			hr = DirectX::LoadFromTGAMemory(fileBytes.data(), fileBytes.size(), &metadata, scratchImage);
		}
		else
		{
			EnsureWICCOMInitialized();
			DirectX::WIC_FLAGS wicFlags = sRGB ? DirectX::WIC_FLAGS_NONE : DirectX::WIC_FLAGS_IGNORE_SRGB;
			hr = DirectX::LoadFromWICMemory(fileBytes.data(), fileBytes.size(), wicFlags, &metadata, scratchImage);
		}

		if (FAILED(hr))
		{
			DF_LOG_WARN("Renderer", "DirectXTex failed to parse texture {}: HRESULT 0x{:08X}", texturePath, static_cast<uint32_t>(hr));
			return nullptr;
		}

		bool isHdrFloat = (ext == ".hdr") ||
			(metadata.format == DXGI_FORMAT_R32G32B32A32_FLOAT) ||
			(metadata.format == DXGI_FORMAT_R16G16B16A16_FLOAT) ||
			(metadata.format == DXGI_FORMAT_R32G32B32_FLOAT);

		bool isCompressed = DirectX::IsCompressed(metadata.format);
		RHI::PixelFormat rhiCompressedFormat = RHI::PixelFormat::Unknown;
		if (isCompressed)
		{
			switch (metadata.format)
			{
			case DXGI_FORMAT_BC1_UNORM: rhiCompressedFormat = RHI::PixelFormat::BC1_UNORM; break;
			case DXGI_FORMAT_BC1_UNORM_SRGB: rhiCompressedFormat = RHI::PixelFormat::BC1_UNORM_SRGB; break;
			case DXGI_FORMAT_BC3_UNORM: rhiCompressedFormat = RHI::PixelFormat::BC3_UNORM; break;
			case DXGI_FORMAT_BC3_UNORM_SRGB: rhiCompressedFormat = RHI::PixelFormat::BC3_UNORM_SRGB; break;
			case DXGI_FORMAT_BC4_UNORM: rhiCompressedFormat = RHI::PixelFormat::BC4_UNORM; break;
			case DXGI_FORMAT_BC5_UNORM: rhiCompressedFormat = RHI::PixelFormat::BC5_UNORM; break;
			case DXGI_FORMAT_BC7_UNORM: rhiCompressedFormat = RHI::PixelFormat::BC7_UNORM; break;
			case DXGI_FORMAT_BC7_UNORM_SRGB: rhiCompressedFormat = RHI::PixelFormat::BC7_UNORM_SRGB; break;
			default: break;
			}
		}

		if (rhiCompressedFormat != RHI::PixelFormat::Unknown)
		{
			std::vector<RHI::SubresourceData> subresources(metadata.mipLevels);
			for (size_t i = 0; i < metadata.mipLevels; ++i)
			{
				const DirectX::Image* img = scratchImage.GetImage(i, 0, 0);
				if (img)
				{
					subresources[i].data = img->pixels;
					subresources[i].rowPitch = static_cast<uint32_t>(img->rowPitch);
					subresources[i].slicePitch = static_cast<uint32_t>(img->slicePitch);
				}
			}

			RHI::GpuTextureDesc texDesc{};
			texDesc.width = static_cast<uint32_t>(metadata.width);
			texDesc.height = static_cast<uint32_t>(metadata.height);
			texDesc.mipLevels = static_cast<uint32_t>(metadata.mipLevels);
			texDesc.arraySize = 1;
			texDesc.format = rhiCompressedFormat;
			texDesc.usage = RHI::BufferUsage::Default;
			texDesc.bindFlags = RHI::BufferBindFlags::ShaderResource;

			auto texRes = m_device.createTextureSubresources(texDesc, subresources);
			if (texRes.isOk())
			{
				m_textureCache[cacheKey] = *texRes;
				DF_LOG_INFO("Renderer", "Loaded BC texture '{}' ({}x{}, {} mips, format {})",
					texturePath, texDesc.width, texDesc.height, texDesc.mipLevels, static_cast<uint32_t>(rhiCompressedFormat));
				return *texRes;
			}
			DF_LOG_WARN("Renderer", "Failed to create GPU compressed texture for '{}': {}", texturePath, texRes.error());
			return nullptr;
		}

		if (!sRGB && DirectX::IsSRGB(metadata.format) && !isHdrFloat)
		{
			metadata.format = DirectX::MakeLinear(metadata.format);
			scratchImage.OverrideFormat(metadata.format);
		}

		DXGI_FORMAT targetFormat = isHdrFloat ? DXGI_FORMAT_R16G16B16A16_FLOAT :
			(sRGB ? DXGI_FORMAT_R8G8B8A8_UNORM_SRGB : DXGI_FORMAT_R8G8B8A8_UNORM);

		// Ensure 32-bit RGBA in target format
		const DirectX::Image* baseImage = scratchImage.GetImage(0, 0, 0);
		DirectX::ScratchImage convertedImage;
		if (metadata.format == DXGI_FORMAT_R8_UNORM || metadata.format == DXGI_FORMAT_A8_UNORM || metadata.format == DXGI_FORMAT_R8_UINT)
		{
			hr = convertedImage.Initialize2D(targetFormat, metadata.width, metadata.height, 1, 1);
			if (SUCCEEDED(hr))
			{
				const DirectX::Image* srcImg = scratchImage.GetImage(0, 0, 0);
				const DirectX::Image* dstImg = convertedImage.GetImage(0, 0, 0);
				for (size_t y = 0; y < metadata.height; ++y)
				{
					const uint8_t* srcRow = srcImg->pixels + y * srcImg->rowPitch;
					uint8_t* dstRow = dstImg->pixels + y * dstImg->rowPitch;
					for (size_t x = 0; x < metadata.width; ++x)
					{
						uint8_t val = srcRow[x];
						dstRow[x * 4 + 0] = val;
						dstRow[x * 4 + 1] = val;
						dstRow[x * 4 + 2] = val;
						dstRow[x * 4 + 3] = 255;
					}
				}
				baseImage = convertedImage.GetImage(0, 0, 0);
				metadata.format = targetFormat;
			}
		}
		else if (metadata.format != targetFormat)
		{
			hr = DirectX::Convert(
				*scratchImage.GetImage(0, 0, 0),
				targetFormat,
				DirectX::TEX_FILTER_DEFAULT,
				DirectX::TEX_THRESHOLD_DEFAULT,
				convertedImage
			);
			if (SUCCEEDED(hr))
			{
				baseImage = convertedImage.GetImage(0, 0, 0);
				metadata.format = targetFormat;
			}
		}

		if (!baseImage)
		{
			return nullptr;
		}

		// Generate full mipmap chain for textures
		DirectX::ScratchImage mipChain;
		if (metadata.mipLevels <= 1 && metadata.width > 1 && metadata.height > 1)
		{
			hr = DirectX::GenerateMipMaps(*baseImage, DirectX::TEX_FILTER_DEFAULT, 0, mipChain);
			if (SUCCEEDED(hr))
			{
				metadata = mipChain.GetMetadata();
			}
		}

		const DirectX::ScratchImage& finalScratch = (mipChain.GetImageCount() > 0) ? mipChain :
			(convertedImage.GetImageCount() > 0 ? convertedImage : scratchImage);

		std::vector<RHI::SubresourceData> subresources(metadata.mipLevels);
		for (size_t i = 0; i < metadata.mipLevels; ++i)
		{
			const DirectX::Image* img = finalScratch.GetImage(i, 0, 0);
			if (img)
			{
				subresources[i].data = img->pixels;
				subresources[i].rowPitch = static_cast<uint32_t>(img->rowPitch);
				subresources[i].slicePitch = static_cast<uint32_t>(img->slicePitch);
			}
		}

		RHI::GpuTextureDesc texDesc{};
		texDesc.width = static_cast<uint32_t>(metadata.width);
		texDesc.height = static_cast<uint32_t>(metadata.height);
		texDesc.mipLevels = static_cast<uint32_t>(metadata.mipLevels);
		texDesc.arraySize = 1;
		texDesc.format = isHdrFloat ? RHI::PixelFormat::R16G16B16A16_FLOAT :
			(sRGB ? RHI::PixelFormat::R8G8B8A8_UNORM_SRGB : RHI::PixelFormat::R8G8B8A8_UNORM);
		texDesc.usage = RHI::BufferUsage::Default;
		texDesc.bindFlags = RHI::BufferBindFlags::ShaderResource;

		auto texRes = m_device.createTextureSubresources(texDesc, subresources);
		if (texRes.isOk())
		{
			m_textureCache[cacheKey] = *texRes;
			DF_LOG_INFO("Renderer", "Loaded texture '{}' ({}x{}, {} mips, {})",
				texturePath, texDesc.width, texDesc.height, texDesc.mipLevels, sRGB ? "sRGB" : "Linear");
			return *texRes;
		}

		DF_LOG_WARN("Renderer", "Failed to create GPU texture for '{}': {}", texturePath, texRes.error());
		return nullptr;
	}

	std::shared_ptr<RHI::IGpuTexture> SceneRenderer::getOrCreateORMTexture(const std::string& roughnessPath, const std::string& metallicPath)
	{
		std::string cacheKey = "orm:" + roughnessPath + "|" + metallicPath;
		auto it = m_textureCache.find(cacheKey);
		if (it != m_textureCache.end())
		{
			return it->second;
		}

		auto loadSingleImage = [](const std::string& path, DirectX::TexMetadata& meta, DirectX::ScratchImage& scratch) -> bool {
			if (path.empty()) return false;
			auto binRes = Core::AssetPathResolver::readBinary(path);
			if (!binRes.isOk() || binRes.value().empty()) return false;
			const auto& bytes = binRes.value();

			std::string ext;
			auto dot = path.find_last_of('.');
			if (dot != std::string::npos)
			{
				ext = path.substr(dot);
				std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
			}

			HRESULT hr = E_FAIL;
			if (ext == ".dds") hr = DirectX::LoadFromDDSMemory(bytes.data(), bytes.size(), DirectX::DDS_FLAGS_NONE, &meta, scratch);
			else if (ext == ".tga") hr = DirectX::LoadFromTGAMemory(bytes.data(), bytes.size(), &meta, scratch);
			else
			{
				EnsureWICCOMInitialized();
				hr = DirectX::LoadFromWICMemory(bytes.data(), bytes.size(), DirectX::WIC_FLAGS_NONE, &meta, scratch);
			}

			return SUCCEEDED(hr) && scratch.GetImageCount() > 0;
		};

		DirectX::TexMetadata roughMeta{};
		DirectX::ScratchImage roughScratch;
		bool hasRough = loadSingleImage(roughnessPath, roughMeta, roughScratch);

		DirectX::TexMetadata metalMeta{};
		DirectX::ScratchImage metalScratch;
		bool hasMetal = loadSingleImage(metallicPath, metalMeta, metalScratch);

		size_t width = 1;
		size_t height = 1;
		if (hasRough)
		{
			width = std::max(width, roughMeta.width);
			height = std::max(height, roughMeta.height);
		}
		if (hasMetal)
		{
			width = std::max(width, metalMeta.width);
			height = std::max(height, metalMeta.height);
		}

		DirectX::ScratchImage ormImage;
		HRESULT hr = ormImage.Initialize2D(DXGI_FORMAT_R8G8B8A8_UNORM, width, height, 1, 1);
		if (FAILED(hr))
		{
			DF_LOG_ERROR("Renderer", "Failed to initialize ORM texture scratch image ({}x{})", width, height);
			return nullptr;
		}

		const DirectX::Image* dstImage = ormImage.GetImage(0, 0, 0);
		const DirectX::Image* roughImg = hasRough ? roughScratch.GetImage(0, 0, 0) : nullptr;
		const DirectX::Image* metalImg = hasMetal ? metalScratch.GetImage(0, 0, 0) : nullptr;

		auto getByteSample = [](const DirectX::Image* img, DXGI_FORMAT fmt, size_t x, size_t y, size_t w, size_t h, uint8_t defVal) -> uint8_t {
			if (!img || w == 0 || h == 0) return defVal;
			size_t sx = x * img->width / w;
			size_t sy = y * img->height / h;
			if (sx >= img->width) sx = img->width - 1;
			if (sy >= img->height) sy = img->height - 1;

			const uint8_t* row = img->pixels + sy * img->rowPitch;
			if (fmt == DXGI_FORMAT_R8_UNORM || fmt == DXGI_FORMAT_A8_UNORM || fmt == DXGI_FORMAT_R8_UINT)
			{
				return row[sx];
			}
			size_t bpp = DirectX::BitsPerPixel(fmt) / 8;
			if (bpp >= 3)
			{
				return row[sx * bpp + 0];
			}
			if (bpp >= 1)
			{
				return row[sx * bpp];
			}
			return defVal;
		};

		bool isConstantMetal = !hasMetal || (metalMeta.width <= 1 && metalMeta.height <= 1);
		uint8_t constMetalVal = isConstantMetal ? (hasMetal ? getByteSample(metalImg, metalMeta.format, 0, 0, 1, 1, 0) : 0) : 0;

		bool directRough = hasRough && (roughMeta.width == width && roughMeta.height == height);
		bool isRoughR8 = directRough && (roughMeta.format == DXGI_FORMAT_R8_UNORM || roughMeta.format == DXGI_FORMAT_A8_UNORM || roughMeta.format == DXGI_FORMAT_R8_UINT);
		size_t roughBpp = directRough ? (DirectX::BitsPerPixel(roughMeta.format) / 8) : 1;

		bool isFloor = (roughnessPath.find("Floor") != std::string::npos || roughnessPath.find("floor") != std::string::npos);
		bool isWall = (roughnessPath.find("Brick") != std::string::npos || roughnessPath.find("brick") != std::string::npos ||
		               roughnessPath.find("Arch") != std::string::npos || roughnessPath.find("arch") != std::string::npos ||
		               roughnessPath.find("Column") != std::string::npos || roughnessPath.find("column") != std::string::npos);

		for (size_t y = 0; y < height; ++y)
		{
			uint8_t* dstRow = dstImage->pixels + y * dstImage->rowPitch;
			const uint8_t* roughRow = directRough ? (roughImg->pixels + y * roughImg->rowPitch) : nullptr;
			const uint8_t* metalRow = (!isConstantMetal && hasMetal && metalMeta.width == width && metalMeta.height == height) ? (metalImg->pixels + y * metalImg->rowPitch) : nullptr;
			size_t metalBpp = metalRow ? (DirectX::BitsPerPixel(metalMeta.format) / 8) : 1;

			for (size_t x = 0; x < width; ++x)
			{
				uint8_t rVal;
				if (isRoughR8)
				{
					rVal = roughRow[x];
				}
				else if (roughRow)
				{
					rVal = roughRow[x * roughBpp];
				}
				else
				{
					rVal = getByteSample(roughImg, roughMeta.format, x, y, width, height, 128);
				}

				if (isFloor)
				{
					// Unity Floor _SmoothnessRemapMax = 0.7328 -> min roughness 0.32 (prevents wet mirror look)
					rVal = std::max(rVal, static_cast<uint8_t>(82));
				}
				else if (isWall)
				{
					// Prevents walls acting like coated with water
					rVal = std::max(rVal, static_cast<uint8_t>(110));
				}

				uint8_t mVal;
				if (isConstantMetal)
				{
					mVal = constMetalVal;
				}
				else if (metalRow)
				{
					mVal = metalRow[x * metalBpp];
				}
				else
				{
					mVal = getByteSample(metalImg, metalMeta.format, x, y, width, height, 0);
				}

				// Pack ORM: R = Occlusion (255), G = Roughness, B = Metallic, A = 255
				dstRow[x * 4 + 0] = 255;
				dstRow[x * 4 + 1] = rVal;
				dstRow[x * 4 + 2] = mVal;
				dstRow[x * 4 + 3] = 255;
			}
		}

		DirectX::ScratchImage mipChain;
		if (width > 1 && height > 1)
		{
			DirectX::GenerateMipMaps(*dstImage, DirectX::TEX_FILTER_DEFAULT, 0, mipChain);
		}

		const DirectX::ScratchImage& finalScratch = (mipChain.GetImageCount() > 0) ? mipChain : ormImage;
		const auto& finalMeta = finalScratch.GetMetadata();

		std::vector<RHI::SubresourceData> subresources(finalMeta.mipLevels);
		for (size_t i = 0; i < finalMeta.mipLevels; ++i)
		{
			const DirectX::Image* img = finalScratch.GetImage(i, 0, 0);
			if (img)
			{
				subresources[i].data = img->pixels;
				subresources[i].rowPitch = static_cast<uint32_t>(img->rowPitch);
				subresources[i].slicePitch = static_cast<uint32_t>(img->slicePitch);
			}
		}

		RHI::GpuTextureDesc texDesc{};
		texDesc.width = static_cast<uint32_t>(finalMeta.width);
		texDesc.height = static_cast<uint32_t>(finalMeta.height);
		texDesc.mipLevels = static_cast<uint32_t>(finalMeta.mipLevels);
		texDesc.arraySize = 1;
		texDesc.format = RHI::PixelFormat::R8G8B8A8_UNORM;
		texDesc.usage = RHI::BufferUsage::Default;
		texDesc.bindFlags = RHI::BufferBindFlags::ShaderResource;

		auto texRes = m_device.createTextureSubresources(texDesc, subresources);
		if (texRes.isOk())
		{
			m_textureCache[cacheKey] = *texRes;
			DF_LOG_INFO("Renderer", "Generated ORM texture '{}' ({}x{}, {} mips)", cacheKey, texDesc.width, texDesc.height, texDesc.mipLevels);
			return *texRes;
		}

		DF_LOG_WARN("Renderer", "Failed to create GPU ORM texture for '{}': {}", cacheKey, texRes.error());
		return nullptr;
	}

	bool SceneRenderer::saveScreenshot(const std::string& filePath, RHI::ISwapChain* swapChain) const
	{
#ifdef _WIN32
		EnsureWICCOMInitialized();
		auto* nativeDev = static_cast<ID3D11Device*>(m_device.getNativeDevice());
		auto* nativeCtx = static_cast<ID3D11DeviceContext*>(m_device.getNativeContext());
		if (!nativeDev || !nativeCtx)
		{
			return false;
		}

		std::shared_ptr<RHI::IGpuTexture> srcTex = m_finalLdrTexture;
		if (swapChain && swapChain->getBackBufferTexture())
		{
			srcTex = swapChain->getBackBufferTexture();
		}
		if (!srcTex || !srcTex->getNativeHandle())
		{
			return false;
		}

		auto* nativeRes = static_cast<ID3D11Resource*>(srcTex->getNativeHandle());
		DirectX::ScratchImage captured;
		HRESULT hr = DirectX::CaptureTexture(nativeDev, nativeCtx, nativeRes, captured);
		if (FAILED(hr) || captured.GetImageCount() == 0)
		{
			DF_LOG_WARN("Renderer", "CaptureTexture failed: 0x{:08X}", static_cast<uint32_t>(hr));
			return false;
		}

		// Force opaque alpha = 255 in saved PNG (in case Tonemap packed luma into alpha when FXAA is off)
		const DirectX::Image* img = captured.GetImage(0, 0, 0);
		if (img && (img->format == DXGI_FORMAT_R8G8B8A8_UNORM || img->format == DXGI_FORMAT_R8G8B8A8_UNORM_SRGB || img->format == DXGI_FORMAT_B8G8R8A8_UNORM))
		{
			for (size_t y = 0; y < img->height; ++y)
			{
				uint8_t* row = img->pixels + y * img->rowPitch;
				for (size_t x = 0; x < img->width; ++x)
				{
					row[x * 4 + 3] = 255;
				}
			}
		}

		std::filesystem::path outPath(filePath);
		if (outPath.has_parent_path())
		{
			std::error_code ec;
			std::filesystem::create_directories(outPath.parent_path(), ec);
		}

		hr = DirectX::SaveToWICFile(
			*img,
			DirectX::WIC_FLAGS_NONE,
			DirectX::GetWICCodec(DirectX::WIC_CODEC_PNG),
			outPath.wstring().c_str()
		);
		if (SUCCEEDED(hr))
		{
			DF_LOG_INFO("Renderer", "Saved screenshot to '{}'", filePath);
			return true;
		}
		DF_LOG_WARN("Renderer", "SaveToWICFile failed for '{}': 0x{:08X}", filePath, static_cast<uint32_t>(hr));
#else
		(void)filePath;
		(void)swapChain;
#endif
		return false;
	}
}
