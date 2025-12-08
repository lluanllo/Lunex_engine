#include "stpch.h"
#include "TextureCube.h"
#include "Renderer/RendererAPI.h"
#include "Platform/OpenGL/OpenGLTextureCube.h"

namespace Lunex {
	
	Ref<TextureCube> TextureCube::Create(const std::array<std::string, 6>& facePaths) {
		switch (RendererAPI::GetAPI()) {
			case RendererAPI::API::None:
				LNX_CORE_ASSERT(false, "RendererAPI::None is currently not supported!");
				return nullptr;
			case RendererAPI::API::OpenGL:
				return CreateRef<OpenGLTextureCube>(facePaths);
		}
		
		LNX_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
	
	Ref<TextureCube> TextureCube::Create(uint32_t size, bool hdr, uint32_t mipLevels) {
		switch (RendererAPI::GetAPI()) {
			case RendererAPI::API::None:
				LNX_CORE_ASSERT(false, "RendererAPI::None is currently not supported!");
				return nullptr;
			case RendererAPI::API::OpenGL:
				return CreateRef<OpenGLTextureCube>(size, hdr, mipLevels);
		}
		
		LNX_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
	
	Ref<TextureCube> TextureCube::CreateFromHDRI(const std::string& hdriPath, uint32_t resolution) {
		switch (RendererAPI::GetAPI()) {
			case RendererAPI::API::None:
				LNX_CORE_ASSERT(false, "RendererAPI::None is currently not supported!");
				return nullptr;
			case RendererAPI::API::OpenGL:
				return OpenGLTextureCube::CreateFromHDRI(hdriPath, resolution);
		}
		
		LNX_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
	
}
