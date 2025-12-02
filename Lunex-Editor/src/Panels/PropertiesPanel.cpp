#include "PropertiesPanel.h"
#include "ContentBrowserPanel.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>

#include "Scene/Components.h"

namespace Lunex {
	extern const std::filesystem::path g_AssetPath;

	// UI Constants for consistent styling
	namespace UIStyle {
		constexpr float SECTION_SPACING = 8.0f;
		constexpr float INDENT_SIZE = 12.0f;
		constexpr float HEADER_HEIGHT = 28.0f;
		constexpr float THUMBNAIL_SIZE = 64.0f;
		constexpr float COLUMN_WIDTH = 120.0f;
		
		const ImVec4 COLOR_HEADER = ImVec4(0.85f, 0.85f, 0.85f, 1.0f);
		const ImVec4 COLOR_SUBHEADER = ImVec4(0.7f, 0.7f, 0.7f, 1.0f);
		const ImVec4 COLOR_HINT = ImVec4(0.5f, 0.5f, 0.5f, 1.0f);
		const ImVec4 COLOR_ACCENT = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);
		const ImVec4 COLOR_SUCCESS = ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
		const ImVec4 COLOR_WARNING = ImVec4(0.8f, 0.6f, 0.2f, 1.0f);
		const ImVec4 COLOR_DANGER = ImVec4(0.8f, 0.3f, 0.3f, 1.0f);
		const ImVec4 COLOR_BG_DARK = ImVec4(0.16f, 0.16f, 0.17f, 1.0f);
		const ImVec4 COLOR_BG_MEDIUM = ImVec4(0.22f, 0.22f, 0.24f, 1.0f);
	}

	// Helper functions for consistent UI elements
	static void BeginPropertyGrid() {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4.0f, 4.0f));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(8.0f, 6.0f));
	}

	static void EndPropertyGrid() {
		ImGui::PopStyleVar(2);
	}

	static void PropertyLabel(const char* label, const char* tooltip = nullptr) {
		ImGui::AlignTextToFramePadding();
		ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_SUBHEADER);
		ImGui::Text("%s", label);
		ImGui::PopStyleColor();
		if (tooltip && ImGui::IsItemHovered()) {
			ImGui::SetTooltip("%s", tooltip);
		}
	}

	static void SectionHeader(const char* icon, const char* title) {
		ImGui::Spacing();
		ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_HEADER);
		ImGui::Text("%s  %s", icon, title);
		ImGui::PopStyleColor();
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
	}

	static bool PropertySlider(const char* label, float* value, float min, float max, const char* format = "%.2f", const char* tooltip = nullptr) {
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, UIStyle::COLUMN_WIDTH);
		PropertyLabel(label, tooltip);
		ImGui::NextColumn();
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, UIStyle::COLOR_ACCENT);
		ImGui::PushStyleColor(ImGuiCol_SliderGrab, UIStyle::COLOR_ACCENT);
		ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.36f, 0.69f, 1.0f, 1.0f));
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::SliderFloat(("##" + std::string(label)).c_str(), value, min, max, format);
		ImGui::PopStyleColor(3);
		ImGui::Columns(1);
		return changed;
	}

	static bool PropertyDrag(const char* label, float* value, float speed = 0.1f, float min = 0.0f, float max = 0.0f, const char* format = "%.2f", const char* tooltip = nullptr) {
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, UIStyle::COLUMN_WIDTH);
		PropertyLabel(label, tooltip);
		ImGui::NextColumn();
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, UIStyle::COLOR_ACCENT);
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::DragFloat(("##" + std::string(label)).c_str(), value, speed, min, max, format);
		ImGui::PopStyleColor();
		ImGui::Columns(1);
		return changed;
	}

	static bool PropertyColor(const char* label, glm::vec3& color, const char* tooltip = nullptr) {
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, UIStyle::COLUMN_WIDTH);
		PropertyLabel(label, tooltip);
		ImGui::NextColumn();
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::ColorEdit3(("##" + std::string(label)).c_str(), glm::value_ptr(color), ImGuiColorEditFlags_NoLabel);
		ImGui::Columns(1);
		return changed;
	}

	static bool PropertyColor4(const char* label, glm::vec4& color, const char* tooltip = nullptr) {
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, UIStyle::COLUMN_WIDTH);
		PropertyLabel(label, tooltip);
		ImGui::NextColumn();
		ImGui::SetNextItemWidth(-1);
		bool changed = ImGui::ColorEdit4(("##" + std::string(label)).c_str(), glm::value_ptr(color), ImGuiColorEditFlags_NoLabel | ImGuiColorEditFlags_AlphaBar);
		ImGui::Columns(1);
		return changed;
	}

	static bool PropertyCheckbox(const char* label, bool* value, const char* tooltip = nullptr) {
		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, UIStyle::COLUMN_WIDTH);
		PropertyLabel(label, tooltip);
		ImGui::NextColumn();
		bool changed = ImGui::Checkbox(("##" + std::string(label)).c_str(), value);
		ImGui::Columns(1);
		return changed;
	}

	PropertiesPanel::PropertiesPanel(const Ref<Scene>& context) {
		SetContext(context);
	}

	void PropertiesPanel::SetContext(const Ref<Scene>& context) {
		m_Context = context;
		m_SelectedEntity = {};
	}

	void PropertiesPanel::SetSelectedEntity(Entity entity) {
		m_SelectedEntity = entity;
	}

	void PropertiesPanel::OnImGuiRender() {
		ImGui::Begin("Properties");
		
		if (m_SelectedEntity) {
			DrawComponents(m_SelectedEntity);
		}
		else {
			// Empty state with helpful message
			ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_HINT);
			float windowWidth = ImGui::GetContentRegionAvail().x;
			const char* text = "No entity selected";
			float textWidth = ImGui::CalcTextSize(text).x;
			ImGui::SetCursorPosX((windowWidth - textWidth) * 0.5f);
			ImGui::SetCursorPosY(ImGui::GetWindowHeight() * 0.4f);
			ImGui::Text("%s", text);
			ImGui::PopStyleColor();
		}

		ImGui::End();
	}

	static void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = UIStyle::COLUMN_WIDTH) {
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.c_str());

		ImGui::Columns(2, nullptr, false);
		ImGui::SetColumnWidth(0, columnWidth);
		
		PropertyLabel(label.c_str());
		
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 2, 0 });

		// X Component
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.70f, 0.20f, 0.20f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.80f, 0.30f, 0.30f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.60f, 0.15f, 0.15f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", ImVec2{ 25, 25 }))
			values.x = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.25f, 0.15f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4{ 0.30f, 0.18f, 0.18f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.70f, 0.20f, 0.20f, 0.50f });
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopStyleColor(3);
		ImGui::PopItemWidth();
		ImGui::SameLine();

		// Y Component
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.20f, 0.70f, 0.20f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.30f, 0.80f, 0.30f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.15f, 0.60f, 0.15f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", ImVec2{ 25, 25 }))
			values.y = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.15f, 0.25f, 0.15f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4{ 0.18f, 0.30f, 0.18f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.70f, 0.20f, 0.20f, 0.50f });
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopStyleColor(3);
		ImGui::PopItemWidth();
		ImGui::SameLine();

		// Z Component
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.20f, 0.40f, 0.90f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.30f, 0.50f, 1.0f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.15f, 0.35f, 0.80f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", ImVec2{ 25, 25 }))
			values.z = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4{ 0.15f, 0.18f, 0.30f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4{ 0.18f, 0.22f, 0.35f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.20f, 0.40f, 0.90f, 0.50f });
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopStyleColor(3);
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();
	}

	template<typename T, typename UIFunction>
	static void DrawComponent(const std::string& name, Entity entity, UIFunction uifunction) {

		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap | ImGuiTreeNodeFlags_FramePadding;

		if (entity.HasComponent<T>()) {
			auto& component = entity.GetComponent<T>();
			ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2{ 6, 6 });
			float lineHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f;
			
			ImGui::PushStyleColor(ImGuiCol_Header, UIStyle::COLOR_BG_MEDIUM);
			ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.26f, 0.26f, 0.28f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.30f, 0.32f, 1.0f));
			
			bool open = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, name.c_str());
			
			ImGui::PopStyleColor(3);
			ImGui::PopStyleVar();

			ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);

			ImGui::PushID((int)(intptr_t)(void*)typeid(T).hash_code());

			// Special handling for MeshComponent - cannot be removed if MaterialComponent exists
			bool canRemove = true;
			if constexpr (std::is_same_v<T, MeshComponent>) {
				canRemove = true;
			}

			// MaterialComponent cannot be removed independently
			if constexpr (std::is_same_v<T, MaterialComponent>) {
				canRemove = false;
			}

			if (!canRemove) {
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
			}

			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.32f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.42f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.37f, 1.0f));
			
			if (ImGui::Button("+", ImVec2{ lineHeight, lineHeight })) {
				if (canRemove) {
					ImGui::OpenPopup("ComponentSettings");
				}
			}
			
			ImGui::PopStyleColor(3);

			if (!canRemove) {
				ImGui::PopStyleVar();
				if (ImGui::IsItemHovered()) {
					ImGui::SetTooltip("This component cannot be removed independently");
				}
			}

			bool removeComponent = false;
			if (canRemove && ImGui::BeginPopup("ComponentSettings")) {
				if (ImGui::MenuItem("Remove component"))
					removeComponent = true;

				ImGui::EndPopup();
			}

			ImGui::PopID();

			if (open) {
				ImGui::Indent(UIStyle::INDENT_SIZE);
				BeginPropertyGrid();
				uifunction(component);
				EndPropertyGrid();
				ImGui::Unindent(UIStyle::INDENT_SIZE);
				ImGui::TreePop();
			}

			if (removeComponent) {
				if constexpr (std::is_same_v<T, MeshComponent>) {
					if (entity.HasComponent<MaterialComponent>()) {
						entity.RemoveComponent<MaterialComponent>();
					}
				}
				entity.RemoveComponent<T>();
			}
		}
	}

	void PropertiesPanel::DrawComponents(Entity entity) {
		// Entity Tag Header
		if (entity.HasComponent<TagComponent>()) {
			auto& tag = entity.GetComponent<TagComponent>().Tag;

			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 8.0f));
			ImGui::PushStyleColor(ImGuiCol_FrameBg, UIStyle::COLOR_BG_DARK);
			ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, UIStyle::COLOR_BG_MEDIUM);
			
			char buffer[256];
			memset(buffer, 0, sizeof(buffer));
			strcpy_s(buffer, sizeof(buffer), tag.c_str());
			
			ImGui::SetNextItemWidth(-1);
			if (ImGui::InputText("##Tag", buffer, sizeof(buffer))) {
				tag = std::string(buffer);
			}
			
			ImGui::PopStyleColor(2);
			ImGui::PopStyleVar();
		}

		ImGui::Spacing();

		// Add Component Button
		ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::COLOR_ACCENT);
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.36f, 0.69f, 1.0f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.20f, 0.50f, 0.90f, 1.0f));
		
		if (ImGui::Button("+ Add Component", ImVec2(-1, 32.0f)))
			ImGui::OpenPopup("AddComponent");
			
		ImGui::PopStyleColor(3);
		
		if (ImGui::BeginPopup("AddComponent")) {
			ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_HEADER);
			ImGui::Text("Add Component");
			ImGui::PopStyleColor();
			ImGui::Separator();
			
			DisplayAddComponentEntry<CameraComponent>("🎥  Camera");
			DisplayAddComponentEntry<ScriptComponent>("📜  C++ Script");
			DisplayAddComponentEntry<SpriteRendererComponent>("🖼️  Sprite Renderer");
			DisplayAddComponentEntry<CircleRendererComponent>("⭕  Circle Renderer");
			DisplayAddComponentEntry<MeshComponent>("🗿  Mesh Renderer");
			DisplayAddComponentEntry<LightComponent>("💡  Light");
			DisplayAddComponentEntry<TextureComponent>("🎨  Textures Mapper");
			
			ImGui::Separator();
			ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_SUBHEADER);
			ImGui::Text("Physics 2D");
			ImGui::PopStyleColor();
			
			DisplayAddComponentEntry<Rigidbody2DComponent>("⚙️  Rigidbody 2D");
			DisplayAddComponentEntry<BoxCollider2DComponent>("📦  Box Collider 2D");
			DisplayAddComponentEntry<CircleCollider2DComponent>("⭕  Circle Collider 2D");
			
			ImGui::Separator();
			ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_SUBHEADER);
			ImGui::Text("Physics 3D");
			ImGui::PopStyleColor();
			
			DisplayAddComponentEntry<Rigidbody3DComponent>("🎲  Rigidbody 3D");
			DisplayAddComponentEntry<BoxCollider3DComponent>("📦  Box Collider 3D");
			DisplayAddComponentEntry<SphereCollider3DComponent>("🌐  Sphere Collider 3D");
			DisplayAddComponentEntry<CapsuleCollider3DComponent>("💊  Capsule Collider 3D");
			DisplayAddComponentEntry<MeshCollider3DComponent>("🗿  Mesh Collider 3D");

			ImGui::EndPopup();
		}

		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();

		// Transform Component
		DrawComponent<TransformComponent>("🔷 Transform", entity, [](auto& component) {
			DrawVec3Control("Translation", component.Translation);
			glm::vec3 rotation = glm::degrees(component.Rotation);
			DrawVec3Control("Rotation", rotation);
			component.Rotation = glm::radians(rotation);
			DrawVec3Control("Scale", component.Scale, 1.0f);
		});

		// Script Component
		DrawComponent<ScriptComponent>("📜 Script", entity, [](auto& component) {
			SectionHeader("📝", "C++ Scripts");
			
			ImGui::Indent(UIStyle::INDENT_SIZE);
			
			// Lista de scripts
			for (size_t i = 0; i < component.GetScriptCount(); i++) {
				ImGui::PushID((int)i);
				
				const std::string& scriptPath = component.GetScriptPath(i);
				std::filesystem::path path(scriptPath);
				std::string filename = path.filename().string();
				bool isLoaded = component.IsScriptLoaded(i);
				
				// Script card
				ImGui::PushStyleColor(ImGuiCol_ChildBg, UIStyle::COLOR_BG_DARK);
				ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
				ImGui::BeginChild(("##ScriptCard" + std::to_string(i)).c_str(), ImVec2(-1, 100.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
				
				// Header row
				ImGui::BeginGroup();
				ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_HINT);
				ImGui::Text("Script #%d", (int)i + 1);
				ImGui::PopStyleColor();
				ImGui::EndGroup();
				
				ImGui::SameLine(ImGui::GetContentRegionAvail().x - 65);
				ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::COLOR_DANGER);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
				if (ImGui::Button("Remove", ImVec2(65, 0))) {
					component.RemoveScript(i);
					ImGui::PopStyleColor(2);
					ImGui::EndChild();
					ImGui::PopStyleVar();
					ImGui::PopStyleColor();
					ImGui::PopID();
					break;
				}
				ImGui::PopStyleColor(2);
				
				ImGui::Separator();
				ImGui::Spacing();
				
				// File info
				ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_ACCENT);
				ImGui::Text("📄");
				ImGui::PopStyleColor();
				ImGui::SameLine();
				ImGui::TextWrapped("%s", filename.c_str());
				
				ImGui::Spacing();
				
				// Status badge
				ImGui::Text("Status:");
				ImGui::SameLine();
				if (isLoaded) {
					ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_SUCCESS);
					ImGui::Text("✓ Loaded");
					ImGui::PopStyleColor();
				}
				else {
					ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_WARNING);
					ImGui::Text("⚠ Will compile on Play");
					ImGui::PopStyleColor();
				}
				
				ImGui::EndChild();
				ImGui::PopStyleVar();
				ImGui::PopStyleColor();
				
				ImGui::Spacing();
				
				ImGui::PopID();
			}
			
			// Add Script button
			ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::COLOR_BG_MEDIUM);
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.28f, 0.30f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_Border, UIStyle::COLOR_ACCENT);
			ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.0f);
			
			if (ImGui::Button("➕ Add Script", ImVec2(-1, 35.0f))) {
				// Drag & drop zone
			}
			
			ImGui::PopStyleVar();
			ImGui::PopStyleColor(3);
			
			// Drag and drop
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
					ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
					std::string ext = data->Extension;
					if (ext == ".cpp" || ext == ".h") {
					component.AddScript(data->RelativePath);
						LNX_LOG_INFO("Added script: {0}", data->RelativePath);
					}
					else {
						LNX_LOG_WARN("Only .cpp files are valid C++ scripts");
					}
				}
				ImGui::EndDragDropTarget();
			}
			
			ImGui::Unindent(UIStyle::INDENT_SIZE);
			
			// Script Properties placeholder
			if (component.GetScriptCount() > 0) {
				SectionHeader("⚙️", "Script Properties");
				ImGui::Indent(UIStyle::INDENT_SIZE);
				ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_HINT);
				ImGui::TextWrapped("Public variables will appear here when the reflection system is implemented.");
				ImGui::PopStyleColor();
				ImGui::Unindent(UIStyle::INDENT_SIZE);
			}
		});

		// Camera Component
		DrawComponent<CameraComponent>("🎥 Camera", entity, [](auto& component) {
			auto& camera = component.Camera;

			PropertyCheckbox("Primary", &component.Primary, "This camera will be used for rendering");
			
			SectionHeader("📐", "Projection");
			ImGui::Indent(UIStyle::INDENT_SIZE);

			const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
			const char* currentProjectionTypeString = projectionTypeStrings[(int)camera.GetProjectionType()];
			
			ImGui::Columns(2, nullptr, false);
			ImGui::SetColumnWidth(0, UIStyle::COLUMN_WIDTH);
			PropertyLabel("Type");
			ImGui::NextColumn();
			ImGui::SetNextItemWidth(-1);
			if (ImGui::BeginCombo("##ProjectionType", currentProjectionTypeString)) {
				for (int i = 0; i < 2; i++) {
					bool isSelected = ((int)camera.GetProjectionType() == i);
					if (ImGui::Selectable(projectionTypeStrings[i], isSelected)) {
						camera.SetProjectionType((SceneCamera::ProjectionType)i);
					}
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::Columns(1);

			ImGui::Spacing();

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective) {
				float perspectiveVerticalFov = glm::degrees(camera.GetPerspectiveVerticalFOV());
				if (PropertySlider("FOV", &perspectiveVerticalFov, 1.0f, 120.0f, "%.1f°", "Field of View"))
					camera.SetPerspectiveVerticalFOV(glm::radians(perspectiveVerticalFov));

				float perspectiveNear = camera.GetPerspectiveNearClip();
				float perspectiveFar = camera.GetPerspectiveFarClip();
				
				if (PropertyDrag("Near", &perspectiveNear, 0.01f, 0.01f, perspectiveFar - 0.01f))
					camera.SetPerspectiveNearClip(perspectiveNear);

				if (PropertyDrag("Far", &perspectiveFar, 0.1f, perspectiveNear + 0.01f, 10000.0f))
				 camera.SetPerspectiveFarClip(perspectiveFar);
			}

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic) {
				float orthoSize = camera.GetOrthographicSize();
				if (PropertyDrag("Size", &orthoSize, 0.1f, 0.1f, 100.0f))
					camera.SetOrthographicSize(orthoSize);

				float orthoNear = camera.GetOrthographicNearClip();
				float orthoFar = camera.GetOrthographicFarClip();
				
				if (PropertyDrag("Near", &orthoNear, 0.1f, -1000.0f, orthoFar - 0.1f))
					camera.SetOrthographicNearClip(orthoNear);

				if (PropertyDrag("Far", &orthoFar, 0.1f, orthoNear + 0.1f, 1000.0f))
					camera.SetOrthographicFarClip(orthoFar);

				PropertyCheckbox("Fixed Aspect", &component.FixedAspectRatio);
			}
			
			ImGui::Unindent(UIStyle::INDENT_SIZE);
		});

		// Sprite Renderer Component
		DrawComponent<SpriteRendererComponent>("🖼️ Sprite Renderer", entity, [](auto& component) {
			SectionHeader("🎨", "Appearance");
			ImGui::Indent(UIStyle::INDENT_SIZE);
			
			PropertyColor4("Color", component.Color);

			ImGui::Unindent(UIStyle::INDENT_SIZE);
			
			SectionHeader("🖼️", "Texture");
			ImGui::Indent(UIStyle::INDENT_SIZE);

			if (component.Texture && component.Texture->IsLoaded()) {
				ImGui::PushStyleColor(ImGuiCol_ChildBg, UIStyle::COLOR_BG_DARK);
				ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
				ImGui::BeginChild("##TextureInfo", ImVec2(-1, 90.0f), true);
				
				ImGui::Image(
					(void*)(intptr_t)component.Texture->GetRendererID(),
					ImVec2(70, 70),
					ImVec2(0, 1), ImVec2(1, 0)
				);
				
				ImGui::SameLine();
				ImGui::BeginGroup();
				ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_HEADER);
				ImGui::Text("Loaded Texture");
				ImGui::PopStyleColor();
				ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_HINT);
				ImGui::Text("Size: %dx%d", component.Texture->GetWidth(), component.Texture->GetHeight());
				ImGui::PopStyleColor();
				ImGui::Spacing();
				ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::COLOR_DANGER);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
				if (ImGui::Button("Remove", ImVec2(80, 0))) {
					component.Texture.reset();
				}
				ImGui::PopStyleColor(2);
				ImGui::EndGroup();
				
				ImGui::EndChild();
				ImGui::PopStyleVar();
				ImGui::PopStyleColor();
			}
			else {
				ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::COLOR_BG_MEDIUM);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.28f, 0.30f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_Border, UIStyle::COLOR_ACCENT);
				ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
				ImGui::Button("📁 Drop Texture Here\n(.png, .jpg, .bmp, .tga, .hdr)", ImVec2(-1, 70.0f));
				ImGui::PopStyleVar();
				ImGui::PopStyleColor(3);
			}
			
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
					ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
					std::string ext = data->Extension;
					if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" ||
						ext == ".bmp" || ext == ".tga" || ext == ".hdr") {
						Ref<Texture2D> texture = Texture2D::Create(data->FilePath);
						if (texture->IsLoaded())
							component.Texture = texture;
						else
							LNX_LOG_WARN("Could not load texture {0}", data->FilePath);
					}
					else {
						LNX_LOG_WARN("File is not a valid texture format");
					}
				}
				ImGui::EndDragDropTarget();
			}

			PropertyDrag("Tiling Factor", &component.TilingFactor, 0.1f, 0.0f, 100.0f, "%.2f", "Texture repeat multiplier");
			
			ImGui::Unindent(UIStyle::INDENT_SIZE);
		});

		// Circle Renderer Component
		DrawComponent<CircleRendererComponent>("⭕ Circle Renderer", entity, [](auto& component) {
			SectionHeader("🎨", "Appearance");
			ImGui::Indent(UIStyle::INDENT_SIZE);
			
			PropertyColor4("Color", component.Color);
			PropertySlider("Thickness", &component.Thickness, 0.0f, 1.0f, "%.3f", "0 = Filled, 1 = Outline");
			PropertySlider("Fade", &component.Fade, 0.0f, 1.0f, "%.3f", "Edge softness");
			
			ImGui::Unindent(UIStyle::INDENT_SIZE);
		});

		// Rigidbody 2D Component
		DrawComponent<Rigidbody2DComponent>("⚙️ Rigidbody 2D", entity, [](auto& component) {
			SectionHeader("🔧", "Body Configuration");
			ImGui::Indent(UIStyle::INDENT_SIZE);
			
			const char* bodyTypeStrings[] = { "Static", "Dynamic", "Kinematic" };
			const char* currentBodyTypeString = bodyTypeStrings[(int)component.Type];
			
			ImGui::Columns(2, nullptr, false);
			ImGui::SetColumnWidth(0, UIStyle::COLUMN_WIDTH);
			PropertyLabel("Type", "Defines how the body responds to physics");
			ImGui::NextColumn();
			ImGui::SetNextItemWidth(-1);
			if (ImGui::BeginCombo("##BodyType", currentBodyTypeString)) {
				for (int i = 0; i < 3; i++) {
					bool isSelected = ((int)component.Type == i);
					if (ImGui::Selectable(bodyTypeStrings[i], isSelected)) {
						component.Type = (Rigidbody2DComponent::BodyType)i;
					}
					if (isSelected)
						ImGui::SetItemDefaultFocus();
				}
				ImGui::EndCombo();
			}
			ImGui::Columns(1);

			PropertyCheckbox("Fixed Rotation", &component.FixedRotation, "Prevent rotation from physics");
			
			ImGui::Unindent(UIStyle::INDENT_SIZE);
		});

		// Box Collider 2D Component
		DrawComponent<BoxCollider2DComponent>("📦 Box Collider 2D", entity, [](auto& component) {
			SectionHeader("📐", "Shape");
			ImGui::Indent(UIStyle::INDENT_SIZE);
			
			ImGui::Columns(2, nullptr, false);
			ImGui::SetColumnWidth(0, UIStyle::COLUMN_WIDTH);
			PropertyLabel("Offset");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, UIStyle::COLOR_ACCENT);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat2("##Offset", glm::value_ptr(component.Offset), 0.01f);
			ImGui::PopStyleColor();
			ImGui::Columns(1);
			
			ImGui::Columns(2, nullptr, false);
			ImGui::SetColumnWidth(0, UIStyle::COLUMN_WIDTH);
			PropertyLabel("Size");
		 ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, UIStyle::COLOR_ACCENT);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat2("##Size", glm::value_ptr(component.Size), 0.01f, 0.01f);
			ImGui::PopStyleColor();
			ImGui::Columns(1);
			
			ImGui::Unindent(UIStyle::INDENT_SIZE);
			
			SectionHeader("⚗️", "Physics Material");
			ImGui::Indent(UIStyle::INDENT_SIZE);
			
			PropertyDrag("Density", &component.Density, 0.01f, 0.0f, 100.0f, "%.2f", "Mass per unit area");
			PropertyDrag("Friction", &component.Friction, 0.01f, 0.0f, 1.0f, "%.2f", "Surface friction coefficient");
			PropertyDrag("Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f, "%.2f", "Bounciness (0 = no bounce, 1 = perfect bounce)");
			PropertyDrag("Restitution Threshold", &component.RestitutionThreshold, 0.01f, 0.0f, 10.0f, "%.2f", "Minimum velocity for bounce");
			
			ImGui::Unindent(UIStyle::INDENT_SIZE);
		});

		// Circle Collider 2D Component
		DrawComponent<CircleCollider2DComponent>("⭕ Circle Collider 2D", entity, [](auto& component) {
			SectionHeader("📐", "Shape");
			ImGui::Indent(UIStyle::INDENT_SIZE);
			
			ImGui::Columns(2, nullptr, false);
			ImGui::SetColumnWidth(0, UIStyle::COLUMN_WIDTH);
			PropertyLabel("Offset");
			ImGui::NextColumn();
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, UIStyle::COLOR_ACCENT);
			ImGui::SetNextItemWidth(-1);
			ImGui::DragFloat2("##Offset", glm::value_ptr(component.Offset), 0.01f);
			ImGui::PopStyleColor();
			ImGui::Columns(1);
			
			PropertyDrag("Radius", &component.Radius, 0.01f, 0.01f);
			
			ImGui::Unindent(UIStyle::INDENT_SIZE);
			
			SectionHeader("⚗️", "Physics Material");
			ImGui::Indent(UIStyle::INDENT_SIZE);
			
			PropertyDrag("Density", &component.Density, 0.01f, 0.0f, 100.0f, "%.2f", "Mass per unit area");
			PropertyDrag("Friction", &component.Friction, 0.01f, 0.0f, 1.0f, "%.2f", "Surface friction coefficient");
			PropertyDrag("Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f, "%.2f", "Bounciness");
			PropertyDrag("Restitution Threshold", &component.RestitutionThreshold, 0.01f, 0.0f, 10.0f);
			
			ImGui::Unindent(UIStyle::INDENT_SIZE);
		});

		// Mesh Component
		DrawComponent<MeshComponent>("🗿 Mesh Renderer", entity, [](auto& component) {
			SectionHeader("🎲", "Model");
			ImGui::Indent(UIStyle::INDENT_SIZE);
			
			const char* modelTypes[] = { "Cube", "Sphere", "Plane", "Cylinder", "Custom Model" };
		 int currentType = (int)component.Type;

			ImGui::Columns(2, nullptr, false);
			ImGui::SetColumnWidth(0, UIStyle::COLUMN_WIDTH);
			PropertyLabel("Type");
			ImGui::NextColumn();
			ImGui::SetNextItemWidth(-1);
			if (ImGui::Combo("##ModelType", &currentType, modelTypes, IM_ARRAYSIZE(modelTypes))) {
				component.Type = (ModelType)currentType;
				if (component.Type != ModelType::FromFile) {
					component.CreatePrimitive(component.Type);
				}
			}
			ImGui::Columns(1);

			// Custom Model Section
			if (component.Type == ModelType::FromFile) {
				ImGui::Spacing();
				
				if (component.MeshModel) {
					ImGui::PushStyleColor(ImGuiCol_ChildBg, UIStyle::COLOR_BG_DARK);
					ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
					ImGui::BeginChild("##ModelInfo", ImVec2(-1, 145.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
					
					std::filesystem::path modelPath(component.FilePath);
					ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_ACCENT);
					ImGui::Text("🗿 %s", modelPath.filename().string().c_str());
					ImGui::PopStyleColor();
					
					ImGui::Spacing();
					ImGui::Separator();
					ImGui::Spacing();
					
					uint32_t totalVertices = 0;
					uint32_t totalIndices = 0;
					for (const auto& mesh : component.MeshModel->GetMeshes()) {
						totalVertices += (uint32_t)mesh->GetVertices().size();
						totalIndices += (uint32_t)mesh->GetIndices().size();
					}
					
					ImGui::Columns(2, nullptr, false);
					ImGui::SetColumnWidth(0, 100.0f);
					ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_SUBHEADER);
					ImGui::Text("Submeshes"); ImGui::NextColumn();
					ImGui::PopStyleColor();
					ImGui::Text("%d", component.MeshModel->GetMeshes().size()); ImGui::NextColumn();
					
					ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_SUBHEADER);
					ImGui::Text("Vertices"); ImGui::NextColumn();
					ImGui::PopStyleColor();
					ImGui::Text("%d", totalVertices); ImGui::NextColumn();
					
					ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_SUBHEADER);
					ImGui::Text("Triangles"); ImGui::NextColumn();
					ImGui::PopStyleColor();
					ImGui::Text("%d", totalIndices / 3); ImGui::NextColumn();
					ImGui::Columns(1);
					
					ImGui::Spacing();
					ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::COLOR_DANGER);
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.4f, 0.4f, 1.0f));
					if (ImGui::Button("Remove Model", ImVec2(-1, 0))) {
						component.FilePath.clear();
						component.MeshModel.reset();
					}
					ImGui::PopStyleColor(2);
					
					ImGui::EndChild();
					ImGui::PopStyleVar();
					ImGui::PopStyleColor();
				}
				else {
					ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::COLOR_BG_MEDIUM);
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.28f, 0.28f, 0.30f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_Border, UIStyle::COLOR_ACCENT);
					ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 1.5f);
					ImGui::Button("📁 Drop 3D Model Here\n(.obj, .fbx, .gltf, .glb, .dae)", ImVec2(-1, 60.0f));
					ImGui::PopStyleVar();
					ImGui::PopStyleColor(3);
					
					if (ImGui::BeginDragDropTarget()) {
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
							ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
							std::string ext = data->Extension;
							if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb" || ext == ".dae") {
								component.LoadFromFile(data->FilePath);
								LNX_LOG_INFO("Loaded model: {0}", data->FilePath);
							}
							else {
							 LNX_LOG_WARN("Unsupported model format: {0}", ext);
							}
						}
						ImGui::EndDragDropTarget();
					}
				}
			}
			
			ImGui::Unindent(UIStyle::INDENT_SIZE);
			
			SectionHeader("🎨", "Color Tint");
			ImGui::Indent(UIStyle::INDENT_SIZE);
			PropertyColor4("Color", component.Color);
			ImGui::Unindent(UIStyle::INDENT_SIZE);
		});

		// Material Component - NUEVO SISTEMA
		DrawComponent<MaterialComponent>("✨ Material", entity, [&](auto& component) {
			// ========== MATERIAL ASSET SECTION ==========
			SectionHeader("📦", "Material Asset");
			ImGui::Indent(UIStyle::INDENT_SIZE);
			
			// Material Asset Card
			ImGui::PushStyleColor(ImGuiCol_ChildBg, UIStyle::COLOR_BG_DARK);
			ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
			ImGui::BeginChild("##MaterialAssetCard", ImVec2(-1, 150.0f), true, ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
			
			if (component.Instance && component.Instance->GetBaseAsset()) {
				auto asset = component.Instance->GetBaseAsset();
				
				// Header row
				ImGui::BeginGroup();
				
				// Preview thumbnail (placeholder por ahora)
				ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::COLOR_BG_MEDIUM);
				ImGui::Button("##preview", ImVec2(70, 70));
				ImGui::PopStyleColor();
				
				// TODO: Cuando esté MaterialPreviewRenderer:
				// if (component.PreviewThumbnail) {
				//     ImGui::Image((void*)(intptr_t)component.PreviewThumbnail->GetRendererID(), 
				//                  ImVec2(70, 70), ImVec2(0, 1), ImVec2(1, 0));
				// }
				
				ImGui::EndGroup();
				ImGui::SameLine();
				
				// Material info
				ImGui::BeginGroup();
				
				ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_HEADER);
				ImGui::Text("🎨 %s", component.GetMaterialName().c_str());
				ImGui::PopStyleColor();
				
				ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_HINT);
				if (!component.GetAssetPath().empty()) {
					std::filesystem::path matPath(component.GetAssetPath());
					ImGui::Text("📁 %s", matPath.filename().string().c_str());
				} else {
					ImGui::Text("📁 Default Material");
				}
				ImGui::PopStyleColor();
				
				ImGui::Spacing();
				
				// Local overrides indicator
				if (component.HasLocalOverrides()) {
					ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_WARNING);
					ImGui::Text("⚙️ Has local overrides");
					ImGui::PopStyleColor();
				} else {
					ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_SUCCESS);
					ImGui::Text("✓ Using base asset");
					ImGui::PopStyleColor();
				}
				
				ImGui::EndGroup();
				
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();
				
				// Action buttons
				ImGui::BeginGroup();
				
				// Open in Editor button
				ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::COLOR_ACCENT);
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.36f, 0.69f, 1.0f, 1.0f));
				if (ImGui::Button("🖊️ Edit Material", ImVec2(140, 0))) {
					// TODO: Abrir MaterialEditorPanel
					// m_MaterialEditorPanel->OpenMaterial(asset);
					LNX_LOG_INFO("Open Material Editor: {0}", component.GetMaterialName());
				}
				ImGui::PopStyleColor(2);
				
				ImGui::SameLine();
				
				// Reset overrides button (solo si hay overrides)
				if (component.HasLocalOverrides()) {
					ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::COLOR_WARNING);
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.9f, 0.7f, 0.3f, 1.0f));
					if (ImGui::Button("🔄 Reset Overrides", ImVec2(140, 0))) {
						component.ResetOverrides();
					}
					ImGui::PopStyleColor(2);
				}
				
				ImGui::EndGroup();
			}
			else {
				// No material assigned
				ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_HINT);
				ImGui::TextWrapped("No material assigned. Drop a .lumat file here.");
				ImGui::PopStyleColor();
			}
			
			ImGui::EndChild();
			ImGui::PopStyleVar();
			ImGui::PopStyleColor();
			
			// Drag & Drop for .lumat files
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
					ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
					std::string ext = data->Extension;
					if (ext == ".lumat") {
						component.SetMaterialAsset(data->FilePath);
						LNX_LOG_INFO("Material assigned: {0}", data->FilePath);
					}
					else {
						LNX_LOG_WARN("Only .lumat files are valid materials");
					}
				}
				ImGui::EndDragDropTarget();
			}
			
			ImGui::Unindent(UIStyle::INDENT_SIZE);
			
			// ========== PBR PROPERTIES (con override support) ==========
			SectionHeader("🎨", "Surface Properties");
			ImGui::Indent(UIStyle::INDENT_SIZE);
			
			ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_HINT);
			ImGui::TextWrapped("💡 Tip: Changes here create local overrides. Use 'Reset Overrides' to revert.");
			ImGui::PopStyleColor();
			ImGui::Spacing();

			glm::vec4 color = component.GetAlbedo();
			if (PropertyColor4("Base Color", color)) {
				component.SetAlbedo(color, true); // asOverride = true
			}

			float metallic = component.GetMetallic();
			if (PropertySlider("Metallic", &metallic, 0.0f, 1.0f, "%.2f", "0 = Dielectric, 1 = Metal")) {
				component.SetMetallic(metallic, true);
			}

			float roughness = component.GetRoughness();
			if (PropertySlider("Roughness", &roughness, 0.0f, 1.0f, "%.2f", "0 = Smooth, 1 = Rough")) {
				component.SetRoughness(roughness, true);
			}

			float specular = component.GetSpecular();
			if (PropertySlider("Specular", &specular, 0.0f, 1.0f)) {
				component.SetSpecular(specular, true);
			}
			
			ImGui::Unindent(UIStyle::INDENT_SIZE);

			SectionHeader("💡", "Emission");
			ImGui::Indent(UIStyle::INDENT_SIZE);

			glm::vec3 emissionColor = component.GetEmissionColor();
			if (PropertyColor("Color", emissionColor)) {
				component.SetEmissionColor(emissionColor, true);
			}

			float emissionIntensity = component.GetEmissionIntensity();
			if (PropertyDrag("Intensity", &emissionIntensity, 0.1f, 0.0f, 100.0f)) {
				component.SetEmissionIntensity(emissionIntensity, true);
			}
			
			ImGui::Unindent(UIStyle::INDENT_SIZE);
			
			// ========== TEXTURE MAPS INFO (desde MaterialAsset) ==========
			if (component.Instance && component.Instance->GetBaseAsset()) {
				auto asset = component.Instance->GetBaseAsset();
				
				if (asset->HasAnyTexture()) {
					SectionHeader("🖼️", "Texture Maps");
					ImGui::Indent(UIStyle::INDENT_SIZE);
					
					ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_HINT);
					ImGui::TextWrapped("Textures are managed in the Material Asset. Open the Material Editor to modify them.");
					ImGui::PopStyleColor();
					
					ImGui::Spacing();
					
					// Lista de texturas cargadas
					if (asset->HasAlbedoMap()) ImGui::BulletText("🎨 Albedo Map");
					if (asset->HasNormalMap()) ImGui::BulletText("🧭 Normal Map");
					if (asset->HasMetallicMap()) ImGui::BulletText("⚙️ Metallic Map");
					if (asset->HasRoughnessMap()) ImGui::BulletText("🔧 Roughness Map");
					if (asset->HasSpecularMap()) ImGui::BulletText("💎 Specular Map");
					if (asset->HasEmissionMap()) ImGui::BulletText("💡 Emission Map");
					if (asset->HasAOMap()) ImGui::BulletText("🌑 AO Map");
					
					ImGui::Unindent(UIStyle::INDENT_SIZE);
				}
			}
			
			// ========== DEPRECATED: TextureComponent warning ==========
			if (entity.HasComponent<TextureComponent>()) {
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();
				
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.3f, 0.2f, 0.1f, 0.5f));
				ImGui::PushStyleVar(ImGuiStyleVar_ChildRounding, 4.0f);
				ImGui::BeginChild("##DeprecatedWarning", ImVec2(-1, 80.0f), true);
				
				ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_WARNING);
				ImGui::Text("⚠️ DEPRECATED: TextureComponent Detected");
				ImGui::PopStyleColor();
				
				ImGui::PushStyleColor(ImGuiCol_Text, UIStyle::COLOR_HINT);
				ImGui::TextWrapped("TextureComponent is deprecated. Migrate textures to MaterialAsset and remove this component.");
				ImGui::PopStyleColor();
				
				ImGui::Spacing();
				
				ImGui::PushStyleColor(ImGuiCol_Button, UIStyle::COLOR_DANGER);
				if (ImGui::Button("Remove TextureComponent", ImVec2(-1, 0))) {
					entity.RemoveComponent<TextureComponent>();
					LNX_LOG_INFO("TextureComponent removed. Migrate textures to MaterialAsset.");
				}
				ImGui::PopStyleColor();
				
				ImGui::EndChild();
				ImGui::PopStyleVar();
				ImGui::PopStyleColor();
			}
		});
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