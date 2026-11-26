#include "SceneHierarchyPanel.h"
#include "ContentBrowserPanel.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <glm/gtc/type_ptr.hpp>

#include <filesystem>
#include <algorithm>

#include "Scene/Components.h"
#include "Asset/Prefab.h"

// Lunex UI Framework
#include "../UI/UICore.h"
#include "../UI/UIComponents.h"
#include "../UI/UILayout.h"

namespace Lunex {
	extern const std::filesystem::path g_AssetPath;

	SceneHierarchyPanel::SceneHierarchyPanel(const Ref<Scene>& context) {
		SetContext(context);

		m_CameraIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/CameraIcon.png");
		m_EntityIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/EntityIcon.png");
		m_GroupIcon = Texture2D::Create("Resources/Icons/ContentBrowser/FolderIcon.png");
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
		
		ScopedStyle indentStyle(ImGuiStyleVar_IndentSpacing, m_Style.IndentSpacing);
		ScopedStyle spacingStyles({
			{ImGuiStyleVar_ItemSpacing, ImVec2(4, 1)},
			{ImGuiStyleVar_FramePadding, ImVec2(6, 3)}
		});
		
		if (!BeginPanel("Scene Hierarchy")) {
			EndPanel();
			return;
		}

		RenderToolbar();
		RenderSearchBar();
		
		Separator();
		AddSpacing(SpacingValues::XS);
		
		RenderEntityList();
		RenderContextMenu();

		EndPanel();
	}

	void SceneHierarchyPanel::SetSelectedEntity(Entity entity) {
		SelectEntity(entity);
	}

	void SceneHierarchyPanel::RenderToolbar() {
		using namespace UI;
		
		ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(8, 4));
		ScopedColor buttonColors({
			{ImGuiCol_Button, Color(0.167f, 0.167f, 0.167f, 1.0f)},
			{ImGuiCol_ButtonHovered, Color(0.227f, 0.227f, 0.227f, 1.0f)},
			{ImGuiCol_ButtonActive, Color(0.851f, 0.510f, 0.169f, 0.6f)}
		});
		
		if (Button("+ Create")) {
			m_ShowCreateMenu = true;
			OpenPopup("CreateEntityPopup");
		}
		
		if (BeginPopup("CreateEntityPopup")) {
			if (MenuItem("Empty Entity"))
				m_Context->CreateEntity("Empty Entity");

			if (MenuItem("Particle System")) {
				Entity particles = m_Context->CreateEntity("Particle System");
				particles.AddComponent<ParticleSystemComponent>();
				SelectEntity(particles, true);
			}

			// Folder-like group (Blender collection): wraps the selection, or starts empty
			if (ImGui::MenuItem(m_SelectedEntities.empty() ? "Group" : "Group Selection", "Ctrl+G")) {
				if (!m_SelectedEntities.empty()) GroupSelection();
				else {
					Entity group = m_Context->CreateEntity("Group");
					group.AddComponent<GroupComponent>();
					SelectEntity(group, true);
				}
			}

			Separator();
			
			if (BeginMenu("3D Object")) {
				if (MenuItem("Cube"))
					CreateMeshEntity("Cube", ModelType::Cube);
				if (MenuItem("Sphere"))
					CreateMeshEntity("Sphere", ModelType::Sphere);
				if (MenuItem("Plane"))
					CreateMeshEntity("Plane", ModelType::Plane);
				if (MenuItem("Cylinder"))
					CreateMeshEntity("Cylinder", ModelType::Cylinder);
				EndMenu();
			}
			
			if (BeginMenu("2D Object")) {
				if (MenuItem("Sprite"))
					CreateEntityWithComponent<SpriteRendererComponent>("Sprite");
				if (MenuItem("Circle"))
					CreateEntityWithComponent<CircleRendererComponent>("Circle");
				EndMenu();
			}
			
			if (BeginMenu("Light")) {
				if (MenuItem("Directional Light")) {
					Entity entity = m_Context->CreateEntity("Directional Light");
					auto& light = entity.AddComponent<LightComponent>();
					light.SetType(LightType::Directional);
					SelectEntity(entity);
				}
				if (MenuItem("Point Light")) {
					Entity entity = m_Context->CreateEntity("Point Light");
					auto& light = entity.AddComponent<LightComponent>();
					light.SetType(LightType::Point);
					SelectEntity(entity);
				}
				if (MenuItem("Spot Light")) {
					Entity entity = m_Context->CreateEntity("Spot Light");
					auto& light = entity.AddComponent<LightComponent>();
					light.SetType(LightType::Spot);
					SelectEntity(entity);
				}
				EndMenu();
			}
			
			if (MenuItem("Camera"))
				CreateEntityWithComponent<CameraComponent>("Camera");
			
			EndPopup();
		}
		
		SameLine();
		
		// Entity count display
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 80);
		{
			ScopedColor textColor(ImGuiCol_Text, m_Style.TextMuted);
			if (m_SelectedEntities.size() > 0) {
				Text("%d/%d", (int)m_SelectedEntities.size(), m_TotalEntities);
			} else {
				Text("%d", m_TotalEntities);
			}
		}
	}
	
	void SceneHierarchyPanel::RenderSearchBar() {
		using namespace UI;
		
		ScopedColor frameColors({
			{ImGuiCol_FrameBg, Color(0.117f, 0.117f, 0.117f, 1.0f).ToImVec4()},
			{ImGuiCol_FrameBgHovered, Color(0.157f, 0.157f, 0.157f, 1.0f).ToImVec4()},
			{ImGuiCol_FrameBgActive, Color(0.851f, 0.510f, 0.169f, 0.3f).ToImVec4()}
		});
		
		ImGui::SetNextItemWidth(-1);
		InputText("##HierarchySearch", m_SearchFilter, sizeof(m_SearchFilter), "Search entities...");
	}
	
	void SceneHierarchyPanel::RenderEntityList() {
		using namespace UI;
		
		if (!m_Context) return;
		
		m_EntityIndexCounter = 0;
		m_TotalEntities = 0;
		m_VisibleEntities = 0;
		m_DrawOrderBuilding.clear();

		auto rootEntities = GetSortedRootEntities();
		m_TotalEntities = (int)m_Context->m_Registry.view<TagComponent>().size();
		
		if (BeginChild("##EntityList", Size(0, 0), false)) {
			for (auto entity : rootEntities) {
				DrawEntityNode(entity, 0);
			}

			// Drop target for root level (unparent)
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_NODE")) {
					Entity droppedEntity = *(Entity*)payload->Data;
					UnparentEntity(droppedEntity);
				}
				
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
		}

		HandleHierarchyShortcuts();

		ImGui::EndChild();
		m_DrawOrder = m_DrawOrderBuilding;
	}
	
	void SceneHierarchyPanel::RenderContextMenu() {
		using namespace UI;
		
		if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
			SeparatorText("Create Entity");
			
			if (MenuItem("Empty Entity"))
				m_Context->CreateEntity("Empty Entity");

			if (ImGui::MenuItem("Group")) {
				if (!m_SelectedEntities.empty()) GroupSelection();
				else {
					Entity group = m_Context->CreateEntity("Group");
					group.AddComponent<GroupComponent>();
					SelectEntity(group, true);
				}
			}
			
			Separator();
			
			if (MenuItem("Camera"))
				CreateEntityWithComponent<CameraComponent>("Camera");
			
			if (MenuItem("Light"))
				CreateEntityWithComponent<LightComponent>("Light");
			
			if (MenuItem("Sprite"))
				CreateEntityWithComponent<SpriteRendererComponent>("Sprite");
			
			if (MenuItem("3D Object"))
				CreateMeshEntity("Cube", ModelType::Cube);
			
			EndPopup();
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

	// ============================================================================
	// TYPE ICONS (vector, Blender outliner style: readable without knowing the colors)
	// ============================================================================

	namespace {
		enum class EntityGlyph { Empty, Group, Camera, Sun, PointLight, SpotLight, Mesh, SkinnedMesh, Sprite, Particles, Environment };

		EntityGlyph GlyphOf(Entity entity) {
			if (entity.HasComponent<GroupComponent>()) return EntityGlyph::Group;
			if (entity.HasComponent<CameraComponent>()) return EntityGlyph::Camera;
			if (entity.HasComponent<LightComponent>()) {
				auto& light = entity.GetComponent<LightComponent>();
				switch (light.GetType()) {
					case LightType::Directional: return EntityGlyph::Sun;
					case LightType::Spot: return EntityGlyph::SpotLight;
					default: return EntityGlyph::PointLight;
				}
			}
			if (entity.HasComponent<AnimatorComponent>()) return EntityGlyph::SkinnedMesh;
			if (entity.HasComponent<MeshComponent>()) return EntityGlyph::Mesh;
			if (entity.HasComponent<ParticleSystemComponent>()) return EntityGlyph::Particles;
			if (entity.HasComponent<SpriteRendererComponent>() || entity.HasComponent<CircleRendererComponent>()) return EntityGlyph::Sprite;
			if (entity.HasComponent<EnvironmentComponent>()) return EntityGlyph::Environment;
			return EntityGlyph::Empty;
		}

		ImU32 GlyphColor(EntityGlyph g) {
			switch (g) {
				case EntityGlyph::Group:       return IM_COL32(232, 178, 72, 255);
				case EntityGlyph::Camera:      return IM_COL32(150, 190, 255, 255);
				case EntityGlyph::Sun:
				case EntityGlyph::PointLight:
				case EntityGlyph::SpotLight:   return IM_COL32(255, 214, 90, 255);
				case EntityGlyph::Mesh:        return IM_COL32(255, 150, 60, 255);
				case EntityGlyph::SkinnedMesh: return IM_COL32(190, 130, 255, 255);
				case EntityGlyph::Sprite:      return IM_COL32(110, 210, 150, 255);
				case EntityGlyph::Particles:   return IM_COL32(240, 120, 220, 255);
				case EntityGlyph::Environment: return IM_COL32(120, 200, 230, 255);
				default:                       return IM_COL32(170, 170, 170, 255);
			}
		}

		void DrawGlyph(ImDrawList* dl, EntityGlyph g, ImVec2 p, float s) {
			const ImU32 c = GlyphColor(g);
			const float t = std::max(1.2f, s * 0.09f);   // stroke
			auto P = [&](float x, float y) { return ImVec2(p.x + x * s, p.y + y * s); };
			switch (g) {
				case EntityGlyph::Group:   // folder
					dl->AddRectFilled(P(0.05f, 0.22f), P(0.45f, 0.38f), c, 1.5f);
					dl->AddRectFilled(P(0.05f, 0.32f), P(0.95f, 0.85f), c, 2.0f);
					break;
				case EntityGlyph::Camera:  // body + lens
					dl->AddRectFilled(P(0.05f, 0.30f), P(0.65f, 0.78f), c, 2.0f);
					dl->AddTriangleFilled(P(0.62f, 0.54f), P(0.97f, 0.28f), P(0.97f, 0.80f), c);
					break;
				case EntityGlyph::Sun: {   // disc + rays
					const ImVec2 o = P(0.5f, 0.5f);
					dl->AddCircleFilled(o, s * 0.2f, c);
					for (int i = 0; i < 8; i++) {
						const float a = i * 3.14159265f / 4.0f;
						dl->AddLine(ImVec2(o.x + std::cos(a) * s * 0.31f, o.y + std::sin(a) * s * 0.31f),
							ImVec2(o.x + std::cos(a) * s * 0.46f, o.y + std::sin(a) * s * 0.46f), c, t);
					}
					break;
				}
				case EntityGlyph::PointLight:   // bulb
					dl->AddCircleFilled(P(0.5f, 0.40f), s * 0.30f, c);
					dl->AddRectFilled(P(0.36f, 0.66f), P(0.64f, 0.90f), c, 1.5f);
					break;
				case EntityGlyph::SpotLight:    // cone
					dl->AddRectFilled(P(0.38f, 0.08f), P(0.62f, 0.28f), c, 1.5f);
					dl->AddTriangleFilled(P(0.5f, 0.22f), P(0.10f, 0.92f), P(0.90f, 0.92f), (c & 0x00FFFFFF) | 0x99000000);
					break;
				case EntityGlyph::Mesh: {  // cube (isometric)
					const ImVec2 top = P(0.5f, 0.06f), l = P(0.08f, 0.28f), r = P(0.92f, 0.28f), mid = P(0.5f, 0.50f);
					const ImVec2 bl = P(0.08f, 0.74f), br = P(0.92f, 0.74f), bot = P(0.5f, 0.96f);
					dl->AddQuadFilled(top, r, mid, l, c);
					dl->AddQuadFilled(l, mid, bot, bl, (c & 0x00FFFFFF) | 0xC0000000);
					dl->AddQuadFilled(mid, r, br, bot, (c & 0x00FFFFFF) | 0x80000000);
					break;
				}
				case EntityGlyph::SkinnedMesh: {   // bone
					dl->AddLine(P(0.28f, 0.72f), P(0.72f, 0.28f), c, s * 0.18f);
					dl->AddCircleFilled(P(0.20f, 0.70f), s * 0.13f, c); dl->AddCircleFilled(P(0.30f, 0.82f), s * 0.13f, c);
					dl->AddCircleFilled(P(0.70f, 0.18f), s * 0.13f, c); dl->AddCircleFilled(P(0.82f, 0.30f), s * 0.13f, c);
					break;
				}
				case EntityGlyph::Sprite:  // picture
					dl->AddRect(P(0.06f, 0.14f), P(0.94f, 0.86f), c, 1.5f, 0, t);
					dl->AddTriangleFilled(P(0.16f, 0.78f), P(0.42f, 0.44f), P(0.66f, 0.78f), c);
					dl->AddCircleFilled(P(0.70f, 0.36f), s * 0.09f, c);
					break;
				case EntityGlyph::Particles:
					dl->AddCircleFilled(P(0.25f, 0.70f), s * 0.13f, c);
					dl->AddCircleFilled(P(0.55f, 0.35f), s * 0.11f, c);
					dl->AddCircleFilled(P(0.80f, 0.72f), s * 0.09f, c);
					dl->AddCircleFilled(P(0.30f, 0.22f), s * 0.07f, c);
					break;
				case EntityGlyph::Environment:   // globe
					dl->AddCircle(P(0.5f, 0.5f), s * 0.42f, c, 0, t);
					dl->AddLine(P(0.08f, 0.5f), P(0.92f, 0.5f), c, t);
					dl->AddBezierQuadratic(P(0.5f, 0.08f), P(0.22f, 0.5f), P(0.5f, 0.92f), c, t);
					dl->AddBezierQuadratic(P(0.5f, 0.08f), P(0.78f, 0.5f), P(0.5f, 0.92f), c, t);
					break;
				default:   // empty: axes
					dl->AddLine(P(0.5f, 0.08f), P(0.5f, 0.92f), c, t);
					dl->AddLine(P(0.08f, 0.5f), P(0.92f, 0.5f), c, t);
					dl->AddLine(P(0.22f, 0.78f), P(0.78f, 0.22f), c, t);
					break;
			}
		}
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
		m_DrawOrderBuilding.push_back(entity);

		// Check if has children (relationship or submeshes)
		bool hasChildren = false;
		if (entity.HasComponent<RelationshipComponent>()) {
			hasChildren = entity.GetComponent<RelationshipComponent>().HasChildren();
		}
		
		// Check if has submeshes to display (multiple submeshes = expandable)
		bool hasSubmeshes = false;
		if (!hasChildren && entity.HasComponent<MeshComponent>()) {
			auto& meshComp = entity.GetComponent<MeshComponent>();
			if (meshComp.MeshModel && meshComp.MeshModel->GetMeshes().size() > 1) {
				hasSubmeshes = true;
			}
		}
		
		bool isExpandable = hasChildren || hasSubmeshes;

		// Prepare drawing
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 cursorScreenPos = ImGui::GetCursorScreenPos();
		float itemWidth = ImGui::GetContentRegionAvail().x;
		float itemHeight = m_Style.ItemHeight;
		
		Color bgColor = (m_EntityIndexCounter % 2 == 0) ? m_Style.ItemEven : m_Style.ItemOdd;
		m_EntityIndexCounter++;
		
		bool isSelected = IsEntitySelected(entity);
		bool isActive = (entity == m_LastSelectedEntity);
		bool isRenaming = (m_IsRenaming && m_EntityBeingRenamed == entity);

		ImVec2 itemMin = cursorScreenPos;
		ImVec2 itemMax = ImVec2(cursorScreenPos.x + itemWidth, cursorScreenPos.y + itemHeight);
		
		// Draw selection/background
		if (isSelected) {
			if (isActive && m_SelectedEntities.size() > 1) {
				drawList->AddRectFilled(itemMin, itemMax, m_Style.ItemActive.ToImU32());
				drawList->AddRectFilled(itemMin, ImVec2(itemMin.x + m_Style.TypeIndicatorWidth, itemMax.y), m_Style.ItemActiveBorder.ToImU32());
			}
			else if (m_SelectedEntities.size() > 1) {
				drawList->AddRectFilled(itemMin, itemMax, m_Style.ItemSelectedMulti.ToImU32());
				drawList->AddRectFilled(itemMin, ImVec2(itemMin.x + m_Style.TypeIndicatorWidth, itemMax.y), m_Style.ItemSelectedMultiBorder.ToImU32());
			}
			else {
				drawList->AddRectFilled(itemMin, itemMax, m_Style.ItemSelected.ToImU32());
				drawList->AddRectFilled(itemMin, ImVec2(itemMin.x + m_Style.TypeIndicatorWidth, itemMax.y), m_Style.ItemSelectedBorder.ToImU32());
			}
		} else {
			drawList->AddRectFilled(itemMin, itemMax, bgColor.ToImU32());
		}

		// Hover highlight goes UNDER the label (drawing it after the tree node covered the text)
		if (!isRenaming && !isSelected && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem) &&
			ImGui::IsMouseHoveringRect(itemMin, itemMax)) {
			drawList->AddRectFilled(itemMin, itemMax, m_Style.ItemHover.ToImU32());
		}

		// Type indicator bar on left
		if (!isSelected) {
			Color typeColor = GetEntityTypeColor(entity);
			drawList->AddRectFilled(itemMin, ImVec2(itemMin.x + m_Style.TypeIndicatorWidth, itemMax.y), typeColor.ToImU32());
		}

		ScopedID entityScope((void*)(uint64_t)(uint32_t)entity);
		
		float indentOffset = depth * m_Style.IndentSpacing + m_Style.TypeIndicatorWidth + 4.0f;
		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + indentOffset);
		
		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_FramePadding;
		if (!isExpandable)
			flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
		if (isSelected)
			flags |= ImGuiTreeNodeFlags_Selected;

		// Type icon (vector glyph)
		ImVec2 iconPos = ImVec2(cursorScreenPos.x + indentOffset, cursorScreenPos.y + (itemHeight - m_Style.IconSize) * 0.5f);
		DrawGlyph(drawList, GlyphOf(entity), iconPos, m_Style.IconSize);

		ImGui::SetCursorPosX(ImGui::GetCursorPosX() + m_Style.IconSize + 4.0f);
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (itemHeight - ImGui::GetFrameHeight()) * 0.5f);

		bool opened = false;
		
		if (isRenaming) {
			ImGui::SetKeyboardFocusHere();
			ScopedColor frameColor(ImGuiCol_FrameBg, Color(0.851f, 0.510f, 0.169f, 0.2f).ToImVec4());
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
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() - (itemHeight - ImGui::GetFrameHeight()) * 0.5f);
			
			ScopedColor treeColors({
				{ImGuiCol_Header, Color(0, 0, 0, 0)},
				{ImGuiCol_HeaderHovered, Color(0, 0, 0, 0)},
				{ImGuiCol_HeaderActive, Color(0, 0, 0, 0)}
			});
			
			opened = ImGui::TreeNodeEx(tag.c_str(), flags);
		}


		// Click handling
		if (ImGui::IsItemClicked() && !isRenaming) {
			ImGuiIO& io = ImGui::GetIO();
			if (io.KeyCtrl) {
				ToggleEntitySelection(entity);
			}
			else if (io.KeyShift) {
				// Shift+Click: select the whole range from the last clicked entity
				if (m_LastSelectedEntity && m_LastSelectedEntity != entity) {
					SelectRange(m_LastSelectedEntity, entity);
				}
				else if (!IsEntitySelected(entity))
					AddEntityToSelection(entity);
			}
			else {
				if (IsEntitySelected(entity) && m_SelectedEntities.size() > 1)
					SetActiveEntityInSelection(entity);
				else
					SelectEntity(entity, true);
			}
		}

		// Drag & Drop (low-level ImGui - no abstraction available)
		if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
			ImGui::SetDragDropPayload("ENTITY_NODE", &entity, sizeof(Entity));
			Text("%s", tag.c_str());
			m_DraggedEntity = entity;
			ImGui::EndDragDropSource();
		}

		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("ENTITY_NODE")) {
				Entity droppedEntity = *(Entity*)payload->Data;
				if (droppedEntity != entity) {
					Entity currentParent = m_Context->GetParent(droppedEntity);
					if (currentParent == entity)
						UnparentEntity(droppedEntity);
					else if (!m_Context->IsAncestorOf(droppedEntity, entity))
						SetEntityParent(droppedEntity, entity);
				}
			}
			ImGui::EndDragDropTarget();
		}

		// Context menu
		RenderEntityContextMenu(entity);
		
		// Advance cursor
		float advanceAmount = itemHeight - ImGui::GetFrameHeight();
		if (advanceAmount > 0) {
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + advanceAmount);
			Dummy(Size(0, 0));
		}

		// Draw children recursively
		if (opened && hasChildren) {
			auto children = m_Context->GetChildren(entity);
			for (auto child : children)
				DrawEntityNode(child, depth + 1);
			TreePop();
		}
		
		// Draw submeshes
		if (opened && hasSubmeshes && !hasChildren) {
			auto& meshComp = entity.GetComponent<MeshComponent>();
			const auto& meshes = meshComp.MeshModel->GetMeshes();
			const auto& materialData = meshComp.MeshModel->GetMaterialData();
			const auto& matIndices = meshComp.MeshModel->GetSubmeshMaterialIndices();
			
			for (size_t si = 0; si < meshes.size(); si++) {
				float subIndent = (depth + 1) * m_Style.IndentSpacing + m_Style.TypeIndicatorWidth + 4.0f;
				
				ImVec2 subCursor = ImGui::GetCursorScreenPos();
				float subHeight = 20.0f;
				float subWidth = ImGui::GetContentRegionAvail().x;
				
				Color subBg = (si % 2 == 0) 
					? Color(0.075f, 0.095f, 0.120f, 1.0f)
					: Color(0.085f, 0.105f, 0.135f, 1.0f);
				drawList->AddRectFilled(subCursor, ImVec2(subCursor.x + subWidth, subCursor.y + subHeight), subBg.ToImU32());
				drawList->AddRectFilled(subCursor, ImVec2(subCursor.x + 2.0f, subCursor.y + subHeight), Color(0.30f, 0.60f, 0.35f, 0.6f).ToImU32());
				
				std::string subLabel;
				if (si < matIndices.size() && matIndices[si] < materialData.size()) {
					const auto& matName = materialData[matIndices[si]].Name;
					subLabel = "Submesh " + std::to_string(si) + " [" + matName + "]";
				} else {
					subLabel = "Submesh " + std::to_string(si);
				}
				
				uint32_t verts = (uint32_t)meshes[si]->GetVertices().size();
				uint32_t tris = (uint32_t)meshes[si]->GetIndices().size() / 3;
				
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + subIndent);
				
				{
					ScopedColor textColor(ImGuiCol_Text, m_Style.TextMuted);
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + (subHeight - ImGui::GetTextLineHeight()) * 0.5f);
					Text("%s", subLabel.c_str());
				}
				
				{
					std::string statsStr = std::to_string(verts) + "v / " + std::to_string(tris) + "t";
					float statsWidth = ImGui::CalcTextSize(statsStr.c_str()).x;
					ScopedColor statsColor(ImGuiCol_Text, Color(0.45f, 0.50f, 0.55f, 1.0f));
					SameLine(ImGui::GetWindowWidth() - statsWidth - 12.0f);
					Text("%s", statsStr.c_str());
				}
				
				float remaining = subHeight - ImGui::GetTextLineHeightWithSpacing();
				if (remaining > 0)
					ImGui::SetCursorPosY(ImGui::GetCursorPosY() + remaining);
			}
			
			TreePop();
		}
	}
	
	void SceneHierarchyPanel::RenderEntityContextMenu(Entity entity) {
		using namespace UI;
		
		auto& tag = entity.GetComponent<TagComponent>().Tag;
		bool entityDeleted = false;
		
		if (ImGui::BeginPopupContextItem()) {
			{
				ScopedColor textColor(ImGuiCol_Text, m_Style.TextMuted);
				Text("%s", tag.c_str());
			}
			Separator();
			
			if (MenuItem("Rename", "F2"))
				RenameEntity(entity);
			
			if (MenuItem("Duplicate", "Ctrl+D"))
				DuplicateEntity(entity);
			
			Separator();
			
			if (MenuItem("Create Prefab"))
				CreatePrefabFromEntity(entity);
			
			Separator();
			
			Entity parent = m_Context->GetParent(entity);
			if (parent) {
				if (MenuItem("Unparent"))
					UnparentEntity(entity);
			}
			
			if (MenuItem("Create Child")) {
				Entity child = m_Context->CreateEntity("Child");
				SetEntityParent(child, entity);
				SelectEntity(child);
			}

			ImGui::Separator();

			// Groups (Blender-style collections)
			if (!IsEntitySelected(entity)) SelectEntity(entity, true);
			if (ImGui::MenuItem("Group Selection", "Ctrl+G")) {
				GroupSelection();
			}
			if (entity.HasComponent<GroupComponent>() && ImGui::MenuItem("Ungroup", "Alt+G")) {
				UngroupEntity(entity);
				ImGui::EndPopup();
				return;
			}
			
			Separator();
			
			{
				ScopedColor deleteColor(ImGuiCol_Text, Color(1.0f, 0.3f, 0.3f, 1.0f));
				if (MenuItem("Delete", "Del"))
					entityDeleted = true;
			}
			
			Separator();
			
			{
				ScopedColor infoColor(ImGuiCol_Text, m_Style.TextMuted);
				Text("UUID: %llu", (uint64_t)entity.GetComponent<IDComponent>().ID);
				if (parent)
					Text("Parent: %s", parent.GetComponent<TagComponent>().Tag.c_str());
			}

			EndPopup();
		}

		if (entityDeleted) {
			auto children = m_Context->GetChildren(entity);
			for (auto child : children)
				UnparentEntity(child);
			
			m_Context->DestroyEntity(entity);
			if (m_SelectionContext == entity)
				ClearSelection();
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
	
	// ============================================================================
	// GROUPS & RANGE SELECTION
	// ============================================================================

	Entity SceneHierarchyPanel::GroupSelection(const std::string& name) {
		if (!m_Context || m_SelectedEntities.empty()) return {};

		// Only group top-most selected entities (children move with their parents)
		std::vector<Entity> roots;
		for (Entity e : m_SelectedEntities) {
			bool hasSelectedAncestor = false;
			for (Entity other : m_SelectedEntities) {
				if (other != e && m_Context->IsAncestorOf(other, e)) { hasSelectedAncestor = true; break; }
			}
			if (!hasSelectedAncestor) roots.push_back(e);
		}
		if (roots.empty()) return {};

		// Keep the group where its members live if they share a parent
		Entity commonParent = m_Context->GetParent(roots.front());
		for (Entity e : roots) {
			if (m_Context->GetParent(e) != commonParent) { commonParent = {}; break; }
		}

		Entity group = m_Context->CreateEntity(name);
		group.AddComponent<GroupComponent>();
		if (commonParent) m_Context->SetParent(group, commonParent);
		for (Entity e : roots) m_Context->SetParent(e, group);   // keeps world transforms

		SelectEntity(group, true);
		return group;
	}

	void SceneHierarchyPanel::UngroupEntity(Entity group) {
		if (!m_Context || !group || !group.HasComponent<GroupComponent>()) return;
		Entity parent = m_Context->GetParent(group);
		auto children = m_Context->GetChildren(group);
		ClearSelection();
		for (Entity child : children) {
			if (parent) m_Context->SetParent(child, parent);
			else m_Context->RemoveParent(child);
			AddEntityToSelection(child);
		}
		m_Context->DestroyEntity(group);
	}

	void SceneHierarchyPanel::SelectRange(Entity from, Entity to) {
		auto a = std::find(m_DrawOrder.begin(), m_DrawOrder.end(), from);
		auto b = std::find(m_DrawOrder.begin(), m_DrawOrder.end(), to);
		if (a == m_DrawOrder.end() || b == m_DrawOrder.end()) { AddEntityToSelection(to); return; }
		if (a > b) std::swap(a, b);
		for (auto it = a; it <= b; ++it) {
			if (!IsEntitySelected(*it)) m_SelectedEntities.insert(*it);
		}
		m_SelectionContext = to;
		m_LastSelectedEntity = to;
	}

	void SceneHierarchyPanel::HandleHierarchyShortcuts() {
		if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || ImGui::GetIO().WantTextInput) return;
		const ImGuiIO& io = ImGui::GetIO();
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_G, false)) GroupSelection();
		if (io.KeyAlt && ImGui::IsKeyPressed(ImGuiKey_G, false)) {
			Entity active = m_LastSelectedEntity;
			if (active && active.HasComponent<GroupComponent>()) UngroupEntity(active);
		}
		if (io.KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_A, false)) SelectAll();
	}

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