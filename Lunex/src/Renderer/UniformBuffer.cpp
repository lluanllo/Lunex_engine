#include "stpch.h"
#include "UniformBuffer.h"
#include "RHI/RHIDevice.h"

namespace Lunex {
	
	UniformBuffer::UniformBuffer(uint32_t size, uint32_t binding)
		: m_Binding(binding)
	{
		m_RHIBuffer = RHI::RHIDevice::Get()->CreateUniformBuffer(size, RHI::BufferUsage::Dynamic);
		
		// Bind to the specified binding point
		if (m_RHIBuffer) {
			m_RHIBuffer->BindToPoint(binding);
		}
	}
	
	void UniformBuffer::SetData(const void* data, uint32_t size, uint32_t offset) {
		if (m_RHIBuffer) {
			m_RHIBuffer->SetData(data, size, offset);
			// Re-bind to binding point to ensure this buffer is active
			// (multiple UBOs may share the same binding point)
			m_RHIBuffer->BindToPoint(m_Binding);
		}
	}
	
	Ref<UniformBuffer> UniformBuffer::Create(uint32_t size, uint32_t binding) {
		return CreateRef<UniformBuffer>(size, binding);
	}
}