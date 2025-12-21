#include "stpch.h"
#include "TextureCube.h"
#include "RHI/RHI.h"
#include "RHI/OpenGL/OpenGLRHITextureCube.h"

namespace Lunex {
	
	Ref<TextureCube> TextureCube::Create(const std::array<std::string, 6>& facePaths) {
		switch (RHI::GetCurrentAPI()) {
			case RHI::GraphicsAPI::None:
				LNX_CORE_ASSERT(false, "RHI::GraphicsAPI::None is currently not supported!");
				return nullptr;
			case RHI::GraphicsAPI::OpenGL:
				return CreateRef<OpenGLTextureCube>(facePaths);
			default:
				break;
		}
		
		LNX_CORE_ASSERT(false, "Unknown RHI::GraphicsAPI!");
		return nullptr;
	}
	
	Ref<TextureCube> TextureCube::Create(uint32_t size, bool hdr, uint32_t mipLevels) {
		switch (RHI::GetCurrentAPI()) {
			case RHI::GraphicsAPI::None:
				LNX_CORE_ASSERT(false, "RHI::GraphicsAPI::None is currently not supported!");
				return nullptr;
			case RHI::GraphicsAPI::OpenGL:
				return CreateRef<OpenGLTextureCube>(size, hdr, mipLevels);
			default:
				break;
		}
		
		LNX_CORE_ASSERT(false, "Unknown RHI::GraphicsAPI!");
		return nullptr;
	}
	
	Ref<TextureCube> TextureCube::CreateFromHDRI(const std::string& hdriPath, uint32_t resolution) {
		switch (RHI::GetCurrentAPI()) {
			case RHI::GraphicsAPI::None:
				LNX_CORE_ASSERT(false, "RHI::GraphicsAPI::None is currently not supported!");
				return nullptr;
			case RHI::GraphicsAPI::OpenGL:
				return OpenGLTextureCube::CreateFromHDRI(hdriPath, resolution);
			default:
				break;
		}
		
		LNX_CORE_ASSERT(false, "Unknown RHI::GraphicsAPI!");
		return nullptr;
	}
	
}
