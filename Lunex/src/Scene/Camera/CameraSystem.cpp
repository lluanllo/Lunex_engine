#include "stpch.h"
#include "CameraSystem.h"
#include "Scene/Entity.h"
#include "Scene/Scene.h"
#include "Scene/Components.h"
#include "Scene/Camera/EditorCamera.h"
#include "Log/Log.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Lunex {

	// ============================================================================
	// SINGLETON
	// ============================================================================

	CameraSystem& CameraSystem::Get() {
		static CameraSystem instance;
		return instance;
	}

	// ============================================================================
	// LIFECYCLE
	// ============================================================================

	void CameraSystem::Initialize() {
		if (m_Initialized) return;
		
		m_Cameras.clear();
		m_CameraOrder.clear();
		m_PrimaryCamera = CameraHandle{};
		m_EditorCameraPtr = nullptr;
		m_EditorCameraHandle = CameraHandle{};
		
		// Setup default view data
		m_DefaultViewData.ViewMatrix = glm::lookAt(
			glm::vec3(0.0f, 5.0f, 10.0f),
			glm::vec3(0.0f, 0.0f, 0.0f),
			glm::vec3(0.0f, 1.0f, 0.0f)
		);
		m_DefaultViewData.ProjectionMatrix = glm::perspective(
			glm::radians(45.0f),
			16.0f / 9.0f,
			0.1f,
			1000.0f
		);
		m_DefaultViewData.ComputeDerivedMatrices();
		m_DefaultFrustum = ViewFrustum::FromViewProjection(m_DefaultViewData.ViewProjectionMatrix);
		
		m_Initialized = true;
		LNX_LOG_INFO("CameraSystem initialized");
	}

	void CameraSystem::Shutdown() {
		m_Cameras.clear();
		m_CameraOrder.clear();
		m_PrimaryCamera = CameraHandle{};
		m_EditorCameraPtr = nullptr;
		m_EditorCameraHandle = CameraHandle{};
		m_Initialized = false;
		
		LNX_LOG_INFO("CameraSystem shutdown");
	}

	void CameraSystem::Update(float deltaTime) {
		// Update primary camera selection
		UpdatePrimaryCamera();
		
		// Update frustums for all active cameras
		for (auto& [id, entry] : m_Cameras) {
			if (entry.IsActive) {
				UpdateFrustum(entry);
			}
		}
	}

	// ============================================================================
	// CAMERA REGISTRATION
	// ============================================================================

	CameraHandle CameraSystem::RegisterSceneCamera(Entity entity, Scene* scene) {
		if (!entity.HasComponent<CameraComponent>()) {
			LNX_LOG_WARN("CameraSystem: Entity does not have CameraComponent");
			return CameraHandle{};
		}
		
		UUID entityID = entity.GetUUID();
		
		// Check if already registered
		for (auto& [id, entry] : m_Cameras) {
			if (entry.EntityID == entityID) {
				return entry.Handle;
			}
		}
		
		// Create new entry
		CameraEntry entry;
		entry.Handle = CameraHandle{ UUID() };
		entry.EntityID = entityID;
		entry.IsEditorCamera = false;
		
		// Get name from entity
		if (entity.HasComponent<TagComponent>()) {
			entry.Name = entity.GetComponent<TagComponent>().Tag;
		} else {
			entry.Name = "Camera";
		}
		
		// Get camera properties
		auto& cameraComp = entity.GetComponent<CameraComponent>();
		entry.IsPrimary = cameraComp.Primary;
		entry.IsActive = true;
		
		// Build initial view data
		UpdateSceneCamera(entity, scene);
		entry.CachedViewData = m_DefaultViewData;  // Will be updated properly
		
		// Store
		m_Cameras[entry.Handle.ID] = entry;
		m_CameraOrder.push_back(entry.Handle);
		
		// Update primary if needed
		if (entry.IsPrimary) {
			UpdatePrimaryCamera();
		}
		
		LNX_LOG_INFO("CameraSystem: Registered scene camera '{0}'", entry.Name);
		return entry.Handle;
	}

	void CameraSystem::UnregisterCamera(CameraHandle handle) {
		auto it = m_Cameras.find(handle.ID);
		if (it == m_Cameras.end()) return;
		
		std::string name = it->second.Name;
		m_Cameras.erase(it);
		
		// Remove from order
		m_CameraOrder.erase(
			std::remove(m_CameraOrder.begin(), m_CameraOrder.end(), handle),
			m_CameraOrder.end()
		);
		
		// Update primary if we removed it
		if (m_PrimaryCamera == handle) {
			m_PrimaryCamera = CameraHandle{};
			UpdatePrimaryCamera();
		}
		
		LNX_LOG_INFO("CameraSystem: Unregistered camera '{0}'", name);
	}

	void CameraSystem::UnregisterCameraByEntity(UUID entityID) {
		CameraHandle toRemove{};
		for (auto& [id, entry] : m_Cameras) {
			if (entry.EntityID == entityID) {
				toRemove = entry.Handle;
				break;
			}
		}
		
		if (toRemove.IsValid()) {
			UnregisterCamera(toRemove);
		}
	}

	void CameraSystem::UpdateSceneCamera(Entity entity, Scene* scene) {
		if (!entity.HasComponent<CameraComponent>()) return;
		
		UUID entityID = entity.GetUUID();
		CameraEntry* entry = nullptr;
		
		for (auto& [id, e] : m_Cameras) {
			if (e.EntityID == entityID) {
				entry = &e;
				break;
			}
		}
		
		if (!entry) return;
		
		auto& cameraComp = entity.GetComponent<CameraComponent>();
		auto& transform = entity.GetComponent<TransformComponent>();
		
		entry->IsPrimary = cameraComp.Primary;
		
		// Build view matrix from transform
		glm::mat4 transformMat = transform.GetTransform();
		entry->CachedViewData.ViewMatrix = glm::inverse(transformMat);
		
		// Get projection from camera
		entry->CachedViewData.ProjectionMatrix = cameraComp.Camera.GetProjection();
		
		// Camera position and direction
		entry->CachedViewData.CameraPosition = transform.Translation;
		entry->CachedViewData.CameraDirection = -glm::normalize(glm::vec3(transformMat[2]));
		entry->CachedViewData.CameraUp = glm::normalize(glm::vec3(transformMat[1]));
		entry->CachedViewData.CameraRight = glm::normalize(glm::vec3(transformMat[0]));
		
		// Clip planes from scene camera
		entry->CachedViewData.NearPlane = cameraComp.Camera.GetPerspectiveNearClip();
		entry->CachedViewData.FarPlane = cameraComp.Camera.GetPerspectiveFarClip();
		entry->CachedViewData.FieldOfView = glm::degrees(cameraComp.Camera.GetPerspectiveVerticalFOV());
		entry->CachedViewData.AspectRatio = cameraComp.Camera.GetAspectRatio();
		
		// Compute derived matrices
		entry->CachedViewData.ComputeDerivedMatrices();
		
		// Update frustum
		UpdateFrustum(*entry);
	}

	// ============================================================================
	// EDITOR CAMERA
	// ============================================================================

	void CameraSystem::SetEditorCamera(EditorCamera* camera) {
		m_EditorCameraPtr = camera;
		
		if (camera) {
			// Create or update editor camera entry
			if (!m_EditorCameraHandle.IsValid()) {
				m_EditorCameraHandle = CameraHandle{ UUID() };
				
				CameraEntry entry;
				entry.Handle = m_EditorCameraHandle;
				entry.Name = "EditorCamera";
				entry.IsEditorCamera = true;
				entry.IsActive = true;
				entry.IsPrimary = true;
				entry.Priority = 1000;  // High priority
				
				m_Cameras[m_EditorCameraHandle.ID] = entry;
				m_CameraOrder.insert(m_CameraOrder.begin(), m_EditorCameraHandle);
			}
			
			// Update view data
			auto& entry = m_Cameras[m_EditorCameraHandle.ID];
			entry.CachedViewData.ViewMatrix = camera->GetViewMatrix();
			entry.CachedViewData.ProjectionMatrix = camera->GetProjection();
			entry.CachedViewData.CameraPosition = camera->GetPosition();
			entry.CachedViewData.CameraDirection = camera->GetForwardDirection();
			entry.CachedViewData.CameraUp = camera->GetUpDirection();
			entry.CachedViewData.CameraRight = camera->GetRightDirection();
			entry.CachedViewData.NearPlane = camera->GetNearClip();
			entry.CachedViewData.FarPlane = camera->GetFarClip();
			entry.CachedViewData.ComputeDerivedMatrices();
			
			UpdateFrustum(entry);
		}
		
		UpdatePrimaryCamera();
	}

	void CameraSystem::SetEditorCameraActive(bool active) {
		m_EditorCameraActive = active;
		
		if (m_EditorCameraHandle.IsValid()) {
			auto it = m_Cameras.find(m_EditorCameraHandle.ID);
			if (it != m_Cameras.end()) {
				it->second.IsActive = active;
			}
		}
		
		UpdatePrimaryCamera();
	}

	// ============================================================================
	// VIEW DATA ACCESS
	// ============================================================================

	const ViewData& CameraSystem::GetPrimaryViewData() const {
		if (!m_PrimaryCamera.IsValid()) {
			return m_DefaultViewData;
		}
		
		auto it = m_Cameras.find(m_PrimaryCamera.ID);
		if (it == m_Cameras.end()) {
			return m_DefaultViewData;
		}
		
		return it->second.CachedViewData;
	}

	const ViewData& CameraSystem::GetViewData(CameraHandle handle) const {
		auto it = m_Cameras.find(handle.ID);
		if (it == m_Cameras.end()) {
			return m_DefaultViewData;
		}
		return it->second.CachedViewData;
	}

	const ViewFrustum& CameraSystem::GetFrustum(CameraHandle handle) const {
		auto it = m_Cameras.find(handle.ID);
		if (it == m_Cameras.end()) {
			return m_DefaultFrustum;
		}
		return it->second.CachedFrustum;
	}

	std::vector<CameraRenderInfo> CameraSystem::GetActiveCameraInfos() const {
		std::vector<CameraRenderInfo> infos;
		
		uint32_t viewIndex = 0;
		for (const auto& handle : m_CameraOrder) {
			auto it = m_Cameras.find(handle.ID);
			if (it == m_Cameras.end() || !it->second.IsActive) continue;
			
			CameraRenderInfo info;
			info.View = it->second.CachedViewData;
			info.Frustum = it->second.CachedFrustum;
			info.ViewIndex = viewIndex++;
			info.IsPrimaryView = (handle == m_PrimaryCamera);
			
			infos.push_back(info);
		}
		
		return infos;
	}

	// ============================================================================
	// CAMERA QUERIES
	// ============================================================================

	void CameraSystem::SetPrimaryCamera(CameraHandle handle) {
		if (!handle.IsValid()) {
			m_PrimaryCamera = CameraHandle{};
			return;
		}
		
		auto it = m_Cameras.find(handle.ID);
		if (it != m_Cameras.end()) {
			m_PrimaryCamera = handle;
		}
	}

	CameraHandle CameraSystem::FindCameraByName(const std::string& name) const {
		for (const auto& [id, entry] : m_Cameras) {
			if (entry.Name == name) {
				return entry.Handle;
			}
		}
		return CameraHandle{};
	}

	bool CameraSystem::HasCamera(CameraHandle handle) const {
		return m_Cameras.find(handle.ID) != m_Cameras.end();
	}

	// ============================================================================
	// FRUSTUM CULLING
	// ============================================================================

	bool CameraSystem::IsVisibleFromPrimary(const glm::vec3& center, float radius) const {
		const ViewFrustum& frustum = m_PrimaryCamera.IsValid() ? 
			GetFrustum(m_PrimaryCamera) : m_DefaultFrustum;
		return frustum.IntersectsSphere(center, radius);
	}

	bool CameraSystem::IsVisibleFromPrimary(const glm::vec3& min, const glm::vec3& max) const {
		const ViewFrustum& frustum = m_PrimaryCamera.IsValid() ? 
			GetFrustum(m_PrimaryCamera) : m_DefaultFrustum;
		return frustum.IntersectsAABB(min, max);
	}

	// ============================================================================
	// SCENE INTEGRATION
	// ============================================================================

	void CameraSystem::SyncFromScene(Scene* scene) {
		if (!scene) return;
		
		// Update existing and register new cameras
		auto view = scene->GetAllEntitiesWith<CameraComponent>();
		for (auto entityID : view) {
			Entity entity{ entityID, scene };
			
			// Check if already registered
			UUID uuid = entity.GetUUID();
			bool found = false;
			for (auto& [id, entry] : m_Cameras) {
				if (entry.EntityID == uuid) {
					UpdateSceneCamera(entity, scene);
					found = true;
					break;
				}
			}
			
			if (!found) {
				RegisterSceneCamera(entity, scene);
			}
		}
		
		UpdatePrimaryCamera();
	}

	void CameraSystem::ClearSceneCameras() {
		std::vector<CameraHandle> toRemove;
		
		for (auto& [id, entry] : m_Cameras) {
			if (!entry.IsEditorCamera) {
				toRemove.push_back(entry.Handle);
			}
		}
		
		for (auto handle : toRemove) {
			UnregisterCamera(handle);
		}
	}

	// ============================================================================
	// INTERNAL HELPERS
	// ============================================================================

	CameraEntry* CameraSystem::FindEntry(CameraHandle handle) {
		auto it = m_Cameras.find(handle.ID);
		return it != m_Cameras.end() ? &it->second : nullptr;
	}

	const CameraEntry* CameraSystem::FindEntry(CameraHandle handle) const {
		auto it = m_Cameras.find(handle.ID);
		return it != m_Cameras.end() ? &it->second : nullptr;
	}

	void CameraSystem::UpdateFrustum(CameraEntry& entry) {
		entry.CachedFrustum = ViewFrustum::FromViewProjection(
			entry.CachedViewData.ViewProjectionMatrix
		);
	}

	void CameraSystem::UpdatePrimaryCamera() {
		// Priority:
		// 1. Active editor camera (if in editor mode)
		// 2. Scene camera marked as primary
		// 3. First available scene camera
		
		if (m_EditorCameraActive && m_EditorCameraHandle.IsValid()) {
			auto it = m_Cameras.find(m_EditorCameraHandle.ID);
			if (it != m_Cameras.end() && it->second.IsActive) {
				m_PrimaryCamera = m_EditorCameraHandle;
				return;
			}
		}
		
		// Look for primary scene camera
		for (const auto& handle : m_CameraOrder) {
			auto it = m_Cameras.find(handle.ID);
			if (it == m_Cameras.end()) continue;
			if (!it->second.IsActive) continue;
			if (it->second.IsEditorCamera) continue;
			
			if (it->second.IsPrimary) {
				m_PrimaryCamera = handle;
				return;
			}
		}
		
		// Fallback to first active scene camera
		for (const auto& handle : m_CameraOrder) {
			auto it = m_Cameras.find(handle.ID);
			if (it == m_Cameras.end()) continue;
			if (!it->second.IsActive) continue;
			if (it->second.IsEditorCamera) continue;
			
			m_PrimaryCamera = handle;
			return;
		}
		
		// No camera available
		m_PrimaryCamera = CameraHandle{};
	}

} // namespace Lunex
