#pragma once

#include "Core/Core.h"

#include "RenderPass.h"
#include "RenderContext.h"

#include <vector>

namespace Lunex {
	class RenderGraph {
		public:
			void Init();
			void Shutdown();
			
			void AddPass(Ref<RenderPass> pass);
			void Execute(RenderContext& context);
			
			void Clear();
			
		private:
			std::vector<Ref<RenderPass>> m_Passes;
	};
}
