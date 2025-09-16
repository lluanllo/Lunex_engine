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
		std::string shaderSource = ReadFile(filepath);
	}
	
	OpenGLShader::OpenGLShader(const std::string& vertexSrc, const std::string& fragmentSrc) {
		// Create an empty vertex shader handleAdd commentMore actions
		GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
		
		// Send the vertex shader source code to GL
		// Note that std::string's .c_str is NULL character terminated.
		const GLchar* source = vertexSrc.c_str();
		glShaderSource(vertexShader, 1, &source, 0);
		
		// Compile the vertex shader
		glCompileShader(vertexShader);
		
		GLint isCompiled = 0;
		glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE) {
			GLint maxLength = 0;
			glGetShaderiv(vertexShader, GL_INFO_LOG_LENGTH, &maxLength);
			
			// The maxLength includes the NULL character
			std::vector<GLchar> infoLog(maxLength);
			glGetShaderInfoLog(vertexShader, maxLength, &maxLength, &infoLog[0]);
			
			// We don't need the shader anymore.
			glDeleteShader(vertexShader);
			
			LNX_LOG_ERROR("{0}", infoLog.data());
			LN_CORE_ASSERT(false, "Vertex shader compilation failure!");
			return;
		}
		
		// Create an empty fragment shader handle
		GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
		
		// Send the fragment shader source code to GL
		// Note that std::string's .c_str is NULL character terminated.
		source = fragmentSrc.c_str();
		glShaderSource(fragmentShader, 1, &source, 0);
		
		// Compile the fragment shader
		glCompileShader(fragmentShader);
		
		glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &isCompiled);
		if (isCompiled == GL_FALSE) {
			GLint maxLength = 0;
			glGetShaderiv(fragmentShader, GL_INFO_LOG_LENGTH, &maxLength);
			
			// The maxLength includes the NULL character
			std::vector<GLchar> infoLog(maxLength);
			glGetShaderInfoLog(fragmentShader, maxLength, &maxLength, &infoLog[0]);
			
			// We don't need the shader anymore.
			glDeleteShader(fragmentShader);
			// Either of them. Don't leak shaders.
			glDeleteShader(vertexShader);
			
			//STLR_LOG_ERROR("{0}", infoLog.data());
			LN_CORE_ASSERT(false, "Fragment shader compilation failure!");
			return;
		}
		
		// Vertex and fragment shaders are successfully compiled.
		// Now time to link them together into a program.
		// Get a program object.
		m_RendererID = glCreateProgram();
		GLuint program = m_RendererID;
		
		// Attach our shaders to our program
		glAttachShader(program, vertexShader);
		glAttachShader(program, fragmentShader);
		
		// Link our program
		glLinkProgram(program);
		
		// Note the different functions here: glGetProgram* instead of glGetShader*.
		GLint isLinked = 0;
		glGetProgramiv(program, GL_LINK_STATUS, (int*)&isLinked);
		if (isLinked == GL_FALSE) {
			GLint maxLength = 0;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
			
			// The maxLength includes the NULL character
			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(program, maxLength, &maxLength, &infoLog[0]);
			
			// We don't need the program anymore.
			glDeleteProgram(program);
			// Don't leak shaders either.
			glDeleteShader(vertexShader);
			glDeleteShader(fragmentShader);
			
			//LNX_LOG_ERROR("{0}", infoLog.data());
			LN_CORE_ASSERT(false, "Shader link failure!");
			return;
		}
		
		// Always detach shaders after a successful link.
		glDetachShader(program, vertexShader);
		glDetachShader(program, fragmentShader);
	}
	
	OpenGLShader::~OpenGLShader() {
		glDeleteProgram(m_RendererID);
	}
	
	std::string OpenGLShader::ReadFile(const std::string& filepath) {
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
	
	std::unordered_map<GLenum, std::string> OpenGLShader::PreProcess(const std::string& source) {
		// Formato esperado:
		//   #type vertex
		//   ...código...
		//   #type fragment
		//   ...código...
		std::unordered_map<GLenum, std::string> shaderSources;

		const std::string typeToken = "#type";
		size_t pos = source.find(typeToken, 0);
		if (pos == std::string::npos) {
			// No hay marcadores #type
			return {};
		}

		while (pos != std::string::npos) {
			size_t eol = source.find_first_of("\r\n", pos);
			LN_CORE_ASSERT(eol != std::string::npos, "Syntax error: no EOL after #type");

			size_t begin = pos + typeToken.size() + 1; // espacio tras "#type "
			std::string type = source.substr(begin, eol - begin);
			GLenum glType = ShaderTypeFromString(type);

			size_t nextLinePos = source.find_first_not_of("\r\n", eol);
			LN_CORE_ASSERT(nextLinePos != std::string::npos, "Syntax error: no code after #type line");

			pos = source.find(typeToken, nextLinePos);
			shaderSources[glType] = (pos == std::string::npos)
				? source.substr(nextLinePos)
				: source.substr(nextLinePos, pos - nextLinePos);
		}

		return shaderSources;
	}

	std::string OpenGLShader::InsertDefineAfterVersion(const std::string& source, const std::string& defineLine) {
		// Si la primera línea es #version, insertamos el define justo después
		std::istringstream iss(source);
		std::string firstLine;
		std::getline(iss, firstLine);

		std::ostringstream oss;
		if (firstLine.rfind("#version", 0) == 0) {
			oss << firstLine << "\n" << defineLine;
			oss << iss.rdbuf();
		}
		else {
			// Si no hay #version al inicio, simplemente lo anteponemos
			oss << defineLine << source;
		}
		return oss.str();
	}
	
	void OpenGLShader::Bind() const {
		glUseProgram(m_RendererID);
	}
	
	void OpenGLShader::Unbind() const {
		glUseProgram(0);
	}
	
	void OpenGLShader::UploadUniformInt(const std::string& name, int value) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1i(location, value);
	}
	
	void OpenGLShader::UploadUniformFloat(const std::string& name, float value) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1f(location, value);
	}
	
	void OpenGLShader::UploadUniformFloat2(const std::string& name, const glm::vec2& value) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform2f(location, value.x, value.y);
	}
	
	void OpenGLShader::UploadUniformFloat3(const std::string& name, const glm::vec3& value) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform3f(location, value.x, value.y, value.z);
	}
	
	void OpenGLShader::UploadUniformFloat4(const std::string& name, const glm::vec4& value) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform4f(location, value.x, value.y, value.z, value.w);
	}
	void OpenGLShader::UploadUniformMat3(const std::string& name, const glm::mat3& matrix) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniformMatrix3fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
	}
	
	void OpenGLShader::UploadUniformMat4(const std::string& name, const glm::mat4& matrix) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniformMatrix4fv(location, 1, GL_FALSE, glm::value_ptr(matrix));
	}
}