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

		// Cargar iconos para la jerarquía (usar rutas absolutas o relativas al working directory)
		m_CameraIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/CameraIcon.png");
		m_EntityIcon = Texture2D::Create("Resources/Icons/HierarchyPanel/EntityIcon.png");

		// Debug: verificar si los iconos se cargaron correctamente
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
			// entt::registry::each ya no existe en versiones recientes -> usar view<entt::entity>().each
			m_Context->m_Registry.view<entt::entity>().each([&](auto entityID) {
				Entity entity{ entityID , m_Context.get() };
				DrawEntityNode(entity);
				});

			if (ImGui::IsMouseDown(0) && ImGui::IsWindowHovered())
				m_SelectionContext = {};

			// Click derecho en espacio en blanco
			// API moderna de ImGui: usar flags en lugar de (mouse_button, also_over_items)
			if (ImGui::BeginPopupContextWindow(nullptr, ImGuiPopupFlags_MouseButtonRight | ImGuiPopupFlags_NoOpenOverItems)) {
				if (ImGui::MenuItem("Create Empty Entity"))
					m_Context->CreateEntity("Empty Entity");
				ImGui::EndPopup();
			}
		}
		ImGui::End();

		ImGui::Begin("Properties");
		if (m_SelectionContext) {
			DrawComponents(m_SelectionContext);
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

		// Determinar qué icono usar
		Ref<Texture2D> icon = entity.HasComponent<CameraComponent>() ? m_CameraIcon : m_EntityIcon;

		// Guardar el cursor actual
		ImVec2 cursorPos = ImGui::GetCursorPos();

		// Si hay icono, dibujarlo
		if (icon) {
			float iconSize = ImGui::GetFrameHeight();

			// Dibujar el icono
			ImGui::Image(
				(void*)(intptr_t)icon->GetRendererID(),
				ImVec2(iconSize, iconSize),
				ImVec2(0, 1),
				ImVec2(1, 0)
			);

			// Mover cursor al lado del icono
			ImGui::SameLine();
			ImGui::SetCursorPosY(cursorPos.y);
		}

		// Dibujar el TreeNode
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
				// Accept new texture-specific payload
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_PATH")) {
					const char* path = (const char*)payload->Data;
					Ref<Texture2D> texture = Texture2D::Create(path);
					if (texture->IsLoaded())
						component.Texture = texture;
					else
						LNX_LOG_WARN("Could not load texture {0}", std::filesystem::path(path).filename().string());
				}
				// Fallback to old payload for compatibility
				else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
					const wchar_t* wpath = (const wchar_t*)payload->Data;
					std::filesystem::path texturePath = std::filesystem::path(g_AssetPath) / wpath;
					Ref<Texture2D> texture = Texture2D::Create(texturePath.string());
					if (texture->IsLoaded())
						component.Texture = texture;
					else
						LNX_LOG_WARN("Could not load texture {0}", texturePath.filename().string());
				}
				// Generic file path
				else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_PATH")) {
					const char* path = (const char*)payload->Data;
					std::string ext = std::filesystem::path(path).extension().string();
					std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
					
					if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".bmp" || ext == ".tga") {
						Ref<Texture2D> texture = Texture2D::Create(path);
						if (texture->IsLoaded())
							component.Texture = texture;
						else
							LNX_LOG_WARN("Could not load texture {0}", std::filesystem::path(path).filename().string());
					}
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
				for (int i = 0; i < 2; i++) {
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

		DrawComponent<MeshComponent>("Mesh Renderer", entity, [](auto& component) {
			ImGui::ColorEdit4("Color", glm::value_ptr(component.Color));

			// Model type selector
			const char* modelTypes[] = { "Cube", "Sphere", "Plane", "Cylinder", "From File" };
			int currentType = (int)component.Type;

			if (ImGui::Combo("Model Type", &currentType, modelTypes, IM_ARRAYSIZE(modelTypes))) {
				component.Type = (ModelType)currentType;

				if (component.Type != ModelType::FromFile) {
					component.CreatePrimitive(component.Type);
				}
			}

			// File path input for FromFile type
			if (component.Type == ModelType::FromFile) {
				ImGui::Text("File Path:");
				ImGui::SameLine();

				char buffer[256];
				memset(buffer, 0, sizeof(buffer));
				if (!component.FilePath.empty()) {
					strcpy_s(buffer, sizeof(buffer), component.FilePath.c_str());
				}

				if (ImGui::InputText("##FilePath", buffer, sizeof(buffer))) {
					component.FilePath = std::string(buffer);
				}

				ImGui::SameLine();
				if (ImGui::Button("Load")) {
					if (!component.FilePath.empty()) {
						component.LoadFromFile(component.FilePath);
					}
				}

				// Drag and drop support
				ImGui::Button("Drop Model Here", ImVec2(200.0f, 30.0f));
				if (ImGui::BeginDragDropTarget()) {
					// Accept new model-specific payload
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("MODEL_PATH")) {
						const char* path = (const char*)payload->Data;
						std::string ext = std::filesystem::path(path).extension().string();
						std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

						if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb") {
							component.LoadFromFile(path);
						}
						else {
							LNX_LOG_WARN("Unsupported model format: {0}", ext);
						}
					}
					// Fallback to old payload for compatibility
					else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
						const wchar_t* wpath = (const wchar_t*)payload->Data;
						std::filesystem::path modelPath = std::filesystem::path(g_AssetPath) / wpath;

						std::string ext = modelPath.extension().string();
						std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

						if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb") {
							component.LoadFromFile(modelPath.string());
						}
						else {
							LNX_LOG_WARN("Unsupported model format: {0}", ext);
						}
					}
					// Generic file path
					else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_PATH")) {
						const char* path = (const char*)payload->Data;
						std::string ext = std::filesystem::path(path).extension().string();
						std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);

						if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb" || ext == ".dae") {
							component.LoadFromFile(path);
						}
						else {
							LNX_LOG_WARN("Unsupported model format: {0}", ext);
						}
					}
					ImGui::EndDragDropTarget();
				}
			}

			// Display mesh info
			if (component.MeshModel) {
				ImGui::Separator();
				ImGui::Text("Mesh Info:");
				ImGui::Text("  Submeshes: %d", component.MeshModel->GetMeshes().size());

				uint32_t totalVertices = 0;
				uint32_t totalIndices = 0;
				for (const auto& mesh : component.MeshModel->GetMeshes()) {
					totalVertices += (uint32_t)mesh->GetVertices().size();
					totalIndices += (uint32_t)mesh->GetIndices().size();
				}
				ImGui::Text("  Total Vertices: %d", totalVertices);
				ImGui::Text("  Total Triangles: %d", totalIndices / 3);
			}
			});

		DrawComponent<MaterialComponent>("Material", entity, [](auto& component) {
			ImGui::Text("Surface Properties");
			ImGui::Separator();

			// Color
			glm::vec4 color = component.GetColor();
			if (ImGui::ColorEdit4("Base Color", glm::value_ptr(color))) {
				component.SetColor(color);
			}

			// Metallic
			float metallic = component.GetMetallic();
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
			if (ImGui::SliderFloat("Metallic", &metallic, 0.0f, 1.0f)) {
				component.SetMetallic(metallic);
			}
			ImGui::PopStyleColor();

			// Roughness
			float roughness = component.GetRoughness();
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
			if (ImGui::SliderFloat("Roughness", &roughness, 0.0f, 1.0f)) {
				component.SetRoughness(roughness);
			}
			ImGui::PopStyleColor();

			// Specular
			float specular = component.GetSpecular();
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
			if (ImGui::SliderFloat("Specular", &specular, 0.0f, 1.0f)) {
				component.SetSpecular(specular);
			}
			ImGui::PopStyleColor();

			ImGui::Spacing();
			ImGui::Text("Emission");
			ImGui::Separator();

			// Emission Color
			glm::vec3 emissionColor = component.GetEmissionColor();
			if (ImGui::ColorEdit3("Emission Color", glm::value_ptr(emissionColor))) {
				component.SetEmissionColor(emissionColor);
			}

			// Emission Intensity
			float emissionIntensity = component.GetEmissionIntensity();
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
			if (ImGui::DragFloat("Emission Intensity", &emissionIntensity, 0.1f, 0.0f, 100.0f)) {
				component.SetEmissionIntensity(emissionIntensity);
			}
			ImGui::PopStyleColor();
			});

		DrawComponent<LightComponent>("Light", entity, [](auto& component) {
			// Light Type selector
			const char* lightTypes[] = { "Directional", "Point", "Spot" };
			int currentType = (int)component.GetType();

			if (ImGui::Combo("Type", &currentType, lightTypes, IM_ARRAYSIZE(lightTypes))) {
				component.SetType((LightType)currentType);
			}

			ImGui::Separator();
			ImGui::Text("Basic Properties");
			ImGui::Separator();

			// Color
			glm::vec3 color = component.GetColor();
			if (ImGui::ColorEdit3("Color", glm::value_ptr(color))) {
				component.SetColor(color);
			}

			// Intensity
			float intensity = component.GetIntensity();
			ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
			if (ImGui::DragFloat("Intensity", &intensity, 0.1f, 0.0f, 100.0f)) {
				component.SetIntensity(intensity);
			}
			ImGui::PopStyleColor();

			// Point and Spot specific properties
			if (component.GetType() == LightType::Point || component.GetType() == LightType::Spot) {
				ImGui::Spacing();
				ImGui::Text("Attenuation");
				ImGui::Separator();

				// Range
				float range = component.GetRange();
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
				if (ImGui::DragFloat("Range", &range, 0.1f, 0.0f, 100.0f)) {
					component.SetRange(range);
				}
				ImGui::PopStyleColor();

				// Attenuation
				glm::vec3 attenuation = component.GetAttenuation();
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
				if (ImGui::DragFloat3("Attenuation (C/L/Q)", glm::value_ptr(attenuation), 0.001f, 0.0f, 10.0f)) {
					component.SetAttenuation(attenuation);
				}
				ImGui::PopStyleColor();
			}

			// Spot specific properties
			if (component.GetType() == LightType::Spot) {
				ImGui::Spacing();
				ImGui::Text("Spot Properties");
				ImGui::Separator();

				// Inner Cone Angle
				float innerAngle = component.GetInnerConeAngle();
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
				if (ImGui::SliderFloat("Inner Cone Angle", &innerAngle, 0.0f, 90.0f, "%.1f°")) {
					component.SetInnerConeAngle(innerAngle);
				}
				ImGui::PopStyleColor();

				// Outer Cone Angle
				float outerAngle = component.GetOuterConeAngle();
				ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
				if (ImGui::SliderFloat("Outer Cone Angle", &outerAngle, 0.0f, 90.0f, "%.1f°")) {
					component.SetOuterConeAngle(outerAngle);
				}
				ImGui::PopStyleColor();
			}

			ImGui::Spacing();
			ImGui::Text("Shadows");
			ImGui::Separator();

			// Cast Shadows
			bool castShadows = component.GetCastShadows();
			if (ImGui::Checkbox("Cast Shadows", &castShadows)) {
				component.SetCastShadows(castShadows);
			}
			});

		DrawComponent<TextureComponent>("Textures", entity, [](auto& component) {
			ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(10, 10));
			ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(10, 10));

			const float imageSize = 64.0f;
			const float labelWidth = 120.0f;

			// Helper lambda to draw texture slot
			auto DrawTextureSlot = [&](const char* label, Ref<Texture2D>& texture, std::string& path, 
										auto loadFunc, float multiplier = 1.0f, bool hasMultiplier = false) {
				ImGui::PushID(label);
				
				// Draw label
				ImGui::Text(label);
				ImGui::SameLine(labelWidth);
				
				// Draw texture preview or placeholder
				ImVec2 cursorPos = ImGui::GetCursorPos();
				
				if (texture && texture->IsLoaded()) {
					// Draw texture preview
					ImGui::Image(
						(void*)(intptr_t)texture->GetRendererID(),
						ImVec2(imageSize, imageSize),
						ImVec2(0, 1),
						ImVec2(1, 0)
					);
				} else {
					// Draw placeholder
					ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.2f, 1.0f));
					ImGui::Button("##empty", ImVec2(imageSize, imageSize));
					ImGui::PopStyleColor();
				}
				
				// Drag and drop target
				if (ImGui::BeginDragDropTarget()) {
					// Accept new texture-specific payload
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("TEXTURE_PATH")) {
						const char* texturePath = (const char*)payload->Data;
						std::string ext = std::filesystem::path(texturePath).extension().string();
						std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
						
						if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr") {
							loadFunc(component, texturePath);
						} else {
							LNX_LOG_WARN("Unsupported texture format: {0}", ext);
						}
					}
					// Fallback to old payload for compatibility
					else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
						const wchar_t* wpath = (const wchar_t*)payload->Data;
						std::filesystem::path texturePath = std::filesystem::path(g_AssetPath) / wpath;
						
						std::string ext = texturePath.extension().string();
						std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
						
						if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr") {
							loadFunc(component, texturePath.string());
						} else {
							LNX_LOG_WARN("Unsupported texture format: {0}", ext);
						}
					}
					// Generic file path
					else if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("FILE_PATH")) {
						const char* texturePath = (const char*)payload->Data;
						std::string ext = std::filesystem::path(texturePath).extension().string();
						std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
						
						if (ext == ".png" || ext == ".jpg" || ext == ".jpeg" || ext == ".tga" || ext == ".bmp" || ext == ".hdr") {
							loadFunc(component, texturePath);
						} else {
							LNX_LOG_WARN("Unsupported texture format: {0}", ext);
						}
					}
					ImGui::EndDragDropTarget();
				}
				
				ImGui::SameLine();
				
				// Draw texture info and controls
				ImGui::BeginGroup();
				
				if (texture && texture->IsLoaded()) {
					// Show texture name
					std::filesystem::path texPath(path);
					std::string filename = texPath.filename().string();
					ImGui::TextWrapped("%s", filename.c_str());
					
					// Show texture resolution
					ImGui::Text("%dx%d", texture->GetWidth(), texture->GetHeight());
					
					// Remove button
					if (ImGui::Button("Remove")) {
						texture.reset();
						path.clear();
					}
				} else {
					ImGui::TextDisabled("No texture");
					ImGui::TextDisabled("Drag & Drop here");
				}
				
				// Multiplier slider if applicable
				if (hasMultiplier && texture) {
					ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4{ 1.0f, 0.55f, 0.0f, 0.7f });
					
					if (label == std::string("Metallic")) {
						ImGui::SliderFloat("##mult", &component.MetallicMultiplier, 0.0f, 2.0f, "%.2f");
					} else if (label == std::string("Roughness")) {
						ImGui::SliderFloat("##mult", &component.RoughnessMultiplier, 0.0f, 2.0f, "%.2f");
					} else if (label == std::string("Specular")) {
						ImGui::SliderFloat("##mult", &component.SpecularMultiplier, 0.0f, 2.0f, "%.2f");
					} else if (label == std::string("AO")) {
						ImGui::SliderFloat("##mult", &component.AOMultiplier, 0.0f, 2.0f, "%.2f");
					}
					
					ImGui::PopStyleColor(); // Balance the Push
				}
				
				ImGui::EndGroup();
				
				ImGui::PopID();
				ImGui::Separator();
			};

			// Albedo
			DrawTextureSlot("Albedo", component.AlbedoMap, component.AlbedoPath,
				[](TextureComponent& tc, const std::string& path) { tc.LoadAlbedo(path); });

			// Normal
			DrawTextureSlot("Normal", component.NormalMap, component.NormalPath,
				[](TextureComponent& tc, const std::string& path) { tc.LoadNormal(path); });

			// Metallic
			DrawTextureSlot("Metallic", component.MetallicMap, component.MetallicPath,
				[](TextureComponent& tc, const std::string& path) { tc.LoadMetallic(path); },
				component.MetallicMultiplier, true);

			// Roughness
			DrawTextureSlot("Roughness", component.RoughnessMap, component.RoughnessPath,
				[](TextureComponent& tc, const std::string& path) { tc.LoadRoughness(path); },
				component.RoughnessMultiplier, true);

			// Specular
			DrawTextureSlot("Specular", component.SpecularMap, component.SpecularPath,
				[](TextureComponent& tc, const std::string& path) { tc.LoadSpecular(path); },
				component.SpecularMultiplier, true);

			// Emission
			DrawTextureSlot("Emission", component.EmissionMap, component.EmissionPath,
				[](TextureComponent& tc, const std::string& path) { tc.LoadEmission(path); });

			// AO
			DrawTextureSlot("AO", component.AOMap, component.AOPath,
				[](TextureComponent& tc, const std::string& path) { tc.LoadAO(path); },
				component.AOMultiplier, true);

			ImGui::PopStyleVar(2);
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