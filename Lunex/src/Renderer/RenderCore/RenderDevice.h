#pragma once

#include "Core/Core.h"
#include "glm/glm.hpp"

#include "Renderer/Buffer/Buffer.h"
#include "Renderer/Buffer/FrameBuffer.h"

#include "Renderer/Shader.h"
#include "Renderer/Texture.h"

namespace Lunex {
	struct DeviceCapabilities {
		int MaxTextureSlots =16;
		int MaxVertexAttributes =16;
	};
	
	class RenderDevice {
		public:
			static void Init();
			static void Shutdown();
			
			virtual ~RenderDevice() = default;
			
			// Creación de recursos
			virtual Ref<VertexBuffer> CreateVertexBuffer(const void* data, uint32_t size) = 0;
			virtual Ref<IndexBuffer> CreateIndexBuffer(const uint32_t* data, uint32_t count) = 0;
			virtual Ref<Shader> CreateShader(const std::string& filepath) = 0;
			virtual Ref<Texture2D> CreateTexture(const TextureSpecification& spec) = 0;
			virtual Ref<Framebuffer> CreateFrameBuffer(const FramebufferSpecification& spec) = 0;
			
			// Ejecución de comandos
			virtual void Submit(const std::function<void()>& command) = 0;
			
			static const DeviceCapabilities& GetCapabilities();
			
		private:
			static DeviceCapabilities s_Capabilities;
	};
}
