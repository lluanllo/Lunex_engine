#pragma once

#include "Core/Core.h"

namespace Lunex {
	
	// Shader Storage Buffer Object (SSBO) for compute shaders
	class StorageBuffer {
	public:
		virtual ~StorageBuffer() = default;
		
		virtual void Bind(uint32_t binding) const = 0;
		virtual void Unbind() const = 0;
		
		virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) = 0;
		virtual void GetData(void* data, uint32_t size, uint32_t offset = 0) const = 0;
		
		virtual uint32_t GetSize() const = 0;
		
		static Ref<StorageBuffer> Create(uint32_t size, const void* data = nullptr);
	};
}
