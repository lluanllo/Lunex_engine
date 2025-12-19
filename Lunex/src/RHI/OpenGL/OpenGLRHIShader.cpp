#include "stpch.h"
#include "OpenGLRHIShader.h"
#include "Log/Log.h"
#include <fstream>
#include <sstream>
#include <glm/gtc/type_ptr.hpp>

namespace Lunex {
namespace RHI {

	// ============================================================================
	// OPENGL RHI SHADER IMPLEMENTATION
	// ============================================================================
	
	OpenGLRHIShader::OpenGLRHIShader(const std::string& filePath) {
		m_FilePath = filePath;
		
		// Extract name from path
		auto lastSlash = filePath.find_last_of("/\\");
		auto lastDot = filePath.rfind('.');
		m_Name = filePath.substr(lastSlash + 1, lastDot - lastSlash - 1);
		
		CompileFromFile(filePath);
	}
	
	OpenGLRHIShader::OpenGLRHIShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc)
		: m_Name(name)
	{
		CompileFromSource(vertexSrc, fragmentSrc);
	}
	
	OpenGLRHIShader::~OpenGLRHIShader() {
		if (m_ProgramID) {
			glDeleteProgram(m_ProgramID);
		}
	}
	
	void OpenGLRHIShader::CompileFromFile(const std::string& filePath) {
		std::string source = ReadFile(filePath);
		if (source.empty()) {
			LNX_LOG_ERROR("Failed to read shader file: {0}", filePath);
			return;
		}
		
		std::string vertexSrc, fragmentSrc;
		ParseShaderSource(source, vertexSrc, fragmentSrc);
		CompileFromSource(vertexSrc, fragmentSrc);
	}
	
	void OpenGLRHIShader::CompileFromSource(const std::string& vertexSrc, const std::string& fragmentSrc) {
		GLuint vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSrc);
		GLuint fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSrc);
		
		if (vertexShader && fragmentShader) {
			LinkProgram(vertexShader, fragmentShader);
			m_Stages = ShaderStage::VertexFragment;
			Reflect();
		}
		
		if (vertexShader) glDeleteShader(vertexShader);
		if (fragmentShader) glDeleteShader(fragmentShader);
	}
	
	GLuint OpenGLRHIShader::CompileShader(GLenum type, const std::string& source) {
		GLuint shader = glCreateShader(type);
		const char* src = source.c_str();
		glShaderSource(shader, 1, &src, nullptr);
		glCompileShader(shader);
		
		GLint success;
		glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
		if (!success) {
			GLint length;
			glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
			std::vector<char> log(length);
			glGetShaderInfoLog(shader, length, nullptr, log.data());
			
			const char* typeStr = type == GL_VERTEX_SHADER ? "Vertex" : 
								  type == GL_FRAGMENT_SHADER ? "Fragment" : "Compute";
			LNX_LOG_ERROR("{0} shader compilation failed in {1}:\n{2}", typeStr, m_Name, log.data());
			
			glDeleteShader(shader);
			return 0;
		}
		
		return shader;
	}
	
	void OpenGLRHIShader::LinkProgram(GLuint vertexShader, GLuint fragmentShader) {
		m_ProgramID = glCreateProgram();
		glAttachShader(m_ProgramID, vertexShader);
		glAttachShader(m_ProgramID, fragmentShader);
		glLinkProgram(m_ProgramID);
		
		GLint success;
		glGetProgramiv(m_ProgramID, GL_LINK_STATUS, &success);
		if (!success) {
			GLint length;
			glGetProgramiv(m_ProgramID, GL_INFO_LOG_LENGTH, &length);
			std::vector<char> log(length);
			glGetProgramInfoLog(m_ProgramID, length, nullptr, log.data());
			
			LNX_LOG_ERROR("Shader link failed for {0}:\n{1}", m_Name, log.data());
			
			glDeleteProgram(m_ProgramID);
			m_ProgramID = 0;
		}
	}
	
	void OpenGLRHIShader::Reflect() {
		if (!m_ProgramID) return;
		
		// Get active uniforms
		GLint uniformCount;
		glGetProgramiv(m_ProgramID, GL_ACTIVE_UNIFORMS, &uniformCount);
		
		for (GLint i = 0; i < uniformCount; i++) {
			char name[256];
			GLsizei length;
			GLint size;
			GLenum type;
			glGetActiveUniform(m_ProgramID, i, sizeof(name), &length, &size, &type, name);
			
			ShaderUniform uniform;
			uniform.Name = name;
			uniform.Location = glGetUniformLocation(m_ProgramID, name);
			uniform.ArraySize = size;
			
			// Convert GL type to our type
			switch (type) {
				case GL_FLOAT: uniform.Type = DataType::Float; break;
				case GL_FLOAT_VEC2: uniform.Type = DataType::Float2; break;
				case GL_FLOAT_VEC3: uniform.Type = DataType::Float3; break;
				case GL_FLOAT_VEC4: uniform.Type = DataType::Float4; break;
				case GL_INT: uniform.Type = DataType::Int; break;
				case GL_INT_VEC2: uniform.Type = DataType::Int2; break;
				case GL_INT_VEC3: uniform.Type = DataType::Int3; break;
				case GL_INT_VEC4: uniform.Type = DataType::Int4; break;
				case GL_FLOAT_MAT3: uniform.Type = DataType::Mat3; break;
				case GL_FLOAT_MAT4: uniform.Type = DataType::Mat4; break;
				case GL_BOOL: uniform.Type = DataType::Bool; break;
				case GL_SAMPLER_2D: case GL_SAMPLER_CUBE: 
					uniform.Type = DataType::Int; break;
				default: uniform.Type = DataType::None; break;
			}
			
			m_Reflection.Uniforms.push_back(uniform);
		}
		
		// Get samplers
		for (const auto& u : m_Reflection.Uniforms) {
			if (u.Name.find("u_") == 0 || u.Name.find("sampler") != std::string::npos) {
				ShaderSampler sampler;
				sampler.Name = u.Name;
				sampler.Binding = u.Location;
				m_Reflection.Samplers.push_back(sampler);
			}
		}
	}
	
	std::string OpenGLRHIShader::ReadFile(const std::string& filePath) {
		std::ifstream file(filePath, std::ios::binary);
		if (!file) return "";
		
		std::stringstream ss;
		ss << file.rdbuf();
		return ss.str();
	}
	
	void OpenGLRHIShader::ParseShaderSource(const std::string& source, std::string& vertexSrc, std::string& fragmentSrc) {
		// Parse #type directives
		const char* typeToken = "#type";
		size_t typeTokenLength = strlen(typeToken);
		size_t pos = source.find(typeToken, 0);
		
		while (pos != std::string::npos) {
			size_t eol = source.find_first_of("\r\n", pos);
			size_t begin = pos + typeTokenLength + 1;
			std::string type = source.substr(begin, eol - begin);
			
			size_t nextLinePos = source.find_first_not_of("\r\n", eol);
			size_t nextTypePos = source.find(typeToken, nextLinePos);
			
			std::string shaderSource = (nextTypePos == std::string::npos) ? 
				source.substr(nextLinePos) : source.substr(nextLinePos, nextTypePos - nextLinePos);
			
			if (type == "vertex") {
				vertexSrc = shaderSource;
			} else if (type == "fragment" || type == "pixel") {
				fragmentSrc = shaderSource;
			}
			
			pos = nextTypePos;
		}
	}
	
	void OpenGLRHIShader::Bind() const {
		if (m_ProgramID) {
			glUseProgram(m_ProgramID);
		}
	}
	
	void OpenGLRHIShader::Unbind() const {
		glUseProgram(0);
	}
	
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
	
	void OpenGLRHIShader::Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) {
		if (m_ProgramID && IsCompute()) {
			Bind();
			glDispatchCompute(groupsX, groupsY, groupsZ);
		}
	}
	
	void OpenGLRHIShader::GetWorkGroupSize(uint32_t& x, uint32_t& y, uint32_t& z) const {
		x = m_WorkGroupSize[0];
		y = m_WorkGroupSize[1];
		z = m_WorkGroupSize[2];
	}
	
	bool OpenGLRHIShader::Reload() {
		if (m_FilePath.empty()) return false;
		
		GLuint oldProgram = m_ProgramID;
		m_ProgramID = 0;
		m_UniformLocationCache.clear();
		m_Reflection = ShaderReflection{};
		
		CompileFromFile(m_FilePath);
		
		if (m_ProgramID) {
			glDeleteProgram(oldProgram);
			LNX_LOG_INFO("Shader reloaded: {0}", m_Name);
			return true;
		} else {
			m_ProgramID = oldProgram;
			LNX_LOG_WARN("Shader reload failed, keeping old: {0}", m_Name);
			return false;
		}
	}
	
	void OpenGLRHIShader::OnDebugNameChanged() {
		if (m_ProgramID && GLAD_GL_KHR_debug) {
			glObjectLabel(GL_PROGRAM, m_ProgramID, -1, m_DebugName.c_str());
		}
	}

	// ============================================================================
	// OPENGL GRAPHICS PIPELINE IMPLEMENTATION
	// ============================================================================
	
	OpenGLRHIGraphicsPipeline::OpenGLRHIGraphicsPipeline(const GraphicsPipelineDesc& desc) {
		m_Desc = desc;
	}
	
	void OpenGLRHIGraphicsPipeline::Bind() const {
		// Bind shader
		if (m_Desc.Shader) {
			m_Desc.Shader->Bind();
		}
		
		// Apply fixed function states
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
		
		// Culling
		if (rs.Culling == CullMode::None) {
			glDisable(GL_CULL_FACE);
		} else {
			glEnable(GL_CULL_FACE);
			glCullFace(rs.Culling == CullMode::Front ? GL_FRONT : GL_BACK);
		}
		
		// Front face
		glFrontFace(rs.WindingOrder == FrontFace::CounterClockwise ? GL_CCW : GL_CW);
		
		// Fill mode
		glPolygonMode(GL_FRONT_AND_BACK, rs.Fill == FillMode::Wireframe ? GL_LINE : GL_FILL);
		
		// Depth bias
		if (rs.DepthBias != 0.0f || rs.SlopeScaledDepthBias != 0.0f) {
			glEnable(GL_POLYGON_OFFSET_FILL);
			glPolygonOffset(rs.SlopeScaledDepthBias, rs.DepthBias);
		} else {
			glDisable(GL_POLYGON_OFFSET_FILL);
		}
		
		// Scissor
		if (rs.ScissorEnabled) {
			glEnable(GL_SCISSOR_TEST);
		} else {
			glDisable(GL_SCISSOR_TEST);
		}
	}
	
	void OpenGLRHIGraphicsPipeline::ApplyDepthStencilState() const {
		const auto& ds = m_Desc.DepthStencil;
		
		// Depth test
		if (ds.DepthTestEnabled) {
			glEnable(GL_DEPTH_TEST);
		} else {
			glDisable(GL_DEPTH_TEST);
		}
		
		// Depth write
		glDepthMask(ds.DepthWriteEnabled ? GL_TRUE : GL_FALSE);
		
		// Depth function
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
		
		// Stencil test
		if (ds.StencilTestEnabled) {
			glEnable(GL_STENCIL_TEST);
			glStencilMask(ds.StencilWriteMask);
		} else {
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
		} else {
			glDisable(GL_BLEND);
		}
	}

	// ============================================================================
	// OPENGL COMPUTE PIPELINE IMPLEMENTATION
	// ============================================================================
	
	OpenGLRHIComputePipeline::OpenGLRHIComputePipeline(const ComputePipelineDesc& desc) {
		m_Desc = desc;
	}
	
	void OpenGLRHIComputePipeline::Bind() const {
		if (m_Desc.Shader) {
			m_Desc.Shader->Bind();
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
		} else {
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
	
	Ref<RHIGraphicsPipeline> RHIGraphicsPipeline::Create(const GraphicsPipelineDesc& desc) {
		return CreateRef<OpenGLRHIGraphicsPipeline>(desc);
	}
	
	Ref<RHIComputePipeline> RHIComputePipeline::Create(const ComputePipelineDesc& desc) {
		return CreateRef<OpenGLRHIComputePipeline>(desc);
	}

} // namespace RHI
} // namespace Lunex
