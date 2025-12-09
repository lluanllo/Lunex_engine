#pragma once

#include "Core/Core.h"
#include "Renderer/Shader.h"
#include "Renderer/Framebuffer.h"
#include "Renderer/VertexArray.h"

#include <glm/glm.hpp>

namespace Lunex {

	// ============================================================================
	// SSR SETTINGS
	// Configuration for Screen Space Reflections
	// ============================================================================
	
	struct SSRSettings {
		bool Enabled = false;  // Disabled by default
		
		// Quality settings
		float MaxDistance = 100.0f;        // Maximum ray travel distance
		float Resolution = 1.0f;           // Resolution scale (0.5 = half res)
		float Thickness = 1.0f;            // Depth comparison thickness (increased from 0.1)
		float StepSize = 1.0f;             // Step size multiplier
		int MaxSteps = 128;                // Maximum ray march steps
		
		// Visual settings
		float Intensity = 1.5f;            // Reflection intensity (increased from 1.0)
		float RoughnessThreshold = 0.5f;   // Max roughness for SSR
		float EdgeFade = 0.1f;             // Screen edge fade
		
		// Environment fallback
		bool UseEnvironmentFallback = false; // Use cubemap when SSR misses (off by default)
		float EnvironmentIntensity = 0.5f;   // Environment reflection intensity
		
		// Debug
		bool DebugMode = false;            // Show SSR only
	};

	// ============================================================================
	// SSR RENDERER
	// Screen Space Reflections using Linear Ray Tracing
	// ============================================================================
	
	class SSRRenderer {
	public:
		static void Init();
		static void Shutdown();
		
		// Configure SSR
		static void SetSettings(const SSRSettings& settings);
		static SSRSettings& GetSettings();
		
		// Enable/Disable
		static void SetEnabled(bool enabled);
		static bool IsEnabled();
		
		// Render SSR pass
		// Inputs:
		//   - sceneColorTexture: Final rendered scene color (after lighting)
		//   - sceneDepthTexture: Scene depth buffer
		//   - sceneNormalTexture: World-space normals + reflection mask in alpha
		//   - environmentMap: Cubemap for fallback reflections (0 if none)
		//   - viewMatrix: Camera view matrix
		//   - projectionMatrix: Camera projection matrix
		//   - viewportSize: Current viewport dimensions
		static void Render(
			uint32_t sceneColorTexture,
			uint32_t sceneDepthTexture,
			uint32_t sceneNormalTexture,
			uint32_t environmentMap,
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec2& viewportSize
		);
		
		// Composite SSR result back to the target framebuffer
		// This should be called after Render() with the main framebuffer bound
		static void Composite(
			uint32_t sceneColorTexture,
			const glm::vec2& viewportSize
		);
		
		// Get the result texture (to be composited)
		static uint32_t GetResultTexture();
		
		// Resize internal buffers
		static void Resize(uint32_t width, uint32_t height);
		
	private:
		static void CreateResources();
		static void UpdateUniformBuffer(
			const glm::mat4& viewMatrix,
			const glm::mat4& projectionMatrix,
			const glm::vec2& viewportSize
		);
	};

}
