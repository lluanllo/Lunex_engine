#pragma once

/**
 * @file OpenGLRHIShader.h
 * @brief OpenGL implementation of RHI shader with SPIR-V compilation pipeline
 */

#include "RHI/RHIShader.h"
#include "RHI/RHIPipeline.h"
#include <glad/glad.h>
#include <unordered_map>
#include <vector>
#include <filesystem>

namespace Lunex {
namespace RHI {

	// ============================================================================
	// OPENGL RHI SHADER
	// ============================================================================
	
	class OpenGLRHIShader : public RHIShader {
	public:
		// Standard graphics shader from file (supports #ifdef VERTEX/#ifdef FRAGMENT format)
		OpenGLRHIShader(const std::string& filePath);
		
		// Graphics shader from separate sources
		OpenGLRHIShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
		
		// Compute shader from file
		OpenGLRHIShader(const std::string& filePath, bool isCompute);
		
		virtual ~OpenGLRHIShader();
		
		// RHIResource
		RHIHandle GetNativeHandle() const override { return static_cast<RHIHandle>(m_ProgramID); }
		bool IsValid() const override { return m_ProgramID != 0; }
		
		// Shader info
		const std::string& GetName() const override { return m_Name; }
		const std::string& GetFilePath() const override { return m_FilePath; }
		ShaderStage GetStages() const override { return m_Stages; }
		bool IsCompute() const override { return m_IsCompute; }
		const ShaderReflection& GetReflection() const override { return m_Reflection; }
		
		// Binding
		void Bind() const override;
		void Unbind() const override;
		
		// Uniforms
		void SetInt(const std::string& name, int value) override;
		void SetIntArray(const std::string& name, const int* values, uint32_t count) override;
		void SetFloat(const std::string& name, float value) override;
		void SetFloat2(const std::string& name, const glm::vec2& value) override;
		void SetFloat3(const std::string& name, const glm::vec3& value) override;
		void SetFloat4(const std::string& name, const glm::vec4& value) override;
		void SetMat3(const std::string& name, const glm::mat3& value) override;
		void SetMat4(const std::string& name, const glm::mat4& value) override;
		int GetUniformLocation(const std::string& name) const override;
		
		// Compute
		void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) override;
		void GetWorkGroupSize(uint32_t& x, uint32_t& y, uint32_t& z) const override;
		
		// Hot reload
		bool Reload() override;
		
		// OpenGL specific
		GLuint GetProgramID() const { return m_ProgramID; }
		
	protected:
		void OnDebugNameChanged() override;
		
	private:
		// File reading
		std::string ReadFile(const std::string& filePath);
		
		// Source preprocessing (handles #ifdef VERTEX/FRAGMENT format)
		std::string InsertDefineAfterVersion(const std::string& source, const std::string& defineLine);
		
		// SPIR-V compilation pipeline
		void CompileOrGetVulkanBinaries(const std::unordered_map<GLenum, std::string>& shaderSources);
		void CompileOrGetOpenGLBinaries();
		void CreateProgram();
		
		// Reflection from SPIR-V
		void ReflectStage(GLenum stage, const std::vector<uint32_t>& spirvData);
		
		// Cache utilities
		static std::filesystem::path GetCacheDirectory();
		static void CreateCacheDirectoryIfNeeded();
		static const char* GetVulkanCacheExtension(GLenum stage);
		static const char* GetOpenGLCacheExtension(GLenum stage);
		static const char* StageToString(GLenum stage);
		
		// Program data
		GLuint m_ProgramID = 0;
		std::string m_Name;
		std::string m_FilePath;
		ShaderStage m_Stages = ShaderStage::None;
		ShaderReflection m_Reflection;
		bool m_IsCompute = false;
		
		// SPIR-V data (for caching and reflection)
		std::unordered_map<GLenum, std::vector<uint32_t>> m_VulkanSPIRV;
		std::unordered_map<GLenum, std::vector<uint32_t>> m_OpenGLSPIRV;
		std::unordered_map<GLenum, std::string> m_OpenGLSourceCode;
		
		// Uniform cache
		mutable std::unordered_map<std::string, int> m_UniformLocationCache;
		
		// Compute work group size
		uint32_t m_WorkGroupSize[3] = { 1, 1, 1 };
	};

	// ============================================================================
	// OPENGL RHI GRAPHICS PIPELINE
	// ============================================================================
	
	class OpenGLRHIGraphicsPipeline : public RHIGraphicsPipeline {
	public:
		OpenGLRHIGraphicsPipeline(const GraphicsPipelineDesc& desc);
		virtual ~OpenGLRHIGraphicsPipeline() = default;
		
		// RHIResource
		RHIHandle GetNativeHandle() const override { return 0; }
		bool IsValid() const override { return m_Desc.Shader != nullptr; }
		
		// Binding
		void Bind() const override;
		void Unbind() const override;
		
	protected:
		void OnDebugNameChanged() override {}
		
	private:
		void ApplyRasterizerState() const;
		void ApplyDepthStencilState() const;
		void ApplyBlendState() const;
	};

	// ============================================================================
	// OPENGL RHI COMPUTE PIPELINE
	// ============================================================================
	
	class OpenGLRHIComputePipeline : public RHIComputePipeline {
	public:
		OpenGLRHIComputePipeline(const ComputePipelineDesc& desc);
		virtual ~OpenGLRHIComputePipeline() = default;
		
		// RHIResource
		RHIHandle GetNativeHandle() const override { return 0; }
		bool IsValid() const override { return m_Desc.Shader != nullptr; }
		
		// Binding and dispatch
		void Bind() const override;
		void Unbind() const override;
		void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) const override;
		void GetWorkGroupSize(uint32_t& x, uint32_t& y, uint32_t& z) const override;
		
	protected:
		void OnDebugNameChanged() override {}
	};

} // namespace RHI
} // namespace Lunex
