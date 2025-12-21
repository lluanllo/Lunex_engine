#pragma once

/**
 * @file CameraSystem.h
 * @brief Multi-camera management system
 * 
 * AAA Architecture:
 * - Manages all cameras in the scene
 * - Supports multiple active cameras (split-screen, reflections, shadow maps)
 * - Produces ViewData for the renderer
 * - Handles camera switching and transitions
 */

#include "CameraData.h"
#include "Core/Core.h"
#include "Core/UUID.h"

#include <vector>
#include <unordered_map>

namespace Lunex {

	// Forward declarations
	class Entity;
	class Scene;
	class EditorCamera;

	// ============================================================================
	// CAMERA HANDLE
	// ============================================================================
	
	/**
	 * @struct CameraHandle
	 * @brief Lightweight handle to reference a camera
	 */
	struct CameraHandle {
		UUID ID = 0;
		
		bool IsValid() const { return ID != 0; }
		bool operator==(const CameraHandle& other) const { return ID == other.ID; }
		bool operator!=(const CameraHandle& other) const { return ID != other.ID; }
	};

	// ============================================================================
	// CAMERA ENTRY
	// ============================================================================
	
	/**
	 * @struct CameraEntry
	 * @brief Internal storage for a registered camera
	 */
	struct CameraEntry {
		CameraHandle Handle;
		std::string Name;
		
		// Cached view data (updated each frame)
		ViewData CachedViewData;
		ViewFrustum CachedFrustum;
		
		// Camera state
		bool IsActive = true;
		bool IsPrimary = false;
		int Priority = 0;  // Higher priority cameras render first
		
		// For scene cameras
		UUID EntityID = 0;
		
		// For editor camera (special case)
		bool IsEditorCamera = false;
	};

	// ============================================================================
	// CAMERA SYSTEM
	// ============================================================================
	
	/**
	 * @class CameraSystem
	 * @brief Manages all cameras in the engine
	 * 
	 * Usage:
	 * ```cpp
	 * // Get primary camera view data
	 * ViewData view = CameraSystem::Get().GetPrimaryViewData();
	 * 
	 * // Get all active cameras (for multi-view rendering)
	 * auto cameras = CameraSystem::Get().GetActiveCameras();
	 * ```
	 */
	class CameraSystem {
	public:
		// ========== SINGLETON ==========
		static CameraSystem& Get();
		
		// ========== LIFECYCLE ==========
		void Initialize();
		void Shutdown();
		void Update(float deltaTime);
		
		// ========== CAMERA REGISTRATION ==========
		
		/**
		 * @brief Register an entity with CameraComponent
		 */
		CameraHandle RegisterSceneCamera(Entity entity, Scene* scene);
		
		/**
		 * @brief Unregister a camera
		 */
		void UnregisterCamera(CameraHandle handle);
		void UnregisterCameraByEntity(UUID entityID);
		
		/**
		 * @brief Update a scene camera from entity transform
		 */
		void UpdateSceneCamera(Entity entity, Scene* scene);
		
		// ========== EDITOR CAMERA ==========
		
		/**
		 * @brief Set the editor camera (only one can exist)
		 */
		void SetEditorCamera(EditorCamera* camera);
		
		/**
		 * @brief Get editor camera handle
		 */
		CameraHandle GetEditorCameraHandle() const { return m_EditorCameraHandle; }
		
		/**
		 * @brief Enable/disable editor camera as primary
		 */
		void SetEditorCameraActive(bool active);
		
		// ========== VIEW DATA ACCESS ==========
		
		/**
		 * @brief Get view data for primary camera
		 */
		const ViewData& GetPrimaryViewData() const;
		
		/**
		 * @brief Get view data for a specific camera
		 */
		const ViewData& GetViewData(CameraHandle handle) const;
		
		/**
		 * @brief Get frustum for a specific camera
		 */
		const ViewFrustum& GetFrustum(CameraHandle handle) const;
		
		/**
		 * @brief Get all active camera render infos
		 */
		std::vector<CameraRenderInfo> GetActiveCameraInfos() const;
		
		// ========== CAMERA QUERIES ==========
		
		/**
		 * @brief Get the primary camera handle
		 */
		CameraHandle GetPrimaryCamera() const { return m_PrimaryCamera; }
		
		/**
		 * @brief Set the primary camera
		 */
		void SetPrimaryCamera(CameraHandle handle);
		
		/**
		 * @brief Find camera by name
		 */
		CameraHandle FindCameraByName(const std::string& name) const;
		
		/**
		 * @brief Get all registered cameras
		 */
		const std::vector<CameraHandle>& GetAllCameras() const { return m_CameraOrder; }
		
		/**
		 * @brief Check if a camera exists
		 */
		bool HasCamera(CameraHandle handle) const;
		
		// ========== FRUSTUM CULLING ==========
		
		/**
		 * @brief Check if a sphere is visible from primary camera
		 */
		bool IsVisibleFromPrimary(const glm::vec3& center, float radius) const;
		
		/**
		 * @brief Check if an AABB is visible from primary camera
		 */
		bool IsVisibleFromPrimary(const glm::vec3& min, const glm::vec3& max) const;
		
		// ========== SCENE INTEGRATION ==========
		
		/**
		 * @brief Sync all cameras from scene entities
		 * Call this when scene changes or on frame start
		 */
		void SyncFromScene(Scene* scene);
		
		/**
		 * @brief Clear all scene cameras (keep editor camera)
		 */
		void ClearSceneCameras();
		
	private:
		CameraSystem() = default;
		~CameraSystem() = default;
		
		// Non-copyable
		CameraSystem(const CameraSystem&) = delete;
		CameraSystem& operator=(const CameraSystem&) = delete;
		
		// Internal helpers
		CameraEntry* FindEntry(CameraHandle handle);
		const CameraEntry* FindEntry(CameraHandle handle) const;
		void UpdateFrustum(CameraEntry& entry);
		void UpdatePrimaryCamera();
		
	private:
		// Camera storage
		std::unordered_map<uint64_t, CameraEntry> m_Cameras;
		std::vector<CameraHandle> m_CameraOrder;  // Sorted by priority
		
		// Primary camera (used when no specific camera requested)
		CameraHandle m_PrimaryCamera;
		
		// Editor camera (special handling)
		EditorCamera* m_EditorCameraPtr = nullptr;
		CameraHandle m_EditorCameraHandle;
		bool m_EditorCameraActive = true;
		
		// Default view data (returned when no camera available)
		ViewData m_DefaultViewData;
		ViewFrustum m_DefaultFrustum;
		
		// State
		bool m_Initialized = false;
	};

} // namespace Lunex
