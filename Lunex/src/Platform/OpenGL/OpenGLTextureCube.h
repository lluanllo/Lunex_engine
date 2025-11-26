#pragma once

#include "Core/Core.h"
#include <array>
#include <string>

namespace Lunex {
	
	class OpenGLTextureCube {
	public:
		OpenGLTextureCube() = default; // Default constructor
		OpenGLTextureCube(const std::string& path);
		OpenGLTextureCube(const std::array<std::string, 6>& faces);
		~OpenGLTextureCube();

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }
		uint32_t GetRendererID() const { return m_RendererID; }
		
		void Bind(uint32_t slot = 0) const;
		
		bool IsLoaded() const { return m_IsLoaded; }
		bool IsHDR() const { return m_IsHDR; }

		// Static factory methods
		static Ref<OpenGLTextureCube> CreateFromHDR(const std::string& path);
		static Ref<OpenGLTextureCube> CreateFromFaces(const std::array<std::string, 6>& faces);

	private:
		void LoadFromSingleFile(const std::string& path);
		void LoadFromSixFiles(const std::array<std::string, 6>& faces);
		void LoadFromHDREquirectangular(const std::string& path);
		void ConvertEquirectangularToCubemap(uint32_t equirectangularMap, uint32_t resolution);

	private:
		uint32_t m_RendererID = 0;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		bool m_IsLoaded = false;
		bool m_IsHDR = false;
	};

}
