#pragma once

#include "Renderer/StorageBuffer.h"

namespace Lunex {
	class OpenGLStorageBuffer : public StorageBuffer {
	public:
		OpenGLStorageBuffer(uint32_t size, const void* data = nullptr);
		virtual ~OpenGLStorageBuffer();
		
		virtual void Bind(uint32_t binding) const override;
		virtual void Unbind() const override;
		
		virtual void SetData(const void* data, uint32_t size, uint32_t offset = 0) override;
		virtual void GetData(void* data, uint32_t size, uint32_t offset = 0) const override;
		
		virtual uint32_t GetSize() const override { return m_Size; }
		
	private:
		uint32_t m_RendererID = 0;
		uint32_t m_Size = 0;
	};
}
