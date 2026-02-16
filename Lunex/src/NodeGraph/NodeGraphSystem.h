#pragma once

/**
 * @file NodeGraphSystem.h
 * @brief Node graph system initialization and management
 * 
 * Call NodeGraphSystem::Init() at engine startup to register all
 * built-in node types. Domain-specific modules can register additional
 * nodes after initialization.
 */

#include "NodeGraphCore.h"
#include "CommonNodes.h"
#include "Shader/ShaderNodeTypes.h"

namespace Lunex::NodeGraph {

	class NodeGraphSystem {
	public:
		static void Init() {
			if (s_Initialized) return;

			RegisterCommonNodes();
			Shader::RegisterShaderNodes();
			s_Initialized = true;
		}

		static void Shutdown() {
			s_Initialized = false;
		}

		static bool IsInitialized() { return s_Initialized; }

	private:
		static inline bool s_Initialized = false;
	};

} // namespace Lunex::NodeGraph
