#pragma once

#include "Core/Core.h"

namespace Lunex {
	class StorageBuffer {
	public:
		virtual ~StorageBuffer() {}
		virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) = 0;
		
		static Ref<StorageBuffer> Create(uint32_t size, uint32_t binding);
	};
}
