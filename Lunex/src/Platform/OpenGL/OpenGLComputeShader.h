#pragma once

#include "Renderer/ComputeShader.h"
#include <glm/glm.hpp>

typedef unsigned int GLenum;

namespace Lunex {
	class OpenGLComputeShader : public ComputeShader {
	public:
		OpenGLComputeShader(const std::string& filepath);
		OpenGLComputeShader(const std::string& name, const std::string& source);
		virtual ~OpenGLComputeShader();
		
		virtual void Bind() const override;
		virtual void Unbind() const override;
		
		virtual void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) override;
		virtual void MemoryBarrier(uint32_t barriers) override;
		
		virtual void SetInt(const std::string& name, int value) override;
		virtual void SetFloat(const std::string& name, float value) override;
		virtual void SetFloat3(const std::string& name, const glm::vec3& value) override;
		virtual void SetMat4(const std::string& name, const glm::mat4& value) override;
		void UploadUniformInt(const std::string& name, int value);
		void UploadUniformFloat(const std::string& name, float value);
		void UploadUniformFloat3(const std::string& name, const glm::vec3& value);
		void UploadUniformMat4(const std::string& name, const glm::mat4& matrix);
		
		virtual uint32_t GetRendererID() const override { return m_RendererID; }
		
		virtual const std::string& GetName() const override { return m_Name; }
		
	private:
		std::string ReadFile(const std::string& filepath);
		void CompileShader(const std::string& source);
		void Reflect(const std::vector<uint32_t>& shaderData);
		
	private:
		uint32_t m_RendererID;
		std::string m_FilePath;
		std::string m_Name;
	};
}
