#pragma once

#include "Renderer/Shader.h"
#include <glm/glm.hpp>

// TODO: Remove!!!!!!!
typedef unsigned int GLenum;

namespace Lunex {
	class OpenGLShader : public Shader {
		public:
			OpenGLShader(const std::string& filepath);
			OpenGLShader(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
			
			// ? NEW: Compute shader constructor
			OpenGLShader(const std::string& filepath, bool isCompute);
			
			virtual ~OpenGLShader();
			
			virtual void Bind() const override;
			virtual void Unbind() const override;
			
			virtual void SetInt(const std::string& name, int value) override;
			virtual void SetIntArray(const std::string& name, int* values, uint32_t count) override;
			virtual void SetFloat(const std::string& name, float value) override;
			virtual void setFloat2(const std::string& name, const glm::vec2& value) override;
			virtual void SetFloat3(const std::string& name, const glm::vec3& value) override;
			virtual void SetFloat4(const std::string& name, const glm::vec4& value) override;
			virtual void SetMat3(const std::string& name, const glm::mat3& value) override;
			virtual void SetMat4(const std::string& name, const glm::mat4& value) override;
			
			virtual const std::string& GetName() const override { return m_Name; }
			
			// ========================================
			// COMPUTE SHADER IMPLEMENTATION
			// ========================================
			virtual void DispatchCompute(uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ) override;
			virtual void GetComputeWorkGroupSize(uint32_t& sizeX, uint32_t& sizeY, uint32_t& sizeZ) const override;
			virtual bool IsComputeShader() const override { return m_IsCompute; }
			
			void UploadUniformInt(const std::string& name, int value);
			void UploadUniformIntArray(const std::string& name, int* values, uint32_t count);
			
			void UploadUniformFloat(const std::string& name, float value);
			void UploadUniformFloat2(const std::string& name, const glm::vec2& value);
			void UploadUniformFloat3(const std::string& name, const glm::vec3& value);
			void UploadUniformFloat4(const std::string& name, const glm::vec4& value);
			
			void UploadUniformMat3(const std::string& name, const glm::mat3& matrix);
			void UploadUniformMat4(const std::string& name, const glm::mat4& matrix);
			
		private:
			std::string ReadFile(const std::string& filepath);
			std::string InsertDefineAfterVersion(const std::string& source, const std::string& defineLine);
			
			void CompileOrGetVulkanBinaries(const std::unordered_map<GLenum, std::string>& shaderSources);
			void CompileOrGetOpenGLBinaries();
			void CreateProgram();
			void Reflect(GLenum stage, const std::vector<uint32_t>& shaderData);
			
		private:
			uint32_t m_RendererID;
			std::string m_FilePath;
			std::string m_Name;
			
			bool m_IsCompute = false; // ? NEW: Track if this is a compute shader
			
			std::unordered_map<GLenum, std::vector<uint32_t>> m_VulkanSPIRV;
			std::unordered_map<GLenum, std::vector<uint32_t>> m_OpenGLSPIRV;
			
			std::unordered_map<GLenum, std::string> m_OpenGLSourceCode;
	};
}