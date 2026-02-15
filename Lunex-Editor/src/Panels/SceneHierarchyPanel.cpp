#include "SceneHierarchyPanel.h"
#include "ContentBrowserPanel.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <algorithm>

#include "Scene/Components.h"
#include "Asset/Prefab.h"

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
		using namespace UI;
		
		// Apply custom panel styling
		ScopedColor colors({
			{ImGuiCol_WindowBg, m_Style.WindowBg},
			{ImGuiCol_ChildBg, m_Style.ChildBg},
			{ImGuiCol_Border, m_Style.Border},
			{ImGuiCol_Header, m_Style.ItemSelected},
			{ImGuiCol_HeaderHovered, Color(m_Style.ItemSelected.r, m_Style.ItemSelected.g, m_Style.ItemSelected.b, 0.50f)},
			{ImGuiCol_HeaderActive, Color(m_Style.ItemSelected.r, m_Style.ItemSelected.g, m_Style.ItemSelected.b, 0.65f)},
			{ImGuiCol_Text, m_Style.TextPrimary}
		});
		
		ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, m_Style.IndentSpacing);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 1));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 3));
		
		if (!BeginPanel("Scene Hierarchy")) {
			ImGui::PopStyleVar(3);
			EndPanel();
			return;
		}

		RenderToolbar();
		RenderSearchBar();
		
		Separator();
		AddSpacing(SpacingValues::XS);
		
		RenderEntityList();
		RenderContextMenu();

		ImGui::PopStyleVar(3);
		EndPanel();
	}

	void SceneHierarchyPanel::SetSelectedEntity(Entity entity) {
		SelectEntity(entity);
	}

	void SceneHierarchyPanel::RenderToolbar() {
		using namespace UI;
		
		ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
		ScopedColor buttonColors({
			{ImGuiCol_Button, Color(0.16f, 0.16f, 0.18f, 1.0f)},
			{ImGuiCol_ButtonHovered, Color(0.22f, 0.22f, 0.25f, 1.0f)},
			{ImGuiCol_ButtonActive, Color(0.26f, 0.50f, 0.85f, 0.6f)}
		});
		
		if (ImGui::Button("+ Create")) {
			m_ShowCreateMenu = true;
			ImGui::OpenPopup("CreateEntityPopup");
		}
		
		if (ImGui::BeginPopup("CreateEntityPopup")) {
			if (ImGui::MenuItem("Empty Entity"))
				m_Context->CreateEntity("Empty Entity");
			
			ImGui::Separator();
			
			// 3D Objects submenu
			if (ImGui::BeginMenu("3D Object")) {
				if (ImGui::MenuItem("Cube"))
					CreateMeshEntity("Cube", ModelType::Cube);
				if (ImGui::MenuItem("Sphere"))
					CreateMeshEntity("Sphere", ModelType::Sphere);
				if (ImGui::MenuItem("Plane"))
					CreateMeshEntity("Plane", ModelType::Plane);
				if (ImGui::MenuItem("Cylinder"))
					CreateMeshEntity("Cylinder", ModelType::Cylinder);
				ImGui::EndMenu();
			}
			
			// 2D Objects submenu
			if (ImGui::BeginMenu("2D Object")) {
				if (ImGui::MenuItem("Sprite"))
					CreateEntityWithComponent<SpriteRendererComponent>("Sprite");
				if (ImGui::MenuItem("Circle"))
					CreateEntityWithComponent<CircleRendererComponent>("Circle");
				ImGui::EndMenu();
			}
			
			// Lights submenu
			if (ImGui::BeginMenu("Light")) {
				if (ImGui::MenuItem("Directional Light")) {
					Entity entity = m_Context->CreateEntity("Directional Light");
					auto& light = entity.AddComponent<LightComponent>();
					light.SetType(LightType::Directional);
					SelectEntity(entity);
				}
				if (ImGui::MenuItem("Point Light")) {
					Entity entity = m_Context->CreateEntity("Point Light");
					auto& light = entity.AddComponent<LightComponent>();
					light.SetType(LightType::Point);
					SelectEntity(entity);
				}
				if (ImGui::MenuItem("Spot Light")) {
					Entity entity = m_Context->CreateEntity("Spot Light");
					auto& light = entity.AddComponent<LightComponent>();
					light.SetType(LightType::Spot);
					SelectEntity(entity);
				}
				ImGui::EndMenu();
			}
			
			if (ImGui::MenuItem("Camera"))
				CreateEntityWithComponent<CameraComponent>("Camera");
			
			ImGui::EndPopup();
		}
		
		ImGui::SameLine();
		
		// Entity count display
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 80);
		{
			ScopedColor textColor(ImGuiCol_Text, m_Style.TextMuted);
			if (m_SelectedEntities.size() > 0) {
				ImGui::Text("%d/%d", (int)m_SelectedEntities.size(), m_TotalEntities);
			} else {
				ImGui::Text("%d", m_TotalEntities);
			}
		}
	}
	
	void SceneHierarchyPanel::RenderSearchBar() {
		using namespace UI;
		
		ScopedColor frameColors({
			{ImGuiCol_FrameBg, Color(0.14f, 0.14f, 0.16f, 1.0f).ToImVec4()},
			{ImGuiCol_FrameBgHovered, Color(0.18f, 0.18f, 0.20f, 1.0f).ToImVec4()},
			{ImGuiCol_FrameBgActive, Color(0.20f, 0.40f, 0.68f, 0.3f).ToImVec4()}
		});
		
		ImGui::SetNextItemWidth(-1);
		ImGui::InputTextWithHint("##HierarchySearch", "Search entities...", m_SearchFilter, 256);
	}
	
	void SceneHierarchyPanel::RenderEntityList() {
		using namespace UI;
		
		if (!m_Context) return;
		
		m_EntityIndexCounter = 0;
		m_TotalEntities = 0;
		m_VisibleEntities = 0;
		
		auto rootEntities = GetSortedRootEntities();
		m_TotalEntities = (int)m_Context->m_Registry.view<TagComponent>().size();
		
		ImGui::BeginChild("##EntityList", ImVec2(0, 0), false);
		
		for (auto entity : rootEntities) {
			DrawEntityNode(entity, 0);
		}

		// Drop target for root level (unparent)
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_NODE")) {
				Entity droppedEntity = *(Entity*)payload->Data;
				UnparentEntity(droppedEntity);
			}
			
			// Accept prefab drops from Content Browser
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
				ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
				std::string ext = data->Extension;
				if (ext == ".luprefab") {
					InstantiatePrefab(data->FilePath);
				}
			}
			
			ImGui::EndDragDropTarget();
		}

		// Click on empty area to deselect
		if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered() && !ImGui::IsAnyItemHovered()) {
			ClearSelection();
		}
		
		ImGui::EndChild();
	}
	
	void SceneHierarchyPanel::RenderContextMenu() {
		// Context menu for empty area
		if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
			using namespace UI;
			
			SeparatorText("Create Entity");
			
			if (ImGui::MenuItem("Empty Entity"))
				m_Context->CreateEntity("Empty Entity");
			
			ImGui::Separator();
			
			if (ImGui::MenuItem("Camera"))
				CreateEntityWithComponent<CameraComponent>("Camera");
			
			if (ImGui::MenuItem("Light"))
				CreateEntityWithComponent<LightComponent>("Light");
			
			if (ImGui::MenuItem("Sprite"))
				CreateEntityWithComponent<SpriteRendererComponent>("Sprite");
			
			if (ImGui::MenuItem("3D Object"))
				CreateMeshEntity("Cube", ModelType::Cube);
			
			ImGui::EndPopup();
		}
	}
	
	UI::Color SceneHierarchyPanel::GetEntityTypeColor(Entity entity) const {
		if (entity.HasComponent<CameraComponent>()) return m_Style.TypeCamera;
		if (entity.HasComponent<LightComponent>()) return m_Style.TypeLight;
		if (entity.HasComponent<MeshComponent>()) return m_Style.TypeMesh;
		if (entity.HasComponent<SpriteRendererComponent>()) return m_Style.TypeSprite;
		return m_Style.TypeDefault;
	}
	
	const char* SceneHierarchyPanel::GetEntityTypeIcon(Entity entity) const {
		// Return simple text indicators instead of emojis for AAA look
		if (entity.HasComponent<CameraComponent>()) return "CAM";
		if (entity.HasComponent<LightComponent>()) return "LGT";
		if (entity.HasComponent<MeshComponent>()) return "MSH";
		if (entity.HasComponent<SpriteRendererComponent>()) return "SPR";
		return "";
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity, int depth) {
		using namespace UI;
		
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

		// Prepare drawing
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
		float itemWidth = ImGui::GetContentRegionAvail().x;
		float itemHeight = m_Style.ItemHeight;
		
		// Alternate background colors
		Color bgColor = (m_EntityIndexCounter % 2 == 0) ? m_Style.ItemEven : m_Style.ItemOdd;
		m_EntityIndexCounter++;
		
		bool isSelected = IsEntitySelected(entity);
		bool isActive = (entity == m_LastSelectedEntity);
		bool isRenaming = (m_IsRenaming && m_EntityBeingRenamed == entity);

		// Draw background
		ImVec2 itemMin = cursorScreenPos;
		ImVec2 itemMax = ImVec2(cursorScreenPos.x + itemWidth, cursorScreenPos.y + itemHeight);
		
		if (isSelected) {
			if (isActive && m_SelectedEntities.size() > 1) {
				// Active element in multi-selection: light orange (Blender-style)
				drawList->AddRectFilled(itemMin, itemMax, m_Style.ItemActive.ToImU32());
				drawList->AddRectFilled(
					itemMin, 
					ImVec2(itemMin.x + m_Style.TypeIndicatorWidth, itemMax.y),
					m_Style.ItemActiveBorder.ToImU32()
				);
			}
			else if (isSelected && m_SelectedEntities.size() > 1) {
				// Non-active selected in multi-selection: dark orange
				drawList->AddRectFilled(itemMin, itemMax, m_Style.ItemSelectedMulti.ToImU32());
				drawList->AddRectFilled(
					itemMin, 
					ImVec2(itemMin.x + m_Style.TypeIndicatorWidth, itemMax.y),
					m_Style.ItemSelectedMultiBorder.ToImU32()
				);
			}
			else {
				// Single selection: blue highlight
				drawList->AddRectFilled(itemMin, itemMax, m_Style.ItemSelected.ToImU32());
				drawList->AddRectFilled(
					itemMin, 
					ImVec2(itemMin.x + m_Style.TypeIndicatorWidth, itemMax.y),
					m_Style.ItemSelectedBorder.ToImU32()
				);
			}
		} else {
			drawList->AddRectFilled(itemMin, itemMax, bgColor.ToImU32());
		}

		// Type indicator bar on left
		if (!isSelected) {
			Color typeColor = GetEntityTypeColor(entity);
			drawList->AddRectFilled(
				itemMin,
				ImVec2(itemMin.x + m_Style.TypeIndicatorWidth, itemMax.y),
				typeColor.ToImU32()
			);
		}

		// Push ID and set cursor for content
		ImGui::PushID((void*)(uint64_t)(uint32_t)entity);
		
		// Indent based on depth
		float indentOffset = depth * m_Style.IndentSpacing + m_Style.TypeIndicatorWidth + 4.0f;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indentOffset);
		
		// Tree node flags
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
		
		if (!hasChildren) {
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		}
		
		if (isSelected) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		// Draw icon
		Ref<Texture2D> icon = m_EntityIcon;
		if (entity.HasComponent<CameraComponent>()) icon = m_CameraIcon ? m_CameraIcon : m_EntityIcon;
		else if (entity.HasComponent<LightComponent>()) icon = m_LightIcon ? m_LightIcon : m_EntityIcon;
		else if (entity.HasComponent<MeshComponent>()) icon = m_MeshIcon ? m_MeshIcon : m_EntityIcon;
		else if (entity.HasComponent<SpriteRendererComponent>()) icon = m_SpriteIcon ? m_SpriteIcon : m_EntityIcon;

		ImVec2 iconPos = ImVec2(cursorScreenPos.x + indentOffset, cursorScreenPos.y + (itemHeight - m_Style.IconSize) * 0.5f);
		if (icon) {
			drawList->AddImage(
				(ImTextureID)(intptr_t)icon->GetRendererID(),
				iconPos,
				ImVec2(iconPos.x + m_Style.IconSize, iconPos.y + m_Style.IconSize),
				ImVec2(0, 1), ImVec2(1, 0)
			);
		}
		
		// Adjust cursor for tree node
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + m_Style.IconSize + 4.0f);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (itemHeight - ImGui::GetFrameHeight()) * 0.5f);

		// Rename mode or tree node
		bool opened = false;
		
		if (isRenaming) {
			ImGui::SetKeyboardFocusHere();
			ScopedColor frameColor(ImGuiCol_FrameBg, Color(0.18f, 0.40f, 0.65f, 0.3f).ToImVec4());
			ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 20.0f);
			
			if (ImGui::InputText("##RenameEntity", m_RenameBuffer, sizeof(m_RenameBuffer), 
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
				entity.GetComponent<TagComponent>().Tag = m_RenameBuffer;
				m_IsRenaming = false;
			}
			
			if (!ImGui::IsItemActive() && !ImGui::IsItemFocused()) {
				m_IsRenaming = false;
			}
		} else {
			// Use invisible button for the entire row
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (itemHeight - ImGui::GetFrameHeight()) * 0.5f);
			
			// Custom tree node rendering
			ScopedColor treeColors({
				{ImGuiCol_Header, ImVec4(0, 0, 0, 0)},  // Transparent, we draw our own bg
				{ImGuiCol_HeaderHovered, ImVec4(0, 0, 0, 0)},
				{ImGuiCol_HeaderActive, ImVec4(0, 0, 0, 0)}
			});
			
			opened = ImGui::TreeNodeEx(tag.c_str(), flags);
		}

		// Hover effect
		if (ImGui::IsItemHovered() && !isRenaming) {
			drawList->AddRectFilled(itemMin, itemMax, m_Style.ItemHover.ToImU32());
		}

		// Click handling
		if (ImGui::IsItemClicked() && !isRenaming) {
			ImGuiIO& io = ImGui::GetIO();
			if (io.KeyCtrl) {
				ToggleEntitySelection(entity);
			}
			else if (io.KeyShift) {
				// Shift+Click: Add to selection
				if (!IsEntitySelected(entity)) {
					AddEntityToSelection(entity);
				}
			}
			else {
				// Normal click
				if (IsEntitySelected(entity) && m_SelectedEntities.size() > 1) {
					// Already selected in multi-selection: make active
					SetActiveEntityInSelection(entity);
				}
				else {
					SelectEntity(entity, true);
				}
			}
		}

		// Drag & Drop Source
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			ImGui::SetDragDropPayload("ENTITY_NODE", &entity, sizeof(Entity));
			ImGui::Text("%s", tag.c_str());
			m_DraggedEntity = entity;
			ImGui::EndDragDropSource();
		}

		// Drag & Drop Target (for parenting)
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_NODE")) {
				Entity droppedEntity = *(Entity*)payload->Data;
				
				if (droppedEntity != entity) {
					Entity currentParent = m_Context->GetParent(droppedEntity);
					if (currentParent == entity) {
						UnparentEntity(droppedEntity);
					}
					else if (!m_Context->IsAncestorOf(droppedEntity, entity)) {
						SetEntityParent(droppedEntity, entity);
					}
				}
			}
			ImGui::EndDragDropTarget();
		}

		// Context menu
		RenderEntityContextMenu(entity);

		ImGui::PopID();
		
		// Advance cursor to next row - Need to submit a dummy item to properly extend window bounds
		float advanceAmount = itemHeight - ImGui::GetFrameHeight();
		if (advanceAmount > 0) {
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + advanceAmount);
			ImGui::Dummy(ImVec2(0, 0)); // Dummy item to properly extend window boundaries
		}

		// Draw children recursively if opened
		if (opened && hasChildren) {
			auto children = m_Context->GetChildren(entity);
			for (auto child : children) {
				DrawEntityNode(child, depth + 1);
			}
			ImGui::TreePop();
		}
	}
	
	void SceneHierarchyPanel::RenderEntityContextMenu(Entity entity) {
		using namespace UI;
		
		auto& tag = entity.GetComponent<TagComponent>().Tag;
		bool entityDeleted = false;
		
		if (ImGui::BeginPopupContextItem()) {
			// Header
			{
				ScopedColor textColor(ImGuiCol_Text, m_Style.TextMuted.ToImVec4());
				ImGui::Text("%s", tag.c_str());
			}
			ImGui::Separator();
			
			if (ImGui::MenuItem("Rename", "F2")) {
				RenameEntity(entity);
			}
			
			if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
				DuplicateEntity(entity);
			}
			
			ImGui::Separator();
			
			if (ImGui::MenuItem("Create Prefab")) {
				CreatePrefabFromEntity(entity);
			}
			
			ImGui::Separator();
			
			// Hierarchy options
			Entity parent = m_Context->GetParent(entity);
			if (parent) {
				if (ImGui::MenuItem("Unparent")) {
					UnparentEntity(entity);
				}
			}
			
			if (ImGui::MenuItem("Create Child")) {
				Entity child = m_Context->CreateEntity("Child");
				SetEntityParent(child, entity);
				SelectEntity(child);
			}
			
			ImGui::Separator();
			
			{
				ScopedColor deleteColor(ImGuiCol_Text, Color(1.0f, 0.3f, 0.3f, 1.0f).ToImVec4());
				if (ImGui::MenuItem("Delete", "Del")) {
					entityDeleted = true;
				}
			}
			
			ImGui::Separator();
			
			// Entity info
			{
				ScopedColor infoColor(ImGuiCol_Text, m_Style.TextMuted.ToImVec4());
				ImGui::Text("UUID: %llu", (uint64_t)entity.GetComponent<IDComponent>().ID);
				if (parent) {
					ImGui::Text("Parent: %s", parent.GetComponent<TagComponent>().Tag.c_str());
				}
			}

			ImGui::EndPopup();
		}

		// Delete entity (after popup closed)
		if (entityDeleted) {
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
	
	void SceneHierarchyPanel::SetActiveEntityInSelection(Entity entity) {
		if (entity && m_SelectedEntities.find(entity) != m_SelectedEntities.end()) {
			// Entity is already in selection - just make it active
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
		
		// Generate unique name
		std::string baseName = tag;
		std::string newName;
		
		size_t lastParen = baseName.rfind(" (");
		if (lastParen != std::string::npos) {
			std::string afterParen = baseName.substr(lastParen + 2);
			if (!afterParen.empty() && afterParen.back() == ')') {
				std::string numStr = afterParen.substr(0, afterParen.size() - 1);
				bool isNumber = !numStr.empty() && std::all_of(numStr.begin(), numStr.end(), ::isdigit);
				if (isNumber) {
					baseName = baseName.substr(0, lastParen);
				}
			}
		}
		
		int counter = 1;
		bool nameExists = true;
		
		while (nameExists) {
			newName = baseName + " (" + std::to_string(counter) + ")";
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
		
		Entity newEntity = m_Context->CreateEntity(newName);
		
		// Copy Transform
		if (entity.HasComponent<TransformComponent>()) {
			newEntity.GetComponent<TransformComponent>() = entity.GetComponent<TransformComponent>();
		}
		
		// Copy components
		if (entity.HasComponent<CameraComponent>() && !newEntity.HasComponent<CameraComponent>())
			newEntity.AddComponent<CameraComponent>(entity.GetComponent<CameraComponent>());
		
		if (entity.HasComponent<SpriteRendererComponent>() && !newEntity.HasComponent<SpriteRendererComponent>())
			newEntity.AddComponent<SpriteRendererComponent>(entity.GetComponent<SpriteRendererComponent>());
		
		if (entity.HasComponent<CircleRendererComponent>() && !newEntity.HasComponent<CircleRendererComponent>())
			newEntity.AddComponent<CircleRendererComponent>(entity.GetComponent<CircleRendererComponent>());
		
		if (entity.HasComponent<MeshComponent>() && !newEntity.HasComponent<MeshComponent>())
			newEntity.AddComponent<MeshComponent>(entity.GetComponent<MeshComponent>());
		
		if (entity.HasComponent<LightComponent>() && !newEntity.HasComponent<LightComponent>())
			newEntity.AddComponent<LightComponent>(entity.GetComponent<LightComponent>());
		
		if (entity.HasComponent<MaterialComponent>()) {
			if (newEntity.HasComponent<MaterialComponent>())
				newEntity.GetComponent<MaterialComponent>() = entity.GetComponent<MaterialComponent>();
			else
				newEntity.AddComponent<MaterialComponent>(entity.GetComponent<MaterialComponent>());
		}
		
		if (entity.HasComponent<TextureComponent>()) {
			if (newEntity.HasComponent<TextureComponent>())
				newEntity.GetComponent<TextureComponent>() = entity.GetComponent<TextureComponent>();
			else
				newEntity.AddComponent<TextureComponent>(entity.GetComponent<TextureComponent>());
		}
		
		// Copy Physics 2D
		if (entity.HasComponent<Rigidbody2DComponent>() && !newEntity.HasComponent<Rigidbody2DComponent>())
			newEntity.AddComponent<Rigidbody2DComponent>(entity.GetComponent<Rigidbody2DComponent>());
		
		if (entity.HasComponent<BoxCollider2DComponent>() && !newEntity.HasComponent<BoxCollider2DComponent>())
			newEntity.AddComponent<BoxCollider2DComponent>(entity.GetComponent<BoxCollider2DComponent>());
		
		if (entity.HasComponent<CircleCollider2DComponent>() && !newEntity.HasComponent<CircleCollider2DComponent>())
			newEntity.AddComponent<CircleCollider2DComponent>(entity.GetComponent<CircleCollider2DComponent>());
		
		// Copy Physics 3D
		if (entity.HasComponent<Rigidbody3DComponent>() && !newEntity.HasComponent<Rigidbody3DComponent>())
			newEntity.AddComponent<Rigidbody3DComponent>(entity.GetComponent<Rigidbody3DComponent>());
		
		if (entity.HasComponent<BoxCollider3DComponent>() && !newEntity.HasComponent<BoxCollider3DComponent>())
			newEntity.AddComponent<BoxCollider3DComponent>(entity.GetComponent<BoxCollider3DComponent>());
		
		if (entity.HasComponent<SphereCollider3DComponent>() && !newEntity.HasComponent<SphereCollider3DComponent>())
			newEntity.AddComponent<SphereCollider3DComponent>(entity.GetComponent<SphereCollider3DComponent>());
		
		if (entity.HasComponent<CapsuleCollider3DComponent>() && !newEntity.HasComponent<CapsuleCollider3DComponent>())
			newEntity.AddComponent<CapsuleCollider3DComponent>(entity.GetComponent<CapsuleCollider3DComponent>());
		
		if (entity.HasComponent<MeshCollider3DComponent>() && !newEntity.HasComponent<MeshCollider3DComponent>())
			newEntity.AddComponent<MeshCollider3DComponent>(entity.GetComponent<MeshCollider3DComponent>());
		
		// Copy Script
		if (entity.HasComponent<ScriptComponent>() && !newEntity.HasComponent<ScriptComponent>())
			newEntity.AddComponent<ScriptComponent>(entity.GetComponent<ScriptComponent>());
		
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
	// SORTING
	// ============================================================================
	
	std::vector<Entity> SceneHierarchyPanel::GetSortedRootEntities() {
		std::vector<Entity> rootEntities;
		
		auto view = m_Context->m_Registry.view<TagComponent>();
		for (auto entityID : view) {
			Entity entity{ entityID, m_Context.get() };
			
			bool isRoot = true;
			if (entity.HasComponent<RelationshipComponent>()) {
				isRoot = !entity.GetComponent<RelationshipComponent>().HasParent();
			}
			
			if (isRoot) {
				rootEntities.push_back(entity);
			}
		}
		
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

	// ============================================================================
	// PREFAB SYSTEM
	// ============================================================================
	
	void SceneHierarchyPanel::CreatePrefabFromEntity(Entity entity) {
		if (!entity) {
			LNX_LOG_WARN("SceneHierarchyPanel::CreatePrefabFromEntity - No entity selected");
			return;
		}

		Ref<Prefab> prefab = Prefab::CreateFromEntity(entity, true);
		if (!prefab) {
			LNX_LOG_ERROR("Failed to create prefab from entity");
			return;
		}

		std::filesystem::path prefabsDir = m_PrefabsDirectory;
		if (prefabsDir.empty()) {
			prefabsDir = g_AssetPath / "Prefabs";
		}

		if (!std::filesystem::exists(prefabsDir)) {
			std::filesystem::create_directories(prefabsDir);
		}

		std::string entityName = entity.GetComponent<TagComponent>().Tag;
		std::filesystem::path prefabPath;
		int counter = 1;
		
		do {
			std::string filename = entityName;
			if (counter > 1) {
				filename += " (" + std::to_string(counter) + ")";
			}
			filename += ".luprefab";
			prefabPath = prefabsDir / filename;
			counter++;
		} while (std::filesystem::exists(prefabPath));

		if (prefab->SaveToFile(prefabPath)) {
			LNX_LOG_INFO("Prefab created: {0}", prefabPath.filename().string());
		} else {
			LNX_LOG_ERROR("Failed to save prefab: {0}", prefabPath.string());
		}
	}

	void SceneHierarchyPanel::InstantiatePrefab(const std::filesystem::path& prefabPath) {
		if (!m_Context) {
			LNX_LOG_WARN("SceneHierarchyPanel::InstantiatePrefab - No scene context");
			return;
		}

		if (!std::filesystem::exists(prefabPath)) {
			LNX_LOG_ERROR("Prefab file not found: {0}", prefabPath.string());
			return;
		}

		Ref<Prefab> prefab = Prefab::LoadFromFile(prefabPath);
		if (!prefab) {
			LNX_LOG_ERROR("Failed to load prefab: {0}", prefabPath.string());
			return;
		}

		glm::vec3 position(0.0f);
		if (m_SelectionContext && m_SelectionContext.HasComponent<TransformComponent>()) {
			position = m_SelectionContext.GetComponent<TransformComponent>().Translation;
			position.x += 1.0f;
		}

		Entity rootEntity = prefab->Instantiate(m_Context, position);
		if (rootEntity) {
			SelectEntity(rootEntity);
			LNX_LOG_INFO("Instantiated prefab: {0}", prefabPath.filename().string());
		} else {
			LNX_LOG_ERROR("Failed to instantiate prefab: {0}", prefabPath.string());
		}
	}
}