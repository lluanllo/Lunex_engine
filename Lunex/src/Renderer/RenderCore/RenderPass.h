#pragma once

#include "Core/Core.h"
#include <string>

namespace Lunex {
	struct RenderPassDesc {
		std::string Name;
		uint32_t Width =0, Height =0;
		bool ClearOnStart = true;
	};
	
	class RenderPass {
		public:
			RenderPass() = default;
			virtual ~RenderPass() = default;
			
			virtual void Init(const RenderPassDesc& desc) { m_Desc = desc; }
			virtual void Shutdown() {}
			
			virtual void Begin() {}
			virtual void Execute() =0;
			virtual void End() {}
			
			const RenderPassDesc& GetDesc() const { return m_Desc; }
			
		protected:
			RenderPassDesc m_Desc;
	};
}
