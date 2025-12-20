#pragma once

#include "Core/Core.h"
#include "RHI/RHIBuffer.h"

namespace Lunex {
	
	/**
	 * @class StorageBuffer
	 * @brief Wrapper around RHI::RHIBuffer for SSBO data
	 */
	class StorageBuffer {
	public:
		StorageBuffer(uint32_t size, uint32_t binding);
		~StorageBuffer() = default;
		
		void SetData(const void* data, uint32_t size, uint32_t offset = 0);
		void GetData(void* data, uint32_t size, uint32_t offset = 0) const;
		
		void BindForCompute(uint32_t binding) const;
		void BindForRead(uint32_t binding) const;
		
		// Access underlying RHI buffer
		RHI::RHIBuffer* GetRHIBuffer() const { return m_RHIBuffer.get(); }
		
		static Ref<StorageBuffer> Create(uint32_t size, uint32_t binding);
		
	private:
		Ref<RHI::RHIBuffer> m_RHIBuffer;
		uint32_t m_Binding = 0;
	};
}
