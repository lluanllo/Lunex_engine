#pragma once

/**
 * @file LightSystem.h
 * @brief Lighting system for managing scene lights
 * 
 * AAA Architecture: LightSystem lives in Scene/Lighting/
 * Aggregates lights, performs culling, generates GPU buffers.
 */

#include "LightTypes.h"
#include "Core/Core.h"
#include "Core/UUID.h"
#include "Scene/Camera/CameraData.h"

#include <vector>
#include <unordered_map>

namespace Lunex {

	// Forward declarations
	class Entity;
	class Scene;

	/**
	 * @struct LightEntry
	 * @brief Internal storage for a registered light
	 */
	struct LightEntry {
		UUID EntityID;
		LightProperties Properties;
		glm::vec3 WorldPosition;
		glm::vec3 WorldDirection;
		bool IsActive = true;
		bool IsVisible = true;  // After culling
	};

	/**
	 * @class LightSystem
	 * @brief Manages all lights in the scene
	 * 
	 * Provides:
	 * - Light aggregation from scene entities
	 * - Frustum culling for lights
	 * - GPU buffer generation
	 * - Shadow map assignment
	 */
	class LightSystem {
	public:
		// ========== SINGLETON ==========
		static LightSystem& Get();
		
		// ========== LIFECYCLE ==========
		void Initialize();
		void Shutdown();
		
		// ========== SCENE SYNC ==========
		
		/**
		 * @brief Sync all lights from scene entities
		 */
		void SyncFromScene(Scene* scene);
		
		/**
		 * @brief Clear all lights
		 */
		void ClearLights();
		
		// ========== CULLING ==========
		
		/**
		 * @brief Cull lights against a view frustum
		 */
		void CullLights(const ViewFrustum& frustum);
		
		// ========== DATA ACCESS ==========
		
		/**
		 * @brief Get lighting data for rendering
		 * Returns all visible lights after culling.
		 */
		LightingData GetLightingData() const;
		
		/**
		 * @brief Get all lights (no culling)
		 */
		const std::vector<LightEntry>& GetAllLights() const { return m_Lights; }
		
		/**
		 * @brief Get visible light count
		 */
		uint32_t GetVisibleLightCount() const;
		
		// ========== AMBIENT ==========
		
		void SetAmbientColor(const glm::vec3& color) { m_AmbientColor = color; }
		void SetAmbientIntensity(float intensity) { m_AmbientIntensity = intensity; }
		
		const glm::vec3& GetAmbientColor() const { return m_AmbientColor; }
		float GetAmbientIntensity() const { return m_AmbientIntensity; }
		
		// ========== STATISTICS ==========
		
		uint32_t GetTotalLightCount() const { return static_cast<uint32_t>(m_Lights.size()); }
		uint32_t GetDirectionalLightCount() const { return m_DirectionalCount; }
		uint32_t GetPointLightCount() const { return m_PointCount; }
		uint32_t GetSpotLightCount() const { return m_SpotCount; }
		
		// ========== LIMITS ==========
		
		static constexpr uint32_t MAX_LIGHTS = 256;
		static constexpr uint32_t MAX_SHADOW_CASTING_LIGHTS = 16;
		
	private:
		LightSystem() = default;
		~LightSystem() = default;
		
		LightSystem(const LightSystem&) = delete;
		LightSystem& operator=(const LightSystem&) = delete;
		
		void UpdateLightCounts();
		bool IsLightVisible(const LightEntry& light, const ViewFrustum& frustum) const;
		
	private:
		std::vector<LightEntry> m_Lights;
		
		// Ambient lighting
		glm::vec3 m_AmbientColor = { 0.03f, 0.03f, 0.03f };
		float m_AmbientIntensity = 1.0f;
		
		// Counts by type
		uint32_t m_DirectionalCount = 0;
		uint32_t m_PointCount = 0;
		uint32_t m_SpotCount = 0;
		
		bool m_Initialized = false;
	};

} // namespace Lunex
