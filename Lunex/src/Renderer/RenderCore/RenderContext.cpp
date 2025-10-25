#include "stpch.h"
#include "RenderContext.h"

namespace Lunex {
    void RenderContext::BeginFrame() {
        if (TargetFramebuffer)
            TargetFramebuffer->Bind();
    }
        
    void RenderContext::EndFrame() {
        if (TargetFramebuffer)
            TargetFramebuffer->Unbind();
    }
}
