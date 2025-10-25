#include "stpch.h"
#include "RenderGraph.h"

namespace Lunex {
    void RenderGraph::Init() {
        for (auto& p : m_Passes)
            p->Init(p->GetDesc());
    }
        
    void RenderGraph::Shutdown() {
        for (auto& p : m_Passes)
            p->Shutdown();
        m_Passes.clear();
    }
        
    void RenderGraph::AddPass(Ref<RenderPass> pass) {
        m_Passes.push_back(pass);
    }
        
    void RenderGraph::Execute(RenderContext& context) {
        context.BeginFrame();
            
        for (auto& p : m_Passes) {
            p->Begin(context);
            p->Execute(context);
            p->End(context);
        }
            
        context.EndFrame();
    }
        
    void RenderGraph::Clear() {
        m_Passes.clear();
    }
}
