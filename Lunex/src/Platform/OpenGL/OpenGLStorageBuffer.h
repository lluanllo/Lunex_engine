#pragma once

#include "Renderer/StorageBuffer.h"

namespace Lunex {
	class OpenGLStorageBuffer : public StorageBuffer {
	public:
		OpenGLStorageBuffer(uint32_t size, uint32_t binding);
		virtual ~OpenGLStorageBuffer();
		
		virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) override;
	private:
		uint32_t m_RendererID = 0;
	};
}
