#include "SceneHierarchyPanel.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <algorithm>

#include "Scene/Components.h"

namespace Lunex {
	extern const std::filesystem::path g_AssetPath;

	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context) {
		SetContext(context);

		// Cargar iconos para la jerarquía
		m_CameraIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/CameraIcon.png");
		m_EntityIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/EntityIcon.png");
		m_LightIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/LightIcon.png");
		m_MeshIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/MeshIcon.png");
		m_SpriteIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/SpriteIcon.png");

		if (!m_CameraIcon)
			LNX_LOG_WARN("Failed to load Camera Icon, using fallback");
		if (!m_EntityIcon)
			LNX_LOG_WARN("Failed to load Entity Icon, using fallback");
	}

	void SceneHierarchyPanel::SetContext(const Ref<Scene>& context) {
		m_Context = context;
		m_SelectionContext = {};
		m_SelectedEntities.clear();
		m_EntityIndexCounter = 0;
		m_IsRenaming = false;
	}

	void SceneHierarchyPanel::OnImGuiRender() {
		// ✅ REMOVED: HandleKeyboardShortcuts() - ahora es global vía InputManager
		
		// ===== ESTILO PROFESIONAL (BLENDER/UNREAL) =====
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.11f, 0.12f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.11f, 0.12f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.08f, 0.08f, 0.09f, 1.0f));
		
		// Header colors (selección)
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.26f, 0.59f, 0.98f, 0.35f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.50f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.26f, 0.59f, 0.98f, 0.65f));
		
		// Text colors
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.88f, 0.88f, 0.90f, 1.0f));
		
		ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 16.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
		
		ImGui::Begin("Scene Hierarchy");

		// ===== TOP BAR CON BOTONES =====
		RenderTopBar();
		
		ImGui::Separator();
		ImGui::Spacing();

		if (m_Context) {
			m_EntityIndexCounter = 0;
			m_TotalEntities = 0;
			m_VisibleEntities = 0;
			
			// Get sorted entities
			auto entities = GetSortedEntities();
			m_TotalEntities = (int)entities.size();
			
			// BeginChild para el área scrollable
			ImGui::BeginChild("##EntityList", ImVec2(0, 0), false);
			
			for (auto entity : entities) {
				DrawEntityNode(entity);
				m_EntityIndexCounter++;
			}

			// Click en área vacía para deseleccionar
			if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()) {
				ClearSelection();
			}

			// Context menu en área vacía
			if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
				ImGui::SeparatorText("Create");
				
				if (ImGui::MenuItem("📦 Empty Entity"))
					m_Context->CreateEntity("Empty Entity");
				
				if (ImGui::MenuItem("📷 Camera"))
					CreateEntityWithComponent<CameraComponent>("Camera");
				
				if (ImGui::MenuItem("💡 Light"))
					CreateEntityWithComponent<LightComponent>("Light");
				
				if (ImGui::MenuItem("🎨 Sprite"))
					CreateEntityWithComponent<SpriteRendererComponent>("Sprite");
				
				if (ImGui::MenuItem("🗿 3D Object"))
					CreateEntityWithComponent<MeshComponent>("Cube");
				
				ImGui::Separator();
				
				if (ImGui::MenuItem("📋 Paste", "Ctrl+V", false, false)) {
					// TODO: Implement paste
				}
				
				ImGui::EndPopup();
			}
			
			ImGui::EndChild();
		}
		
		ImGui::End();
		
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(7);
	}

	void SceneHierarchyPanel::SetSelectedEntity(Entity entity) {
		SelectEntity(entity);
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity) {
		auto& tag = entity.GetComponent<TagComponent>().Tag;

		// ===== FILTRO DE BÚSQUEDA =====
		if (m_SearchFilter[0] != '\0') {
			std::string tagLower = tag;
			std::string searchLower = m_SearchFilter;
			std::transform(tagLower.begin(), tagLower.end(), tagLower.begin(), ::tolower);
			std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
			
			if (tagLower.find(searchLower) == std::string::npos) {
				return;
			}
		}

		m_VisibleEntities++;

		// ===== COLORES ALTERNOS (BLENDER STYLE) =====
		ImU32 bgColor;
		if (m_EntityIndexCounter % 2 == 0) {
			bgColor = IM_COL32(28, 28, 30, 255);
		} else {
			bgColor = IM_COL32(32, 32, 34, 255);
		}

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		flags |= ImGuiTreeNodeFlags_FramePadding;
		
		// Multi-selection support
		if (IsEntitySelected(entity)) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		// Determinar icono según componentes (priority order)
		Ref<Texture2D> icon = m_EntityIcon;
		
		if (entity.HasComponent<CameraComponent>()) {
			icon = m_CameraIcon ? m_CameraIcon : m_EntityIcon;
		}
		else if (entity.HasComponent<LightComponent>()) {
			icon = m_LightIcon ? m_LightIcon : m_EntityIcon;
		}
		else if (entity.HasComponent<MeshComponent>()) {
			icon = m_MeshIcon ? m_MeshIcon : m_EntityIcon;
		}
		else if (entity.HasComponent<SpriteRendererComponent>()) {
			icon = m_SpriteIcon ? m_SpriteIcon : m_EntityIcon;
		}

		// ===== DIBUJAR FONDO ALTERNO =====
		ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
		ImVec2 itemSize = ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight());
		
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(
			cursorScreenPos,
			ImVec2(cursorScreenPos.x + itemSize.x, cursorScreenPos.y + itemSize.y),
			bgColor
		);

		// ===== ICONO + TREE NODE / RENAME FIELD =====
		ImVec2 cursorPos = ImGui::GetCursorPos();
		ImGui::SetCursorPosY(cursorPos.y + 2.0f);
		
		if (icon) {
			float iconSize = 18.0f;
			ImGui::Image(
				(void*)(intptr_t)icon->GetRendererID(),
				ImVec2(iconSize, iconSize),
				ImVec2(0, 1),
				ImVec2(1, 0)
			);
			ImGui::SameLine();
			ImGui::SetCursorPosY(cursorPos.y);
		}

		// Rename inline mode
		bool isRenaming = (m_IsRenaming && m_EntityBeingRenamed == entity);
		bool opened = false;
		
		if (isRenaming) {
			// Inline rename field
			ImGui::SetKeyboardFocusHere();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.18f, 0.40f, 0.65f, 0.3f));
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			
			if (ImGui::InputText("##RenameEntity", m_RenameBuffer, sizeof(m_RenameBuffer), 
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
				// Apply rename
				entity.GetComponent<TagComponent>().Tag = m_RenameBuffer;
				m_IsRenaming = false;
				LNX_LOG_INFO("Renamed entity to: {0}", m_RenameBuffer);
			}
			ImGui::PopStyleColor();
			
			// Cancel rename with Escape (handled in HandleKeyboardShortcuts)
			if (!ImGui::IsItemActive() && !ImGui::IsItemFocused()) {
				m_IsRenaming = false;
			}
		} else {
			// Normal tree node
			opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", tag.c_str());
		}

		// ===== HOVER EFFECT =====
		if (ImGui::IsItemHovered() && !isRenaming) {
			ImU32 hoverColor = IM_COL32(50, 50, 55, 180);
			drawList->AddRectFilled(
				cursorScreenPos,
				ImVec2(cursorScreenPos.x + itemSize.x, cursorScreenPos.y + itemSize.y),
				hoverColor
			);
		}

		// ===== MULTI-SELECTION INTERACTIONS =====
		if (ImGui::IsItemClicked() && !isRenaming) {
			ImGuiIO& io = ImGui::GetIO();
			
			if (io.KeyCtrl) {
				// Ctrl+Click: Toggle selection
				ToggleEntitySelection(entity);
			}
			else if (io.KeyShift && m_LastSelectedEntity) {
				// Shift+Click: Range selection
				// TODO: Implement range selection between m_LastSelectedEntity and entity
				SelectEntity(entity, false);
			}
			else {
				// Normal click: Select this entity only
				SelectEntity(entity, true);
			}
		}

		// ===== DRAG & DROP SOURCE (MULTI-ENTITY SUPPORT) =====
		if (ImGui::BeginDragDropSource()) {
			if (m_SelectedEntities.size() > 1 && IsEntitySelected(entity)) {
				// Multiple entities drag
				ImGui::Text("📦 %d entities", (int)m_SelectedEntities.size());
				// TODO: Set multi-entity payload
			} else {
				// Single entity drag
				ImGui::SetDragDropPayload("ENTITY_NODE", &entity, sizeof(Entity));
				ImGui::Text("📦 %s", tag.c_str());
			}
			ImGui::EndDragDropSource();
		}

		// ===== CONTEXT MENU (IMPROVED) =====
		bool entityDeleted = false;
		if (ImGui::BeginPopupContextItem()) {
			// Show multi-selection info
			if (m_SelectedEntities.size() > 1) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
				ImGui::Text("📦 %d entities selected", (int)m_SelectedEntities.size());
				ImGui::PopStyleColor();
			} else {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
				ImGui::Text("📦 %s", tag.c_str());
				ImGui::PopStyleColor();
			}
			ImGui::Separator();
			
			// Single entity operations
			if (m_SelectedEntities.size() == 1) {
				if (ImGui::MenuItem("✏️ Rename", "F2")) {
					RenameEntity(entity);
				}
			}
			
			// Multi-entity operations
			if (ImGui::MenuItem("🔄 Duplicate", "Ctrl+D")) {
				DuplicateSelectedEntities();
			}
			
			if (ImGui::MenuItem("📋 Copy", "Ctrl+C", false, false)) {
				// TODO: Implement copy to clipboard
			}
			
			ImGui::Separator();
			
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
			if (ImGui::MenuItem("🗑️ Delete", "Del")) {
				entityDeleted = true;
			}
			ImGui::PopStyleColor();
			
			ImGui::Separator();
			
			// Additional info
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
			ImGui::Text("UUID: %llu", (uint64_t)entity.GetComponent<IDComponent>().ID);
			ImGui::PopStyleColor();

			ImGui::EndPopup();
		}

		// Highlight multi-selection
		if (IsEntitySelected(entity) && m_SelectedEntities.size() > 1) {
			ImU32 multiSelectColor = IM_COL32(90, 150, 255, 30);
			drawList->AddRectFilled(
				cursorScreenPos,
				ImVec2(cursorScreenPos.x + itemSize.x, cursorScreenPos.y + itemSize.y),
				multiSelectColor
			);
		}

		// Eliminar entidad/entidades
		if (entityDeleted) {
			DeleteSelectedEntities();
		}
	}
	
	void SceneHierarchyPanel::RenderTopBar() {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.17f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.4f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.59f, 0.98f, 0.6f));
		
		// Botón Create Entity
		if (ImGui::Button("➕ Create Entity")) {
			ImGui::OpenPopup("CreateEntityPopup");
		}
		
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Create new entity (Ctrl+N)");
		}
		
		// Popup para crear entidades
		if (ImGui::BeginPopup("CreateEntityPopup")) {
			ImGui::SeparatorText("Create Entity");
			
			if (ImGui::MenuItem("📦 Empty Entity", "Ctrl+N"))
				m_Context->CreateEntity("Empty Entity");
			
			ImGui::Separator();
			
			if (ImGui::MenuItem("📷 Camera"))
				CreateEntityWithComponent<CameraComponent>("Camera");
			
			if (ImGui::MenuItem("💡 Light"))
				CreateEntityWithComponent<LightComponent>("Light");
			
			if (ImGui::MenuItem("🎨 Sprite"))
				CreateEntityWithComponent<SpriteRendererComponent>("Sprite");
			
			if (ImGui::MenuItem("🗿 3D Object"))
				CreateEntityWithComponent<MeshComponent>("Cube");
			
			ImGui::EndPopup();
		}
		
		ImGui::SameLine();
		
		// Search bar funcional
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.15f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.19f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.26f, 0.59f, 0.98f, 0.3f));
		ImGui::SetNextItemWidth(180);
		ImGui::InputTextWithHint("##Search", "🔍 Search...", m_SearchFilter, 256);
		ImGui::PopStyleColor(3);
		
		ImGui::SameLine();
		
		// Sort button
		const char* sortModes[] = { "None", "Name", "Type" };
		ImGui::SetNextItemWidth(80);
		int currentSort = (int)m_SortMode;
		if (ImGui::Combo("##Sort", &currentSort, sortModes, 3)) {
			m_SortMode = (SortMode)currentSort;
		}
		
		if (ImGui::IsItemHovered()) {
			ImGui::SetTooltip("Sort entities");
		}
		
		// Entity count display
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
		if (m_SelectedEntities.size() > 0) {
			ImGui::Text("%d/%d selected", (int)m_SelectedEntities.size(), m_TotalEntities);
		} else {
			ImGui::Text("%d entities", m_TotalEntities);
		}
		ImGui::PopStyleColor();
		
		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
	}
	
	// ============================================================================
	// KEYBOARD SHORTCUTS
	// ============================================================================
	
	// ============================================================================
	// SELECTION OPERATIONS
	// ============================================================================
	void SceneHierarchyPanel::SelectEntity(Entity entity, bool clearPrevious) {
		if (clearPrevious) {
			m_SelectedEntities.clear();
		}
		
		m_SelectionContext = entity;
		m_SelectedEntities.insert(entity);
		m_LastSelectedEntity = entity;
	}
	
	void SceneHierarchyPanel::ToggleEntitySelection(Entity entity) {
		if (m_SelectedEntities.find(entity) != m_SelectedEntities.end()) {
			m_SelectedEntities.erase(entity);
			if (m_SelectionContext == entity) {
				m_SelectionContext = m_SelectedEntities.empty() ? Entity{} : *m_SelectedEntities.begin();
			}
		} else {
			m_SelectedEntities.insert(entity);
			m_SelectionContext = entity;
		}
		m_LastSelectedEntity = entity;
	}
	
	void SceneHierarchyPanel::ClearSelection() {
		m_SelectedEntities.clear();
		m_SelectionContext = {};
		m_LastSelectedEntity = {};
	}
	
	void SceneHierarchyPanel::SelectAll() {
		m_SelectedEntities.clear();
		
		auto view = m_Context->m_Registry.view<TagComponent>();
		for (auto entityID : view) {
			Entity entity{ entityID, m_Context.get() };
			
			// Apply search filter
			if (m_SearchFilter[0] != '\0') {
				auto& tag = entity.GetComponent<TagComponent>().Tag;
				std::string tagLower = tag;
				std::string searchLower = m_SearchFilter;
				std::transform(tagLower.begin(), tagLower.end(), tagLower.begin(), ::tolower);
				std::transform(searchLower.begin(), searchLower.end(), searchLower.begin(), ::tolower);
				
				if (tagLower.find(searchLower) == std::string::npos)
					continue;
			}
			
			m_SelectedEntities.insert(entity);
		}
		
		if (!m_SelectedEntities.empty()) {
			m_SelectionContext = *m_SelectedEntities.begin();
		}
		
		LNX_LOG_INFO("Selected all {0} entities", m_SelectedEntities.size());
	}
	
	bool SceneHierarchyPanel::IsEntitySelected(Entity entity) const {
		return m_SelectedEntities.find(entity) != m_SelectedEntities.end();
	}
	
	void SceneHierarchyPanel::DeleteSelectedEntities() {
		for (auto entity : m_SelectedEntities) {
			m_Context->DestroyEntity(entity);
		}
		
		LNX_LOG_INFO("Deleted {0} entities", m_SelectedEntities.size());
		ClearSelection();
	}
	
	// ============================================================================
	// ENTITY OPERATIONS
	// ============================================================================
	void SceneHierarchyPanel::DuplicateEntity(Entity entity) {
		if (!entity)
			return;
			
		auto& tag = entity.GetComponent<TagComponent>().Tag;
		std::string newName = tag + " - Copy";
		
		Entity newEntity = m_Context->CreateEntity(newName);
		
		// Copy Transform
		if (entity.HasComponent<TransformComponent>()) {
			newEntity.GetComponent<TransformComponent>() = entity.GetComponent<TransformComponent>();
		}
		
		// Copy other components (simplified - should use proper component copying)
		if (entity.HasComponent<CameraComponent>()) {
			newEntity.AddComponent<CameraComponent>(entity.GetComponent<CameraComponent>());
		}
		if (entity.HasComponent<SpriteRendererComponent>()) {
			newEntity.AddComponent<SpriteRendererComponent>(entity.GetComponent<SpriteRendererComponent>());
		}
		if (entity.HasComponent<MeshComponent>()) {
			newEntity.AddComponent<MeshComponent>(entity.GetComponent<MeshComponent>());
		}
		if (entity.HasComponent<LightComponent>()) {
			newEntity.AddComponent<LightComponent>(entity.GetComponent<LightComponent>());
		}
		
		SelectEntity(newEntity);
		LNX_LOG_INFO("Duplicated entity: {0}", tag);
	}
	
	void SceneHierarchyPanel::DuplicateSelectedEntities() {
		std::vector<Entity> entitiesToDuplicate(m_SelectedEntities.begin(), m_SelectedEntities.end());
		ClearSelection();
		
		for (auto entity : entitiesToDuplicate) {
			DuplicateEntity(entity);
		}
	}
	
	void SceneHierarchyPanel::RenameEntity(Entity entity) {
		if (!entity)
			return;
			
		m_IsRenaming = true;
		m_EntityBeingRenamed = entity;
		
		auto& tag = entity.GetComponent<TagComponent>().Tag;
		strncpy_s(m_RenameBuffer, sizeof(m_RenameBuffer), tag.c_str(), _TRUNCATE);
	}
	
	void SceneHierarchyPanel::RenameSelectedEntity() {
		if (m_SelectionContext) {
			RenameEntity(m_SelectionContext);
		}
	}
	
	// ============================================================================
	// SORTING
	// ============================================================================
	std::vector<Entity> SceneHierarchyPanel::GetSortedEntities() {
		std::vector<Entity> entities;
		
		auto view = m_Context->m_Registry.view<TagComponent>();
		for (auto entityID : view) {
			entities.push_back(Entity{ entityID, m_Context.get() });
		}
		
		if (m_SortMode == SortMode::Name) {
			std::sort(entities.begin(), entities.end(), [](Entity a, Entity b) {
				return a.GetComponent<TagComponent>().Tag < b.GetComponent<TagComponent>().Tag;
			});
		}
		else if (m_SortMode == SortMode::Type) {
			std::sort(entities.begin(), entities.end(), [](Entity a, Entity b) {
				// Sort by component type (Camera > Light > Mesh > Sprite > Others)
				int priorityA = 100;
				int priorityB = 100;
				
				if (a.HasComponent<CameraComponent>()) priorityA = 0;
				else if (a.HasComponent<LightComponent>()) priorityA = 1;
				else if (a.HasComponent<MeshComponent>()) priorityA = 2;
				else if (a.HasComponent<SpriteRendererComponent>()) priorityA = 3;
				
				if (b.HasComponent<CameraComponent>()) priorityB = 0;
				else if (b.HasComponent<LightComponent>()) priorityB = 1;
				else if (b.HasComponent<MeshComponent>()) priorityB = 2;
				else if (b.HasComponent<SpriteRendererComponent>()) priorityB = 3;
				
				return priorityA < priorityB;
			});
		}
		
		return entities;
	}
}