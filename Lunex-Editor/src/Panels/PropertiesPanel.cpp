#include "PropertiesPanel.h"

#include <Lunex.h>

#include "Log/Log.h"
#include "Renderer/Renderer3D/BasicShapes.h"
#include "Renderer/MaterialSystem/MaterialInstance.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>
#include <filesystem>
#include <string>
#include <cstring>

namespace Lunex {
	extern const std::filesystem::path g_AssetPath;

	PropertiesPanel::PropertiesPanel() {
		// Load component icons
		m_CameraIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/CameraIcon.png");
		m_SpriteIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/MeshIcon.png");
		m_PhysicsIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/EntityIcon.png");
		
		// Debug
		if (!m_CameraIcon) LNX_LOG_ERROR("Failed to load Camera Icon!");
		if (!m_SpriteIcon) LNX_LOG_ERROR("Failed to load Sprite Icon!");
	}

	void PropertiesPanel::OnImGuiRender() {
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("Properties");
		ImGui::PopStyleVar();
		
		if (m_SelectedEntity) {
			// Header Section with dark background
			ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.08f, 0.08f, 0.08f, 1.0f));
			ImGui::BeginChild("PropertiesHeader", ImVec2(0, 50), true, ImGuiWindowFlags_NoScrollbar);
			ImGui::PopStyleColor();

			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 8);
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 8);

			// Entity name with styled input
			if (m_SelectedEntity.HasComponent<TagComponent>()) {
				auto& tag = m_SelectedEntity.GetComponent<TagComponent>().Tag;
				
				char buffer[256];
				memset(buffer, 0, sizeof(buffer));
				strcpy_s(buffer, sizeof(buffer), tag.c_str());
				
				ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.50f, 0.10f, 1.0f));
				ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 120);
				
				if (ImGui::InputText("##EntityName", buffer, sizeof(buffer))) {
					tag = std::string(buffer);
				}
				
				ImGui::PopStyleColor(2);
				ImGui::PopStyleVar();
			}

			ImGui::SameLine();

			// Add Component button
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.90f, 0.50f, 0.10f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.90f, 0.50f, 0.10f, 0.8f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.90f, 0.50f, 0.10f, 1.0f));
			
			if (ImGui::Button("+ Add", ImVec2(100, 30))) {
				ImGui::OpenPopup("AddComponent");
			}
			
			ImGui::PopStyleColor(3);

			// Add Component popup
			if (ImGui::BeginPopup("AddComponent")) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.50f, 0.10f, 1.0f));
				ImGui::Text("Add Component");
				ImGui::PopStyleColor();
				ImGui::Separator();

				DisplayAddComponentEntry<CameraComponent>("Camera");
				DisplayAddComponentEntry<SpriteRendererComponent>("Sprite Renderer");
				DisplayAddComponentEntry<CircleRendererComponent>("Circle Renderer");
				ImGui::Separator();
				DisplayAddComponentEntry<Rigidbody2DComponent>("Rigidbody 2D");
				DisplayAddComponentEntry<BoxCollider2DComponent>("Box Collider 2D");
				DisplayAddComponentEntry<CircleCollider2DComponent>("Circle Collider 2D");
				ImGui::Separator();
				DisplayAddComponentEntry<MeshComponent>("Mesh");
				DisplayAddComponentEntry<MaterialComponent>("Material");

				ImGui::EndPopup();
			}

			ImGui::EndChild();

			// Components Section
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 4));
			ImGui::BeginChild("ComponentsRegion", ImVec2(0, 0), false);
			
			DrawComponents(m_SelectedEntity);
			
			ImGui::EndChild();
			ImGui::PopStyleVar();
		}
		else {
			// No entity selected message
			ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.4f);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
			
			ImVec2 textSize = ImGui::CalcTextSize("No entity selected");
			ImGui::SetCursorPosX((ImGui::GetWindowWidth() - textSize.x) * 0.5f);
			ImGui::Text("No entity selected");
			
			ImGui::PopStyleColor();
		}
		
		ImGui::End();
	}

	// ============================================================================
	// UTILITY FUNCTIONS FOR DRAWING COMPONENTS
	// ============================================================================

	static void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f) {
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, columnWidth);
		
		ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
		ImGui::Text("%s", label.c_str());
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 4, 0 });

		float lineHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

		// X Component
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.8f, 0.1f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.9f, 0.2f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.7f, 0.05f, 0.1f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", buttonSize))
			values.x = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopStyleColor(2);
		ImGui::PopItemWidth();
		ImGui::SameLine();

		// Y Component
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.2f, 0.7f, 0.2f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.3f, 0.8f, 0.3f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.15f, 0.6f, 0.15f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize))
			values.y = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopStyleColor(2);
		ImGui::PopItemWidth();
		ImGui::SameLine();

		// Z Component
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.1f, 0.25f, 0.8f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.2f, 0.35f, 0.9f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.05f, 0.2f, 0.7f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize))
			values.z = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopStyleColor(2);
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();
		ImGui::Columns(1);
		ImGui::PopID();
	}

	template<typename T, typename UIFunction>
	static void DrawComponent(const std::string& name, Entity entity, UIFunction uifunction, Ref<Texture2D> icon = nullptr) {
		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed
			| ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding;

		if (entity.HasComponent<T>()) {
			auto& component = entity.GetComponent<T>();
			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 8, 8 });
			ImGui::PushStyleColor(ImGuiCol_Header, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4{ 0.15f, 0.15f, 0.15f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			
			float lineHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f;
			
			ImGui::Separator();
			
			
			bool open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, "");
			
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar();

			// Draw icon and label
			ImGui::SameLine();
			if (icon) {
				ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4);
				ImGui::Image((void*)(intptr_t)icon->GetRendererID(), ImVec2(16, 16), ImVec2(0, 1), ImVec2(1, 0));
				ImGui::SameLine();
			}
			ImGui::SetCursorPosX(ImGui::GetCursorPosX() + 4);
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 0.90f, 0.50f, 0.10f, 1.0f });
			ImGui::Text("%s", name.c_str());
			ImGui::PopStyleColor();

			// Settings button
			ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
			ImGui::PushID((int)(intptr_t)(void*)typeid(T).hash_code());
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.18f, 0.18f, 0.18f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.6f });

			if (ImGui::Button("...", ImVec2{ lineHeight, lineHeight })) {
				ImGui::OpenPopup("ComponentSettings");
			}

			ImGui::PopStyleColor(3);

			bool removeComponent = false;
			if (ImGui::BeginPopup("ComponentSettings")) {
				if (ImGui::MenuItem("Remove component"))
					removeComponent = true;
				if (ImGui::MenuItem("Reset component"))
					component = T();
				ImGui::EndPopup();
			}

			ImGui::PopID();

			if (open) {
				ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 8));
				ImGui::Dummy(ImVec2(0, 4));
				uifunction(component);
				ImGui::Dummy(ImVec2(0, 4));
				ImGui::PopStyleVar();
				ImGui::TreePop();
			}

			if (removeComponent)
				entity.RemoveComponent<T>();
		}
	}

	// ============================================================================

	// COMPONENT DRAWING
	// ============================================================================

	void PropertiesPanel::DrawComponents(Entity entity) {
		DrawComponent<TransformComponent>("Transform", entity, [](auto& component) {
			DrawVec3Control("Translation", component.Translation);
			glm::vec3 rotation = glm::degrees(component.Rotation);
			DrawVec3Control("Rotation", rotation);
			component.Rotation = glm::radians(rotation);
			DrawVec3Control("Scale", component.Scale, 1.0f);
		});

		DrawComponent<CameraComponent>("Camera", entity, [](auto& component) {
			auto& camera = component.Camera;

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Primary");
			ImGui::NextColumn();
			ImGui::Checkbox("##Primary", &component.Primary);
			ImGui::Columns(1);

			ImGui::Spacing();

			const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
			const char* currentProjectionTypeString = projectionTypeStrings[(int)camera.GetProjectionType()];
			
			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Projection");
			ImGui::NextColumn();
			
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			
			if (ImGui::BeginCombo("##Projection", currentProjectionTypeString)) {
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
			
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);

			ImGui::Spacing();

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective) {
				float perspectiveVerticalFov = glm::degrees(camera.GetPerspectiveVerticalFOV());
				
				ImGui::Columns(2);
				ImGui::SetColumnWidth(0, 100);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
				ImGui::Text("Vertical FOV");
				ImGui::NextColumn();
				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
				if (ImGui::DragFloat("##VerticalFOV", &perspectiveVerticalFov))
					camera.SetPerspectiveVerticalFOV(glm::radians(perspectiveVerticalFov));
				ImGui::PopStyleColor(2);
				ImGui::Columns(1);

				float perspectiveNear = camera.GetPerspectiveNearClip();
				ImGui::Columns(2);
				ImGui::SetColumnWidth(0, 100);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
				ImGui::Text("Near Clip");
				ImGui::NextColumn();
				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
				if (ImGui::DragFloat("##Near", &perspectiveNear))
					camera.SetPerspectiveNearClip(perspectiveNear);
				ImGui::PopStyleColor(2);
				ImGui::Columns(1);

				float perspectiveFar = camera.GetPerspectiveFarClip();
				ImGui::Columns(2);
				ImGui::SetColumnWidth(0, 100);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
				ImGui::Text("Far Clip");
			 ImGui::NextColumn();
				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
				if (ImGui::DragFloat("##Far", &perspectiveFar))
					camera.SetPerspectiveFarClip(perspectiveFar);
				ImGui::PopStyleColor(2);
				ImGui::Columns(1);
			}

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic) {
				float orthoSize = camera.GetOrthographicSize();
				ImGui::Columns(2);
				ImGui::SetColumnWidth(0, 100);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
				ImGui::Text("Size");
				ImGui::NextColumn();
				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
				if (ImGui::DragFloat("##Size", &orthoSize))
					camera.SetOrthographicSize(orthoSize);
				ImGui::PopStyleColor(2);
				ImGui::Columns(1);

				float orthoNear = camera.GetOrthographicNearClip();
				ImGui::Columns(2);
				ImGui::SetColumnWidth(0, 100);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
				ImGui::Text("Near Clip");
				ImGui::NextColumn();
				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
				if (ImGui::DragFloat("##OrthoNear", &orthoNear))
					camera.SetOrthographicNearClip(orthoNear);
				ImGui::PopStyleColor(2);
				ImGui::Columns(1);

				float orthoFar = camera.GetOrthographicFarClip();
				ImGui::Columns(2);
				ImGui::SetColumnWidth(0, 100);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
				ImGui::Text("Far Clip");
				ImGui::NextColumn();
				ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
				if (ImGui::DragFloat("##OrthoFar", &orthoFar))
					camera.SetOrthographicFarClip(orthoFar);
				ImGui::PopStyleColor(2);
				ImGui::Columns(1);

				ImGui::Columns(2);
				ImGui::SetColumnWidth(0, 100);
				ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
				ImGui::Text("Fixed Aspect");
				ImGui::NextColumn();
				ImGui::Checkbox("##FixedAspect", &component.FixedAspectRatio);
				ImGui::Columns(1);
			}
		}, m_CameraIcon);

		DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, [&](auto& component) {
			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Color");
			ImGui::NextColumn();
			ImGui::ColorEdit4("##Color", glm::value_ptr(component.Color));
			ImGui::Columns(1);

			ImGui::Spacing();

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Texture");
			ImGui::NextColumn();
			
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.18f, 0.18f, 0.18f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.6f });
			ImGui::Button("Texture", ImVec2(ImGui::GetContentRegionAvail().x, 30.0f));
			ImGui::PopStyleColor(3);
			
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
					const wchar_t* path = (const wchar_t*)payload->Data;
					std::filesystem::path texturePath = std::filesystem::path(g_AssetPath) / path;
					Ref<Texture2D> texture = Texture2D::Create(texturePath.string());
					if (texture->IsLoaded())
					component.Texture = texture;
				}
				ImGui::EndDragDropTarget();
			}
			ImGui::Columns(1);

			ImGui::Spacing();

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Tiling Factor");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			ImGui::DragFloat("##TilingFactor", &component.TilingFactor, 0.1f, 0.0f, 100.0f);
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);
		}, m_SpriteIcon);

		DrawComponent<CircleRendererComponent>("Circle Renderer", entity, [](auto& component) {
			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Color");
			ImGui::NextColumn();
			ImGui::ColorEdit4("##Color", glm::value_ptr(component.Color));
			ImGui::Columns(1);

			ImGui::Spacing();

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Thickness");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			ImGui::DragFloat("##Thickness", &component.Thickness, 0.025f, 0.0f, 1.0f);
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Fade");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			ImGui::DragFloat("##Fade", &component.Fade, 0.00025f, 0.0f, 1.0f);
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);
		});

		DrawComponent<Rigidbody2DComponent>("Rigidbody 2D", entity, [](auto& component) {
			const char* bodyTypeStrings[] = { "Static", "Dynamic", "Kinematic" };
			const char* currentBodyTypeString = bodyTypeStrings[(int)component.Type];
			
			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Body Type");
		 ImGui::NextColumn();
			
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			
			if (ImGui::BeginCombo("##BodyType", currentBodyTypeString)) {
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
			
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);

			ImGui::Spacing();

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Fixed Rotation");
			ImGui::NextColumn();
			ImGui::Checkbox("##FixedRotation", &component.FixedRotation);
			ImGui::Columns(1);
		}, m_PhysicsIcon);

		DrawComponent<BoxCollider2DComponent>("Box Collider 2D", entity, [](auto& component) {
			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Offset");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			ImGui::DragFloat2("##Offset", glm::value_ptr(component.Offset));
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Size");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			ImGui::DragFloat2("##Size", glm::value_ptr(component.Size));
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);

			ImGui::Spacing();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 0.7f, 0.7f, 0.7f, 1.0f });
			ImGui::Text("Physics Properties");
			ImGui::PopStyleColor();
			ImGui::Spacing();

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Density");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			ImGui::DragFloat("##Density", &component.Density, 0.01f, 0.0f, 1.0f);
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Friction");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			ImGui::DragFloat("##Friction", &component.Friction, 0.01f, 0.0f, 1.0f);
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Restitution");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			ImGui::DragFloat("##Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Rest. Threshold");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			ImGui::DragFloat("##RestitutionThreshold", &component.RestitutionThreshold, 0.01f, 0.0f);
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);
		}, m_PhysicsIcon);

		DrawComponent<CircleCollider2DComponent>("Circle Collider 2D", entity, [](auto& component) {
			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Offset");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			ImGui::DragFloat2("##Offset", glm::value_ptr(component.Offset));
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Radius");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			ImGui::DragFloat("##Radius", &component.Radius);
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);

			ImGui::Spacing();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 0.7f, 0.7f, 0.7f, 1.0f });
			ImGui::Text("Physics Properties");
			ImGui::PopStyleColor();
			ImGui::Spacing();

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Density");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			ImGui::DragFloat("##Density", &component.Density, 0.01f, 0.0f, 1.0f);
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Friction");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			ImGui::DragFloat("##Friction", &component.Friction, 0.01f, 0.0f, 1.0f);
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Restitution");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			ImGui::DragFloat("##Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Rest. Threshold");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			ImGui::DragFloat("##RestitutionThreshold", &component.RestitutionThreshold, 0.01f, 0.0f);
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);
		}, m_PhysicsIcon);

		// 3D Components
		DrawComponent<MeshComponent>("Mesh", entity, [](auto& component) {
			// Shape Type Selector
			const char* shapeTypes[] = { "Cube", "Sphere", "Plane", "Cylinder", "Cone", "Torus", "Quad" };
			static int currentShapeIndex = 0;

			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Shape Type");
			ImGui::NextColumn();

			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });

			if (ImGui::BeginCombo("##ShapeType", shapeTypes[currentShapeIndex])) {
				for (int i = 0; i < 7; i++) {
					bool isSelected = (currentShapeIndex == i);
					if (ImGui::Selectable(shapeTypes[i], isSelected)) {
						currentShapeIndex = i;
						
						// Create new mesh based on selection
						switch (i) {
							case 0: component.MeshAsset = BasicShapes::CreateCube(); break;
							case 1: component.MeshAsset = BasicShapes::CreateSphere(); break;
							case 2: component.MeshAsset = BasicShapes::CreatePlane(); break;
							case 3: component.MeshAsset = BasicShapes::CreateCylinder(); break;
							case 4: component.MeshAsset = BasicShapes::CreateCone(); break;
							case 5: component.MeshAsset = BasicShapes::CreateTorus(); break;
							case 6: component.MeshAsset = BasicShapes::CreateQuad(); break;
						}
					}
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}

			ImGui::PopStyleColor(2);
			ImGui::Columns(1);

			ImGui::Spacing();

			// Mesh Info
			if (component.MeshAsset) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 0.7f, 0.7f, 0.7f, 1.0f });
				ImGui::Text("Mesh Info:");
				ImGui::PopStyleColor();
				
				ImGui::Columns(2);
				ImGui::SetColumnWidth(0, 100);
				ImGui::Text("Vertices");
				ImGui::NextColumn();
				ImGui::Text("%d", component.MeshAsset->GetVertexCount());
				ImGui::Columns(1);

				ImGui::Columns(2);
				ImGui::SetColumnWidth(0, 100);
				ImGui::Text("Triangles");
				ImGui::NextColumn();
				ImGui::Text("%d", component.MeshAsset->GetIndexCount() / 3);
				ImGui::Columns(1);
			}
			else {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 0.8f, 0.3f, 0.2f, 1.0f });
				ImGui::Text("No mesh assigned!");
				ImGui::PopStyleColor();
			}
		}, m_SpriteIcon);

		DrawComponent<MaterialComponent>("Material", entity, [](auto& component) {
			if (!component.MaterialInstance) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 0.8f, 0.3f, 0.2f, 1.0f });
				ImGui::Text("No material assigned!");
				ImGui::PopStyleColor();
				return;
			}

			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 0.7f, 0.7f, 0.7f, 1.0f });
			ImGui::Text("PBR Properties");
			ImGui::PopStyleColor();
			ImGui::Spacing();

			// Albedo
			glm::vec3 albedo = component.MaterialInstance->GetAlbedo();
			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Albedo");
			ImGui::NextColumn();
			if (ImGui::ColorEdit3("##Albedo", glm::value_ptr(albedo))) {
				component.MaterialInstance->SetAlbedo(albedo);
			}
			ImGui::Columns(1);

			// Metallic
			float metallic = component.MaterialInstance->GetMetallic();
			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Metallic");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			if (ImGui::SliderFloat("##Metallic", &metallic, 0.0f, 1.0f)) {
				component.MaterialInstance->SetMetallic(metallic);
			}
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);

			// Roughness
			float roughness = component.MaterialInstance->GetRoughness();
			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Roughness");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.12f, 0.12f, 0.12f, 1.0f });
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.90f, 0.50f, 0.10f, 0.7f });
			if (ImGui::SliderFloat("##Roughness", &roughness, 0.0f, 1.0f)) {
				component.MaterialInstance->SetRoughness(roughness);
			}
			ImGui::PopStyleColor(2);
			ImGui::Columns(1);

			// Emission
			glm::vec3 emission = component.MaterialInstance->GetEmission();
			ImGui::Columns(2);
			ImGui::SetColumnWidth(0, 100);
			ImGui::SetCursorPosY(ImGui::GetCursorPosY() + 4);
			ImGui::Text("Emission");
			ImGui::NextColumn();
			if (ImGui::ColorEdit3("##Emission", glm::value_ptr(emission))) {
				component.MaterialInstance->SetEmission(emission);
			}
			ImGui::Columns(1);

			ImGui::Spacing();
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4{ 0.7f, 0.7f, 0.7f, 1.0f });
			ImGui::Text("Texture Maps (Coming Soon)");
			ImGui::PopStyleColor();
		}, m_SpriteIcon);
	}

	template<typename T>
	void PropertiesPanel::DisplayAddComponentEntry(const std::string& entryName) {
		if (!m_SelectedEntity.HasComponent<T>()) {
			if (ImGui::MenuItem(entryName.c_str())) {
				m_SelectedEntity.AddComponent<T>();
				ImGui::CloseCurrentPopup();
			}
		}
	}
}
