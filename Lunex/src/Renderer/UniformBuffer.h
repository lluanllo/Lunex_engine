#pragma once

#include "Core/Core.h"
#include "RHI/RHIBuffer.h"

namespace Lunex {
	
	/**
	 * @class UniformBuffer
	 * @brief Wrapper around RHI::RHIBuffer for uniform data
	 */
	class UniformBuffer {
	public:
		UniformBuffer(uint32_t size, uint32_t binding);
		~UniformBuffer() = default;
		
		void SetData(const void* data, uint32_t size, uint32_t offset = 0);
		
		// Access underlying RHI buffer
		RHI::RHIBuffer* GetRHIBuffer() const { return m_RHIBuffer.get(); }
		
		static Ref<UniformBuffer> Create(uint32_t size, uint32_t binding);
		
	private:
		Ref<RHI::RHIBuffer> m_RHIBuffer;
		uint32_t m_Binding = 0;
	};
}