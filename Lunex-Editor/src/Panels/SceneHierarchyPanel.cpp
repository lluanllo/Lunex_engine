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

		m_CameraIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/CameraIcon.png");
		m_EntityIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/EntityIcon.png");
		m_LightIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/LightIcon.png");
		m_MeshIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/MeshIcon.png");
		m_SpriteIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/SpriteIcon.png");
	}

	void SceneHierarchyPanel::SetContext(const Ref<Scene>& context) {
		m_Context = context;
		m_SelectionContext = {};
		m_SelectedEntities.clear();
		m_EntityIndexCounter = 0;
		m_IsRenaming = false;
		m_DraggedEntity = {};
	}

	void SceneHierarchyPanel::OnImGuiRender() {
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.11f, 0.12f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.11f, 0.12f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.08f, 0.08f, 0.09f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.26f, 0.59f, 0.98f, 0.35f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.50f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.26f, 0.59f, 0.98f, 0.65f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.88f, 0.88f, 0.90f, 1.0f));
		
		ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 16.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 1));
		
		ImGui::Begin("Scene Hierarchy");

		RenderTopBar();
		ImGui::Separator();
		ImGui::Spacing();

		if (m_Context) {
			m_EntityIndexCounter = 0;
			m_TotalEntities = 0;
			m_VisibleEntities = 0;
			
			// Get only root entities (no parent)
			auto rootEntities = GetSortedRootEntities();
			m_TotalEntities = (int)m_Context->m_Registry.view<TagComponent>().size();
			
			ImGui::BeginChild("##EntityList", ImVec2(0, 0), false);
			
			// Draw root entities (children are drawn recursively)
			for (auto entity : rootEntities) {
				DrawEntityNode(entity, 0);
			}

			// Drop target for root level (unparent)
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_NODE")) {
					Entity droppedEntity = *(Entity*)payload->Data;
					UnparentEntity(droppedEntity);
				}
				ImGui::EndDragDropTarget();
			}

			// Click on empty area to deselect
			if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()) {
				ClearSelection();
			}

			// Context menu for empty area
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

	void SceneHierarchyPanel::DrawEntityNode(Entity entity, int depth) {
		auto& tag = entity.GetComponent<TagComponent>().Tag;

		// Search filter
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

		// Check if has children
		bool hasChildren = false;
		if (entity.HasComponent<RelationshipComponent>()) {
			hasChildren = entity.GetComponent<RelationshipComponent>().HasChildren();
		}

		// Alternate background colors
		ImU32 bgColor = (m_EntityIndexCounter % 2 == 0) ? IM_COL32(28, 28, 30, 255) : IM_COL32(32, 32, 34, 255);
		m_EntityIndexCounter++;

		// Tree node flags
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
		
		if (!hasChildren) {
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}
		
		if (IsEntitySelected(entity)) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		// Determine icon
		Ref<Texture2D> icon = m_EntityIcon;
		if (entity.HasComponent<CameraComponent>()) icon = m_CameraIcon ? m_CameraIcon : m_EntityIcon;
		else if (entity.HasComponent<LightComponent>()) icon = m_LightIcon ? m_LightIcon : m_EntityIcon;
		else if (entity.HasComponent<MeshComponent>()) icon = m_MeshIcon ? m_MeshIcon : m_EntityIcon;
		else if (entity.HasComponent<SpriteRendererComponent>()) icon = m_SpriteIcon ? m_SpriteIcon : m_EntityIcon;

		// Draw background
		ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
		ImVec2 itemSize = ImVec2(ImGui::GetContentRegionAvail().x, ImGui::GetFrameHeight());
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		drawList->AddRectFilled(cursorScreenPos, ImVec2(cursorScreenPos.x + itemSize.x, cursorScreenPos.y + itemSize.y), bgColor);

		// Indent based on depth
		if (depth > 0) {
			ImGui::Indent(depth * 16.0f);
		}

		// Icon
		ImVec2 cursorPos = ImGui::GetCursorPos();
		ImGui::SetCursorPosY(cursorPos.y + 2.0f);
		
		if (icon) {
			float iconSize = 18.0f;
			ImGui::Image((void*)(intptr_t)icon->GetRendererID(), ImVec2(iconSize, iconSize), ImVec2(0, 1), ImVec2(1, 0));
			ImGui::SameLine();
			ImGui::SetCursorPosY(cursorPos.y);
		}

		// Rename mode or tree node
		bool isRenaming = (m_IsRenaming && m_EntityBeingRenamed == entity);
		bool opened = false;
		
		if (isRenaming) {
			ImGui::SetKeyboardFocusHere();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.18f, 0.40f, 0.65f, 0.3f));
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
			
			if (ImGui::InputText("##RenameEntity", m_RenameBuffer, sizeof(m_RenameBuffer), 
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
				entity.GetComponent<TagComponent>().Tag = m_RenameBuffer;
				m_IsRenaming = false;
			}
			ImGui::PopStyleColor();
			
			if (!ImGui::IsItemActive() && !ImGui::IsItemFocused()) {
				m_IsRenaming = false;
			}
		} else {
			opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, "%s", tag.c_str());
		}

		// Hover effect
		if (ImGui::IsItemHovered() && !isRenaming) {
			drawList->AddRectFilled(cursorScreenPos, ImVec2(cursorScreenPos.x + itemSize.x, cursorScreenPos.y + itemSize.y), IM_COL32(50, 50, 55, 180));
		}

		// Click handling
		if (ImGui::IsItemClicked() && !isRenaming) {
			ImGuiIO& io = ImGui::GetIO();
			if (io.KeyCtrl) {
				ToggleEntitySelection(entity);
			} else {
				SelectEntity(entity, true);
			}
		}

		// ========================================
		// DRAG & DROP SOURCE
		// ========================================
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			ImGui::SetDragDropPayload("ENTITY_NODE", &entity, sizeof(Entity));
			ImGui::Text("📦 %s", tag.c_str());
			m_DraggedEntity = entity;
			ImGui::EndDragDropSource();
		}

		// ========================================
		// DRAG & DROP TARGET (for parenting)
		// ========================================
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_NODE")) {
				Entity droppedEntity = *(Entity*)payload->Data;
				
				// Don't parent to self
				if (droppedEntity != entity) {
					// Check if dropped entity is already a child of this entity
					Entity currentParent = m_Context->GetParent(droppedEntity);
					if (currentParent == entity) {
						// Already a child - unparent it
						UnparentEntity(droppedEntity);
					}
					else if (!m_Context->IsAncestorOf(droppedEntity, entity)) {
						// Not a child and not an ancestor - parent it
						SetEntityParent(droppedEntity, entity);
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		// Context menu
		bool entityDeleted = false;
		if (ImGui::BeginPopupContextItem()) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
			ImGui::Text("📦 %s", tag.c_str());
			ImGui::PopStyleColor();
			ImGui::Separator();
			
			if (ImGui::MenuItem("✏️ Rename", "F2")) {
				RenameEntity(entity);
			}
			
			if (ImGui::MenuItem("🔄 Duplicate", "Ctrl+D")) {
				DuplicateEntity(entity);
			}
			
			ImGui::Separator();
			
			// Hierarchy options
			Entity parent = m_Context->GetParent(entity);
			if (parent) {
				if (ImGui::MenuItem("⬆️ Unparent")) {
					UnparentEntity(entity);
				}
			}
			
			if (ImGui::MenuItem("📁 Create Child")) {
				Entity child = m_Context->CreateEntity("Child");
				SetEntityParent(child, entity);
				SelectEntity(child);
			}
			
			ImGui::Separator();
			
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
			if (ImGui::MenuItem("🗑️ Delete", "Del")) {
				entityDeleted = true;
			}
			ImGui::PopStyleColor();
			
			ImGui::Separator();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
			ImGui::Text("UUID: %llu", (uint64_t)entity.GetComponent<IDComponent>().ID);
			if (parent) {
				ImGui::Text("Parent: %s", parent.GetComponent<TagComponent>().Tag.c_str());
			}
			ImGui::PopStyleColor();

			ImGui::EndPopup();
		}

		// Unindent
		if (depth > 0) {
			ImGui::Unindent(depth * 16.0f);
		}

		// Draw children recursively if opened
		if (opened && hasChildren) {
			auto children = m_Context->GetChildren(entity);
			for (auto child : children) {
				DrawEntityNode(child, depth + 1);
			}
			ImGui::TreePop();
		}

		// Delete entity
		if (entityDeleted) {
			// First unparent all children
			auto children = m_Context->GetChildren(entity);
			for (auto child : children) {
				UnparentEntity(child);
			}
			
			m_Context->DestroyEntity(entity);
			if (m_SelectionContext == entity) {
				ClearSelection();
			}
		}
	}
	
	void SceneHierarchyPanel::RenderTopBar() {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 4));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.17f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.4f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.59f, 0.98f, 0.6f));
		
		if (ImGui::Button("➕ Create")) {
			ImGui::OpenPopup("CreateEntityPopup");
		}
		
		if (ImGui::BeginPopup("CreateEntityPopup")) {
			if (ImGui::MenuItem("📦 Empty Entity"))
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
		
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.15f, 1.0f));
		ImGui::SetNextItemWidth(180);
		ImGui::InputTextWithHint("##Search", "🔍 Search...", m_SearchFilter, 256);
		ImGui::PopStyleColor();
		
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
		if (m_SelectedEntities.size() > 0) {
			ImGui::Text("%d/%d", (int)m_SelectedEntities.size(), m_TotalEntities);
		} else {
			ImGui::Text("%d entities", m_TotalEntities);
		}
		ImGui::PopStyleColor();
		
		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
	}
	
	// ============================================================================
	// HIERARCHY OPERATIONS
	// ============================================================================
	
	void SceneHierarchyPanel::SetEntityParent(Entity child, Entity parent) {
		m_Context->SetParent(child, parent);
	}
	
	void SceneHierarchyPanel::UnparentEntity(Entity entity) {
		m_Context->RemoveParent(entity);
	}
	
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
			m_SelectedEntities.insert(Entity{ entityID, m_Context.get() });
		}
		if (!m_SelectedEntities.empty()) {
			m_SelectionContext = *m_SelectedEntities.begin();
		}
	}
	
	bool SceneHierarchyPanel::IsEntitySelected(Entity entity) const {
		return m_SelectedEntities.find(entity) != m_SelectedEntities.end();
	}
	
	void SceneHierarchyPanel::DeleteSelectedEntities() {
		for (auto entity : m_SelectedEntities) {
			// Unparent children first
			auto children = m_Context->GetChildren(entity);
			for (auto child : children) {
				m_Context->RemoveParent(child);
			}
			m_Context->DestroyEntity(entity);
		}
		ClearSelection();
	}
	
	void SceneHierarchyPanel::AddEntityToSelection(Entity entity) {
		if (entity) {
			m_SelectedEntities.insert(entity);
			m_SelectionContext = entity;
			m_LastSelectedEntity = entity;
		}
	}
	
	// ============================================================================
	// ENTITY OPERATIONS
	// ============================================================================
	void SceneHierarchyPanel::DuplicateEntity(Entity entity) {
		if (!entity)
			return;
	
		auto& tag = entity.GetComponent<TagComponent>().Tag;
		
		// ========================================
		// GENERATE UNIQUE NAME (Windows style)
		// ========================================
		std::string baseName = tag;
		std::string newName;
		
		// Remove existing " (X)" suffix to get clean base name
		size_t lastParen = baseName.rfind(" (");
		if (lastParen != std::string::npos) {
			std::string afterParen = baseName.substr(lastParen + 2);
			// Check if it's a number followed by )
			if (!afterParen.empty() && afterParen.back() == ')') {
				std::string numStr = afterParen.substr(0, afterParen.size() - 1);
				bool isNumber = !numStr.empty() && std::all_of(numStr.begin(), numStr.end(), ::isdigit);
				if (isNumber) {
					baseName = baseName.substr(0, lastParen);
				}
			}
		}
		
		// Find unique name with counter (Windows style: Name (1), Name (2), ...)
		int counter = 1;
		bool nameExists = true;
		
		while (nameExists) {
			newName = baseName + " (" + std::to_string(counter) + ")";
			
			// Check if name already exists in scene
			nameExists = false;
			auto view = m_Context->m_Registry.view<TagComponent>();
			for (auto entityID : view) {
				Entity e{ entityID, m_Context.get() };
				if (e.GetComponent<TagComponent>().Tag == newName) {
					nameExists = true;
					break;
				}
			}
			
			counter++;
		}
		
		// ========================================
		// CREATE NEW ENTITY WITH UNIQUE NAME
		// ========================================
		Entity newEntity = m_Context->CreateEntity(newName);
		
		// ========================================
		// COPY TRANSFORM (Always present)
		// ========================================
		if (entity.HasComponent<TransformComponent>()) {
			newEntity.GetComponent<TransformComponent>() = entity.GetComponent<TransformComponent>();
		}
		
		// ========================================
		// COPY RENDERING COMPONENTS
		// ========================================
		if (entity.HasComponent<CameraComponent>() && !newEntity.HasComponent<CameraComponent>()) {
			newEntity.AddComponent<CameraComponent>(entity.GetComponent<CameraComponent>());
		}
		
		if (entity.HasComponent<SpriteRendererComponent>() && !newEntity.HasComponent<SpriteRendererComponent>()) {
			newEntity.AddComponent<SpriteRendererComponent>(entity.GetComponent<SpriteRendererComponent>());
		}
		
		if (entity.HasComponent<CircleRendererComponent>() && !newEntity.HasComponent<CircleRendererComponent>()) {
			newEntity.AddComponent<CircleRendererComponent>(entity.GetComponent<CircleRendererComponent>());
		}
		
		if (entity.HasComponent<MeshComponent>() && !newEntity.HasComponent<MeshComponent>()) {
			newEntity.AddComponent<MeshComponent>(entity.GetComponent<MeshComponent>());
		}
		
		if (entity.HasComponent<LightComponent>() && !newEntity.HasComponent<LightComponent>()) {
			newEntity.AddComponent<LightComponent>(entity.GetComponent<LightComponent>());
		}
		
		// ========================================
		// COPY MATERIAL & TEXTURE (Check if already exists!)
		// MeshComponent might auto-create MaterialComponent
		// ========================================
		if (entity.HasComponent<MaterialComponent>()) {
			if (newEntity.HasComponent<MaterialComponent>()) {
				// Already exists, just copy the data
				newEntity.GetComponent<MaterialComponent>() = entity.GetComponent<MaterialComponent>();
			} else {
				// Doesn't exist, add it
				newEntity.AddComponent<MaterialComponent>(entity.GetComponent<MaterialComponent>());
	}
		}
		
		if (entity.HasComponent<TextureComponent>()) {
			if (newEntity.HasComponent<TextureComponent>()) {
				// Already exists, just copy the data
				newEntity.GetComponent<TextureComponent>() = entity.GetComponent<TextureComponent>();
			} else {
				// Doesn't exist, add it
				newEntity.AddComponent<TextureComponent>(entity.GetComponent<TextureComponent>());
			}
		}
		
		// ========================================
		// COPY PHYSICS 2D COMPONENTS
		// ========================================
		if (entity.HasComponent<Rigidbody2DComponent>() && !newEntity.HasComponent<Rigidbody2DComponent>()) {
			newEntity.AddComponent<Rigidbody2DComponent>(entity.GetComponent<Rigidbody2DComponent>());
		}
		
		if (entity.HasComponent<BoxCollider2DComponent>() && !newEntity.HasComponent<BoxCollider2DComponent>()) {
			newEntity.AddComponent<BoxCollider2DComponent>(entity.GetComponent<BoxCollider2DComponent>());
		}
		
		if (entity.HasComponent<CircleCollider2DComponent>() && !newEntity.HasComponent<CircleCollider2DComponent>()) {
			newEntity.AddComponent<CircleCollider2DComponent>(entity.GetComponent<CircleCollider2DComponent>());
		}
		
		// ========================================
		// COPY PHYSICS 3D COMPONENTS
		// ========================================
		if (entity.HasComponent<Rigidbody3DComponent>() && !newEntity.HasComponent<Rigidbody3DComponent>()) {
			newEntity.AddComponent<Rigidbody3DComponent>(entity.GetComponent<Rigidbody3DComponent>());
		}
		
		if (entity.HasComponent<BoxCollider3DComponent>() && !newEntity.HasComponent<BoxCollider3DComponent>()) {
			newEntity.AddComponent<BoxCollider3DComponent>(entity.GetComponent<BoxCollider3DComponent>());
		}
		
		if (entity.HasComponent<SphereCollider3DComponent>() && !newEntity.HasComponent<SphereCollider3DComponent>()) {
			newEntity.AddComponent<SphereCollider3DComponent>(entity.GetComponent<SphereCollider3DComponent>());
		}
		
		if (entity.HasComponent<CapsuleCollider3DComponent>() && !newEntity.HasComponent<CapsuleCollider3DComponent>()) {
			newEntity.AddComponent<CapsuleCollider3DComponent>(entity.GetComponent<CapsuleCollider3DComponent>());
		}
		
		if (entity.HasComponent<MeshCollider3DComponent>() && !newEntity.HasComponent<MeshCollider3DComponent>()) {
			newEntity.AddComponent<MeshCollider3DComponent>(entity.GetComponent<MeshCollider3DComponent>());
		}
		
		// ========================================
		// COPY SCRIPTING COMPONENT
		// ========================================
		if (entity.HasComponent<ScriptComponent>() && !newEntity.HasComponent<ScriptComponent>()) {
			newEntity.AddComponent<ScriptComponent>(entity.GetComponent<ScriptComponent>());
		}
		
		SelectEntity(newEntity);
		LNX_LOG_INFO("Duplicated entity: {0} -> {1}", tag, newName);
	}
	
	void SceneHierarchyPanel::DuplicateSelectedEntities() {
		std::vector<Entity> toDuplicate(m_SelectedEntities.begin(), m_SelectedEntities.end());
		for (auto entity : toDuplicate) {
			DuplicateEntity(entity);
		}
	}
	
	void SceneHierarchyPanel::RenameEntity(Entity entity) {
		if (!entity) return;
		m_IsRenaming = true;
		m_EntityBeingRenamed = entity;
		strncpy_s(m_RenameBuffer, sizeof(m_RenameBuffer), entity.GetComponent<TagComponent>().Tag.c_str(), _TRUNCATE);
	}
	
	void SceneHierarchyPanel::RenameSelectedEntity() {
		if (m_SelectionContext) {
			RenameEntity(m_SelectionContext);
		}
	}
	
	// ============================================================================
	// SORTING - Get only root entities
	// ============================================================================
	
	std::vector<Entity> SceneHierarchyPanel::GetSortedRootEntities() {
		std::vector<Entity> rootEntities;
		
		auto view = m_Context->m_Registry.view<TagComponent>();
		for (auto entityID : view) {
			Entity entity{ entityID, m_Context.get() };
			
			// Check if this entity has no parent (is root)
			bool isRoot = true;
			if (entity.HasComponent<RelationshipComponent>()) {
				isRoot = !entity.GetComponent<RelationshipComponent>().HasParent();
			}
			
			if (isRoot) {
				rootEntities.push_back(entity);
			}
		}
		
		// Sort if needed
		if (m_SortMode == SortMode::Name) {
			std::sort(rootEntities.begin(), rootEntities.end(), [](Entity a, Entity b) {
				return a.GetComponent<TagComponent>().Tag < b.GetComponent<TagComponent>().Tag;
			});
		}
		
		return rootEntities;
	}
	
	// ============================================================================
	// PIVOT POINT CALCULATIONS
	// ============================================================================
	
	glm::vec3 SceneHierarchyPanel::CalculateMedianPoint() const {
		if (m_SelectedEntities.empty()) return glm::vec3(0.0f);
		
		glm::vec3 sum(0.0f);
		int count = 0;
		
		for (auto entity : m_SelectedEntities) {
			if (entity.HasComponent<TransformComponent>()) {
				sum += entity.GetComponent<TransformComponent>().Translation;
				count++;
			}
		}
		
		return count > 0 ? sum / static_cast<float>(count) : glm::vec3(0.0f);
	}
	
	glm::vec3 SceneHierarchyPanel::CalculateActiveElementPosition() const {
		Entity activeEntity = m_LastSelectedEntity;
		if (activeEntity && activeEntity.HasComponent<TransformComponent>()) {
			return activeEntity.GetComponent<TransformComponent>().Translation;
		}
		return CalculateMedianPoint();
	}
	
	glm::vec3 SceneHierarchyPanel::CalculateBoundingBoxCenter() const {
		if (m_SelectedEntities.empty()) return glm::vec3(0.0f);
		
		glm::vec3 min(FLT_MAX);
		glm::vec3 max(-FLT_MAX);
		
		for (auto entity : m_SelectedEntities) {
			if (entity.HasComponent<TransformComponent>()) {
				const auto& transform = entity.GetComponent<TransformComponent>();
				glm::vec3 pos = transform.Translation;
				glm::vec3 halfExtents = transform.Scale * 0.5f;
				
				min = glm::min(min, pos - halfExtents);
				max = glm::max(max, pos + halfExtents);
			}
		}
		
		return (min + max) * 0.5f;
	}
}