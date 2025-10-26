#include "SceneHierarchyPanel.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include "Scene/Components.h"

namespace Lunex {
	extern const std::filesystem::path g_AssetPath;

	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context) {
		SetContext(context);

		// Cargar iconos
		m_CameraIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/CameraIcon.png");
		m_EntityIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/EntityIcon.png");
		m_LightIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/LightIcon.png");
		m_MeshIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/MeshIcon.png");
		m_VisibleIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/VisibleIcon.png");
		m_HiddenIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/HiddenIcon.png");
		m_LockedIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/LockedIcon.png");
		m_UnlockedIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/UnlockedIcon.png");

		// Debug
		if (!m_CameraIcon) LNX_LOG_ERROR("Failed to load Camera Icon!");
		if (!m_EntityIcon) LNX_LOG_ERROR("Failed to load Entity Icon!");
	}

	void SceneHierarchyPanel::SetContext(const Ref<Scene>& context) {
		m_Context = context;
		m_SelectionContext = {};
		m_SelectedEntities.clear();
		m_EntityVisibility.clear();
		m_EntityLocked.clear();
	}

	void SceneHierarchyPanel::OnImGuiRender() {
		// ============ HIERARCHY PANEL ============
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("Scene Hierarchy");
		ImGui::PopStyleVar();

		if (m_Context) {
			// Toolbar con fondo oscuro
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
			ImGui::BeginChild("HierarchyToolbar", ImVec2(0, 40), true, ImGuiWindowFlags_NoScrollbar);
			ImGui::PopStyleColor();
			RenderHierarchyToolbar();
			ImGui::EndChild();

			// Search bar
			if (m_Settings.ShowSearchBar) {
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8, 4));
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
				RenderSearchBar();
				ImGui::PopStyleVar();
			}

			ImGui::Separator();

			// Scrollable entity tree area
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
			ImGui::BeginChild("EntityTreeRegion", ImVec2(0, 0), false);
			RenderEntityTree();

			// Click en espacio vacío para deseleccionar
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && ImGui::IsWindowHovered()) {
				m_SelectionContext = {};
				m_SelectedEntities.clear();
			}

			// Right-click en espacio vacío
			if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
				ImGui::OpenPopup("EmptySpaceContextMenu");
			}

			RenderEmptySpaceContextMenu();

			ImGui::EndChild();
			ImGui::PopStyleVar();
		}

		ImGui::End();

		// ============ PROPERTIES PANEL ============
		ImGui::Begin("Properties");
		if (m_SelectionContext) {
			DrawComponents(m_SelectionContext);
		}
		ImGui::End();
	}

	void SceneHierarchyPanel::SetSelectedEntity(Entity entity) {
		m_SelectionContext = entity;
		m_SelectedEntities.clear();
		if (entity) {
			m_SelectedEntities.push_back(entity);
		}
	}

	void SceneHierarchyPanel::RenderHierarchyToolbar() {
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 0));
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);

		// Botones estilo UE5
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.22f, 0.22f, 0.22f, 1.0f));

		// Create Entity button
		if (ImGui::Button("+ Create", ImVec2(80, 30))) {
			ImGui::OpenPopup("CreateEntityPopup");
		}

		if (ImGui::BeginPopup("CreateEntityPopup")) {
			if (ImGui::MenuItem("Empty Entity")) {
				m_Context->CreateEntity("Empty Entity");
			}
			if (ImGui::MenuItem("Camera")) {
				auto entity = m_Context->CreateEntity("Camera");
				entity.AddComponent<CameraComponent>();
			}
			if (ImGui::MenuItem("Sprite")) {
				auto entity = m_Context->CreateEntity("Sprite");
				entity.AddComponent<SpriteRendererComponent>();
			}
			if (ImGui::MenuItem("Circle")) {
				auto entity = m_Context->CreateEntity("Circle");
				entity.AddComponent<CircleRendererComponent>();
			}
			ImGui::EndPopup();
		}

		ImGui::SameLine();
		ImGui::Dummy(ImVec2(8, 0));
		ImGui::SameLine();

		// Filter combo
		ImGui::Text("Filter:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(120.0f);
		const char* filterNames[] = { "All", "Cameras", "Lights", "Meshes", "Empty" };
		if (ImGui::BeginCombo("##Filter", filterNames[(int)m_CurrentFilter])) {
			for (int i = 0; i < 5; i++) {
				bool isSelected = (int)m_CurrentFilter == i;
				if (ImGui::Selectable(filterNames[i], isSelected)) {
					m_CurrentFilter = (HierarchyFilter)i;
				}
				if (isSelected) {
					ImGui::SetItemDefaultFocus();
				}
			}
			ImGui::EndCombo();
		}

		// Settings button
		ImGui::SameLine(ImGui::GetWindowWidth() - 90);
		if (ImGui::Button("Settings", ImVec2(70, 30))) {
			ImGui::OpenPopup("HierarchySettings");
		}

		if (ImGui::BeginPopup("HierarchySettings")) {
			ImGui::Checkbox("Show Search Bar", &m_Settings.ShowSearchBar);
			ImGui::Checkbox("Show Entity Icons", &m_Settings.ShowEntityIcons);
			ImGui::Checkbox("Show Visibility Toggles", &m_Settings.ShowVisibilityToggles);
			ImGui::Checkbox("Show Lock Toggles", &m_Settings.ShowLockToggles);
			ImGui::Separator();
			ImGui::SliderFloat("Indent Spacing", &m_Settings.IndentSpacing, 8.0f, 32.0f);
			ImGui::EndPopup();
		}

		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar(2);
	}

	void SceneHierarchyPanel::RenderSearchBar() {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.05f, 0.05f, 0.05f, 1.0f));
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 8);
		if (ImGui::InputTextWithHint("##HierarchySearch", "Search entities...", m_SearchBuffer, sizeof(m_SearchBuffer))) {
			m_SearchQuery = m_SearchBuffer;
		}
		ImGui::PopStyleColor();
		ImGui::PopStyleVar();
	}

	void SceneHierarchyPanel::RenderEntityTree() {
		auto entities = GetFilteredEntities();

		for (auto& entity : entities) {
			RenderEntityNode(entity);
		}
	}

	void SceneHierarchyPanel::RenderEntityNode(Entity entity) {
		if (!entity || !entity.HasComponent<TagComponent>())
			return;

		auto& tag = entity.GetComponent<TagComponent>().Tag;
		uint32_t entityID = (uint32_t)entity;

		// Check if entity is visible/locked
		bool isVisible = m_EntityVisibility.find(entityID) != m_EntityVisibility.end()
			? m_EntityVisibility[entityID] : true;
		bool isLocked = m_EntityLocked.find(entityID) != m_EntityLocked.end()
			? m_EntityLocked[entityID] : false;

		// Si está en modo renombrado
		if (m_IsRenaming && m_RenamingEntity == entity) {
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
			ImGui::SetNextItemWidth(-1);
			if (ImGui::InputText("##Rename", m_RenameBuffer, sizeof(m_RenameBuffer),
				ImGuiInputTextFlags_EnterReturnsTrue | ImGuiInputTextFlags_AutoSelectAll)) {
				tag = std::string(m_RenameBuffer);
				m_IsRenaming = false;
			}

			if (ImGui::IsKeyPressed(ImGuiKey_Escape)) {
				m_IsRenaming = false;
			}

			ImGui::PopStyleColor();
			return;
		}

		bool isSelected = (m_SelectionContext == entity);
		Ref<Texture2D> icon = GetEntityIcon(entity);

		// Layout horizontal con iconos a la izquierda
		ImGui::PushID(entityID);

		// Begin selectable row (estilo UE5)
		ImVec4 selectableBg = isSelected ? ImVec4(0.90f, 0.50f, 0.10f, 0.35f) : ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
		ImVec4 selectableHover = isSelected ? ImVec4(0.90f, 0.50f, 0.10f, 0.5f) : ImVec4(0.12f, 0.12f, 0.12f, 0.8f);

		ImGui::PushStyleColor(ImGuiCol_Header, selectableBg);
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, selectableHover);
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.90f, 0.50f, 0.10f, 0.7f));

		ImGuiSelectableFlags flags = ImGuiSelectableFlags_SpanAllColumns | ImGuiSelectableFlags_AllowOverlap;
		bool clicked = ImGui::Selectable(("##" + std::to_string(entityID)).c_str(), isSelected, flags, ImVec2(0, 24));

		ImGui::PopStyleColor(3);

		// Guardar posición para dibujar contenido encima
		ImVec2 selectableMin = ImGui::GetItemRectMin();
		ImVec2 selectableMax = ImGui::GetItemRectMax();

		// Click logic
		if (clicked && !isLocked) {
			m_SelectionContext = entity;
			m_SelectedEntities.clear();
			m_SelectedEntities.push_back(entity);
		}

		// Double-click to rename
		if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left) && !isLocked) {
			m_IsRenaming = true;
			m_RenamingEntity = entity;
			strcpy_s(m_RenameBuffer, sizeof(m_RenameBuffer), tag.c_str());
		}

		// Right-click context menu
		if (ImGui::IsItemHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			m_SelectionContext = entity;
			m_SelectedEntities.clear();
			m_SelectedEntities.push_back(entity);
			ImGui::OpenPopup("EntityContextMenu");
		}

		// Drag and drop
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_None)) {
			ImGui::SetDragDropPayload("HIERARCHY_ENTITY", &entityID, sizeof(uint32_t));
			ImGui::Text("Moving: %s", tag.c_str());
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("HIERARCHY_ENTITY")) {
				// TODO: Implement parenting
				LNX_LOG_INFO("Dropped entity onto {0}", tag);
			}
			ImGui::EndDragDropTarget();
		}

		// Dibujar contenido sobre el selectable
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		float xOffset = selectableMin.x + 4.0f;
		float yCenter = (selectableMin.y + selectableMax.y) * 0.5f;
		float iconSize = 16.0f;

		// Visibility toggle
		if (m_Settings.ShowVisibilityToggles) {
			ImVec2 checkboxPos(xOffset, yCenter - iconSize * 0.5f);
			ImGui::SetCursorScreenPos(checkboxPos);

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
			bool visible = isVisible;
			if (ImGui::Checkbox(("##Vis" + std::to_string(entityID)).c_str(), &visible)) {
				m_EntityVisibility[entityID] = visible;
			}
			ImGui::PopStyleVar();

			xOffset += 24.0f;
		}

		// Lock toggle
		if (m_Settings.ShowLockToggles) {
			ImVec2 lockPos(xOffset, yCenter - iconSize * 0.5f);
			ImGui::SetCursorScreenPos(lockPos);

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));
			bool locked = isLocked;
			if (ImGui::Checkbox(("##Lock" + std::to_string(entityID)).c_str(), &locked)) {
				m_EntityLocked[entityID] = locked;
			}
			ImGui::PopStyleVar();

			xOffset += 24.0f;
		}

		// Icon
		if (m_Settings.ShowEntityIcons && icon) {
			ImVec2 iconPos(xOffset, yCenter - iconSize * 0.5f);
			drawList->AddImage(
				(void*)(intptr_t)icon->GetRendererID(),
				iconPos,
				ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
				ImVec2(0, 1),
				ImVec2(1, 0)
			);
			xOffset += iconSize + 6.0f;
		}

		// Entity name
		ImVec2 textPos(xOffset, yCenter - ImGui::GetTextLineHeight() * 0.5f);
		ImU32 textColor = isLocked ? IM_COL32(128, 128, 128, 255) : IM_COL32(230, 230, 230, 255);
		if (isSelected) {
			textColor = IM_COL32(255, 255, 255, 255);
		}
		drawList->AddText(textPos, textColor, tag.c_str());

		ImGui::PopID();

		// Context menu (se renderiza fuera del nodo)
		RenderEntityContextMenu(entity);
	}

	void SceneHierarchyPanel::RenderEntityContextMenu(Entity entity) {
		if (ImGui::BeginPopup("EntityContextMenu")) {
			if (entity && entity.HasComponent<TagComponent>()) {
				auto& tag = entity.GetComponent<TagComponent>().Tag;

				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.50f, 0.10f, 1.0f));
				ImGui::Text("%s", tag.c_str());
				ImGui::PopStyleColor();
				ImGui::Separator();

				if (ImGui::MenuItem("Rename", "F2")) {
					m_IsRenaming = true;
					m_RenamingEntity = entity;
					strcpy_s(m_RenameBuffer, sizeof(m_RenameBuffer), tag.c_str());
				}

				if (ImGui::MenuItem("Duplicate", "Ctrl+D")) {
					DuplicateEntity(entity);
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Delete", "Del")) {
					m_Context->DestroyEntity(entity);
					if (m_SelectionContext == entity) {
						m_SelectionContext = {};
					}
					ImGui::CloseCurrentPopup();
				}

				ImGui::Separator();

				if (ImGui::MenuItem("Copy")) {
					// TODO: Implement clipboard copy
				}

				if (ImGui::MenuItem("Paste")) {
					// TODO: Implement clipboard paste
				}

				ImGui::Separator();

				uint32_t entityID = (uint32_t)entity;
				bool isVisible = m_EntityVisibility.find(entityID) != m_EntityVisibility.end()
					? m_EntityVisibility[entityID] : true;
				bool isLocked = m_EntityLocked.find(entityID) != m_EntityLocked.end()
					? m_EntityLocked[entityID] : false;

				if (ImGui::MenuItem(isVisible ? "Hide" : "Show")) {
					m_EntityVisibility[entityID] = !isVisible;
				}

				if (ImGui::MenuItem(isLocked ? "Unlock" : "Lock")) {
					m_EntityLocked[entityID] = !isLocked;
				}
			}

			ImGui::EndPopup();
		}
	}

	void SceneHierarchyPanel::RenderEmptySpaceContextMenu() {
		if (ImGui::BeginPopup("EmptySpaceContextMenu")) {
			if (ImGui::MenuItem("Create Empty Entity")) {
				m_Context->CreateEntity("Empty Entity");
			}

			if (ImGui::BeginMenu("Create")) {
				if (ImGui::MenuItem("Camera")) {
					auto entity = m_Context->CreateEntity("Camera");
					entity.AddComponent<CameraComponent>();
				}
				if (ImGui::MenuItem("Sprite")) {
					auto entity = m_Context->CreateEntity("Sprite");
					entity.AddComponent<SpriteRendererComponent>();
				}
				if (ImGui::MenuItem("Circle")) {
					auto entity = m_Context->CreateEntity("Circle");
					entity.AddComponent<CircleRendererComponent>();
				}
				ImGui::EndMenu();
			}

			ImGui::Separator();

			if (ImGui::MenuItem("Paste")) {
				// TODO: Implement paste
			}

			ImGui::EndPopup();
		}
	}

	bool SceneHierarchyPanel::PassesSearchFilter(Entity entity) {
		if (m_SearchQuery.empty()) return true;

		auto& tag = entity.GetComponent<TagComponent>().Tag;
		std::string tagLower = tag;
		std::transform(tagLower.begin(), tagLower.end(), tagLower.begin(), ::tolower);

		std::string queryLower = m_SearchQuery;
		std::transform(queryLower.begin(), queryLower.end(), queryLower.begin(), ::tolower);

		return tagLower.find(queryLower) != std::string::npos;
	}

	bool SceneHierarchyPanel::PassesTypeFilter(Entity entity) {
		switch (m_CurrentFilter) {
		case HierarchyFilter::All:
			return true;
		case HierarchyFilter::Cameras:
			return entity.HasComponent<CameraComponent>();
		case HierarchyFilter::Lights:
			return false;
		case HierarchyFilter::Meshes:
			return entity.HasComponent<SpriteRendererComponent>()
				|| entity.HasComponent<CircleRendererComponent>();
		case HierarchyFilter::Empty:
			return !entity.HasComponent<CameraComponent>()
				&& !entity.HasComponent<SpriteRendererComponent>()
				&& !entity.HasComponent<CircleRendererComponent>();
		}
		return true;
	}

	std::vector<Entity> SceneHierarchyPanel::GetFilteredEntities() {
		std::vector<Entity> entities;

		m_Context->m_Registry.view<entt::entity>().each([&](auto entityID) {
			Entity entity{ entityID, m_Context.get() };

			if (!entity.HasComponent<TagComponent>())
				return;

			if (PassesSearchFilter(entity) && PassesTypeFilter(entity)) {
				entities.push_back(entity);
			}
			});

		// Sort by name
		std::sort(entities.begin(), entities.end(), [](Entity a, Entity b) {
			return a.GetComponent<TagComponent>().Tag < b.GetComponent<TagComponent>().Tag;
			});

		return entities;
	}

	void SceneHierarchyPanel::DuplicateEntity(Entity entity) {
		auto& tag = entity.GetComponent<TagComponent>().Tag;
		auto newEntity = m_Context->CreateEntity(tag + " (Copy)");

		// Copy transform
		if (entity.HasComponent<TransformComponent>()) {
			newEntity.AddOrReplaceComponent<TransformComponent>(entity.GetComponent<TransformComponent>());
		}

		// Copy other components
		if (entity.HasComponent<SpriteRendererComponent>()) {
			newEntity.AddOrReplaceComponent<SpriteRendererComponent>(entity.GetComponent<SpriteRendererComponent>());
		}

		if (entity.HasComponent<CircleRendererComponent>()) {
			newEntity.AddOrReplaceComponent<CircleRendererComponent>(entity.GetComponent<CircleRendererComponent>());
		}

		if (entity.HasComponent<CameraComponent>()) {
			newEntity.AddOrReplaceComponent<CameraComponent>(entity.GetComponent<CameraComponent>());
		}

		LNX_LOG_INFO("Duplicated entity: {0}", tag);
	}

	Ref<Texture2D> SceneHierarchyPanel::GetEntityIcon(Entity entity) {
		if (entity.HasComponent<CameraComponent>()) {
			return m_CameraIcon ? m_CameraIcon : m_EntityIcon;
		}
		if (entity.HasComponent<SpriteRendererComponent>() || entity.HasComponent<CircleRendererComponent>()) {
			return m_MeshIcon ? m_MeshIcon : m_EntityIcon;
		}
		return m_EntityIcon;
	}

	// ============================================================================
	// COMPONENT DRAWING - MANTENIDO ORIGINAL SIN CAMBIOS
	// ============================================================================

	static void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f) {
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, columnWidth);
		ImGui::Text(label.c_str());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

		float lineHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", buttonSize))
			values.x = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopStyleColor();
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize))
			values.y = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopStyleColor();
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize))
			values.z = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopStyleColor();
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();
		ImGui::Columns(1);
		ImGui::PopID();
	}

	template<typename T, typename UIFunction>
	static void DrawComponent(const std::string& name, Entity entity, UIFunction uifunction) {
		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed
			| ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;

		if (entity.HasComponent<T>()) {
			auto& component = entity.GetComponent<T>();
			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 4, 4 });
			float lineHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f;
			ImGui::Separator();
			bool open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, name.c_str());
			ImGui::PopStyleVar();

			ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
			ImGui::PushID((int)(intptr_t)(void*)typeid(T).hash_code());

			if (ImGui::Button("+", ImVec2{ lineHeight, lineHeight })) {
				ImGui::OpenPopup("ComponentSettings");
			}

			bool removeComponent = false;
			if (ImGui::BeginPopup("ComponentSettings")) {
				if (ImGui::MenuItem("Remove component"))
					removeComponent = true;
				ImGui::EndPopup();
			}

			ImGui::PopID();

			if (open) {
				uifunction(component);
				ImGui::TreePop();
			}

			if (removeComponent)
				entity.RemoveComponent<T>();
		}
	}

	void SceneHierarchyPanel::DrawComponents(Entity entity) {
		if (entity.HasComponent<TagComponent>()) {
			auto& tag = entity.GetComponent<TagComponent>().Tag;

			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strcpy_s(buffer, sizeof(buffer), tag.c_str());
			if (ImGui::InputText("##Tag", buffer, sizeof(buffer))) {
				tag = std::string(buffer);
			}
		}

		ImGui::SameLine();
		ImGui::PushItemWidth(-1);

		if (ImGui::Button("Add Component"))
			ImGui::OpenPopup("AddComponent");
		if (ImGui::BeginPopup("AddComponent")) {
			DisplayAddComponentEntry<CameraComponent>("Camera");
			DisplayAddComponentEntry<SpriteRendererComponent>("Sprite Renderer");
			DisplayAddComponentEntry<CircleRendererComponent>("Circle Renderer");
			DisplayAddComponentEntry<Rigidbody2DComponent>("Rigidbody 2D");
			DisplayAddComponentEntry<BoxCollider2DComponent>("Box Collider 2D");
			DisplayAddComponentEntry<CircleCollider2DComponent>("Circle Collider 2D");

			ImGui::EndPopup();
		}

		ImGui::PopItemWidth();

		DrawComponent<TransformComponent>("Transform", entity, [](auto& component) {
			DrawVec3Control("Translation", component.Translation);
			glm::vec3 rotation = glm::degrees(component.Rotation);
			DrawVec3Control("Rotation", rotation);
			component.Rotation = glm::radians(rotation);
			DrawVec3Control("Scale", component.Scale, 1.0f);
			});

		DrawComponent<CameraComponent>("Camera", entity, [](auto& component) {
			auto& camera = component.Camera;

			ImGui::Checkbox("Primary", &component.Primary);

			const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
			const char* currentProjectionTypeString = projectionTypeStrings[(int)camera.GetProjectionType()];
			if (ImGui::BeginCombo("Projection", currentProjectionTypeString)) {
				for (int i = 0; i < 2; i++) {
					bool isSelected = currentProjectionTypeString == projectionTypeStrings[i];
					if (ImGui::Selectable(projectionTypeStrings[i], isSelected)) {
						currentProjectionTypeString = projectionTypeStrings[i];
						camera.SetProjectionType((SceneCamera::ProjectionType)i);
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}

				ImGui::EndCombo();
			}

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective) {
				float perspectiveVerticalFov = glm::degrees(camera.GetPerspectiveVerticalFOV());
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
				if (ImGui::DragFloat("Vertical FOV", &perspectiveVerticalFov))
					camera.SetPerspectiveVerticalFOV(glm::radians(perspectiveVerticalFov));
				ImGui::PopStyleColor();

				float perspectiveNear = camera.GetPerspectiveNearClip();
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
				if (ImGui::DragFloat("Near", &perspectiveNear))
					camera.SetPerspectiveNearClip(perspectiveNear);
				ImGui::PopStyleColor();

				float perspectiveFar = camera.GetPerspectiveFarClip();
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
				if (ImGui::DragFloat("Far", &perspectiveFar))
					camera.SetPerspectiveFarClip(perspectiveFar);
				ImGui::PopStyleColor();
			}

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic) {
				float orthoSize = camera.GetOrthographicSize();
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
				if (ImGui::DragFloat("Size", &orthoSize))
					camera.SetOrthographicSize(orthoSize);
				ImGui::PopStyleColor();

				float orthoNear = camera.GetOrthographicNearClip();
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
				if (ImGui::DragFloat("Near", &orthoNear))
					camera.SetOrthographicNearClip(orthoNear);
				ImGui::PopStyleColor();

				float orthoFar = camera.GetOrthographicFarClip();
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
				if (ImGui::DragFloat("Far", &orthoFar))
					camera.SetOrthographicFarClip(orthoFar);
				ImGui::PopStyleColor();

				ImGui::Checkbox("Fixed Aspect Ratio", &component.FixedAspectRatio);
			}
			});

		DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, [](auto& component) {
			ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));

			ImGui::Button("Texture", ImVec2(100.0f, 0.0f));
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
					const wchar_t* path = (const wchar_t*)payload->Data;
					std::filesystem::path texturePath = std::filesystem::path(g_AssetPath) / path;
					Ref<Texture2D> texture = Texture2D::Create(texturePath.string());
					if (texture->IsLoaded())
						component.Texture = texture;
					else
						LNX_LOG_WARN("Could not load texture {0}", texturePath.filename().string());
				}
				ImGui::EndDragDropTarget();
			}

			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
			ImGui::DragFloat("Tiling Factor", &component.TilingFactor, 0.1f, 0.0f, 100.0f);
			ImGui::PopStyleColor();
			});

		DrawComponent<CircleRendererComponent>("Circle Renderer", entity, [](auto& component) {
			ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));
			ImGui::DragFloat("Thickness", &component.Thickness, 0.025f, 0.0f, 1.0f);
			ImGui::DragFloat("Fade", &component.Fade, 0.00025f, 0.0f, 1.0f);
			});

		DrawComponent<Rigidbody2DComponent>("Rigidbody 2D", entity, [](auto& component) {
			const char* bodyTypeStrings[] = { "Static", "Dynamic", "Kinematic" };
			const char* currentBodyTypeString = bodyTypeStrings[(int)component.Type];
			if (ImGui::BeginCombo("Body Type", currentBodyTypeString)) {
				for (int i = 0; i < 3; i++) {
					bool isSelected = currentBodyTypeString == bodyTypeStrings[i];
					if (ImGui::Selectable(bodyTypeStrings[i], isSelected)) {
						currentBodyTypeString = bodyTypeStrings[i];
						component.Type = (Rigidbody2DComponent::BodyType)i;
					}

					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::Checkbox("Fixed Rotation", &component.FixedRotation);
			});

		DrawComponent<BoxCollider2DComponent>("Box Collider 2D", entity, [](auto& component) {
			ImGui::DragFloat2("Offset", glm::value_ptr(component.Offset));
			ImGui::DragFloat2("Size", glm::value_ptr(component.Size));
			ImGui::DragFloat("Density", &component.Density, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Friction", &component.Friction, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution Threshold", &component.RestitutionThreshold, 0.01f, 0.0f);
			});

		DrawComponent<CircleCollider2DComponent>("Circle Collider 2D", entity, [](auto& component) {
			ImGui::DragFloat2("Offset", glm::value_ptr(component.Offset));
			ImGui::DragFloat("Radius", &component.Radius);
			ImGui::DragFloat("Density", &component.Density, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Friction", &component.Friction, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution Threshold", &component.RestitutionThreshold, 0.01f, 0.0f);
			});
	}

	template<typename T>
	void SceneHierarchyPanel::DisplayAddComponentEntry(const std::string& entryName) {
		if (!m_SelectionContext.HasComponent<T>()) {
			if (ImGui::MenuItem(entryName.c_str())) {
				m_SelectionContext.AddComponent<T>();
				ImGui::CloseCurrentPopup();
			}
		}
	}
}