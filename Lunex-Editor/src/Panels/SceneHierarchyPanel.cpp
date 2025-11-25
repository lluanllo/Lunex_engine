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

		// Cargar iconos para la jerarquía
		m_CameraIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/CameraIcon.png");
		m_EntityIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/EntityIcon.png");

		if (!m_CameraIcon)
			LNX_LOG_ERROR("Failed to load Camera Icon!");
		if (!m_EntityIcon)
			LNX_LOG_ERROR("Failed to load Entity Icon!");
	}

	void SceneHierarchyPanel::SetContext(const Ref<Scene>& context) {
		m_Context = context;
		m_SelectionContext = {};
		m_EntityIndexCounter = 0; // Reset counter
	}

	void SceneHierarchyPanel::OnImGuiRender() {
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
			m_EntityIndexCounter = 0; // Reset counter al inicio
			
			// BeginChild para el área scrollable
			ImGui::BeginChild("##EntityList", ImVec2(0, 0), false);
			
			m_Context->m_Registry.view<entt::entity>().each([&](auto entityID) {
				Entity entity{ entityID , m_Context.get() };
				DrawEntityNode(entity);
				m_EntityIndexCounter++;
				});

			// Click en área vacía para deseleccionar
			if (ImGui::IsMouseClicked(0) && ImGui::IsWindowHovered())
				m_SelectionContext = {};

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
				
				ImGui::EndPopup();
			}
			
			ImGui::EndChild();
		}
		
		ImGui::End();
		
		ImGui::PopStyleVar(2);
		ImGui::PopStyleColor(7);
	}

	void SceneHierarchyPanel::SetSelectedEntity(Entity entity) {
		m_SelectionContext = entity;
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
				return; // No coincide con la búsqueda, saltar
			}
		}

		// ===== COLORES ALTERNOS (BLENDER STYLE) =====
		ImU32 bgColor;
		if (m_EntityIndexCounter % 2 == 0) {
			bgColor = IM_COL32(28, 28, 30, 255);  // Par - Más oscuro
		} else {
			bgColor = IM_COL32(32, 32, 34, 255);  // Impar - Más claro
		}

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
		flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen; // Sin hijos por ahora
		flags |= ImGuiTreeNodeFlags_FramePadding;
		
		if (m_SelectionContext == entity) {
			flags |= ImGuiTreeNodeFlags_Selected;
		}

		// Determinar icono según componentes
		Ref<Texture2D> icon = m_EntityIcon;
		
		if (entity.HasComponent<CameraComponent>()) {
			icon = m_CameraIcon;
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

		// ===== ICONO + TREE NODE =====
		ImVec2 cursorPos = ImGui::GetCursorPos();
		
		// Ajustar padding vertical para centrar
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

		// Tree node con texto
		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag.c_str());

		// ===== HOVER EFFECT =====
		if (ImGui::IsItemHovered()) {
			ImU32 hoverColor = IM_COL32(50, 50, 55, 180);
			drawList->AddRectFilled(
				cursorScreenPos,
				ImVec2(cursorScreenPos.x + itemSize.x, cursorScreenPos.y + itemSize.y),
				hoverColor
			);
		}

		// ===== INTERACTIONS =====
		if (ImGui::IsItemClicked()) {
			m_SelectionContext = entity;
		}

		// Drag & Drop Source
		if (ImGui::BeginDragDropSource()) {
			ImGui::SetDragDropPayload("ENTITY_NODE", &entity, sizeof(Entity));
			ImGui::Text("📦 %s", tag.c_str());
			ImGui::EndDragDropSource();
		}

		// Context menu
		bool entityDeleted = false;
		if (ImGui::BeginPopupContextItem()) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
			ImGui::Text("📦 %s", tag.c_str());
			ImGui::PopStyleColor();
			ImGui::Separator();
			
			if (ImGui::MenuItem("🔄 Duplicate")) {
				// TODO: Implementar duplicación
				LNX_LOG_INFO("Duplicate entity: {0}", tag);
			}
			
			if (ImGui::MenuItem("✏️ Rename")) {
				// TODO: Implementar rename inline
				LNX_LOG_INFO("Rename entity: {0}", tag);
			}
			
			ImGui::Separator();
			
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.3f, 0.3f, 1.0f));
			if (ImGui::MenuItem("🗑️ Delete"))
			entityDeleted = true;
			ImGui::PopStyleColor();

			ImGui::EndPopup();
		}

		// Si el nodo estaba abierto (aunque no tenemos hijos ahora)
		if (opened) {
			// Aquí irían los hijos si implementamos jerarquía parent-child
			// ImGui::TreePop(); // NO llamar TreePop con NoTreePushOnOpen
		}

		// Eliminar entidad
		if (entityDeleted) {
			m_Context->DestroyEntity(entity);
			if (m_SelectionContext == entity)
				m_SelectionContext = {};
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
		
		// Popup para crear entidades
		if (ImGui::BeginPopup("CreateEntityPopup")) {
			ImGui::SeparatorText("Create Entity");
			
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
		
		ImGui::SameLine();
		
		// Search bar funcional
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.15f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.19f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.26f, 0.59f, 0.98f, 0.3f));
		ImGui::SetNextItemWidth(200);
		ImGui::InputTextWithHint("##Search", "🔍 Search...", m_SearchFilter, 256);
		ImGui::PopStyleColor(3);
		
		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
	}
}