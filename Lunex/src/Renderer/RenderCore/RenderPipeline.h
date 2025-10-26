#pragma once

#include "Core/Core.h"
#include "RenderContext.h"
#include "RenderCommand.h"
#include "Renderer/Shader.h"
#include "Renderer/Buffer/FrameBuffer.h"

namespace Lunex {
	class RenderPipeline {
		public:
			virtual ~RenderPipeline() = default;
			
			// Inicializa el pipeline (carga shaders, buffers, etc.)
			virtual void Init() = 0;
			
			// Prepara el frame (limpia framebuffer, setea viewport, etc.)
			virtual void BeginFrame() = 0;
			
			// Finaliza el frame (swap buffers, postprocesado, etc.)
			virtual void EndFrame() = 0;
			
			// Envia un draw call (batch o individual)
			virtual void Submit(const Ref<VertexArray>& vertexArray,
				const Ref<Shader>& shader,
				const glm::mat4& transform = glm::mat4(1.0f)) = 0;
			
			// Acceso al framebuffer asociado
			virtual Ref<Framebuffer> GetFramebuffer() const = 0;
			
			// Devuelve nombre o tipo del pipeline
			virtual const std::string& GetName() const = 0;
	};
}
