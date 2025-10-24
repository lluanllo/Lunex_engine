#pragma once

#include "Core/Core.h"
#include "RenderPass.h"

namespace Lunex {
	class RenderGraph {
		public:
			void Init();
			void Shutdown();
			
			void AddPass(Ref<RenderPass> pass);
			void Execute();
			
			void Clear();
			
		private:
			std::vector<Ref<RenderPass>> m_Passes;
	};
}
