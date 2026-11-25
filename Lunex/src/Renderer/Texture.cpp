#include "stpch.h"
#include "Texture.h"
#include "TextureCompression.h"
#include "RHI/RHIDevice.h"
#include "Log/Log.h"

#include "stb_image.h"

namespace Lunex {

	// ============================================================================
	// TEXTURE2D IMPLEMENTATION (uses RHI directly)
	// ============================================================================
	
	Texture2D::Texture2D(uint32_t width, uint32_t height)
		: m_Width(width), m_Height(height), m_IsLoaded(true)
	{
		RHI::TextureCreateInfo info;
		info.Width = width;
		info.Height = height;
		info.Format = RHI::TextureFormat::RGBA8;
		info.GenerateMipmaps = false;
		
		m_RHITexture = RHI::RHIDevice::Get()->CreateTexture2D(info);
		m_MipLevels = 1;
	}
	
	Texture2D::Texture2D(const std::string& path)
		: m_Path(path)
	{
		LoadFromFile(path);
	}
	
	Texture2D::Texture2D(const std::string& path, const TextureImportSettings& settings)
		: m_Path(path)
	{
		LoadFromFileWithSettings(path, settings);
	}
	
	Texture2D::Texture2D(const CompressedTextureData& compressedData) {
		LoadFromCompressedData(compressedData);
	}
	
	void Texture2D::LoadFromFile(const std::string& path) {
		LNX_PROFILE_FUNCTION();
		
		m_IsLoaded = false;
		m_Path = path;
		
		// Try compression if available
		auto& config = TextureCompressionConfig::Get();
		if (config.EnableAutoCompression && TextureCompressionConfig::IsKTXAvailable()) {
			TextureImportSettings settings = config.GetDefaultSettings();
			if (settings.CompressionFormat != TextureCompressionFormat::None) {
				auto& compressor = TextureCompressor::Get();
				if (!compressor.IsInitialized()) {
					compressor.Initialize();
				}
				
				CompressedTextureData compressedData = compressor.CompressFromFile(path, settings);
				if (compressedData.IsValid()) {
					LoadFromCompressedData(compressedData);
					return;
				}
			}
		}
		
		// Standard loading: CPU decode + GPU upload
		DecodedImage image = DecodeFromFile(path);
		if (image.IsValid()) {
			UploadDecoded(image);
		}
		else {
			m_IsLoaded = false;
		}
	}

	Texture2D::DecodedImage Texture2D::DecodeFromFile(const std::string& path) {
		DecodedImage image;
		image.Path = path;

		int width = 0, height = 0, channels = 0;
		// Thread-local flip flag: safe when several workers decode at once
		stbi_set_flip_vertically_on_load_thread(1);
		stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
		if (!data) {
			LNX_LOG_ERROR("Failed to load texture: {0} - {1}", path, stbi_failure_reason());
			return image;
		}

		image.Width = width;
		image.Height = height;
		const size_t pixelCount = static_cast<size_t>(width) * height;

		if (channels == 1) {
			image.Channels = 1;
			image.Pixels.assign(data, data + pixelCount);
		}
		else {
			// Expand to RGBA (handles 2, 3 and 4 channel sources)
			image.Channels = 4;
			image.Pixels.resize(pixelCount * 4);
			for (size_t i = 0; i < pixelCount; i++) {
				const stbi_uc* src = data + i * channels;
				uint8_t* dst = image.Pixels.data() + i * 4;
				if (channels == 2) {
					dst[0] = dst[1] = dst[2] = src[0];
					dst[3] = src[1];
				}
				else {
					dst[0] = src[0];
					dst[1] = src[1];
					dst[2] = src[2];
					dst[3] = (channels == 4) ? src[3] : 255;
				}
			}
		}

		stbi_image_free(data);
		return image;
	}

	void Texture2D::UploadDecoded(const DecodedImage& image) {
		auto& config = TextureCompressionConfig::Get();

		m_Path = image.Path;
		m_Width = image.Width;
		m_Height = image.Height;
		m_IsLoaded = true;

		m_MipLevels = config.GenerateMipmaps
			? static_cast<uint32_t>(std::floor(std::log2(std::max(image.Width, image.Height)))) + 1
			: 1;

		RHI::TextureCreateInfo info;
		info.Width = image.Width;
		info.Height = image.Height;
		info.Format = (image.Channels == 1) ? RHI::TextureFormat::R8 : RHI::TextureFormat::RGBA8;
		info.MipLevels = m_MipLevels;
		info.GenerateMipmaps = config.GenerateMipmaps;
		info.InitialData = image.Pixels.data();
		info.InitialDataSize = image.Pixels.size();

		m_RHITexture = RHI::RHIDevice::Get()->CreateTexture2D(info);
	}

	Ref<Texture2D> Texture2D::CreateFromDecoded(const DecodedImage& image) {
		auto texture = CreateRef<Texture2D>();
		if (image.IsValid()) {
			texture->UploadDecoded(image);
		}
		return texture;
	}
	
	void Texture2D::LoadFromFileWithSettings(const std::string& path, const TextureImportSettings& settings) {
		LNX_PROFILE_FUNCTION();
		
		m_IsLoaded = false;
		m_Path = path;
		
		// Try compressed cache first
		if (settings.UseCache && settings.CompressionFormat != TextureCompressionFormat::None 
			&& TextureCompressionConfig::IsKTXAvailable()) {
			auto& compressor = TextureCompressor::Get();
			if (!compressor.IsInitialized()) {
				compressor.Initialize();
			}
			
			CompressedTextureData compressedData = compressor.CompressFromFile(path, settings);
			if (compressedData.IsValid()) {
				LoadFromCompressedData(compressedData);
				return;
			}
		}
		
		// Standard loading with settings
		int width, height, channels;
		stbi_set_flip_vertically_on_load(1);
		stbi_uc* data = stbi_load(path.c_str(), &width, &height, &channels, 0);
		
		if (data) {
			m_Width = width;
			m_Height = height;
			m_IsLoaded = true;
			
			// Determine format based on settings
			RHI::TextureFormat format = settings.IsSRGB ? RHI::TextureFormat::SRGBA8 : RHI::TextureFormat::RGBA8;
			
			// Convert to RGBA if needed
			uint8_t* finalData = data;
			size_t finalSize = width * height * 4;
			bool needsConversion = (channels != 4);
			
			if (needsConversion) {
				finalData = new uint8_t[finalSize];
				for (int i = 0; i < width * height; i++) {
					if (channels >= 3) {
						finalData[i * 4 + 0] = data[i * channels + 0];
						finalData[i * 4 + 1] = data[i * channels + 1];
						finalData[i * 4 + 2] = data[i * channels + 2];
					} else {
						finalData[i * 4 + 0] = data[i];
						finalData[i * 4 + 1] = data[i];
						finalData[i * 4 + 2] = data[i];
					}
					finalData[i * 4 + 3] = (channels == 4) ? data[i * channels + 3] : 255;
				}
			}
			
			// Calculate mip levels
			m_MipLevels = settings.GenerateMipmaps 
				? static_cast<uint32_t>(std::floor(std::log2(std::max(width, height)))) + 1 
				: 1;
			if (settings.MaxMipLevels > 0) {
				m_MipLevels = std::min(m_MipLevels, static_cast<uint32_t>(settings.MaxMipLevels));
			}
			
			// Create RHI texture
			RHI::TextureCreateInfo info;
			info.Width = width;
			info.Height = height;
			info.Format = format;
			info.MipLevels = m_MipLevels;
			info.GenerateMipmaps = settings.GenerateMipmaps;
			info.InitialData = finalData;
			info.InitialDataSize = finalSize;
			
			m_RHITexture = RHI::RHIDevice::Get()->CreateTexture2D(info);
			
			if (needsConversion) {
				delete[] finalData;
			}
			stbi_image_free(data);
		}
		else {
			LNX_LOG_ERROR("Failed to load texture: {0} - {1}", path, stbi_failure_reason());
			m_IsLoaded = false;
		}
	}
	
	void Texture2D::LoadFromCompressedData(const CompressedTextureData& data) {
		if (!data.IsValid()) {
			LNX_LOG_ERROR("Texture2D - Invalid compressed data");
			m_IsLoaded = false;
			return;
		}
		
		m_Width = data.Width;
		m_Height = data.Height;
		m_IsCompressed = data.Format != TextureCompressionFormat::None;
		m_CompressionFormat = data.Format;
		m_MipLevels = data.GetMipCount();
		m_IsLoaded = true;
		
		// Create RHI texture from compressed data
		RHI::TextureCreateInfo info;
		info.Width = data.Width;
		info.Height = data.Height;
		info.Format = static_cast<RHI::TextureFormat>(data.InternalFormat);
		info.MipLevels = m_MipLevels;
		info.InitialData = data.Data.data();
		info.InitialDataSize = data.Data.size();
		
		m_RHITexture = RHI::RHIDevice::Get()->CreateTexture2D(info);
		
		LNX_LOG_TRACE("Loaded compressed texture: {0}x{1}, Format: {2}, Mips: {3}",
			m_Width, m_Height, 
			TextureCompressor::GetFormatName(m_CompressionFormat),
			m_MipLevels);
	}
	
	uint32_t Texture2D::GetRendererID() const {
		return m_RHITexture ? m_RHITexture->GetRendererID() : 0;
	}
	
	void Texture2D::SetData(void* data, uint32_t size) {
		if (m_IsCompressed) {
			LNX_LOG_WARN("Cannot SetData on compressed texture");
			return;
		}
		
		if (m_RHITexture) {
			m_RHITexture->SetData(data, size);
		}
	}
	
	void Texture2D::Bind(uint32_t slot) const {
		if (m_RHITexture) {
			m_RHITexture->Bind(slot);
		}
	}
	
	bool Texture2D::operator==(const Texture& other) const {
		auto* otherTexture = dynamic_cast<const Texture2D*>(&other);
		if (!otherTexture) return false;
		return GetRendererID() == otherTexture->GetRendererID();
	}
	
	// ============================================================================
	// FACTORY METHODS
	// ============================================================================
	
	Ref<Texture2D> Texture2D::Create(uint32_t width, uint32_t height) {
		return CreateRef<Texture2D>(width, height);
	}

	Ref<Texture2D> Texture2D::Create(const std::string& path) {
		return CreateRef<Texture2D>(path);
	}
	
	Ref<Texture2D> Texture2D::Create(const std::string& path, const TextureImportSettings& settings) {
		return CreateRef<Texture2D>(path, settings);
	}
	
	Ref<Texture2D> Texture2D::CreateCompressed(const CompressedTextureData& compressedData) {
		return CreateRef<Texture2D>(compressedData);
	}
}