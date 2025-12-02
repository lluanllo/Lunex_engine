#include "stpch.h"
#include "OpenGLShader.h"

#include <fstream>
#include <filesystem>
#include <glad/glad.h>

#include <glm/gtc/type_ptr.hpp>

#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

#include "Core/Timer.h"

namespace Lunex {
	namespace Utils {
		static GLenum ShaderTypeFromString(const std::string& type) {
			if (type == "vertex")
				return GL_VERTEX_SHADER;
			if (type == "fragment" || type == "pixel")
				return GL_FRAGMENT_SHADER;
			if (type == "compute")
				return GL_COMPUTE_SHADER;
			
			LNX_CORE_ASSERT(false, "Unknown shader type!");
			return 0;
		}
		
		static shaderc_shader_kind GLShaderStageToShaderC(GLenum stage) {
			switch (stage) {
				case GL_VERTEX_SHADER:   return shaderc_glsl_vertex_shader;
				case GL_FRAGMENT_SHADER: return shaderc_glsl_fragment_shader;
				case GL_COMPUTE_SHADER:  return shaderc_glsl_compute_shader;
			}
			LNX_CORE_ASSERT(false);
			return (shaderc_shader_kind)0;
		}
		
		static const char* GLShaderStageToString(GLenum stage) {
			switch (stage) {
				case GL_VERTEX_SHADER:   return "GL_VERTEX_SHADER";
				case GL_FRAGMENT_SHADER: return "GL_FRAGMENT_SHADER";
				case GL_COMPUTE_SHADER:  return "GL_COMPUTE_SHADER";
			}
			LNX_CORE_ASSERT(false);
			return nullptr;
		}
		
		static const char* GetCacheDirectory() {
			// TODO: make sure the assets directory is valid
			return "assets/cache/shader/opengl";
		}
		
		static void CreateCacheDirectoryIfNeeded() {
			std::string cacheDirectory = GetCacheDirectory();
			if (!std::filesystem::exists(cacheDirectory))
				std::filesystem::create_directories(cacheDirectory);
		}
		
		static const char* GLShaderStageCachedOpenGLFileExtension(uint32_t stage) {
			switch (stage) {
				case GL_VERTEX_SHADER:    return ".cached_opengl.vert";
				case GL_FRAGMENT_SHADER:  return ".cached_opengl.frag";
				case GL_COMPUTE_SHADER:   return ".cached_opengl.comp";
			}
			LNX_CORE_ASSERT(false);
			return "";
		}
		
		static const char* GLShaderStageCachedVulkanFileExtension(uint32_t stage) {
			switch (stage) {
				case GL_VERTEX_SHADER:    return ".cached_vulkan.vert";
				case GL_FRAGMENT_SHADER:  return ".cached_vulkan.frag";
				case GL_COMPUTE_SHADER:   return ".cached_vulkan.comp";
			}
			LNX_CORE_ASSERT(false);
			return "";
		}
	}
	
	OpenGLShader::OpenGLShader(const std::string& filepath) : m_FilePath(filepath) {
		LNX_PROFILE_FUNCTION();

		Utils::CreateCacheDirectoryIfNeeded();

		std::string source = ReadFile(filepath);

		if (source.empty()) {
			LNX_LOG_ERROR("Shader source file is empty: {0}", filepath);
			LNX_CORE_ASSERT(false, "Empty shader source");
		}

		// Verificar que el archivo tenga #version
		if (source.find("#version") == std::string::npos) {
			LNX_LOG_ERROR("Shader file missing #version directive: {0}", filepath);
			LNX_CORE_ASSERT(false, "Shader missing #version");
		}

		std::unordered_map<GLenum, std::string> shaderSources;

		// Insertar defines
		std::string vertexSource = InsertDefineAfterVersion(source, "#define VERTEX");
		std::string fragmentSource = InsertDefineAfterVersion(source, "#define FRAGMENT");

		// Verificar que se insertaron correctamente
		if (vertexSource.find("#define VERTEX") == std::string::npos) {
			LNX_LOG_ERROR("Failed to insert VERTEX define");
			LNX_CORE_ASSERT(false);
		}

		if (fragmentSource.find("#define FRAGMENT") == std::string::npos) {
			LNX_LOG_ERROR("Failed to insert FRAGMENT define");
			LNX_CORE_ASSERT(false);
		}

		shaderSources[GL_VERTEX_SHADER] = vertexSource;
		shaderSources[GL_FRAGMENT_SHADER] = fragmentSource;

		// Log para debugging
		LNX_LOG_TRACE("Vertex shader preview:\n{0}", vertexSource.substr(0, 300));
		LNX_LOG_TRACE("Fragment shader preview:\n{0}", fragmentSource.substr(0, 300));

		{
			Timer timer;

			try {
				CompileOrGetVulkanBinaries(shaderSources);
				CompileOrGetOpenGLBinaries();
				CreateProgram();

				LNX_LOG_WARN("Shader creation took {0} ms", timer.ElapsedMillis());
			}
			catch (const std::exception& e) {
				LNX_LOG_ERROR("Shader creation failed: {0}", e.what());
				LNX_CORE_ASSERT(false, "Shader compilation failed");
			}
		}

		// Extract name from filepath
		auto lastSlash = filepath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		auto lastDot = filepath.rfind('.');
		auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
		m_Name = filepath.substr(lastSlash, count);

		LNX_LOG_INFO("Shader '{0}' created successfully", m_Name);
	}
	
	OpenGLShader::OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
		: m_Name(name) 
	{
		LNX_PROFILE_FUNCTION();
		std::unordered_map<GLenum, std::string> sources;
		sources[GL_VERTEX_SHADER] = vertexSrc;
		sources[GL_FRAGMENT_SHADER] = fragmentSrc;
		
		CompileOrGetVulkanBinaries(sources);
		CompileOrGetOpenGLBinaries();
		CreateProgram();
	}
	
	// ========================================
	// COMPUTE SHADER CONSTRUCTOR
	// ========================================
	OpenGLShader::OpenGLShader(const std::string& filepath, bool isCompute)
		: m_FilePath(filepath), m_IsCompute(true)
	{
		LNX_PROFILE_FUNCTION();
		
		Utils::CreateCacheDirectoryIfNeeded();
		
		std::string source = ReadFile(filepath);
		
		if (source.empty()) {
			LNX_LOG_ERROR("Compute shader source file is empty: {0}", filepath);
			LNX_CORE_ASSERT(false, "Empty shader source");
		}
		
		// Verificar que el archivo tenga #version
		if (source.find("#version") == std::string::npos) {
			LNX_LOG_ERROR("Compute shader file missing #version directive: {0}", filepath);
			LNX_CORE_ASSERT(false, "Shader missing #version");
		}
		
		std::unordered_map<GLenum, std::string> shaderSources;
		
		// Insertar define para compute shader
		std::string computeSource = InsertDefineAfterVersion(source, "#define COMPUTE");
		
		if (computeSource.find("#define COMPUTE") == std::string::npos) {
			LNX_LOG_ERROR("Failed to insert COMPUTE define");
			LNX_CORE_ASSERT(false);
		}
		
		shaderSources[GL_COMPUTE_SHADER] = computeSource;
		
		LNX_LOG_TRACE("Compute shader preview:\n{0}", computeSource.substr(0, 300));
		
		{
			Timer timer;
			
			try {
				CompileOrGetVulkanBinaries(shaderSources);
				CompileOrGetOpenGLBinaries();
				CreateProgram();
				
				LNX_LOG_WARN("Compute shader creation took {0} ms", timer.ElapsedMillis());
			}
			catch (const std::exception& e) {
				LNX_LOG_ERROR("Compute shader creation failed: {0}", e.what());
				LNX_CORE_ASSERT(false, "Compute shader compilation failed");
			}
		}
		
		// Extract name from filepath
		auto lastSlash = filepath.find_last_of("/\\");
		lastSlash = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		auto lastDot = filepath.rfind('.');
		auto count = lastDot == std::string::npos ? filepath.size() - lastSlash : lastDot - lastSlash;
		m_Name = filepath.substr(lastSlash, count);
		
		LNX_LOG_INFO("Compute shader '{0}' created successfully", m_Name);
	}
	
	OpenGLShader::~OpenGLShader() {
		LNX_PROFILE_FUNCTION();
		glDeleteProgram(m_RendererID);
	}
	
	// REEMPLAZA ReadFile en OpenGLShader.cpp

	std::string OpenGLShader::ReadFile(const std::string& filepath) {
		LNX_PROFILE_FUNCTION();

		// Verificar que el path no esté vacío
		if (filepath.empty()) {
			LNX_LOG_ERROR("ReadFile: filepath is empty!");
			return "";
		}

		// Log del path completo
		std::filesystem::path fullPath = std::filesystem::absolute(filepath);
		LNX_LOG_TRACE("ReadFile: Attempting to read: {0}", fullPath.string());
		LNX_LOG_TRACE("ReadFile: Relative path: {0}", filepath);

		// Verificar que el archivo existe
		if (!std::filesystem::exists(filepath)) {
			LNX_LOG_ERROR("ReadFile: File does not exist: {0}", filepath);
			LNX_LOG_ERROR("ReadFile: Absolute path: {0}", fullPath.string());
			LNX_LOG_ERROR("ReadFile: Current working directory: {0}",
				std::filesystem::current_path().string());
			return "";
		}

		// Verificar el tamaño del archivo
		auto fileSize = std::filesystem::file_size(filepath);
		LNX_LOG_TRACE("ReadFile: File size: {0} bytes", fileSize);

		if (fileSize == 0) {
			LNX_LOG_ERROR("ReadFile: File is empty (0 bytes): {0}", filepath);
			return "";
		}

		std::string result;
		std::ifstream in(filepath, std::ios::in | std::ios::binary);

		if (in) {
			in.seekg(0, std::ios::end);
			std::streamsize size = in.tellg();

			if (size == -1) {
				LNX_LOG_ERROR("ReadFile: Could not determine file size: {0}", filepath);
				in.close();
				return "";
			}

			if (size == 0) {
				LNX_LOG_ERROR("ReadFile: File reports 0 size: {0}", filepath);
				in.close();
				return "";
			}

			result.resize(size);
			in.seekg(0, std::ios::beg);
			in.read(&result[0], size);

			if (!in) {
				LNX_LOG_ERROR("ReadFile: Error reading file content: {0}", filepath);
				LNX_LOG_ERROR("ReadFile: Read {0} of {1} bytes", in.gcount(), size);
				in.close();
				return "";
			}

			in.close();

			LNX_LOG_INFO("ReadFile: Successfully read {0} bytes from {1}", result.size(), filepath);

			// Log primeras líneas para verificar contenido
			size_t firstNewline = result.find('\n');
			if (firstNewline != std::string::npos) {
				LNX_LOG_TRACE("ReadFile: First line: {0}", result.substr(0, firstNewline));
			}
		}
		else {
			LNX_LOG_ERROR("ReadFile: Could not open file: {0}", filepath);
			LNX_LOG_ERROR("ReadFile: Error code: {0}", strerror(errno));

			// Intentar con diferentes modos
			std::ifstream testIn(filepath);
			if (testIn) {
				LNX_LOG_WARN("ReadFile: File CAN be opened in text mode, trying again...");
				testIn.seekg(0, std::ios::end);
				size_t size = testIn.tellg();
				result.resize(size);
				testIn.seekg(0, std::ios::beg);
				testIn.read(&result[0], size);
				testIn.close();

				if (!result.empty()) {
					LNX_LOG_WARN("ReadFile: Successfully read in text mode");
					return result;
				}
			}
		}

		return result;
	}
	
	std::string OpenGLShader::InsertDefineAfterVersion(const std::string& source, const std::string& defineLine) {
		LNX_PROFILE_FUNCTION();

		// Buscar #version
		size_t versionPos = source.find("#version");

		if (versionPos == std::string::npos) {
			// Si no hay #version, agregar uno estándar + el define
			LNX_LOG_WARN("No #version found in shader, adding default");
			return "#version 450 core\n" + defineLine + "\n" + source;
		}

		// Encontrar el final de la línea del #version
		size_t eolPos = source.find('\n', versionPos);

		if (eolPos == std::string::npos) {
			// Si no hay salto de línea, el #version está al final
			LNX_LOG_WARN("No newline after #version, shader might be malformed");
			return source + "\n" + defineLine;
		}

		// Construir el resultado: #version ... \n + define + resto
		std::string result = source.substr(0, eolPos + 1);  // Incluye el \n del #version
		result += defineLine;                                // Agrega el define

		// Si el defineLine no termina en \n, agregarlo
		if (!defineLine.empty() && defineLine.back() != '\n') {
			result += '\n';
		}

		result += source.substr(eolPos + 1);                // Resto del código

		// Log para debugging
		LNX_LOG_TRACE("Inserted define after #version:");
		LNX_LOG_TRACE("{0}", result.substr(0, 200)); // Primeras 200 chars

		return result;
	}
	
	// REEMPLAZA CompileOrGetVulkanBinaries en OpenGLShader.cpp

	void OpenGLShader::CompileOrGetVulkanBinaries(const std::unordered_map<GLenum, std::string>& shaderSources) {
		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);

		const bool optimize = true;
		if (optimize)
			options.SetOptimizationLevel(shaderc_optimization_level_performance);

		std::filesystem::path cacheDirectory = Utils::GetCacheDirectory();

		auto& shaderData = m_VulkanSPIRV;
		shaderData.clear();

		for (auto&& [stage, source] : shaderSources) {
			std::filesystem::path shaderFilePath = m_FilePath;
			std::filesystem::path cachedPath = cacheDirectory / (shaderFilePath.filename().string() + Utils::GLShaderStageCachedVulkanFileExtension(stage));

			// Intentar cargar desde cache
			bool cacheLoaded = false;
			std::ifstream in(cachedPath, std::ios::in | std::ios::binary);
			if (in.is_open()) {
				in.seekg(0, std::ios::end);
				auto size = in.tellg();

				if (size > 0 && size % sizeof(uint32_t) == 0) {
					in.seekg(0, std::ios::beg);

					auto& data = shaderData[stage];
					data.resize(size / sizeof(uint32_t));
					in.read((char*)data.data(), size);

					if (in.good() && !data.empty()) {
						cacheLoaded = true;
						LNX_LOG_TRACE("Loaded cached Vulkan SPIR-V: {0}", cachedPath.string());
					}
				}
				in.close();
			}

			// Si no hay cache válida, compilar
			if (!cacheLoaded) {
				LNX_LOG_WARN("Compiling Vulkan SPIR-V: {0} - {1}", m_FilePath, Utils::GLShaderStageToString(stage));

				// CRÍTICO: Verificar que el source no esté vacío
				if (source.empty()) {
					LNX_LOG_ERROR("Source code is empty for stage: {0}", Utils::GLShaderStageToString(stage));
					LNX_CORE_ASSERT(false);
					continue;
				}

				// Log del código fuente para debugging
				LNX_LOG_TRACE("Source code to compile:\n{0}", source.substr(0, 500)); // Primeras 500 chars

				shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(
					source,
					Utils::GLShaderStageToShaderC(stage),
					m_FilePath.c_str(),
					options
				);

				if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
					LNX_LOG_ERROR("Vulkan SPIR-V compilation failed: {0}", module.GetErrorMessage());
					LNX_LOG_ERROR("Failed source code:\n{0}", source);
					LNX_CORE_ASSERT(false, "Shader compilation failed!");
				}

				shaderData[stage] = std::vector<uint32_t>(module.cbegin(), module.cend());

				if (shaderData[stage].empty()) {
					LNX_LOG_ERROR("Compiled SPIR-V data is empty!");
					LNX_CORE_ASSERT(false);
				}

				// Guardar en cache
				std::ofstream out(cachedPath, std::ios::out | std::ios::binary | std::ios::trunc);
				if (out.is_open()) {
					auto& data = shaderData[stage];
					out.write((char*)data.data(), data.size() * sizeof(uint32_t));
					out.flush();
					out.close();
					LNX_LOG_TRACE("Saved Vulkan SPIR-V cache: {0}", cachedPath.string());
				}
				else {
					LNX_LOG_ERROR("Failed to save cache: {0}", cachedPath.string());
				}
			}
		}

		// Verificar que tengamos datos válidos antes de hacer Reflect
		for (auto&& [stage, data] : shaderData) {
			if (data.empty()) {
				LNX_LOG_ERROR("No SPIR-V data for stage: {0}", Utils::GLShaderStageToString(stage));
				LNX_CORE_ASSERT(false);
			}
			else {
				LNX_LOG_TRACE("Reflecting stage {0} with {1} words", Utils::GLShaderStageToString(stage), data.size());

				// CRÍTICO: Envolver Reflect en try-catch para capturar errores de SPIRV-Cross
				try {
					Reflect(stage, data);
				}
				catch (const std::exception& e) {
					LNX_LOG_ERROR("Reflect failed for stage {0}: {1}", Utils::GLShaderStageToString(stage), e.what());
					LNX_LOG_ERROR("This might be caused by invalid SPIR-V data. Clearing cache and retrying...");

					// Borrar el archivo de cache corrupto
					std::filesystem::path shaderFilePath = m_FilePath;
					std::filesystem::path cachedPath = cacheDirectory / (shaderFilePath.filename().string() + Utils::GLShaderStageCachedVulkanFileExtension(stage));
					if (std::filesystem::exists(cachedPath)) {
						std::filesystem::remove(cachedPath);
						LNX_LOG_WARN("Deleted corrupted cache: {0}", cachedPath.string());
					}

					throw; // Re-lanzar la excepción
				}
			}
		}
	}

	// TAMBIÉN REEMPLAZA CompileOrGetOpenGLBinaries

	void OpenGLShader::CompileOrGetOpenGLBinaries() {
		auto& shaderData = m_OpenGLSPIRV;

		shaderc::Compiler compiler;
		shaderc::CompileOptions options;
		options.SetTargetEnvironment(shaderc_target_env_opengl, shaderc_env_version_opengl_4_5);

		const bool optimize = false;
		if (optimize)
			options.SetOptimizationLevel(shaderc_optimization_level_performance);

		std::filesystem::path cacheDirectory = Utils::GetCacheDirectory();

		shaderData.clear();
		m_OpenGLSourceCode.clear();

		for (auto&& [stage, spirv] : m_VulkanSPIRV) {
			std::filesystem::path shaderFilePath = m_FilePath;
			std::filesystem::path cachedPath = cacheDirectory / (shaderFilePath.filename().string() + Utils::GLShaderStageCachedOpenGLFileExtension(stage));

			bool cacheLoaded = false;
			std::ifstream in(cachedPath, std::ios::in | std::ios::binary);
			if (in.is_open()) {
				in.seekg(0, std::ios::end);
				auto size = in.tellg();

				if (size > 0 && size % sizeof(uint32_t) == 0) {
					in.seekg(0, std::ios::beg);

					auto& data = shaderData[stage];
					data.resize(size / sizeof(uint32_t));
					in.read((char*)data.data(), size);

					if (in.good() && !data.empty()) {
						cacheLoaded = true;
						LNX_LOG_TRACE("Loaded cached OpenGL SPIR-V: {0}", cachedPath.string());
					}
				}
				in.close();
			}

			if (!cacheLoaded) {
				LNX_LOG_WARN("Compiling OpenGL SPIR-V from Vulkan SPIR-V: {0}", Utils::GLShaderStageToString(stage));

				// Cross-compile de Vulkan SPIR-V a GLSL
				try {
					spirv_cross::CompilerGLSL glslCompiler(spirv);

					// Configurar opciones
					spirv_cross::CompilerGLSL::Options glslOptions;
					glslOptions.version = 450;
					glslOptions.es = false;
					glslOptions.vulkan_semantics = false;
					glslCompiler.set_common_options(glslOptions);

					m_OpenGLSourceCode[stage] = glslCompiler.compile();
					auto& source = m_OpenGLSourceCode[stage];

					if (source.empty()) {
						LNX_LOG_ERROR("Cross-compiled GLSL source is empty!");
						LNX_CORE_ASSERT(false);
					}

					LNX_LOG_TRACE("Cross-compiled to GLSL ({0} chars)", source.size());

					// Compilar GLSL a OpenGL SPIR-V
					shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(
						source,
						Utils::GLShaderStageToShaderC(stage),
						m_FilePath.c_str(),
						options
					);

					if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
						LNX_LOG_ERROR("OpenGL SPIR-V compilation failed: {0}", module.GetErrorMessage());
						LNX_LOG_ERROR("Failed GLSL source:\n{0}", source);
						LNX_CORE_ASSERT(false);
					}

					shaderData[stage] = std::vector<uint32_t>(module.cbegin(), module.cend());

					// Guardar cache
					std::ofstream out(cachedPath, std::ios::out | std::ios::binary | std::ios::trunc);
					if (out.is_open()) {
						auto& data = shaderData[stage];
						out.write((char*)data.data(), data.size() * sizeof(uint32_t));
						out.flush();
						out.close();
					}
				}
				catch (const std::exception& e) {
					LNX_LOG_ERROR("SPIR-V Cross compilation failed: {0}", e.what());
					throw;
				}
			}
		}
	}
	
	void OpenGLShader::CreateProgram() {
		GLuint program = glCreateProgram();
		
		std::vector<GLuint> shaderIDs;
		for (auto&& [stage, spirv] : m_OpenGLSPIRV) {
			GLuint shaderID = shaderIDs.emplace_back(glCreateShader(stage));
			glShaderBinary(1, &shaderID, GL_SHADER_BINARY_FORMAT_SPIR_V, spirv.data(), spirv.size() * sizeof(uint32_t));
			glSpecializeShader(shaderID, "main", 0, nullptr, nullptr);
			glAttachShader(program, shaderID);
		}
		
		glLinkProgram(program);
		
		GLint isLinked;
		glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
		if (isLinked == GL_FALSE) {
			GLint maxLength;
			glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);
			
			std::vector<GLchar> infoLog(maxLength);
			glGetProgramInfoLog(program, maxLength, &maxLength, infoLog.data());
			LNX_LOG_ERROR("Shader linking failed ({0}):\n{1}", m_FilePath, infoLog.data());
			
			glDeleteProgram(program);
			
			for (auto id : shaderIDs)
				glDeleteShader(id);
		}
		
		for (auto id : shaderIDs) {
			glDetachShader(program, id);
			glDeleteShader(id);
		}
		
		m_RendererID = program;
	}
	
	void OpenGLShader::Reflect(GLenum stage, const std::vector<uint32_t>& shaderData) {
		spirv_cross::Compiler compiler(shaderData);
		spirv_cross::ShaderResources resources = compiler.get_shader_resources();
		
		LNX_LOG_TRACE("OpenGLShader::Reflect - {0} {1}", Utils::GLShaderStageToString(stage), m_FilePath);
		LNX_LOG_TRACE("    {0} uniform buffers", resources.uniform_buffers.size());
		LNX_LOG_TRACE("    {0} resources", resources.sampled_images.size());
		
		LNX_LOG_TRACE("Uniform buffers:");
		for (const auto& resource : resources.uniform_buffers) {
			const auto& bufferType = compiler.get_type(resource.base_type_id);
			uint32_t bufferSize = compiler.get_declared_struct_size(bufferType);
			uint32_t binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
			int memberCount = bufferType.member_types.size();
			
			LNX_LOG_TRACE("  {0}", resource.name);
			LNX_LOG_TRACE("    Size = {0}", bufferSize);
			LNX_LOG_TRACE("    Binding = {0}", binding);
			LNX_LOG_TRACE("    Members = {0}", memberCount);
		}
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
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1i(location, value);
	}
	
	void OpenGLShader::UploadUniformIntArray(const std::string& name, int* values, uint32_t count) {
		GLint location = glGetUniformLocation(m_RendererID, name.c_str());
		glUniform1iv(location, count, values);
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
	
	// ========================================
	// COMPUTE SHADER IMPLEMENTATION
	// ========================================
	
	void OpenGLShader::DispatchCompute(uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ) {
		LNX_PROFILE_FUNCTION();
		
		if (!m_IsCompute) {
			LNX_LOG_ERROR("DispatchCompute called on non-compute shader: {0}", m_Name);
			return;
		}
		
		// Bind the shader first
		Bind();
		
		// Dispatch compute work groups
		glDispatchCompute(numGroupsX, numGroupsY, numGroupsZ);
		
		// Memory barrier to ensure compute writes are visible
		glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		
		LNX_LOG_TRACE("Dispatched compute shader '{0}' with groups: [{1}, {2}, {3}]", 
			m_Name, numGroupsX, numGroupsY, numGroupsZ);
	}
	
	void OpenGLShader::GetComputeWorkGroupSize(uint32_t& sizeX, uint32_t& sizeY, uint32_t& sizeZ) const {
		if (!m_IsCompute) {
			LNX_LOG_ERROR("GetComputeWorkGroupSize called on non-compute shader: {0}", m_Name);
			sizeX = sizeY = sizeZ = 0;
			return;
		}
		
		// Query the work group size from the shader program
		GLint workGroupSize[3];
		glGetProgramiv(m_RendererID, GL_COMPUTE_WORK_GROUP_SIZE, workGroupSize);
		
		sizeX = static_cast<uint32_t>(workGroupSize[0]);
		sizeY = static_cast<uint32_t>(workGroupSize[1]);
		sizeZ = static_cast<uint32_t>(workGroupSize[2]);
		
		LNX_LOG_TRACE("Compute shader '{0}' work group size: [{1}, {2}, {3}]", 
			m_Name, sizeX, sizeY, sizeZ);
	}
}