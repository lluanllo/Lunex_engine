#pragma once

#include "Renderer/RenderCore/RenderCommand.h"
#include "Renderer/RenderCore/RenderContext.h"
#include "Renderer/RenderCore/RenderGraph.h"
#include "Renderer/RenderCore/RenderPass.h"

#include "Renderer/CameraTypes/OrthographicCamera.h"
#include "Renderer/Shader.h"
#include "Renderer/Buffer/VertexArray.h"

namespace Lunex {
	class Renderer {
		public:
			static void Init();
			static void Shutdown();
			
			static void OnWindowResize(uint32_t width, uint32_t height);
			
			static void BeginScene(OrthographicCamera& camera);
			static void EndScene();
			
			static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform = glm::mat4(1.0f));
			
			static void BeginFrame();
			static void EndFrame();
			
			inline static RendererAPI::API GetAPI() { return RendererAPI::GetAPI(); }
			
		private:
			struct SceneData {
				glm::mat4 ViewProjectionMatrix;
				glm::vec3 CameraPosition;
			};
			
			static Scope<SceneData> s_SceneData;
			static Scope<RenderGraph> s_RenderGraph;
			static Ref<RenderContext> s_Context;
	};
}
