#include "stpch.h"
#include "StorageBuffer.h"
#include "RHI/RHIDevice.h"

namespace Lunex {
	
	StorageBuffer::StorageBuffer(uint32_t size, uint32_t binding)
		: m_Binding(binding)
	{
		RHI::BufferCreateInfo info;
		info.Size = size;
		info.Type = RHI::BufferType::Storage;
		info.Usage = RHI::BufferUsage::Dynamic;
		
		m_RHIBuffer = RHI::RHIDevice::Get()->CreateBuffer(info);
		
		// Bind to the specified binding point
		if (m_RHIBuffer) {
			m_RHIBuffer->BindToPoint(binding);
		}
	}
	
	void StorageBuffer::SetData(const void* data, uint32_t size, uint32_t offset) {
		if (m_RHIBuffer) {
			m_RHIBuffer->SetData(data, size, offset);
		}
	}
	
	void StorageBuffer::GetData(void* data, uint32_t size, uint32_t offset) const {
		if (m_RHIBuffer) {
			m_RHIBuffer->GetData(data, size, offset);
		}
	}
	
	void StorageBuffer::BindForCompute(uint32_t binding) const {
		if (m_RHIBuffer) {
			m_RHIBuffer->BindToPoint(binding);
		}
	}
	
	void StorageBuffer::BindForRead(uint32_t binding) const {
		if (m_RHIBuffer) {
			m_RHIBuffer->BindToPoint(binding);
		}
	}
	
	Ref<StorageBuffer> StorageBuffer::Create(uint32_t size, uint32_t binding) {
		return CreateRef<StorageBuffer>(size, binding);
	}
}
