#include "stpch.h"
#include "ComputeShader.h"

#include "Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLComputeShader.h"

namespace Lunex {
	Ref<ComputeShader> ComputeShader::Create(const std::string& filepath) {
		switch (Renderer::GetAPI()) {
			case RendererAPI::API::None:    
				LNX_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); 
				return nullptr;
			case RendererAPI::API::OpenGL:  
				return CreateRef<OpenGLComputeShader>(filepath);
		}
		
		LNX_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
	
	Ref<ComputeShader> ComputeShader::Create(const std::string& name, const std::string& source) {
		switch (Renderer::GetAPI()) {
			case RendererAPI::API::None:    
				LNX_CORE_ASSERT(false, "RendererAPI::None is currently not supported!"); 
				return nullptr;
			case RendererAPI::API::OpenGL:  
				return CreateRef<OpenGLComputeShader>(name, source);
		}
		
		LNX_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
}
