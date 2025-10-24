#include "stpch.h"
#include "RenderGraph.h"

namespace Lunex {
	void RenderGraph::Init() {
		// Initialize passes if needed
		for (auto& p : m_Passes) p->Init(p->GetDesc());
	}
	
	void RenderGraph::Shutdown() {
		for (auto& p : m_Passes) p->Shutdown();
		m_Passes.clear();
	}
	
	void RenderGraph::AddPass(Ref<RenderPass> pass) {
		m_Passes.push_back(pass);
	}
	
	void RenderGraph::Execute() {
		for (auto& p : m_Passes) {
			p->Begin();
			p->Execute();
			p->End();
		}
	}
	
	void RenderGraph::Clear() {
		m_Passes.clear();
	}
}
