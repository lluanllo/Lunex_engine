#include "PropertiesPanel.h"
#include "ContentBrowserPanel.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>

#include "Scene/Components.h"

namespace Lunex {
	extern const std::filesystem::path g_AssetPath;

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

		ImGui::End();
	}

	static void DrawVec3Control(const std::string& label, glm::vec3& values, float resetValue = 0.0f, float columnWidth = 100.0f) {
		ImGuiIO& io = ImGui::GetIO();
		auto boldFont = io.Fonts->Fonts[0];

		ImGui::PushID(label.c_str());

		ImGui::Columns(2);
		ImGui::SetColumnWidth(0, columnWidth);
		
		// Label con mejor estilo
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
		ImGui::Text(label.c_str());
		ImGui::PopStyleColor();
		
		ImGui::NextColumn();

		ImGui::PushMultiItemsWidths(3, ImGui::CalcItemWidth());
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2{ 0, 0 });

		float lineHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f;
		ImVec2 buttonSize = { lineHeight + 3.0f, lineHeight };

		// Botones con colores profesionales (menos saturados)
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.70f, 0.20f, 0.20f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.80f, 0.30f, 0.30f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.60f, 0.15f, 0.15f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("X", buttonSize))
			values.x = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
		ImGui::DragFloat("##X", &values.x, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopStyleColor();
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.20f, 0.70f, 0.20f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.30f, 0.80f, 0.30f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.15f, 0.60f, 0.15f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Y", buttonSize))
			values.y = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
		ImGui::DragFloat("##Y", &values.y, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopStyleColor();
		ImGui::PopItemWidth();
		ImGui::SameLine();

		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4{ 0.20f, 0.40f, 0.90f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4{ 0.30f, 0.50f, 1.0f, 1.0f });
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4{ 0.15f, 0.35f, 0.80f, 1.0f });
		ImGui::PushFont(boldFont);
		if (ImGui::Button("Z", buttonSize))
			(values.z) = resetValue;
		ImGui::PopFont();
		ImGui::PopStyleColor(3);

		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
		ImGui::DragFloat("##Z", &values.z, 0.1f, 0.0f, 0.0f, "%.2f");
		ImGui::PopStyleColor();
		ImGui::PopItemWidth();

		ImGui::PopStyleVar();

		ImGui::Columns(1);

		ImGui::PopID();
	}

	template<typename T, typename UIFunction>
	static void DrawComponent(const std::string& name, Entity entity, UIFunction uifunction) {

		const ImGuiTreeNodeFlags treeNodeFlags = ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_AllowOverlap;

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

			// Special handling for MeshComponent - cannot be removed if MaterialComponent exists
			bool canRemove = true;
			if constexpr (std::is_same_v<T, MeshComponent>) {
				// MeshComponent can always be removed, it will remove MaterialComponent too
				canRemove = true;
			}

			// MaterialComponent cannot be removed independently
			if constexpr (std::is_same_v<T, MaterialComponent>) {
				canRemove = false;
			}

			// Siempre mostrar el botón, pero desactivarlo visualmente si es necesario
			if (!canRemove) {
				ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
			}

			if (ImGui::Button("+", ImVec2{ lineHeight, lineHeight })) {
				if (canRemove) {
					ImGui::OpenPopup("ComponentSettings");
				}
			}

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
				uifunction(component);
				ImGui::TreePop();
			}

			if (removeComponent) {
				// Special handling: removing MeshComponent also removes MaterialComponent
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
			DisplayAddComponentEntry<ScriptComponent>("C++ Script");
			DisplayAddComponentEntry<SpriteRendererComponent>("Sprite Renderer");
			DisplayAddComponentEntry<CircleRendererComponent>("Circle Renderer");
			DisplayAddComponentEntry<MeshComponent>("Mesh Renderer");
			DisplayAddComponentEntry<LightComponent>("Light");
			DisplayAddComponentEntry<TextureComponent>("Textures Mapper");
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

		DrawComponent<ScriptComponent>("Script", entity, [](auto& component) {
			// Script Path Section
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
			ImGui::Text("Script");
			ImGui::PopStyleColor();
			
			ImGui::Indent(8.0f);
			
			if (component.ScriptPath.empty()) {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.22f, 0.24f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.26f, 0.28f, 1.0f));
				ImGui::Button("Drop C++ Script Here", ImVec2(-1, 40.0f));
				ImGui::PopStyleColor(2);
			}
			else {
				std::filesystem::path scriptPath(component.ScriptPath);
				std::string filename = scriptPath.filename().string();

				// Script info box con fondo
				ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.16f, 0.17f, 1.0f));
				ImGui::BeginChild("##ScriptInfo", ImVec2(-1, 70.0f), true);
				
				// Icono + Nombre
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
				ImGui::Text("📄");
				ImGui::PopStyleColor();
				ImGui::SameLine();
				ImGui::Text("%s", filename.c_str());
				
				// Status
				ImGui::Spacing();
				ImGui::Text("Status: ");
				ImGui::SameLine();
				if (component.IsLoaded) {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.3f, 0.8f, 0.3f, 1.0f));
					ImGui::Text("✓ Loaded & Ready");
					ImGui::PopStyleColor();
				}
				else {
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.6f, 0.2f, 1.0f));
					ImGui::Text("⚠ Will compile on Play");
					ImGui::PopStyleColor();
				}
				
				ImGui::EndChild();
				ImGui::PopStyleColor();
				
				// Action buttons
				ImGui::Spacing();
			 float buttonWidth = (ImGui::GetContentRegionAvail().x - 8.0f) * 0.5f;
				
				if (ImGui::Button("Open in Editor", ImVec2(buttonWidth, 0))) {
					std::filesystem::path fullPath = std::filesystem::path("assets") / component.ScriptPath;
					if (std::filesystem::exists(fullPath)) {
						std::string command = "cmd /c start \"\" \"" + fullPath.string() + "\"";
						system(command.c_str());
					}
				}
				
				ImGui::SameLine();
				
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 0.8f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
				if (ImGui::Button("Remove Script", ImVec2(buttonWidth, 0))) {
				 component.ScriptPath.clear();
				 component.CompiledDLLPath.clear();
				 component.IsLoaded = false;
				}
				ImGui::PopStyleColor(2);
			}
			
			// Drag and drop target
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
					ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
					std::string ext = data->Extension;
					if (ext == ".cpp" || ext == ".h") {
						component.ScriptPath = data->RelativePath;
						component.IsLoaded = false;
						LNX_LOG_INFO("Script assigned: {0}", data->RelativePath);
					}
					else {
						LNX_LOG_WARN("Only .cpp files are valid C++ scripts");
					}
				}
				ImGui::EndDragDropTarget();
			}
			
			ImGui::Unindent(8.0f);
			
			// TODO: Public Variables Section (Unity-style)
			// Esta sección se implementará cuando el sistema de reflexión esté listo
			if (component.IsLoaded && !component.ScriptPath.empty()) {
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();
				
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
				ImGui::Text("Script Properties");
				ImGui::PopStyleColor();
				
				ImGui::Indent(8.0f);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
				ImGui::TextWrapped("Public variables will appear here when the reflection system is implemented.");
				ImGui::PopStyleColor();
				ImGui::Unindent(8.0f);
			}
			});

		DrawComponent<CameraComponent>("Camera", entity, [](auto& component) {
			auto& camera = component.Camera;

			// Primary Camera Toggle
			ImGui::Checkbox("Primary Camera", &component.Primary);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("This camera will be used for rendering the scene");
			}
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();

			// Projection Type
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
			ImGui::Text("Projection");
			ImGui::PopStyleColor();
			
			ImGui::Indent(8.0f);

			const char* projectionTypeStrings[] = { "Perspective", "Orthographic" };
			const char* currentProjectionTypeString = projectionTypeStrings[(int)camera.GetProjectionType()];
			
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

			ImGui::Spacing();

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective) {
				float perspectiveVerticalFov = glm::degrees(camera.GetPerspectiveVerticalFOV());
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
				if (ImGui::SliderFloat("Field of View", &perspectiveVerticalFov, 1.0f, 120.0f, "%.1f°"))
					camera.SetPerspectiveVerticalFOV(glm::radians(perspectiveVerticalFov));
				ImGui::PopStyleColor();

				float perspectiveNear = camera.GetPerspectiveNearClip();
				float perspectiveFar = camera.GetPerspectiveFarClip();
				
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
				if (ImGui::DragFloat("Near Plane", &perspectiveNear, 0.01f, 0.01f, perspectiveFar - 0.01f))
					camera.SetPerspectiveNearClip(perspectiveNear);
				ImGui::PopStyleColor();

				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
				if (ImGui::DragFloat("Far Plane", &perspectiveFar, 0.1f, perspectiveNear + 0.01f, 10000.0f))
					camera.SetPerspectiveFarClip(perspectiveFar);
				ImGui::PopStyleColor();
			}

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Orthographic) {
				float orthoSize = camera.GetOrthographicSize();
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
				if (ImGui::DragFloat("Size", &orthoSize, 0.1f, 0.1f, 100.0f))
					camera.SetOrthographicSize(orthoSize);
				ImGui::PopStyleColor();

				float orthoNear = camera.GetOrthographicNearClip();
				float orthoFar = camera.GetOrthographicFarClip();
				
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
				if (ImGui::DragFloat("Near Plane", &orthoNear, 0.1f, -1000.0f, orthoFar - 0.1f))
					camera.SetOrthographicNearClip(orthoNear);
				ImGui::PopStyleColor();

				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
				if (ImGui::DragFloat("Far Plane", &orthoFar, 0.1f, orthoNear + 0.1f, 1000.0f))
					camera.SetOrthographicFarClip(orthoFar);
				ImGui::PopStyleColor();

				ImGui::Checkbox("Fixed Aspect Ratio", &component.FixedAspectRatio);
			}
			
			ImGui::Unindent(8.0f);
			});

		DrawComponent<SpriteRendererComponent>("Sprite Renderer", entity, [](auto& component) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
			ImGui::Text("Sprite");
			ImGui::PopStyleColor();
			
			ImGui::Indent(8.0f);
			
			ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));

			ImGui::Spacing();

			if (component.Texture && component.Texture->IsLoaded()) {
				ImGui::Image(
					(void*)(intptr_t)component.Texture->GetRendererID(),
					ImVec2(100, 100),
					ImVec2(0, 1), ImVec2(1, 0)
				);
				
				ImGui::Text("%dx%d", component.Texture->GetWidth(), component.Texture->GetHeight());
				
				if (ImGui::Button("Remove Texture", ImVec2(-1, 0))) {
					component.Texture.reset();
				}
			}
			else {
				ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.22f, 0.24f, 1.0f));
				ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.26f, 0.28f, 1.0f));
				ImGui::Button("Drop Sprite Texture Here", ImVec2(-1, 80.0f));
				ImGui::PopStyleColor(2);
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

			ImGui::Spacing();
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
			ImGui::DragFloat("Tiling Factor", &component.TilingFactor, 0.1f, 0.0f, 100.0f);
			ImGui::PopStyleColor();
			
			ImGui::Unindent(8.0f);
			});

		DrawComponent<CircleRendererComponent>("Circle Renderer", entity, [](auto& component) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
			ImGui::Text("Circle Properties");
			ImGui::PopStyleColor();
			
			ImGui::Indent(8.0f);
			
			ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));
			
			ImGui::Spacing();
			
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
			ImGui::SliderFloat("Thickness", &component.Thickness, 0.0f, 1.0f, "%.3f");
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("0 = Filled circle, 1 = Thin outline");
			}
			
			ImGui::SliderFloat("Fade", &component.Fade, 0.0f, 1.0f, "%.5f");
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Edge softness");
			}
			ImGui::PopStyleColor();
			
			ImGui::Unindent(8.0f);
			});

		DrawComponent<Rigidbody2DComponent>("Rigidbody 2D", entity, [](auto& component) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
			ImGui::Text("Body Type");
			ImGui::PopStyleColor();
			
			ImGui::Indent(8.0f);
			
			const char* bodyTypeStrings[] = { "Static", "Dynamic", "Kinematic" };
			const char* currentBodyTypeString = bodyTypeStrings[(int)component.Type];
			
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

			ImGui::Spacing();
			ImGui::Checkbox("Fixed Rotation", &component.FixedRotation);
			if (ImGui::IsItemHovered()) {
				ImGui::SetTooltip("Prevent rotation from physics");
			}
			
			ImGui::Unindent(8.0f);
			});

		DrawComponent<BoxCollider2DComponent>("Box Collider 2D", entity, [](auto& component) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
			ImGui::Text("Collider Shape");
			ImGui::PopStyleColor();
			
			ImGui::Indent(8.0f);
			
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
			ImGui::DragFloat2("Offset", glm::value_ptr(component.Offset), 0.01f);
			ImGui::DragFloat2("Size", glm::value_ptr(component.Size), 0.01f, 0.01f);
			ImGui::PopStyleColor();
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
			ImGui::Text("Physics Material");
			ImGui::PopStyleColor();
			
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
			ImGui::DragFloat("Density", &component.Density, 0.01f, 0.0f, 100.0f);
			ImGui::DragFloat("Friction", &component.Friction, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution Threshold", &component.RestitutionThreshold, 0.01f, 0.0f, 10.0f);
			ImGui::PopStyleColor();
			
			ImGui::Unindent(8.0f);
			});

		DrawComponent<CircleCollider2DComponent>("Circle Collider 2D", entity, [](auto& component) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
			ImGui::Text("Collider Shape");
			ImGui::PopStyleColor();
			
			ImGui::Indent(8.0f);
			
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
			ImGui::DragFloat2("Offset", glm::value_ptr(component.Offset), 0.01f);
			ImGui::DragFloat("Radius", &component.Radius, 0.01f, 0.01f);
			ImGui::PopStyleColor();
			
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
			ImGui::Text("Physics Material");
			ImGui::PopStyleColor();
			
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
			ImGui::DragFloat("Density", &component.Density, 0.01f, 0.0f, 100.0f);
			ImGui::DragFloat("Friction", &component.Friction, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution", &component.Restitution, 0.01f, 0.0f, 1.0f);
			ImGui::DragFloat("Restitution Threshold", &component.RestitutionThreshold, 0.01f, 0.0f, 10.0f);
			ImGui::PopStyleColor();
			
			ImGui::Unindent(8.0f);
			});

		DrawComponent<MeshComponent>("Mesh Renderer", entity, [](auto& component) {
			// Model Selection
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
			ImGui::Text("Model");
			ImGui::PopStyleColor();
			
			ImGui::Indent(8.0f);
			
			const char* modelTypes[] = { "Cube", "Sphere", "Plane", "Cylinder", "Custom Model" };
			int currentType = (int)component.Type;

			if (ImGui::Combo("##ModelType", &currentType, modelTypes, IM_ARRAYSIZE(modelTypes))) {
				component.Type = (ModelType)currentType;
				if (component.Type != ModelType::FromFile) {
					component.CreatePrimitive(component.Type);
				}
			}

			// Custom Model Section
			if (component.Type == ModelType::FromFile) {
				ImGui::Spacing();
				
				if (component.MeshModel) {
					// Model loaded - show info
					ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.16f, 0.16f, 0.17f, 1.0f));
					ImGui::BeginChild("##ModelInfo", ImVec2(-1, 90.0f), true);
					
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
					ImGui::Text("🗿");
					ImGui::PopStyleColor();
					ImGui::SameLine();
					
					std::filesystem::path modelPath(component.FilePath);
					ImGui::Text("%s", modelPath.filename().string().c_str());
					
					ImGui::Spacing();
					ImGui::Separator();
					ImGui::Spacing();
					
					// Stats
					uint32_t totalVertices = 0;
					uint32_t totalIndices = 0;
					for (const auto& mesh : component.MeshModel->GetMeshes()) {
						totalVertices += (uint32_t)mesh->GetVertices().size();
						totalIndices += (uint32_t)mesh->GetIndices().size();
					}
					
					ImGui::Columns(2);
					ImGui::Text("Submeshes:"); ImGui::NextColumn();
					ImGui::Text("%d", component.MeshModel->GetMeshes().size()); ImGui::NextColumn();
					
					ImGui::Text("Vertices:"); ImGui::NextColumn();
					ImGui::Text("%d", totalVertices); ImGui::NextColumn();
					
					ImGui::Text("Triangles:"); ImGui::NextColumn();
					ImGui::Text("%d", totalIndices / 3); ImGui::NextColumn();
					ImGui::Columns(1);
					
					ImGui::EndChild();
					ImGui::PopStyleColor();
					
					ImGui::Spacing();
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.7f, 0.2f, 0.2f, 0.8f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.8f, 0.3f, 0.3f, 1.0f));
					if (ImGui::Button("Remove Model", ImVec2(-1, 0))) {
						component.FilePath.clear();
						component.MeshModel.reset();
					}
					ImGui::PopStyleColor(2);
				}
				else {
					// No model - show drop zone
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.22f, 0.22f, 0.24f, 1.0f));
					ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.26f, 0.28f, 1.0f));
					ImGui::Button("Drop 3D Model Here\n(.obj, .fbx, .gltf, .glb)", ImVec2(-1, 50.0f));
					ImGui::PopStyleColor(2);
					
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
			
			ImGui::Unindent(8.0f);
			
			// Color Tint
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
			ImGui::Text("Color Tint");
			ImGui::PopStyleColor();
			ImGui::Indent(8.0f);
			ImGui::ColorEdit4("##MeshColor", glm::value_ptr(component.Color), ImGuiColorEditFlags_NoLabel);
			ImGui::Unindent(8.0f);
			});

		DrawComponent<MaterialComponent>("Material", entity, [&](auto& component) {

			// ========== SURFACE PROPERTIES ==========
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
			ImGui::Text("Surface Properties");
			ImGui::PopStyleColor();
			
			ImGui::Indent(8.0f);

			glm::vec4 color = component.GetColor();
			if (ImGui::ColorEdit4("Base Color", glm::value_ptr(color), ImGuiColorEditFlags_AlphaBar)) {
				component.SetColor(color);
			}

			float metallic = component.GetMetallic();
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
			if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f)) {
				component.SetMetallic(metallic);
			}
			ImGui::PopStyleColor();

			float roughness = component.GetRoughness();
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
			if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f)) {
				component.SetRoughness(roughness);
			}
			ImGui::PopStyleColor();

			float specular = component.GetSpecular();
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
			if (ImGui::SliderFloat("Specular", &specular, 0.0f, 1.0f)) {
				component.SetSpecular(specular);
			}
			ImGui::PopStyleColor();
			
			ImGui::Unindent(8.0f);

			// ========== EMISSION ==========
			ImGui::Spacing();
			ImGui::Separator();
			ImGui::Spacing();
			
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
			ImGui::Text("Emission");
			ImGui::PopStyleColor();
			
			ImGui::Indent(8.0f);

			glm::vec3 emissionColor = component.GetEmissionColor();
			if (ImGui::ColorEdit3("Color", glm::value_ptr(emissionColor))) {
				component.SetEmissionColor(emissionColor);
			}

			float emissionIntensity = component.GetEmissionIntensity();
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
			if (ImGui::DragFloat("Intensity", &emissionIntensity, 0.1f, 0.0f, 100.0f)) {
				component.SetEmissionIntensity(emissionIntensity);
			}
			ImGui::PopStyleColor();
			
			ImGui::Unindent(8.0f);
			
			// ========== TEXTURES (INTEGRATED) ==========
			if (entity.HasComponent<TextureComponent>()) {
				auto& texComp = entity.GetComponent<TextureComponent>();
				
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();
				
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
				ImGui::Text("Texture Maps");
				ImGui::PopStyleColor();
				
				ImGui::Indent(8.0f);
				
				// Helper lambda SIN COLUMNS - layout vertical simple
				auto DrawTextureSlot = [&](const char* label, Ref<Texture2D>& texture, std::string& path,
					auto loadFunc, float* multiplier = nullptr) {
					
					ImGui::PushID(label);
					
					// Label
					ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
					ImGui::Text("%s", label);
					ImGui::PopStyleColor();
					
					ImGui::Indent(8.0f);
					
					// Layout horizontal con BeginGroup/SameLine
					ImGui::BeginGroup();
					
					// Thumbnail
					if (texture && texture->IsLoaded()) {
						ImGui::Image(
							(void*)(intptr_t)texture->GetRendererID(),
							ImVec2(50, 50),
							ImVec2(0, 1), ImVec2(1, 0)
						);
					}
					else {
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.20f, 0.20f, 0.22f, 1.0f));
						ImGui::Button("##empty", ImVec2(50, 50));
						ImGui::PopStyleColor();
					}
					
					// Drag & Drop
					if (ImGui::BeginDragDropTarget()) {
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
							ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
							std::string ext = data->Extension;
							if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr") {
								loadFunc(texComp, data->FilePath);
							}
						}
						ImGui::EndDragDropTarget();
					}
					
					ImGui::EndGroup();
					ImGui::SameLine();
					
					// Info + Actions
					ImGui::BeginGroup();
					
					if (texture && texture->IsLoaded()) {
						std::filesystem::path texPath(path);
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.26f, 0.59f, 0.98f, 1.0f));
						ImGui::Text("%s", texPath.filename().string().c_str());
						ImGui::PopStyleColor();
						
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
						ImGui::Text("%dx%d", texture->GetWidth(), texture->GetHeight());
						ImGui::PopStyleColor();
						
						// Multiplier if exists
						if (multiplier) {
							ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
							ImGui::SetNextItemWidth(120.0f);
							ImGui::SliderFloat("##mult", multiplier, 0.0f, 2.0f, "×%.2f");
							ImGui::PopStyleColor();
						}
						
						// Remove button
						ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.6f, 0.2f, 0.2f, 0.7f));
						ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.7f, 0.3f, 0.3f, 1.0f));
						if (ImGui::Button("Remove", ImVec2(80, 0))) {
							texture.reset();
							path.clear();
						}
						ImGui::PopStyleColor(2);
					}
					else {
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
						ImGui::TextWrapped("Drop texture here");
						ImGui::PopStyleColor();
					}
					
					ImGui::EndGroup();
					
					ImGui::Unindent(8.0f);
					ImGui::PopID();
					ImGui::Spacing();
				};

				DrawTextureSlot("Albedo", texComp.AlbedoMap, texComp.AlbedoPath,
					[](TextureComponent& tc, const std::string& path) { tc.LoadAlbedo(path); });

				DrawTextureSlot("Normal", texComp.NormalMap, texComp.NormalPath,
					[](TextureComponent& tc, const std::string& path) { tc.LoadNormal(path); });

				DrawTextureSlot("Metallic", texComp.MetallicMap, texComp.MetallicPath,
					[](TextureComponent& tc, const std::string& path) { tc.LoadMetallic(path); },
					&texComp.MetallicMultiplier);

				DrawTextureSlot("Roughness", texComp.RoughnessMap, texComp.RoughnessPath,
					[](TextureComponent& tc, const std::string& path) { tc.LoadRoughness(path); },
					&texComp.RoughnessMultiplier);

				DrawTextureSlot("Specular", texComp.SpecularMap, texComp.SpecularPath,
					[](TextureComponent& tc, const std::string& path) { tc.LoadSpecular(path); },
					&texComp.SpecularMultiplier);

				DrawTextureSlot("Emission", texComp.EmissionMap, texComp.EmissionPath,
					[](TextureComponent& tc, const std::string& path) { tc.LoadEmission(path); });

				DrawTextureSlot("AO", texComp.AOMap, texComp.AOPath,
					[](TextureComponent& tc, const std::string& path) { tc.LoadAO(path); },
					&texComp.AOMultiplier);
				
				ImGui::Unindent(8.0f);
			}
			else {
				// Si no tiene TextureComponent, ofrecer añadirlo
				ImGui::Spacing();
				ImGui::Separator();
				ImGui::Spacing();
				
				ImGui::Indent(8.0f);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
				ImGui::TextWrapped("Add 'Textures Mapper' component to use texture maps");
				ImGui::PopStyleColor();
				ImGui::Unindent(8.0f);
			}
			});

		DrawComponent<LightComponent>("Light", entity, [](auto& component) {
			const char* lightTypes[] = { "Directional", "Point", "Spot" };
			int currentType = (int)component.GetType();

			if (ImGui::Combo("Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes))) {
				component.SetType((LightType)currentType);
			}

			ImGui::Separator();
			ImGui::Text("Basic Properties");
			ImGui::Separator();

			glm::vec3 color = component.GetColor();
			if (ImGui::ColorEdit3("Color", glm::value_ptr(color))) {
				component.SetColor(color);
			}

			float intensity = component.GetIntensity();
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
			if (ImGui::DragFloat("Intensity", &intensity, 0.1f, 0.0f, 100.0f)) {
				component.SetIntensity(intensity);
			}
			ImGui::PopStyleColor();

			if (component.GetType() == LightType::Point || component.GetType() == LightType::Spot) {
				ImGui::Spacing();
				ImGui::Text("Attenuation");
				ImGui::Separator();

			 float range = component.GetRange();
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
				if (ImGui::DragFloat("Range", &range, 0.1f, 0.0f, 100.0f)) {
					component.SetRange(range);
				}
				ImGui::PopStyleColor();

				glm::vec3 attenuation = component.GetAttenuation();
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
				if (ImGui::DragFloat3("Attenuation (C/L/Q)", glm::value_ptr(attenuation), 0.001f, 0.0f, 10.0f)) {
					component.SetAttenuation(attenuation);
				}
				ImGui::PopStyleColor();
			}

			if (component.GetType() == LightType::Spot) {
				ImGui::Spacing();
				ImGui::Text("Spot Properties");
				ImGui::Separator();

				float innerAngle = component.GetInnerConeAngle();
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
				if (ImGui::SliderFloat("Inner Cone Angle", &innerAngle, 0.0f, 90.0f, "%.1f°")) {
					component.SetInnerConeAngle(innerAngle);
				}
				ImGui::PopStyleColor();

				float outerAngle = component.GetOuterConeAngle();
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 0.26f, 0.59f, 0.98f, 0.50f });
				if (ImGui::SliderFloat("Outer Cone Angle", &outerAngle, 0.0f, 90.0f, "%.1f°")) {
					component.SetOuterConeAngle(outerAngle);
				}
				ImGui::PopStyleColor();
			}

			ImGui::Spacing();
			ImGui::Text("Shadows");
			ImGui::Separator();

			bool castShadows = component.GetCastShadows();
				if (ImGui::Checkbox("Cast Shadows", &castShadows)) {
					component.SetCastShadows(castShadows);
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