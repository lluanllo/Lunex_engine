#include "stpch.h"
#include "Shader.h"

#include <glad/glad.h>

#include "Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLShader.h"

namespace Lunex {
	
	Shader* Shader::Create(const std::string& filepath) {
		switch (Renderer::GetAPI()) {
			case RendererAPI::API::None:    LN_CORE_ASSERT(false, "RendererAPI::None is currently not supported!");
			case RendererAPI::API::OpenGL:  return new OpenGLShader(filepath);
		}
		
		LN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
	
	Shader* Shader::Create(const std::string& vertexSrc, const std::string& fragmentSrc) {
		switch (Renderer::GetAPI()) {
			case RendererAPI::API::None:    LN_CORE_ASSERT(false, "RendererAPI::None is currently not supported!");
			case RendererAPI::API::OpenGL:  return new OpenGLShader(vertexSrc, fragmentSrc);
		}
		
		LN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
	
	void SahderLibrary::Add(const Ref<Shader>& shader) {
		
	}
	
	Ref<Shader> SahderLibrary::Load(const std::string& filepath) {
		return Ref<Shader>();
	}
	
	Ref<Shader> SahderLibrary::Load(const std::string& name, const std::string& filepath) {
		return Ref<Shader>();
	}
	
	Ref<Shader> SahderLibrary::Get(const std::string& name) {
		return Ref<Shader>();
	}
}