#include "stpch.h"
#include "Shader.h"

#include <glad/glad.h>

#include "Renderer/Renderer.h"
#include "Platform/OpenGL/OpenGLShader.h"

namespace Lunex {
	
	Ref<Shader> Shader::Create(const std::string& filepath) {
		switch (Renderer::GetAPI()) {
			case RendererAPI::API::None:    LN_CORE_ASSERT(false, "RendererAPI::None is currently not supported!");
			case RendererAPI::API::OpenGL:  std::make_shared<OpenGLShader>(filepath);
		}
		
		LN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
	
	Ref<Shader> Shader::Create(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc) {
		switch (Renderer::GetAPI()) {
			case RendererAPI::API::None:    LN_CORE_ASSERT(false, "RendererAPI::None is currently not supported!");
			case RendererAPI::API::OpenGL:  std::make_shared<OpenGLShader>(name, vertexSrc, fragmentSrc);
		}
		
		LN_CORE_ASSERT(false, "Unknown RendererAPI!");
		return nullptr;
	}
	
	void SahderLibrary::Add(const Ref<Shader>& shader) {
		auto& name = shader->GetName();
		Add(name, shader);
	}

	void SahderLibrary::Add(const std::string& name, const Ref<Shader>& shader) {
		LN_CORE_ASSERT(!Exists(name), "Shader already exists!");
		m_Shaders[name] = shader;
	}
	
	Ref<Shader> SahderLibrary::Load(const std::string& filepath) {
		auto shader = Ref<Shader>(Shader::Create(filepath));
		Add(shader);
		return shader;
	}
	
	Ref<Shader> SahderLibrary::Load(const std::string& name, const std::string& filepath) {
		auto shader = Shader::Create(filepath);
		Add(name, shader);
		return shader;
	}
	
	Ref<Shader> SahderLibrary::Get(const std::string& name) {
		LN_CORE_ASSERT(m_Shaders.find(name) != m_Shaders.end(), "Shader not found!");
		return m_Shaders[name];
	}
	
	bool SahderLibrary::Exists(const std::string& name) const {
		return m_Shaders.find(name) != m_Shaders.end();
	}
}