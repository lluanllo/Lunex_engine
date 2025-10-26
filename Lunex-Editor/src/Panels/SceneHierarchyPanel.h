#pragma once
#include "Core/Core.h"
#include "Log/Log.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Renderer/Texture.h"
#include <vector>
#include <string>

namespace Lunex {
	enum class HierarchyFilter {
		All,
		Cameras,
		Lights,
		Meshes,
		Empty
	};
	
	struct HierarchySettings {
		bool ShowSearchBar = true;
		bool ShowEntityIcons = true;
		bool ShowVisibilityToggles = true;
		bool ShowLockToggles = true;
		bool GroupByType = false;
		float IndentSpacing = 16.0f;
	};
	
	class SceneHierarchyPanel {
		public:
			SceneHierarchyPanel() = default;
			SceneHierarchyPanel(const Ref<Scene>& scene);
			~SceneHierarchyPanel() = default;
			
			void SetContext(const Ref<Scene>& scene);
			void OnImGuiRender();
			
			Entity GetSelectedEntity() const { return m_SelectionContext; }
			void SetSelectedEntity(Entity entity);
			
		private:
			// Rendering methods for hierarchy panel
			void RenderHierarchyToolbar();
			void RenderSearchBar();
			void RenderEntityTree();
			void RenderEntityNode(Entity entity);
			void RenderEntityContextMenu(Entity entity);
			void RenderEmptySpaceContextMenu();
			
			// Component drawing (mantenido original)
			template<typename T>
			void DisplayAddComponentEntry(const std::string& entryName);
			void DrawComponents(Entity entity);
			
			// Utility methods
			bool PassesSearchFilter(Entity entity);
			bool PassesTypeFilter(Entity entity);
			std::vector<Entity> GetFilteredEntities();
			void DuplicateEntity(Entity entity);
			void RenameEntity(Entity entity);
			
			// Icon selection
			Ref<Texture2D> GetEntityIcon(Entity entity);
			
		private:
			Ref<Scene> m_Context;
			Entity m_SelectionContext;
			Entity m_RightClickedEntity;
			
			// Multi-selection support
			std::vector<Entity> m_SelectedEntities;
			
			// Icons
			Ref<Texture2D> m_CameraIcon;
			Ref<Texture2D> m_EntityIcon;
			Ref<Texture2D> m_LightIcon;
			Ref<Texture2D> m_MeshIcon;
			Ref<Texture2D> m_VisibleIcon;
			Ref<Texture2D> m_HiddenIcon;
			Ref<Texture2D> m_LockedIcon;
			Ref<Texture2D> m_UnlockedIcon;
			
			// Hierarchy settings
			HierarchySettings m_Settings;
			HierarchyFilter m_CurrentFilter = HierarchyFilter::All;
			
			// Search
			char m_SearchBuffer[256] = "";
			std::string m_SearchQuery;
			
			// Rename state
			bool m_IsRenaming = false;
			Entity m_RenamingEntity;
			char m_RenameBuffer[256] = "";
			
			// Drag and drop state
			bool m_IsDragging = false;
			Entity m_DraggedEntity;
			
			// Visibility and lock states (could be stored in components in a real implementation)
			std::unordered_map<uint32_t, bool> m_EntityVisibility;
			std::unordered_map<uint32_t, bool> m_EntityLocked;
	};
}