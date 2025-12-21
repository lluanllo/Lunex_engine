#pragma once

#include "Core/Core.h"
#include "Renderer/EnvironmentMap.h"
#include "Scene/Camera/EditorCamera.h"
#include "Scene/Camera/Camera.h"

#include <glm/glm.hpp>

namespace Lunex {
	
	/**
	 * SkyboxRenderer - Renders environment maps as skyboxes
	 * 
	 * Features:
	 *   - Renders at infinite distance (always behind geometry)
	 *   - Does NOT write to entity ID buffer (prevents picking)
	 *   - Supports HDR environments with tone mapping
	 *   - Rotation, intensity, tint, and blur controls
	 *   - Global settings controlled from SettingsPanel (no component needed)
	 */
	class SkyboxRenderer {
	public:
		static void Init();
		static void Shutdown();
		
		// ========================================
		// RENDER WITH SPECIFIC ENVIRONMENT
		// ========================================
		
		/**
		 * Render the skybox using a specific environment
		 * Should be called BEFORE geometry rendering (with depth testing disabled for write)
		 * or AFTER with depth function set to LEQUAL
		 */
		static void Render(const EnvironmentMap& environment, const EditorCamera& camera);
		static void Render(const EnvironmentMap& environment, const Camera& camera, const glm::mat4& transform);
		static void Render(const EnvironmentMap& environment, const glm::mat4& view, const glm::mat4& projection);
		
		/**
		 * Render a simple solid color background
		 * Used when no environment is loaded
		 */
		static void RenderSolidColor(const glm::vec3& color, const EditorCamera& camera);
		
		// ========================================
		// GLOBAL SKYBOX SETTINGS (SettingsPanel)
		// ========================================
		
		// Enable/disable skybox rendering globally
		static void SetEnabled(bool enabled);
		static bool IsEnabled();
		
		// Load HDRI for global environment
		static bool LoadHDRI(const std::string& path);
		static bool HasEnvironmentLoaded();
		
		// Environment settings
		static void SetIntensity(float intensity);
		static float GetIntensity();
		
		static void SetRotation(float rotationDegrees);
		static float GetRotation();
		
		static void SetTint(const glm::vec3& tint);
		static glm::vec3 GetTint();
		
		static void SetBlur(float blur);
		static float GetBlur();
		
		// Background color (when no HDRI loaded)
		static void SetBackgroundColor(const glm::vec3& color);
		static glm::vec3 GetBackgroundColor();
		
		// Apply the stored background color to the renderer clear color
		// This encapsulates RenderCommand usage away from UI code
		static void ApplyBackgroundClearColor();
		
		// Get global environment for IBL lighting
		static Ref<EnvironmentMap> GetGlobalEnvironment();
		
		/**
		 * Get the path of the currently loaded HDRI
		 * Returns empty string if no HDRI is loaded
		 */
		static std::string GetHDRIPath();
		
		// ========================================
		// RENDER GLOBAL SKYBOX
		// ========================================
		
		/**
		 * Render the global skybox (controlled by SettingsPanel)
		 * Call this instead of Render() when using global settings
		 */
		static void RenderGlobalSkybox(const EditorCamera& camera);
		static void RenderGlobalSkybox(const Camera& camera, const glm::mat4& transform);
		
	private:
		static void RenderInternal(const EnvironmentMap& environment, 
								   const glm::mat4& view, 
								   const glm::mat4& projection);
	};
	
}
