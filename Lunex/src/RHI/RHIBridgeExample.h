#pragma once

/**
 * @file RHIBridgeExample.h
 * @brief Example showing how to use the RHI bridge mode
 * 
 * The bridge allows existing code to transparently use the new RHI
 * without any code changes. Simply enable bridge mode at startup.
 */

#include "Core/Core.h"
#include "RHI/RHILegacyBridge.h"

namespace Lunex {
namespace Examples {

	/**
	 * @brief Example: Enable RHI bridge mode
	 * 
	 * Call this early in your application startup, before creating
	 * any renderer resources.
	 */
	inline void EnableRHIBridgeExample() {
		// Enable bridge mode - all old API calls will now use RHI internally
		Bridge::EnableBridgeMode(true);
		
		// Now existing code works automatically:
		
		// Old API (no changes needed):
		auto vertexBuffer = VertexBuffer::Create(vertices, size);
		auto indexBuffer = IndexBuffer::Create(indices, count);
		auto vertexArray = VertexArray::Create();
		auto shader = Shader::Create("assets/shaders/Standard.glsl");
		
		FramebufferSpecification fbSpec;
		fbSpec.Width = 1920;
		fbSpec.Height = 1080;
		auto framebuffer = Framebuffer::Create(fbSpec);
		
		// Internally, these all use the new RHI!
		// You can even mix old and new APIs:
		
		// Get underlying RHI resources if needed:
		if (Bridge::IsBridgeModeEnabled()) {
			// Cast to bridge types to access RHI resources
			if (auto* vbBridge = dynamic_cast<Bridge::RHIVertexBufferBridge*>(vertexBuffer.get())) {
				auto rhiBuffer = vbBridge->GetRHIBuffer();
				// Use rhiBuffer with new RHI APIs...
			}
		}
	}

	/**
	 * @brief Example: Gradual migration strategy
	 * 
	 * This shows how to gradually migrate from old API to new RHI:
	 * 1. Enable bridge mode
	 * 2. Old code continues working
	 * 3. Gradually rewrite systems to use RHI directly
	 * 4. Eventually disable bridge mode when migration is complete
	 */
	inline void GradualMigrationExample() {
		// Step 1: Enable bridge (old code still works)
		Bridge::EnableBridgeMode(true);
		
		// Step 2: Old renderer code continues working unchanged
		// (e.g., Renderer2D, Renderer3D, all existing systems)
		
		// Step 3: New systems use RHI directly
		RHI::BufferDesc desc;
		desc.Type = RHI::BufferType::Vertex;
		desc.Size = 1024;
		auto newRHIBuffer = RHI::RHIBuffer::Create(desc);
		
		// Step 4: Eventually migrate all old systems to RHI
		// Step 5: Disable bridge mode when done
		Bridge::EnableBridgeMode(false);
	}

	/**
	 * @brief Example: Check if migration is complete
	 */
	inline bool IsMigrationComplete() {
		// If bridge mode can be disabled without breaking anything,
		// migration is complete
		return !Bridge::IsBridgeModeEnabled();
	}

} // namespace Examples
} // namespace Lunex
