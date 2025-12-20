#include "stpch.h"
#include "Shader.h"
#include "RHI/RHIShader.h"

namespace Lunex {
	
	Ref<Shader> Shader::Create(const std::string& filepath) {
		auto rhiShader = RHI::RHIShader::CreateFromFile(filepath);
		return CreateRef<Shader>(rhiShader);
	}
	
	Ref<Shader> Shader::Create(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc) {
		auto rhiShader = RHI::RHIShader::CreateFromSource(name, vertexSrc, fragmentSrc);
		return CreateRef<Shader>(rhiShader);
	}
	
	Ref<Shader> Shader::CreateCompute(const std::string& filepath) {
		auto rhiShader = RHI::RHIShader::CreateComputeFromFile(filepath);
		return CreateRef<Shader>(rhiShader);
	}
	
	void ShaderLibrary::Add(const Ref<Shader>& shader) {
		auto& name = shader->GetName();
		Add(name, shader);
	}
	
	void ShaderLibrary::Add(const std::string& name, const Ref<Shader>& shader) {
		LNX_CORE_ASSERT(!Exists(name), "Shader already exists!");
		m_Shaders[name] = shader;
	}
	
	Ref<Shader> ShaderLibrary::Load(const std::string& filepath) {
		auto shader = Shader::Create(filepath);
		Add(shader);
		return shader;
	}
	
	Ref<Shader> ShaderLibrary::Load(const std::string& name, const std::string& filepath) {
		auto shader = Shader::Create(filepath);
		Add(name, shader);
		return shader;
	}
	
	Ref<Shader> ShaderLibrary::Get(const std::string& name) {
		LNX_CORE_ASSERT(m_Shaders.find(name) != m_Shaders.end(), "Shader not found!");
		return m_Shaders[name];
	}
	
	bool ShaderLibrary::Exists(const std::string& name) const {
		return m_Shaders.find(name) != m_Shaders.end();
	}
}