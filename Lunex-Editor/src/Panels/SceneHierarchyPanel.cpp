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
	}

	void SceneHierarchyPanel::OnImGuiRender() {
		ImGui::Begin("Scene Hierarchy");

		if (m_Context) {
			m_Context->m_Registry.view<entt::entity>().each([&](auto entityID) {
				Entity entity{ entityID , m_Context.get() };
				DrawEntityNode(entity);
				});

			if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
				m_SelectionContext = {};

			if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
				if (ImGui::MenuItem("Create Empty Entity"))
					m_Context->CreateEntity("Empty Entity");
				ImGui::EndPopup();
			}
		}
		ImGui::End();
	}

	void SceneHierarchyPanel::SetSelectedEntity(Entity entity) {
		m_SelectionContext = entity;
	}

	void SceneHierarchyPanel::DrawEntityNode(Entity entity) {
		auto& tag = entity.GetComponent<TagComponent>().Tag;

		ImGuiTreeNodeFlags flags = ((m_SelectionContext == entity) ? ImGuiTreeNodeFlags_Selected : 0) | ImGuiTreeNodeFlags_OpenOnArrow;
		flags |= ImGuiTreeNodeFlags_SpanAvailWidth;

		Ref<Texture2D> icon = entity.HasComponent<CameraComponent>() ? m_CameraIcon : m_EntityIcon;

		ImVec2 cursorPos = ImGui::GetCursorPos();

		if (icon) {
			float iconSize = ImGui::GetFrameHeight();

			ImGui::Image(
				(void*)(intptr_t)icon->GetRendererID(),
				ImVec2(iconSize, iconSize),
				ImVec2(0, 1),
				ImVec2(1, 0)
			);

			ImGui::SameLine();
			ImGui::SetCursorPosY(cursorPos.y);
		}

		bool opened = ImGui::TreeNodeEx((void*)(uint64_t)(uint32_t)entity, flags, tag.c_str());

		if (ImGui::IsItemClicked()) {
			m_SelectionContext = entity;
		}

		bool entityDeleted = false;
		if (ImGui::BeginPopupContextItem()) {
			if (ImGui::MenuItem("Delete Entity"))
				entityDeleted = true;

			ImGui::EndPopup();
		}

		if (opened) {
			ImGuiTreeNodeFlags childFlags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
			bool childOpened = ImGui::TreeNodeEx((void*)9817239, childFlags, tag.c_str());
			if (childOpened)
				ImGui::TreePop();
			ImGui::TreePop();
		}

		if (entityDeleted) {
			m_Context->DestroyEntity(entity);
			if (m_SelectionContext == entity)
				m_SelectionContext = {};
		}
	}
}