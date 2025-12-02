#pragma once

#include "PostProcessPass.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexArray.h"
#include "Renderer/UniformBuffer.h"
#include "Scene/Entity.h"
#include <vector>
#include <set>

namespace Lunex {
	class SelectionOutlinePass : public PostProcessPass {
	public:
		struct Settings {
			glm::vec4 OutlineColor = glm::vec4(1.0f, 0.6f, 0.0f, 1.0f);
			float Thickness = 3.0f;
			float BlurRadius = 5.0f;
			bool DepthTest = true;
			int BlendMode = 0; // 0=Alpha, 1=Additive, 2=Screen

			// ? DEBUG: Visualization modes
			int DebugMode = 0; // 0=Final, 1=Mask, 2=Blur, 3=Edge
		};

		SelectionOutlinePass();
		virtual ~SelectionOutlinePass();

		void Init(uint32_t width, uint32_t height) override;
		void Shutdown() override;
		void Execute(const PostProcessContext& ctx) override;
		void Resize(uint32_t width, uint32_t height) override;

		void SetSelectedEntities(const std::set<Entity>& entities);
		Settings& GetSettings() { return m_Settings; }

	private:
		void CreateFramebuffers();
		void CreateFullscreenQuad();
		void RenderMaskPass(const PostProcessContext& ctx);
		void RenderBlurPass();
		void RenderEdgePass();
		void RenderCompositePass(const PostProcessContext& ctx);
		void RenderDebugVisualization(const PostProcessContext& ctx); // ? NEW

		// Framebuffers
		Ref<Framebuffer> m_MaskFBO;
		Ref<Framebuffer> m_BlurFBO_A;
		Ref<Framebuffer> m_BlurFBO_B;
		Ref<Framebuffer> m_EdgeFBO;

		// Shaders
		Ref<Shader> m_MaskShader;
		Ref<Shader> m_BlurShader;
		Ref<Shader> m_EdgeShader;
		Ref<Shader> m_CompositeShader;
		Ref<Shader> m_DebugShader;

		// Rendering
		Ref<VertexArray> m_FullscreenQuad;
		Ref<VertexBuffer> m_FullscreenQuadVB;
		
		// ? NEW: UBOs for mask rendering
		Ref<UniformBuffer> m_CameraUBO;
		Ref<UniformBuffer> m_TransformUBO;
		
		// ? NEW: UBOs for blur and composite
		Ref<UniformBuffer> m_BlurParamsUBO;
		Ref<UniformBuffer> m_CompositeParamsUBO;

		// State
		std::vector<Entity> m_SelectedEntities;
		Settings m_Settings;
	};
}
