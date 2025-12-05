#pragma once

#include "Core/Core.h"
#include "Log/Log.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Renderer/Texture.h"

#include <set>
#include <vector>

namespace Lunex {
	class SceneHierarchyPanel {
	public:
		SceneHierarchyPanel() = default;
		SceneHierarchyPanel(const Ref<Scene>& scene);
		~SceneHierarchyPanel() = default;

		void SetContext(const Ref<Scene>& scene);

		void OnImGuiRender();

		Entity GetSelectedEntity() const { return m_SelectionContext; }
		void SetSelectedEntity(Entity entity);
		
		// Multi-selection
		const std::set<Entity>& GetSelectedEntities() const { return m_SelectedEntities; }
		bool IsEntitySelected(Entity entity) const;
		
		// Active element (last selected entity)
		Entity GetActiveEntity() const { return m_LastSelectedEntity; }
		
		// ========================================
		// PIVOT POINT CALCULATIONS (Blender-style)
		// ========================================
		glm::vec3 CalculateMedianPoint() const;
		glm::vec3 CalculateActiveElementPosition() const;
		glm::vec3 CalculateBoundingBoxCenter() const;
		
		// ========================================
		// PUBLIC API FOR GLOBAL SHORTCUTS
		// ========================================
		void SelectAll();
		void ClearSelection();
		void DeleteSelectedEntities();
		void DuplicateSelectedEntities();
		void RenameSelectedEntity();
		
		// ========================================
		// PUBLIC API FOR MULTI-SELECTION (Ray Picking)
		// ========================================
		void AddEntityToSelection(Entity entity);
		void ToggleEntitySelection(Entity entity);

	private:
		void DrawEntityNode(Entity entity, int depth = 0);
		void RenderTopBar();
		
		// Selection operations
		void SelectEntity(Entity entity, bool clearPrevious = true);
		
		// Entity operations
		void DuplicateEntity(Entity entity);
		void RenameEntity(Entity entity);
		
		// Hierarchy operations
		void SetEntityParent(Entity child, Entity parent);
		void UnparentEntity(Entity entity);
		
		// Sorting
		enum class SortMode { None, Name, Type };
		std::vector<Entity> GetSortedRootEntities();  // Only root entities (no parent)
		
		template<typename T>
		void CreateEntityWithComponent(const std::string& name) {
			Entity entity = m_Context->CreateEntity(name);
			entity.AddComponent<T>();
			SelectEntity(entity);
			LNX_LOG_INFO("Created entity: {0}", name);
		}

	private:
		Ref<Scene> m_Context;
		Entity m_SelectionContext;
		
		// Multi-selection
		std::set<Entity> m_SelectedEntities;
		Entity m_LastSelectedEntity;
		
		// Iconos para la jerarquía
		Ref<Texture2D> m_CameraIcon;
		Ref<Texture2D> m_EntityIcon;
		Ref<Texture2D> m_LightIcon;
		Ref<Texture2D> m_MeshIcon;
		Ref<Texture2D> m_SpriteIcon;
		
		// Para colores alternos
		int m_EntityIndexCounter = 0;
		
		// Search and filter
		char m_SearchFilter[256] = "";
		SortMode m_SortMode = SortMode::None;
		
		// Rename functionality
		bool m_IsRenaming = false;
		Entity m_EntityBeingRenamed;
		char m_RenameBuffer[256] = "";
		
		// Statistics
		int m_TotalEntities = 0;
		int m_VisibleEntities = 0;
		
		// Drag & drop for parenting
		Entity m_DraggedEntity;
	};
}