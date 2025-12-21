#pragma once

/**
 * @file Shader.h
 * @brief Legacy Shader interface - now wraps RHI::RHIShader
 * 
 * This file provides backwards compatibility with existing code.
 * All shader operations are delegated to the RHI layer.
 */

#include <string>
#include <unordered_map>
#include <glm/glm.hpp>
#include "Core/Core.h"
#include "RHI/RHIShader.h"

namespace Lunex {

	/**
	 * @class Shader
	 * @brief Legacy shader wrapper - delegates to RHI::RHIShader
	 */
	class Shader {
	public:
		Shader(Ref<RHI::RHIShader> rhiShader) : m_RHIShader(rhiShader) {}
		virtual ~Shader() = default;
		
		void Bind() const { if (m_RHIShader) m_RHIShader->Bind(); }
		void Unbind() const { if (m_RHIShader) m_RHIShader->Unbind(); }
		
		void SetInt(const std::string& name, int value) { if (m_RHIShader) m_RHIShader->SetInt(name, value); }
		void SetIntArray(const std::string& name, int* values, uint32_t count) { if (m_RHIShader) m_RHIShader->SetIntArray(name, values, count); }
		void SetFloat(const std::string& name, float value) { if (m_RHIShader) m_RHIShader->SetFloat(name, value); }
		void setFloat2(const std::string& name, const glm::vec2& value) { if (m_RHIShader) m_RHIShader->SetFloat2(name, value); }
		void SetFloat2(const std::string& name, const glm::vec2& value) { if (m_RHIShader) m_RHIShader->SetFloat2(name, value); }
		void SetFloat3(const std::string& name, const glm::vec3& value) { if (m_RHIShader) m_RHIShader->SetFloat3(name, value); }
		void SetFloat4(const std::string& name, const glm::vec4& value) { if (m_RHIShader) m_RHIShader->SetFloat4(name, value); }
		void SetMat4(const std::string& name, const glm::mat4& value) { if (m_RHIShader) m_RHIShader->SetMat4(name, value); }
		void SetMat3(const std::string& name, const glm::mat3& value) { if (m_RHIShader) m_RHIShader->SetMat3(name, value); }
		
		const std::string& GetName() const { 
			static std::string empty;
			return m_RHIShader ? m_RHIShader->GetName() : empty; 
		}
		
		// Compute shader API
		void DispatchCompute(uint32_t numGroupsX, uint32_t numGroupsY, uint32_t numGroupsZ) {
			if (m_RHIShader) m_RHIShader->Dispatch(numGroupsX, numGroupsY, numGroupsZ);
		}
		
		void GetComputeWorkGroupSize(uint32_t& sizeX, uint32_t& sizeY, uint32_t& sizeZ) const {
			if (m_RHIShader) {
				m_RHIShader->GetWorkGroupSize(sizeX, sizeY, sizeZ);
			} else {
				sizeX = sizeY = sizeZ = 1;
			}
		}
		
		bool IsComputeShader() const { return m_RHIShader ? m_RHIShader->IsCompute() : false; }
		
		// Access underlying RHI shader
		Ref<RHI::RHIShader> GetRHIShader() const { return m_RHIShader; }
		
		// Factory methods
		static Ref<Shader> Create(const std::string& filepath);
		static Ref<Shader> Create(const std::string& name, const std::string& vertexSrc, const std::string& fragmentSrc);
		static Ref<Shader> CreateCompute(const std::string& filepath);
		
	private:
		Ref<RHI::RHIShader> m_RHIShader;
	};
	
	/**
	 * @class ShaderLibrary
	 * @brief Manages a collection of shaders
	 */
	class ShaderLibrary {
	public:
		void Add(const Ref<Shader>& shader);
		void Add(const std::string& name, const Ref<Shader>& shader);
		
		Ref<Shader> Load(const std::string& filepath);
		Ref<Shader> Load(const std::string& name, const std::string& filepath);
		
		Ref<Shader> Get(const std::string& name);
		
		bool Exists(const std::string& name) const;
		
	private:
		std::unordered_map<std::string, Ref<Shader>> m_Shaders;
	};
}
