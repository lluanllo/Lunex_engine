#include "stpch.h"
#include "OpenGLComputeShader.h"

#include <fstream>
#include <glad/glad.h>
#include <glm/gtc/type_ptr.hpp>

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include "Log/Log.h"

namespace Lunex {
	
	OpenGLComputeShader::OpenGLComputeShader(const std::string& filepath)
		: m_FilePath(filepath) {
		LNX_PROFILE_FUNCTION();
		
		std::string source = ReadFile(filepath);
		CompileShader(source);
		
		// Extract name from filepath
		auto lastSlash = filepath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		auto lastDot = filepath.rfind('.');
		auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
		m_Name = filepath.substr(lastSlash, count);
		
		LNX_LOG_INFO("Compute Shader '{0}' created successfully", m_Name);
	}
	
	OpenGLComputeShader::OpenGLComputeShader(const std::string& name, const std::string& source)
		: m_Name(name) {
		LNX_PROFILE_FUNCTION();
		CompileShader(source);
		LNX_LOG_INFO("Compute Shader '{0}' created successfully", m_Name);
	}
	
	OpenGLComputeShader::~OpenGLComputeShader() {
		LNX_PROFILE_FUNCTION();
		glDeleteProgram(m_RendererID);
	}
	
	std::string OpenGLComputeShader::ReadFile(const std::string& filepath) {
		LNX_PROFILE_FUNCTION();
		
		std::string result;
		std::ifstream in(filepath, std::ios::in | std::ios::binary);
		
		if (in) {
			in.seekg(0, std::ios::end);
			size_t size = in.tellg();
			if (size != -1) {
				result.resize(size);
				in.seekg(0, std::ios::beg);
				in.read(&result[0], size);
			}
			else {
				LNX_LOG_ERROR("Could not read from file '{0}'", filepath);
			}
		}
		else {
			LNX_LOG_ERROR("Could not open file '{0}'", filepath);
		}
		
		return result;
	}
	
	void OpenGLComputeShader::CompileShader(const std::string& source) {
		LNX_PROFILE_FUNCTION();
		
		// Compile GLSL to SPIR-V using shaderc
		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_opengl, shaderc_env_version_opengl_4_5);
		options.SetOptimizationLevel(shaderc_optimization_level_performance);
		
		shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(
			source,
			shaderc_compute_shader,
			m_FilePath.c_str(),
			options
		);
		
		if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
			LNX_LOG_ERROR("Compute shader compilation failed: {0}", module.GetErrorMessage());
			LNX_CORE_ASSERT(false, "Shader compilation failed!");
			return;
		}
		
		std::vector<uint32_t> spirv(module.cbegin(), module.cend());
		
		// Cross-compile SPIR-V to GLSL
		spirv_cross::CompilerGLSL glslCompiler(spirv);
		spirv_cross::CompilerGLSL::Options glslOptions;
		glslOptions.version = 450;
		glslOptions.es = false;
		glslOptions.vulkan_semantics = false;
		glslCompiler.set_common_options(glslOptions);
		
		std::string glslSource = glslCompiler.compile();
		
		// Compile GLSL compute shader
		GLuint shader = glCreateShader(GL_COMPUTE_SHADER);
		const GLchar* sourceCStr = glslSource.c_str();
		glShaderSource(shader, 1, &sourceCStr, nullptr);
		glCompileShader(shader);
		
		GLint isCompiled = 0;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE) {
			GLint maxLength = 0;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);
			
			std::vector<GLchar> infoLog(maxLength);
			glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);
			
			glDeleteShader(shader);
			
			LNX_LOG_ERROR("Compute shader compilation failed:\n{0}", infoLog.data());
			LNX_CORE_ASSERT(false, "Shader compilation failed!");
			return;
		}
		
		// Create program
		m_RendererID = glCreateProgram();
		glAttachShader(m_RendererID, shader);
		glLinkProgram(m_RendererID);
		
		GLint isLinked = 0;
		glGetProgramiv(m_RendererID, GL_LINK_STATUS, &isLinked);
		if (isLinked == GL_FALSE) {
			GLint maxLength = 0;
			glGetProgramiv(m_RendererID, GL_INFO_LOG_LENGTH, &maxLength);
			
			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(m_RendererID, maxLength, &maxLength, &infoLog[0]);
			
			glDeleteProgram(m_RendererID);
			glDeleteShader(shader);
			
			LNX_LOG_ERROR("Compute shader linking failed:\n{0}", infoLog.data());
			LNX_CORE_ASSERT(false, "Shader link failed!");
			return;
		}
		
		glDetachShader(m_RendererID, shader);
		glDeleteShader(shader);
		
		// Reflect
		Reflect(spirv);
	}
	
	void OpenGLComputeShader::Reflect(const std::vector<uint32_t>& shaderData) {
		spirv_cross::Compiler compiler(shaderData);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();
		
		LNX_LOG_TRACE("OpenGLComputeShader::Reflect - {0}", m_Name);
		LNX_LOG_TRACE("    {0} uniform buffers", resources.uniform_buffers.size());
		LNX_LOG_TRACE("    {0} storage buffers", resources.storage_buffers.size());
		LNX_LOG_TRACE("    {0} storage images", resources.storage_images.size());
	}
	
	void OpenGLComputeShader::Bind() const {
		LNX_PROFILE_FUNCTION();
		glUseProgram(m_RendererID);
	}
	
	void OpenGLComputeShader::Unbind() const {
		LNX_PROFILE_FUNCTION();
		glUseProgram(0);
	}
	
	void OpenGLComputeShader::Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) {
		LNX_PROFILE_FUNCTION();
		glDispatchCompute(groupsX, groupsY, groupsZ);
	}
	
	void OpenGLComputeShader::MemoryBarrier(uint32_t barriers) {
		LNX_PROFILE_FUNCTION();
		glMemoryBarrier(barriers);
	}
	
	void OpenGLComputeShader::SetInt(const std::string& name, int value) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1i(location, value);
	}
	
	void OpenGLComputeShader::SetFloat(const std::string& name, float value) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1f(location, value);
	}
	
	void OpenGLComputeShader::SetFloat3(const std::string& name, const glm::vec3& value) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform3f(location, value.x, value.y, value.z);
	}
	
	void OpenGLComputeShader::SetMat4(const std::string& name, const glm::mat4& value) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(value));
	}
}
