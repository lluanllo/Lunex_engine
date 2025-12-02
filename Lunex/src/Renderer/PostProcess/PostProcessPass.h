#pragma once

#include "Core/Core.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/EditorCamera.h"
#include "Scene/Scene.h"

namespace Lunex {
	struct PostProcessContext {
		Ref<Framebuffer> InputFBO;
		Ref<Framebuffer> OutputFBO;
		const EditorCamera* Camera;
		Scene* ActiveScene;
		uint32_t Width;
		uint32_t Height;
	};

	class PostProcessPass {
	public:
		virtual ~PostProcessPass() = default;

		virtual void Init(uint32_t width, uint32_t height) = 0;
		virtual void Shutdown() = 0;
		virtual void Execute(const PostProcessContext& ctx) = 0;
		virtual void Resize(uint32_t width, uint32_t height) = 0;

	protected:
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
	};
}
