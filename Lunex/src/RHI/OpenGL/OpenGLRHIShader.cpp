#include "stpch.h"
#include "OpenGLRHIShader.h"
#include "Log/Log.h"

#include <fstream>
#include <sstream>
#include <glm/gtc/type_ptr.hpp>

// SPIR-V compilation
#include <shaderc/shaderc.hpp>
#include <spirv_cross/spirv_cross.hpp>
#include <spirv_cross/spirv_glsl.hpp>

// Fallback defines for missing GLAD extensions
#ifndef GLAD_GL_KHR_debug
#define GLAD_GL_KHR_debug 0
#endif

namespace Lunex {
	namespace RHI {

		// ============================================================================
		// UTILITY FUNCTIONS
		// ============================================================================

		static shaderc_shader_kind GLStageToShaderC(GLenum stage) {
			switch (stage) {
			case GL_VERTEX_SHADER:   return shaderc_glsl_vertex_shader;
			case GL_FRAGMENT_SHADER: return shaderc_glsl_fragment_shader;
			case GL_COMPUTE_SHADER:  return shaderc_glsl_compute_shader;
			}
			LNX_CORE_ASSERT(false, "Unknown shader stage");
			return shaderc_glsl_vertex_shader;
		}

		std::filesystem::path OpenGLRHIShader::GetCacheDirectory() {
			return "assets/cache/shader/rhi";
		}

		void OpenGLRHIShader::CreateCacheDirectoryIfNeeded() {
			auto dir = GetCacheDirectory();
			if (!std::filesystem::exists(dir)) {
				std::filesystem::create_directories(dir);
			}
		}

		const char* OpenGLRHIShader::GetVulkanCacheExtension(GLenum stage) {
			switch (stage) {
			case GL_VERTEX_SHADER:   return ".cached_vulkan.vert";
			case GL_FRAGMENT_SHADER: return ".cached_vulkan.frag";
			case GL_COMPUTE_SHADER:  return ".cached_vulkan.comp";
			}
			return ".cached_vulkan.unknown";
		}

		const char* OpenGLRHIShader::GetOpenGLCacheExtension(GLenum stage) {
			switch (stage) {
			case GL_VERTEX_SHADER:   return ".cached_opengl.vert";
			case GL_FRAGMENT_SHADER: return ".cached_opengl.frag";
			case GL_COMPUTE_SHADER:  return ".cached_opengl.comp";
			}
			return ".cached_opengl.unknown";
		}

		const char* OpenGLRHIShader::StageToString(GLenum stage) {
			switch (stage) {
			case GL_VERTEX_SHADER:   return "VERTEX";
			case GL_FRAGMENT_SHADER: return "FRAGMENT";
			case GL_COMPUTE_SHADER:  return "COMPUTE";
			}
			return "UNKNOWN";
		}

		// ============================================================================
		// CONSTRUCTOR - FROM FILE (Graphics shader with #ifdef VERTEX/FRAGMENT)
		// ============================================================================

		OpenGLRHIShader::OpenGLRHIShader(const std::string& filePath)
			: m_FilePath(filePath)
		{
			CreateCacheDirectoryIfNeeded();

			// Extract name from path
			auto lastSlash = filePath.find_last_of("/\\");
			auto lastDot = filePath.rfind('.');
			lastSlash = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
			m_Name = filePath.substr(lastSlash, lastDot - lastSlash);

			// Read source
			std::string source = ReadFile(filePath);
			if (source.empty()) {
				LNX_LOG_ERROR("RHIShader: Failed to read file: {0}", filePath);
				return;
			}

			// Prepare sources with defines for #ifdef processing
			std::unordered_map<GLenum, std::string> shaderSources;
			shaderSources[GL_VERTEX_SHADER] = InsertDefineAfterVersion(source, "#define VERTEX");
			shaderSources[GL_FRAGMENT_SHADER] = InsertDefineAfterVersion(source, "#define FRAGMENT");

			// Compile through SPIR-V pipeline
			CompileOrGetVulkanBinaries(shaderSources);
			CompileOrGetOpenGLBinaries();
			CreateProgram();

			if (m_ProgramID) {
				m_Stages = ShaderStage::VertexFragment;
				LNX_LOG_INFO("RHIShader '{0}' created successfully", m_Name);
			}
		}

		// ============================================================================
		// CONSTRUCTOR - FROM SEPARATE SOURCES
		// ============================================================================

		OpenGLRHIShader::OpenGLRHIShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
			: m_Name(name)
		{
			CreateCacheDirectoryIfNeeded();

			std::unordered_map<GLenum, std::string> shaderSources;
			shaderSources[GL_VERTEX_SHADER] = vertexSrc;
			shaderSources[GL_FRAGMENT_SHADER] = fragmentSrc;

			CompileOrGetVulkanBinaries(shaderSources);
			CompileOrGetOpenGLBinaries();
			CreateProgram();

			if (m_ProgramID) {
				m_Stages = ShaderStage::VertexFragment;
				LNX_LOG_INFO("RHIShader '{0}' created successfully", m_Name);
			}
		}

		// ============================================================================
		// CONSTRUCTOR - COMPUTE SHADER
		// ============================================================================

		OpenGLRHIShader::OpenGLRHIShader(const std::string& filePath, bool isCompute)
			: m_FilePath(filePath), m_IsCompute(true)
		{
			CreateCacheDirectoryIfNeeded();

			// Extract name
			auto lastSlash = filePath.find_last_of("/\\");
			auto lastDot = filePath.rfind('.');
			lastSlash = (lastSlash == std::string::npos) ? 0 : lastSlash + 1;
			m_Name = filePath.substr(lastSlash, lastDot - lastSlash);

			std::string source = ReadFile(filePath);
			if (source.empty()) {
				LNX_LOG_ERROR("RHIShader: Failed to read compute shader: {0}", filePath);
				return;
			}

			std::unordered_map<GLenum, std::string> shaderSources;
			shaderSources[GL_COMPUTE_SHADER] = InsertDefineAfterVersion(source, "#define COMPUTE");

			CompileOrGetVulkanBinaries(shaderSources);
			CompileOrGetOpenGLBinaries();
			CreateProgram();

			if (m_ProgramID) {
				m_Stages = ShaderStage::Compute;

				// Query work group size
				GLint workGroupSize[3];
				glGetProgramiv(m_ProgramID, GL_COMPUTE_WORK_GROUP_SIZE, workGroupSize);
				m_WorkGroupSize[0] = static_cast<uint32_t>(workGroupSize[0]);
				m_WorkGroupSize[1] = static_cast<uint32_t>(workGroupSize[1]);
				m_WorkGroupSize[2] = static_cast<uint32_t>(workGroupSize[2]);

				LNX_LOG_INFO("RHIShader '{0}' (compute) created successfully", m_Name);
			}
		}

		OpenGLRHIShader::~OpenGLRHIShader() {
			if (m_ProgramID) {
				glDeleteProgram(m_ProgramID);
			}
		}

		// ============================================================================
		// FILE READING
		// ============================================================================

		std::string OpenGLRHIShader::ReadFile(const std::string& filePath) {
			if (filePath.empty()) return "";

			if (!std::filesystem::exists(filePath)) {
				LNX_LOG_ERROR("RHIShader: File not found: {0}", filePath);
				return "";
			}

			std::ifstream file(filePath, std::ios::binary);
			if (!file) {
				LNX_LOG_ERROR("RHIShader: Cannot open file: {0}", filePath);
				return "";
			}

			std::stringstream ss;
			ss << file.rdbuf();
			return ss.str();
		}

		std::string OpenGLRHIShader::InsertDefineAfterVersion(const std::string& source, const std::string& defineLine) {
			size_t versionPos = source.find("#version");

			if (versionPos == std::string::npos) {
				return "#version 450 core\n" + defineLine + "\n" + source;
			}

			size_t eolPos = source.find('\n', versionPos);
			if (eolPos == std::string::npos) {
				return source + "\n" + defineLine;
			}

			std::string result = source.substr(0, eolPos + 1);
			result += defineLine;
			if (!defineLine.empty() && defineLine.back() != '\n') {
				result += '\n';
			}
			result += source.substr(eolPos + 1);

			return result;
		}

		// ============================================================================
		// SPIR-V COMPILATION PIPELINE
		// ============================================================================

		void OpenGLRHIShader::CompileOrGetVulkanBinaries(const std::unordered_map<GLenum, std::string>& shaderSources) {
			shaderc::Compiler compiler;
			shaderc::CompileOptions options;
			options.SetTargetEnvironment(shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
			options.SetOptimizationLevel(shaderc_optimization_level_performance);

			auto cacheDir = GetCacheDirectory();
			m_VulkanSPIRV.clear();

			// Check if source is newer than cache (invalidation)
			std::filesystem::file_time_type sourceTime{};
			bool hasSourceTime = false;
			if (!m_FilePath.empty() && std::filesystem::exists(m_FilePath)) {
				sourceTime = std::filesystem::last_write_time(m_FilePath);
				hasSourceTime = true;
			}

			for (auto&& [stage, source] : shaderSources) {
				std::filesystem::path shaderPath = m_FilePath.empty() ? m_Name : m_FilePath;
				std::filesystem::path cachePath = cacheDir / (shaderPath.filename().string() + GetVulkanCacheExtension(stage));

				// Try loading from cache (only if cache is newer than source)
				bool cacheLoaded = false;
				if (std::filesystem::exists(cachePath)) {
					bool cacheValid = true;
					if (hasSourceTime) {
						auto cacheTime = std::filesystem::last_write_time(cachePath);
						if (sourceTime > cacheTime) {
							cacheValid = false; // Source is newer, invalidate cache
						}
					}

					if (cacheValid) {
						std::ifstream in(cachePath, std::ios::binary);
						if (in) {
							in.seekg(0, std::ios::end);
							size_t size = in.tellg();
							if (size > 0 && size % sizeof(uint32_t) == 0) {
								in.seekg(0, std::ios::beg);
								m_VulkanSPIRV[stage].resize(size / sizeof(uint32_t));
								in.read(reinterpret_cast<char*>(m_VulkanSPIRV[stage].data()), size);
								cacheLoaded = in.good() && !m_VulkanSPIRV[stage].empty();
							}
						}
					}
				}

				if (!cacheLoaded) {
					// Compile to Vulkan SPIR-V
					if (source.empty()) {
						LNX_LOG_ERROR("RHIShader: Empty source for stage {0}", StageToString(stage));
						continue;
					}

					std::string sourceName = m_FilePath.empty() ? m_Name : m_FilePath;
					shaderc::SpvCompilationResult module = compiler.CompileGlslToSpv(
						source, GLStageToShaderC(stage), sourceName.c_str(), options
					);

					if (module.GetCompilationStatus() != shaderc_compilation_status_success) {
						LNX_LOG_ERROR("RHIShader SPIR-V compilation failed ({0}):\n{1}",
							StageToString(stage), module.GetErrorMessage());
						continue;
					}

					m_VulkanSPIRV[stage] = std::vector<uint32_t>(module.cbegin(), module.cend());

					// Save to cache
					std::ofstream out(cachePath, std::ios::binary | std::ios::trunc);
					if (out) {
						out.write(reinterpret_cast<const char*>(m_VulkanSPIRV[stage].data()),
							m_VulkanSPIRV[stage].size() * sizeof(uint32_t));
					}
				}

				// Reflect
				if (!m_VulkanSPIRV[stage].empty()) {
					ReflectStage(stage, m_VulkanSPIRV[stage]);
				}
			}
		}

		void OpenGLRHIShader::CompileOrGetOpenGLBinaries() {
			m_OpenGLSPIRV.clear();
			m_OpenGLSourceCode.clear();

			for (auto&& [stage, spirv] : m_VulkanSPIRV) {
				if (spirv.empty()) continue;

				// Cross-compile Vulkan SPIR-V to GLSL
				try {
					spirv_cross::CompilerGLSL glslCompiler(spirv);

					spirv_cross::CompilerGLSL::Options glslOptions;
					glslOptions.version = 450;
					glslOptions.es = false;
					glslOptions.vulkan_semantics = false;
					glslCompiler.set_common_options(glslOptions);

					m_OpenGLSourceCode[stage] = glslCompiler.compile();
				}
				catch (const std::exception& e) {
					LNX_LOG_ERROR("RHIShader SPIR-V cross-compilation failed ({0}): {1}",
						StageToString(stage), e.what());
				}
			}
		}

		void OpenGLRHIShader::CreateProgram() {
			m_ProgramID = glCreateProgram();

			std::vector<GLuint> shaderIDs;

			for (auto&& [stage, source] : m_OpenGLSourceCode) {
				if (source.empty()) continue;

				GLuint shaderID = glCreateShader(stage);
				shaderIDs.push_back(shaderID);

				const char* src = source.c_str();
				glShaderSource(shaderID, 1, &src, nullptr);
				glCompileShader(shaderID);

				GLint success;
				glGetShaderiv(shaderID, GL_COMPILE_STATUS, &success);
				if (!success) {
					GLint length;
					glGetShaderiv(shaderID, GL_INFO_LOG_LENGTH, &length);
					std::vector<char> log(length);
					glGetShaderInfoLog(shaderID, length, nullptr, log.data());
					LNX_LOG_ERROR("RHIShader compilation failed ({0} - {1}):\n{2}",
						m_Name, StageToString(stage), log.data());
					glDeleteShader(shaderID);
					shaderIDs.pop_back();
					continue;
				}

				glAttachShader(m_ProgramID, shaderID);
			}

			// Link
			glLinkProgram(m_ProgramID);

			GLint linked;
			glGetProgramiv(m_ProgramID, GL_LINK_STATUS, &linked);
			if (!linked) {
				GLint length;
				glGetProgramiv(m_ProgramID, GL_INFO_LOG_LENGTH, &length);
				std::vector<char> log(length);
				glGetProgramInfoLog(m_ProgramID, length, nullptr, log.data());
				LNX_LOG_ERROR("RHIShader link failed ({0}):\n{1}", m_Name, log.data());

				glDeleteProgram(m_ProgramID);
				m_ProgramID = 0;
			}

			// Cleanup shaders
			for (GLuint id : shaderIDs) {
				glDetachShader(m_ProgramID, id);
				glDeleteShader(id);
			}
		}

		void OpenGLRHIShader::ReflectStage(GLenum stage, const std::vector<uint32_t>& spirvData) {
			try {
				spirv_cross::Compiler compiler(spirvData);
				spirv_cross::ShaderResources resources = compiler.get_shader_resources();

				// Uniform buffers
				for (const auto& resource : resources.uniform_buffers) {
					const auto& type = compiler.get_type(resource.base_type_id);

					ShaderUniformBlock block;
					block.Name = resource.name;
					block.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);
					block.Size = static_cast<uint32_t>(compiler.get_declared_struct_size(type));

					m_Reflection.UniformBlocks.push_back(block);
				}

				// Samplers
				for (const auto& resource : resources.sampled_images) {
					ShaderSampler sampler;
					sampler.Name = resource.name;
					sampler.Binding = compiler.get_decoration(resource.id, spv::DecorationBinding);

					const auto& type = compiler.get_type(resource.type_id);
					sampler.IsCube = (type.image.dim == spv::DimCube);

					m_Reflection.Samplers.push_back(sampler);
				}

			}
			catch (const std::exception& e) {
				LNX_LOG_WARN("RHIShader reflection failed for {0}: {1}", m_Name, e.what());
			}
		}

		// ============================================================================
		// BINDING
		// ============================================================================

		void OpenGLRHIShader::Bind() const {
			if (m_ProgramID) {
				glUseProgram(m_ProgramID);
			}
		}

		void OpenGLRHIShader::Unbind() const {
			glUseProgram(0);
		}

		// ============================================================================
		// UNIFORMS
		// ============================================================================

		int OpenGLRHIShader::GetUniformLocation(const std::string& name) const {
			auto it = m_UniformLocationCache.find(name);
			if (it != m_UniformLocationCache.end()) {
				return it->second;
			}

			int location = glGetUniformLocation(m_ProgramID, name.c_str());
			m_UniformLocationCache[name] = location;
			return location;
		}

		void OpenGLRHIShader::SetInt(const std::string& name, int value) {
			glUniform1i(GetUniformLocation(name), value);
		}

		void OpenGLRHIShader::SetIntArray(const std::string& name, const int* values, uint32_t count) {
			glUniform1iv(GetUniformLocation(name), count, values);
		}

		void OpenGLRHIShader::SetFloat(const std::string& name, float value) {
			glUniform1f(GetUniformLocation(name), value);
		}

		void OpenGLRHIShader::SetFloat2(const std::string& name, const glm::vec2& value) {
			glUniform2f(GetUniformLocation(name), value.x, value.y);
		}

		void OpenGLRHIShader::SetFloat3(const std::string& name, const glm::vec3& value) {
			glUniform3f(GetUniformLocation(name), value.x, value.y, value.z);
		}

		void OpenGLRHIShader::SetFloat4(const std::string& name, const glm::vec4& value) {
			glUniform4f(GetUniformLocation(name), value.x, value.y, value.z, value.w);
		}

		void OpenGLRHIShader::SetMat3(const std::string& name, const glm::mat3& value) {
			glUniformMatrix3fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
		}

		void OpenGLRHIShader::SetMat4(const std::string& name, const glm::mat4& value) {
			glUniformMatrix4fv(GetUniformLocation(name), 1, GL_FALSE, glm::value_ptr(value));
		}

		// ============================================================================
		// COMPUTE
		// ============================================================================

		void OpenGLRHIShader::Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) {
			if (m_ProgramID && m_IsCompute) {
				Bind();
				glDispatchCompute(groupsX, groupsY, groupsZ);
				glMemoryBarrier(GL_SHADER_STORAGE_BARRIER_BIT | GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
			}
		}

		void OpenGLRHIShader::GetWorkGroupSize(uint32_t& x, uint32_t& y, uint32_t& z) const {
			x = m_WorkGroupSize[0];
			y = m_WorkGroupSize[1];
			z = m_WorkGroupSize[2];
		}

		// ============================================================================
		// HOT RELOAD
		// ============================================================================

		bool OpenGLRHIShader::Reload() {
			if (m_FilePath.empty()) return false;

			// Delete cache files to force recompilation
			auto cacheDir = GetCacheDirectory();
			std::filesystem::path shaderPath = m_FilePath;

			for (GLenum stage : {GL_VERTEX_SHADER, GL_FRAGMENT_SHADER, GL_COMPUTE_SHADER}) {
				std::filesystem::path vulkanCache = cacheDir / (shaderPath.filename().string() + GetVulkanCacheExtension(stage));
				std::filesystem::path openglCache = cacheDir / (shaderPath.filename().string() + GetOpenGLCacheExtension(stage));

				if (std::filesystem::exists(vulkanCache)) std::filesystem::remove(vulkanCache);
				if (std::filesystem::exists(openglCache)) std::filesystem::remove(openglCache);
			}

			// Store old program
			GLuint oldProgram = m_ProgramID;
			m_ProgramID = 0;
			m_VulkanSPIRV.clear();
			m_OpenGLSPIRV.clear();
			m_OpenGLSourceCode.clear();
			m_UniformLocationCache.clear();

			// Recompile
			std::string source = ReadFile(m_FilePath);
			if (!source.empty()) {
				std::unordered_map<GLenum, std::string> shaderSources;

				if (m_IsCompute) {
					shaderSources[GL_COMPUTE_SHADER] = InsertDefineAfterVersion(source, "#define COMPUTE");
				}
				else {
					shaderSources[GL_VERTEX_SHADER] = InsertDefineAfterVersion(source, "#define VERTEX");
					shaderSources[GL_FRAGMENT_SHADER] = InsertDefineAfterVersion(source, "#define FRAGMENT");
				}

				CompileOrGetVulkanBinaries(shaderSources);
				CompileOrGetOpenGLBinaries();
				CreateProgram();
			}

			if (m_ProgramID) {
				glDeleteProgram(oldProgram);
				LNX_LOG_INFO("RHIShader '{0}' reloaded successfully", m_Name);
				return true;
			}
			else {
				m_ProgramID = oldProgram;
				LNX_LOG_WARN("RHIShader '{0}' reload failed, keeping old shader", m_Name);
				return false;
			}
		}

		void OpenGLRHIShader::OnDebugNameChanged() {
			if (m_ProgramID && GLAD_GL_KHR_debug) {
				glObjectLabel(GL_PROGRAM, m_ProgramID, -1, m_DebugName.c_str());
			}
		}

		// ============================================================================
		// OPENGL GRAPHICS PIPELINE
		// ============================================================================

		OpenGLRHIGraphicsPipeline::OpenGLRHIGraphicsPipeline(const GraphicsPipelineDesc& desc) {
			m_Desc = desc;
		}

		void OpenGLRHIGraphicsPipeline::Bind() const {
			if (m_Desc.Shader) {
				m_Desc.Shader->Bind();
			}
			ApplyRasterizerState();
			ApplyDepthStencilState();
			ApplyBlendState();
		}

		void OpenGLRHIGraphicsPipeline::Unbind() const {
			if (m_Desc.Shader) {
				m_Desc.Shader->Unbind();
			}
		}

		void OpenGLRHIGraphicsPipeline::ApplyRasterizerState() const {
			const auto& rs = m_Desc.Rasterizer;

			if (rs.Culling == CullMode::None) {
				glDisable(GL_CULL_FACE);
			}
			else {
				glEnable(GL_CULL_FACE);
				glCullFace(rs.Culling == CullMode::Front ? GL_FRONT : GL_BACK);
			}

			glFrontFace(rs.WindingOrder == FrontFace::CounterClockwise ? GL_CCW : GL_CW);
			glPolygonMode(GL_FRONT_AND_BACK, rs.Fill == FillMode::Wireframe ? GL_LINE : GL_FILL);

			if (rs.DepthBias != 0.0f || rs.SlopeScaledDepthBias != 0.0f) {
				glEnable(GL_POLYGON_OFFSET_FILL);
				glPolygonOffset(rs.SlopeScaledDepthBias, rs.DepthBias);
			}
			else {
				glDisable(GL_POLYGON_OFFSET_FILL);
			}

			if (rs.ScissorEnabled) {
				glEnable(GL_SCISSOR_TEST);
			}
			else {
				glDisable(GL_SCISSOR_TEST);
			}
		}

		void OpenGLRHIGraphicsPipeline::ApplyDepthStencilState() const {
			const auto& ds = m_Desc.DepthStencil;

			if (ds.DepthTestEnabled) {
				glEnable(GL_DEPTH_TEST);
			}
			else {
				glDisable(GL_DEPTH_TEST);
			}

			glDepthMask(ds.DepthWriteEnabled ? GL_TRUE : GL_FALSE);

			GLenum depthFunc = GL_LESS;
			switch (ds.DepthCompareFunc) {
			case CompareFunc::Never: depthFunc = GL_NEVER; break;
			case CompareFunc::Less: depthFunc = GL_LESS; break;
			case CompareFunc::Equal: depthFunc = GL_EQUAL; break;
			case CompareFunc::LessEqual: depthFunc = GL_LEQUAL; break;
			case CompareFunc::Greater: depthFunc = GL_GREATER; break;
			case CompareFunc::NotEqual: depthFunc = GL_NOTEQUAL; break;
			case CompareFunc::GreaterEqual: depthFunc = GL_GEQUAL; break;
			case CompareFunc::Always: depthFunc = GL_ALWAYS; break;
			}
			glDepthFunc(depthFunc);

			if (ds.StencilTestEnabled) {
				glEnable(GL_STENCIL_TEST);
				glStencilMask(ds.StencilWriteMask);
			}
			else {
				glDisable(GL_STENCIL_TEST);
			}
		}

		void OpenGLRHIGraphicsPipeline::ApplyBlendState() const {
			const auto& bs = m_Desc.Blend;

			if (bs.Enabled) {
				glEnable(GL_BLEND);

				auto toGL = [](BlendFactor f) -> GLenum {
					switch (f) {
					case BlendFactor::Zero: return GL_ZERO;
					case BlendFactor::One: return GL_ONE;
					case BlendFactor::SrcColor: return GL_SRC_COLOR;
					case BlendFactor::OneMinusSrcColor: return GL_ONE_MINUS_SRC_COLOR;
					case BlendFactor::DstColor: return GL_DST_COLOR;
					case BlendFactor::OneMinusDstColor: return GL_ONE_MINUS_DST_COLOR;
					case BlendFactor::SrcAlpha: return GL_SRC_ALPHA;
					case BlendFactor::OneMinusSrcAlpha: return GL_ONE_MINUS_SRC_ALPHA;
					case BlendFactor::DstAlpha: return GL_DST_ALPHA;
					case BlendFactor::OneMinusDstAlpha: return GL_ONE_MINUS_DST_ALPHA;
					default: return GL_ONE;
					}
					};

				glBlendFuncSeparate(toGL(bs.SrcColor), toGL(bs.DstColor),
					toGL(bs.SrcAlpha), toGL(bs.DstAlpha));

				auto toGLOp = [](BlendOp op) -> GLenum {
					switch (op) {
					case BlendOp::Add: return GL_FUNC_ADD;
					case BlendOp::Subtract: return GL_FUNC_SUBTRACT;
					case BlendOp::ReverseSubtract: return GL_FUNC_REVERSE_SUBTRACT;
					case BlendOp::Min: return GL_MIN;
					case BlendOp::Max: return GL_MAX;
					default: return GL_FUNC_ADD;
					}
					};

				glBlendEquationSeparate(toGLOp(bs.ColorOp), toGLOp(bs.AlphaOp));
			}
			else {
				glDisable(GL_BLEND);
			}
		}

		// ============================================================================
		// OPENGL COMPUTE PIPELINE
		// ============================================================================

		OpenGLRHIComputePipeline::OpenGLRHIComputePipeline(const ComputePipelineDesc& desc) {
			m_Desc = desc;
		}

		void OpenGLRHIComputePipeline::Bind() const {
			if (m_Desc.Shader) {
				m_Desc.Shader->Bind();
			}
		}

		void OpenGLRHIComputePipeline::Unbind() const {
			if (m_Desc.Shader) {
				m_Desc.Shader->Unbind();
			}
		}

		void OpenGLRHIComputePipeline::Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) const {
			if (m_Desc.Shader) {
				m_Desc.Shader->Dispatch(groupsX, groupsY, groupsZ);
			}
		}

		void OpenGLRHIComputePipeline::GetWorkGroupSize(uint32_t& x, uint32_t& y, uint32_t& z) const {
			if (m_Desc.Shader) {
				m_Desc.Shader->GetWorkGroupSize(x, y, z);
			}
			else {
				x = y = z = 1;
			}
		}

		// ============================================================================
		// FACTORY IMPLEMENTATIONS
		// ============================================================================

		Ref<RHIShader> RHIShader::CreateFromFile(const std::string& filePath) {
			return CreateRef<OpenGLRHIShader>(filePath);
		}

		Ref<RHIShader> RHIShader::CreateFromSource(const std::string& name,
			const std::string& vertexSource,
			const std::string& fragmentSource) {
			return CreateRef<OpenGLRHIShader>(name, vertexSource, fragmentSource);
		}

		Ref<RHIShader> RHIShader::CreateComputeFromFile(const std::string& filePath) {
			return CreateRef<OpenGLRHIShader>(filePath, true);
		}

		Ref<RHIShader> RHIShader::CreateComputeFromSource(const std::string& name, const std::string& source) {
			// For compute from source, we need to handle it differently
			auto shader = CreateRef<OpenGLRHIShader>(name, source, ""); // Empty fragment
			return shader;
		}

	} // namespace RHI
} // namespace Lunex
