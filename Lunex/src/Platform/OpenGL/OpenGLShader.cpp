#include "stpch.h"

#include <glad/glad.h>
#include "OpenGLShader.h"

#include <fstream>

#include <glm/gtc/type_ptr.hpp>

namespace Lunex {

	static GLenum ShaderTypeFromString(const std::string& type) {
		if (type == "vertex")
			return GL_VERTEX_SHADER;
		if (type == "fragment" || type == "pixel")
			return GL_FRAGMENT_SHADER;

		LN_CORE_ASSERT(false, "Unknown shader type!");
		return 0;
	}

	OpenGLShader::OpenGLShader(const std::string& filepath) {
		LNX_PROFILE_FUNCTION();
		std::string source = ReadFile(filepath);

		std::unordered_map<GLenum, std::string> shaderSources;
		shaderSources[GL_VERTEX_SHADER] = InsertDefineAfterVersion(source, "#define VERTEX\n");
		shaderSources[GL_FRAGMENT_SHADER] = InsertDefineAfterVersion(source, "#define FRAGMENT\n");

		Compile(shaderSources);

		auto lastSlash = filepath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		auto lastDot = filepath.rfind('.');
		auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
		m_Name = filepath.substr(lastSlash, count);
	}


	OpenGLShader::OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
		: m_Name(name) {
		LNX_PROFILE_FUNCTION();
		std::unordered_map<GLenum, std::string> sources;
		sources[GL_VERTEX_SHADER] = vertexSrc;
		sources[GL_FRAGMENT_SHADER] = fragmentSrc;
		Compile(sources);
	}

	OpenGLShader::~OpenGLShader() {
		LNX_PROFILE_FUNCTION();
		glDeleteProgram(m_RendererID);
	}

	std::string OpenGLShader::ReadFile(const std::string& filepath) {
		LNX_PROFILE_FUNCTION();
		std::string result;
		std::ifstream in(filepath, std::ios::in, std::ios::binary);

		if (in) {
			in.seekg(0, std::ios::end);
			result.resize(in.tellg());
			in.seekg(0, std::ios::beg);
			in.read(&result[0], result.size());
			in.close();
		}
		else {
			LN_CORE_ASSERT(false, "Could not open file '{0}'", filepath);
		}

		return result;
	}

	std::string OpenGLShader::InsertDefineAfterVersion(const std::string& source, const std::string& defineLine) {
		LNX_PROFILE_FUNCTION();
		// Buscar la directiva #version en cualquier parte del shader
		size_t pos = source.find("#version");
		if (pos != std::string::npos) {
			// Buscar el final de la línea donde está el #version
			size_t eol = source.find('\n', pos);
			if (eol != std::string::npos) {
				// Insertar el define justo después de esa línea
				return source.substr(0, eol + 1) + defineLine + source.substr(eol + 1);
			}
		}

		// Si no hay #version, ponemos el define al inicio
		return defineLine + source;
	}


	void OpenGLShader::Compile(const std::unordered_map<GLenum, std::string>& shaderSources) {
		LNX_PROFILE_FUNCTION();
		GLuint program = glCreateProgram();
		LN_CORE_ASSERT(shaderSources.size() <= 2, "We only support 2 shaders for now");
		std::array<GLenum, 2> glShaderIDs;
		int glShaderIDIndex = 0;

		for (auto& kv : shaderSources) {
			GLenum type = kv.first;
			const std::string& source = kv.second;

			GLuint shader = glCreateShader(type);

			const GLchar* sourceCStr = source.c_str();
			glShaderSource(shader, 1, &sourceCStr, 0);

			glCompileShader(shader);

			GLint isCompiled = 0;
			glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
			if (isCompiled == GL_FALSE) {
				GLint maxLength = 0;
				glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

				std::vector<GLchar> infoLog(maxLength);
				glGetShaderInfoLog(shader, maxLength, &maxLength, &infoLog[0]);

				glDeleteShader(shader);

				LNX_LOG_ERROR("{0}", infoLog.data());
				LN_CORE_ASSERT(false, "Shader compilation failure!");
				break;
			}

			glAttachShader(program, shader);
			glShaderIDs[glShaderIDIndex++] = shader;
		}

		m_RendererID = program;

		glLinkProgram(program);

		GLint isLinked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);
		if (isLinked == GL_FALSE) {
			GLint maxLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);

			glDeleteProgram(program);

			for (auto id : glShaderIDs)
				glDeleteShader(id);

			LNX_LOG_ERROR("{0}", infoLog.data());
			LN_CORE_ASSERT(false, "Shader link failure!");
			return;
		}

		for (auto id : glShaderIDs)
			glDetachShader(program, id);
	}

	void OpenGLShader::Bind() const {
		LNX_PROFILE_FUNCTION();
		glUseProgram(m_RendererID);
	}

	void OpenGLShader::Unbind() const {
		LNX_PROFILE_FUNCTION();
		glUseProgram(0);
	}

	void OpenGLShader::SetInt(const std::string& name, int value) {
		LNX_PROFILE_FUNCTION();
		UploadUniformInt(name, value);
	}

	void OpenGLShader::SetIntArray(const std::string& name, int* values, uint32_t count) {
		UploadUniformIntArray(name, values, count);
	}
	
	void OpenGLShader::SetFloat(const std::string& name, float value) {
		LNX_PROFILE_FUNCTION();
		UploadUniformFloat(name, value);
	}
	
	void OpenGLShader::setFloat2(const std::string& name, const glm::vec2& value) {
		LNX_PROFILE_FUNCTION();
		UploadUniformFloat2(name, value);
	}	 
	
	void OpenGLShader::SetFloat3(const std::string& name, const glm::vec3& value) {
		LNX_PROFILE_FUNCTION();
		UploadUniformFloat3(name, value);
	}	
	
	void OpenGLShader::SetFloat4(const std::string& name, const glm::vec4& value) {
		LNX_PROFILE_FUNCTION();
		UploadUniformFloat4(name, value);
	}
	
	void OpenGLShader::SetMat4(const std::string& name, const glm::mat4& value) {
		LNX_PROFILE_FUNCTION(); 
		UploadUniformMat4(name, value);
	}
	
	void OpenGLShader::SetMat3(const std::string& name, const glm::mat3& value) {
		LNX_PROFILE_FUNCTION(); 
		UploadUniformMat3(name, value);
	}
	
	void OpenGLShader::UploadUniformInt(const std::string& name, int value) {
		LNX_PROFILE_FUNCTION(); 
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1i(location, value);
	}
	
	void OpenGLShader::UploadUniformIntArray(const std::string& name, int* values, uint32_t count) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1iv(location, count, values);
	}
	
	void OpenGLShader::UploadUniformFloat(const std::string& name, float value) {
		LNX_PROFILE_FUNCTION();
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1f(location, value);
	}
	
	void OpenGLShader::UploadUniformFloat2(const std::string& name, const glm::vec2& value) {
		LNX_PROFILE_FUNCTION();
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform2f(location, value.x, value.y);
	}
	
	void OpenGLShader::UploadUniformFloat3(const std::string& name, const glm::vec3& value) {
		LNX_PROFILE_FUNCTION();
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform3f(location, value.x, value.y, value.z);
	}
	
	void OpenGLShader::UploadUniformFloat4(const std::string& name, const glm::vec4& value) {
		LNX_PROFILE_FUNCTION();
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform4f(location, value.x, value.y, value.z, value.w);
	}
	void OpenGLShader::UploadUniformMat3(const std::string& name, const glm::mat3& matrix) {
		LNX_PROFILE_FUNCTION();
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
	}
	
	void OpenGLShader::UploadUniformMat4(const std::string& name, const glm::mat4& matrix) {
		LNX_PROFILE_FUNCTION();
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
	}
}