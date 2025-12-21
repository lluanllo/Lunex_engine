#pragma once

#include <string>

#include "Core/Core.h"
#include "TextureCompression.h"
#include "RHI/RHITexture.h"

namespace Lunex {
	
	// Forward declaration
	struct CompressedTextureData;
	struct TextureImportSettings;
	
	class Texture {
		public:
			virtual ~Texture() = default;
			
			virtual uint32_t GetWidth() const = 0;
			virtual uint32_t GetHeight() const = 0;
			virtual uint32_t GetRendererID() const = 0;
			
			virtual const std::string& GetPath() const = 0;
			
			virtual void SetData(void* data, uint32_t size) = 0;
			
			virtual void Bind(uint32_t slot = 0) const = 0;
			
			virtual bool IsLoaded() const = 0;
			
			// ========== COMPRESSION SUPPORT =
			
			virtual bool IsCompressed() const = 0;
			virtual TextureCompressionFormat GetCompressionFormat() const = 0;
			virtual uint32_t GetMipLevelCount() const = 0;
			
			// ========== RHI ACCESS ==========
			virtual RHI::RHITexture2D* GetRHITexture() const = 0;
			
			virtual bool operator==(const Texture& other) const = 0;
	};
	
	/**
	 * @class Texture2D
	 * @brief 2D texture that uses RHI internally
	 */
	class Texture2D : public Texture {
	public:
		Texture2D() = default;
		Texture2D(uint32_t width, uint32_t height);
		Texture2D(const std::string& path);
		Texture2D(const std::string& path, const TextureImportSettings& settings);
		Texture2D(const CompressedTextureData& compressedData);
		virtual ~Texture2D() = default;
		
		uint32_t GetWidth() const override { return m_Width; }
		uint32_t GetHeight() const override { return m_Height; }
		uint32_t GetRendererID() const override;
		
		const std::string& GetPath() const override { return m_Path; }
		
		void SetData(void* data, uint32_t size) override;
		void Bind(uint32_t slot = 0) const override;
		
		bool IsLoaded() const override { return m_IsLoaded; }
		
		bool IsCompressed() const override { return m_IsCompressed; }
		TextureCompressionFormat GetCompressionFormat() const override { return m_CompressionFormat; }
		uint32_t GetMipLevelCount() const override { return m_MipLevels; }
		
		RHI::RHITexture2D* GetRHITexture() const override { return m_RHITexture.get(); }
		
		bool operator==(const Texture& other) const override;
		
		// Factory methods
		static Ref<Texture2D> Create(uint32_t width, uint32_t height);
		static Ref<Texture2D> Create(const std::string& path);
		static Ref<Texture2D> Create(const std::string& path, const TextureImportSettings& settings);
		static Ref<Texture2D> CreateCompressed(const CompressedTextureData& compressedData);
		
	private:
		void LoadFromFile(const std::string& path);
		void LoadFromFileWithSettings(const std::string& path, const TextureImportSettings& settings);
		void LoadFromCompressedData(const CompressedTextureData& data);
		
		Ref<RHI::RHITexture2D> m_RHITexture;
		
		std::string m_Path;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		uint32_t m_MipLevels = 1;
		bool m_IsLoaded = false;
		bool m_IsCompressed = false;
		TextureCompressionFormat m_CompressionFormat = TextureCompressionFormat::None;
	};
}