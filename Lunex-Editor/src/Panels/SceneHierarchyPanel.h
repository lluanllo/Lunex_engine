#pragma once

#include "Core/Core.h"
#include "Log/Log.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Renderer/Texture.h"
#include "Asset/Prefab.h"
#include "../UI/UICore.h"
#include "../UI/UILayout.h"

#include <set>
#include <vector>
#include <functional>

namespace Lunex {

	// ============================================================================
	// HIERARCHY PANEL STYLE
	// ============================================================================
	
	struct HierarchyPanelStyle {
		// Colors
		UI::Color WindowBg = UI::Color(0.11f, 0.11f, 0.12f, 1.0f);
		UI::Color ChildBg = UI::Color(0.11f, 0.11f, 0.12f, 1.0f);
		UI::Color ItemEven = UI::Color(0.11f, 0.11f, 0.12f, 1.0f);
		UI::Color ItemOdd = UI::Color(0.13f, 0.13f, 0.14f, 1.0f);
		UI::Color ItemHover = UI::Color(0.20f, 0.20f, 0.22f, 1.0f);
		UI::Color ItemSelected = UI::Color(0.18f, 0.40f, 0.68f, 0.35f);
		UI::Color ItemSelectedBorder = UI::Color(0.26f, 0.59f, 0.98f, 0.80f);
		// Active element (last selected) - Light orange (Blender-style)
		UI::Color ItemActive = UI::Color(0.90f, 0.60f, 0.20f, 0.40f);
		UI::Color ItemActiveBorder = UI::Color(1.0f, 0.70f, 0.25f, 0.90f);
		// Non-active selected elements - Dark orange (Blender-style)
		UI::Color ItemSelectedMulti = UI::Color(0.70f, 0.40f, 0.10f, 0.30f);
		UI::Color ItemSelectedMultiBorder = UI::Color(0.80f, 0.50f, 0.15f, 0.70f);
		UI::Color TextPrimary = UI::Color(0.92f, 0.92f, 0.94f, 1.0f);
		UI::Color TextMuted = UI::Color(0.60f, 0.60f, 0.62f, 1.0f);
		UI::Color Border = UI::Color(0.08f, 0.08f, 0.09f, 1.0f);
		UI::Color SeparatorLine = UI::Color(0.25f, 0.25f, 0.28f, 1.0f);
		
		// Entity type indicator colors
		UI::Color TypeCamera = UI::Color(0.40f, 0.75f, 0.95f, 1.0f);    // Light Blue
		UI::Color TypeLight = UI::Color(0.95f, 0.85f, 0.30f, 1.0f);     // Yellow
		UI::Color TypeMesh = UI::Color(0.30f, 0.85f, 0.40f, 1.0f);      // Green
		UI::Color TypeSprite = UI::Color(0.80f, 0.50f, 0.80f, 1.0f);    // Purple
		UI::Color TypeDefault = UI::Color(0.65f, 0.65f, 0.68f, 1.0f);   // Gray
		
		// Sizing
		float IndentSpacing = 16.0f;
		float ItemHeight = 24.0f;
		float IconSize = 16.0f;
		float TypeIndicatorWidth = 3.0f;
		float SearchBarHeight = 28.0f;
		float ToolbarHeight = 32.0f;
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
		
		// Multi-selection
		const std::set<Entity>& GetSelectedEntities() const { return m_SelectedEntities; }
		bool IsEntitySelected(Entity entity) const;
		
		// Active element (last selected entity)
		Entity GetActiveEntity() const { return m_LastSelectedEntity; }
		
		// ========================================
		// PREFAB SYSTEM
		// ========================================
		void CreatePrefabFromEntity(Entity entity);
		void InstantiatePrefab(const std::filesystem::path& prefabPath);
		void SetPrefabsDirectory(const std::filesystem::path& directory) { m_PrefabsDirectory = directory; }
		
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
		void SetActiveEntityInSelection(Entity entity);
		
		// Style access
		HierarchyPanelStyle& GetStyle() { return m_Style; }
		const HierarchyPanelStyle& GetStyle() const { return m_Style; }

	private:
		void DrawEntityNode(Entity entity, int depth = 0);
		void RenderToolbar();
		void RenderSearchBar();
		void RenderEntityList();
		void RenderContextMenu();
		void RenderEntityContextMenu(Entity entity);
		
		// Selection operations
		void SelectEntity(Entity entity, bool clearPrevious = true);
		
		// Entity operations
		void DuplicateEntity(Entity entity);
		void RenameEntity(Entity entity);
		
		// Hierarchy operations
		void SetEntityParent(Entity child, Entity parent);
		void UnparentEntity(Entity entity);
		
		// Helper functions
		UI::Color GetEntityTypeColor(Entity entity) const;
		const char* GetEntityTypeIcon(Entity entity) const;
		
		// Sorting
		enum class SortMode { None, Name, Type };
		std::vector<Entity> GetSortedRootEntities();
		
		template<typename T>
		void CreateEntityWithComponent(const std::string& name) {
			Entity entity = m_Context->CreateEntity(name);
			entity.AddComponent<T>();
			// Auto-add MaterialComponent when MeshComponent is created via UI
			if constexpr (std::is_same_v<T, MeshComponent>) {
				if (!entity.HasComponent<MaterialComponent>()) {
					entity.AddComponent<MaterialComponent>();
				}
			}
			SelectEntity(entity);
			LNX_LOG_INFO("Created entity: {0}", name);
		}
		
		void CreateMeshEntity(const std::string& name, ModelType type) {
			Entity entity = m_Context->CreateEntity(name);
			auto& mesh = entity.AddComponent<MeshComponent>();
			mesh.CreatePrimitive(type);
			if (!entity.HasComponent<MaterialComponent>()) {
				entity.AddComponent<MaterialComponent>();
			}
			SelectEntity(entity);
			LNX_LOG_INFO("Created 3D entity: {0} (type: {1})", name, (int)type);
		}

	private:
		Ref<Scene> m_Context;
		Entity m_SelectionContext;
		
		// Multi-selection
		std::set<Entity> m_SelectedEntities;
		Entity m_LastSelectedEntity;
		
		// Prefabs
		std::filesystem::path m_PrefabsDirectory;
		
		// Icons
		Ref<Texture2D> m_CameraIcon;
		Ref<Texture2D> m_EntityIcon;
		Ref<Texture2D> m_LightIcon;
		Ref<Texture2D> m_MeshIcon;
		Ref<Texture2D> m_SpriteIcon;
		
		// Style
		HierarchyPanelStyle m_Style;
		
		// State
		int m_EntityIndexCounter = 0;
		char m_SearchFilter[256] = "";
		SortMode m_SortMode = SortMode::None;
		
		// Rename functionality
		bool m_IsRenaming = false;
		Entity m_EntityBeingRenamed;
		char m_RenameBuffer[256] = "";
		
		// Statistics
		int m_TotalEntities = 0;
		int m_VisibleEntities = 0;
		
		// Drag & drop
		Entity m_DraggedEntity;
		
		// UI state
		bool m_ShowCreateMenu = false;
	};
}