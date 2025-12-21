#pragma once

#include "Scene/Camera/OrthographicCamera.h"
#include "Shader.h"
#include "VertexArray.h"
#include "RHI/RHI.h"

namespace Lunex {
	class Renderer {	
		public:
			static void Init();
			static void Shutdown();
			
			static void onWindowResize(uint32_t width, uint32_t height);
			
			static void BeginScene(OrthographicCamera& camera);
			static void EndScene();
			
			static void Submit(const Ref<Shader>& shader, const Ref<VertexArray>& vertexArray, const glm::mat4& transform = glm::mat4(1.0f));
			
			inline static RHI::GraphicsAPI GetAPI() { return RHI::GetCurrentAPI(); }
			
		private:
			struct SceneData{
				glm::mat4 ViewProjectionMatrix;
			};
			
			static Scope<SceneData> s_SceneData;
	};
}