#pragma once
#include "Core/Core.h"
#include "Renderer/Camera.h"
#include "Renderer/Buffer/FrameBuffer.h"

namespace Lunex {
    struct RenderContext {
        Ref<Framebuffer> TargetFramebuffer;
        Ref<Camera> ActiveCamera;
        glm::mat4 ViewProjectionMatrix{};
        glm::vec3 CameraPosition{};
            
        void BeginFrame();
        void EndFrame();
    };
}
