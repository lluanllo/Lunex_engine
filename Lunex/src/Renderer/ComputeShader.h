#pragma once

#include "Core/Core.h"
#include <glm/glm.hpp>

namespace Lunex {
	
	class ComputeShader {
	public:
		virtual ~ComputeShader() = default;
		
		virtual void Bind() const = 0;
		virtual void Unbind() const = 0;
		
		// Dispatch compute work
		virtual void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) = 0;
		
		// Memory barrier for sync
		virtual void MemoryBarrier(uint32_t barriers) = 0;
		
		// Uniforms
		virtual void SetInt(const std::string& name, int value) = 0;
		virtual void SetFloat(const std::string& name, float value) = 0;
		virtual void SetFloat3(const std::string& name, const glm::vec3& value) = 0;
		virtual void SetMat4(const std::string& name, const glm::mat4& value) = 0;
		
		// Get native ID for direct GL calls
		virtual uint32_t GetRendererID() const = 0;
		
		virtual const std::string& GetName() const = 0;
		
		static Ref<ComputeShader> Create(const std::string& filepath);
		static Ref<ComputeShader> Create(const std::string& name, const std::string& source);
	};
}
