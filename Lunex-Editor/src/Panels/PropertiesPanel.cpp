/**
 * @file PropertiesPanel.cpp
 * @brief Properties Panel Implementation using Lunex UI Framework
 */

#include "stpch.h"
#include "PropertiesPanel.h"
#include "ContentBrowserPanel.h"

#include <glm/gtc/type_ptr.hpp>
#include <filesystem>

#include "Scene/Components.h"
#include "Assets/Mesh/MeshAsset.h"

namespace Lunex {
	extern const std::filesystem::path g_AssetPath;

	// ============================================================================
	// CONSTRUCTOR & SETUP
	// ============================================================================
	
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

	// ============================================================================
	// THUMBNAIL SYSTEM
	// ============================================================================

	Ref<Texture2D> PropertiesPanel::GetOrGenerateThumbnail(const Ref<MaterialAsset>& asset) {
		if (!asset) return nullptr;

		UUID assetID = asset->GetID();

		auto it = m_ThumbnailCache.find(assetID);
		if (it != m_ThumbnailCache.end() && it->second) {
			return it->second;
		}

		if (!m_PreviewRenderer) {
			LNX_LOG_INFO("MaterialPreviewRenderer initializing (lazy)...");
			try {
				m_PreviewRenderer = CreateScope<MaterialPreviewRenderer>();
				m_PreviewRenderer->SetResolution(128, 128);
				m_PreviewRenderer->SetAutoRotate(false);
			}
			catch (const std::exception& e) {
				LNX_LOG_ERROR("Failed to initialize MaterialPreviewRenderer: {0}", e.what());
				return nullptr;
			}
		}

		try {
			Ref<Texture2D> thumbnail = m_PreviewRenderer->RenderToTexture(asset);
			if (thumbnail) {
				m_ThumbnailCache[assetID] = thumbnail;
				return thumbnail;
			}
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Failed to generate thumbnail for material {0}: {1}", asset->GetName(), e.what());
		}

		return nullptr;
	}

	void PropertiesPanel::InvalidateMaterialThumbnail(UUID assetID) {
		auto it = m_ThumbnailCache.find(assetID);
		if (it != m_ThumbnailCache.end()) {
			m_ThumbnailCache.erase(it);
		}
	}

	void PropertiesPanel::ClearThumbnailCache() {
		m_ThumbnailCache.clear();
		LNX_LOG_TRACE("Cleared material thumbnail cache");
	}

	// ============================================================================
	// MAIN RENDER
	// ============================================================================

	void PropertiesPanel::OnImGuiRender() {
		using namespace UI;
		
		if (!BeginPanel("Properties")) {
			EndPanel();
			return;
		}
		
		if (m_SelectedEntity && m_SelectedEntity.GetScene() == m_Context.get()) {
			DrawComponents(m_SelectedEntity);
		}
		else {
			if (m_SelectedEntity && m_SelectedEntity.GetScene() != m_Context.get()) {
				m_SelectedEntity = {};
			}
			DrawEmptyState();
		}

		EndPanel();
	}
	
	void PropertiesPanel::DrawEmptyState() {
		using namespace UI;
		
		float windowWidth = ImGui::GetContentRegionAvail().x;
		float windowHeight = ImGui::GetWindowHeight();
		
		ImGui::SetCursorPosX((windowWidth - ImGui::CalcTextSize("No entity selected").x) * 0.5f);
		ImGui::SetCursorPosY(windowHeight * 0.4f);
		
		TextStyled("No entity selected", TextVariant::Muted);
	}

	// ============================================================================
	// DRAW COMPONENTS
	// ============================================================================

	void PropertiesPanel::DrawComponents(Entity entity) {
		using namespace UI;
		
		// Entity Tag Header
		if (entity.HasComponent<TagComponent>()) {
			auto& tag = entity.GetComponent<TagComponent>().Tag;
			
			{
				ScopedStyle padding(ImGuiStyleVar_FramePadding, ImVec2(8.0f, 8.0f));
				ScopedColor colors({
					{ImGuiCol_FrameBg, ComponentStyle::BgDark()},
					{ImGuiCol_FrameBgHovered, ComponentStyle::BgMedium()}
				});
				
				char buffer[256];
				memset(buffer, 0, sizeof(buffer));
				strcpy_s(buffer, sizeof(buffer), tag.c_str());
				
				ImGui::SetNextItemWidth(-1);
				if (ImGui::InputText("##Tag", buffer, sizeof(buffer))) {
					tag = std::string(buffer);
				}
			}
		}

		AddSpacing(SpacingValues::SM);

		// Add Component Button
		if (Button("+ Add Component", ButtonVariant::Primary, ButtonSize::Large, Size(-1, 32))) {
			OpenPopup("AddComponent");
		}

		if (BeginPopup("AddComponent")) {
			{
				ScopedColor textColor(ImGuiCol_Text, ComponentStyle::HeaderColor());
				ImGui::Text("Add Component");
			}
			Separator();

			DisplayAddComponentEntry<CameraComponent>("Camera");
			DisplayAddComponentEntry<ScriptComponent>("C++ Script");
			DisplayAddComponentEntry<SpriteRendererComponent>("Sprite Renderer");
			DisplayAddComponentEntry<CircleRendererComponent>("Circle Renderer");
			DisplayAddComponentEntry<MeshComponent>("Mesh Renderer");
			DisplayAddComponentEntry<LightComponent>("Light");

			Separator();
			{
				ScopedColor textColor(ImGuiCol_Text, ComponentStyle::SubheaderColor());
				ImGui::Text("Physics 2D");
			}

			DisplayAddComponentEntry<Rigidbody2DComponent>("Rigidbody 2D");
			DisplayAddComponentEntry<BoxCollider2DComponent>("Box Collider 2D");
			DisplayAddComponentEntry<CircleCollider2DComponent>("Circle Collider 2D");

			Separator();
			{
				ScopedColor textColor(ImGuiCol_Text, ComponentStyle::SubheaderColor());
				ImGui::Text("Physics 3D");
			}

			DisplayAddComponentEntry<Rigidbody3DComponent>("Rigidbody 3D");
			DisplayAddComponentEntry<BoxCollider3DComponent>("Box Collider 3D");
			DisplayAddComponentEntry<SphereCollider3DComponent>("Sphere Collider 3D");
			DisplayAddComponentEntry<CapsuleCollider3DComponent>("Capsule Collider 3D");
			DisplayAddComponentEntry<MeshCollider3DComponent>("Mesh Collider 3D");

			EndPopup();
		}

		AddSpacing(SpacingValues::SM);
		Separator();
		AddSpacing(SpacingValues::SM);

		// Draw all components
		DrawTransformComponent(entity);
		DrawScriptComponent(entity);
		DrawCameraComponent(entity);
		DrawSpriteRendererComponent(entity);
		DrawCircleRendererComponent(entity);
		DrawMeshComponent(entity);
		DrawMaterialComponent(entity);
		DrawLightComponent(entity);
		DrawRigidbody2DComponent(entity);
		DrawBoxCollider2DComponent(entity);
		DrawCircleCollider2DComponent(entity);
		DrawRigidbody3DComponent(entity);
		DrawBoxCollider3DComponent(entity);
		DrawSphereCollider3DComponent(entity);
		DrawCapsuleCollider3DComponent(entity);
		DrawMeshCollider3DComponent(entity);
	}

	// ============================================================================
	// TRANSFORM COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawTransformComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<TransformComponent>("Transform", entity, [](auto& component) {
			Vec3Control("Translation", component.Translation);
			glm::vec3 rotation = glm::degrees(component.Rotation);
			if (Vec3Control("Rotation", rotation)) {
				component.Rotation = glm::radians(rotation);
			}
			Vec3Control("Scale", component.Scale, 1.0f);
		}, false); // Transform cannot be removed
	}

	// ============================================================================
	// SCRIPT COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawScriptComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<ScriptComponent>("Script", entity, [](auto& component) {
			ComponentDrawer::DrawSectionHeader("", "C++ Scripts");
			ComponentDrawer::BeginIndent();

			for (size_t i = 0; i < component.GetScriptCount(); i++) {
				ScopedID scriptID((int)i);

				const std::string& scriptPath = component.GetScriptPath(i);
				std::filesystem::path path(scriptPath);
				std::string filename = path.filename().string();
				bool isLoaded = component.IsScriptLoaded(i);

				if (ComponentDrawer::BeginInfoCard("##ScriptCard" + std::to_string(i), 100.0f)) {
					{
						ScopedColor textColor(ImGuiCol_Text, ComponentStyle::HintColor());
						Text("Script #%d", (int)i + 1);
					}
					
					SameLine(ImGui::GetContentRegionAvail().x - 65);
					
					if (Button("Remove", ButtonVariant::Danger, ButtonSize::Small, Size(65, 0))) {
						component.RemoveScript(i);
						ComponentDrawer::EndInfoCard();
						break;
					}
					
					Separator();
					AddSpacing(SpacingValues::XS);
					
					{
						ScopedColor textColor(ImGuiCol_Text, ComponentStyle::AccentColor());
						Text("File:");
					}
					SameLine();
					TextWrapped(filename, TextVariant::Primary);
					
					AddSpacing(SpacingValues::XS);
					
					Text("Status:");
					SameLine();
					if (isLoaded) {
						TextStyled("Loaded", TextVariant::Success);
					}
					else {
						TextStyled("Will compile on Play", TextVariant::Warning);
					}
				}
				ComponentDrawer::EndInfoCard();
				AddSpacing(SpacingValues::XS);
			}

			// Add Script button
			{
				ScopedColor colors({
					{ImGuiCol_Button, ComponentStyle::BgMedium()},
					{ImGuiCol_Border, ComponentStyle::AccentColor()}
				});
				ScopedStyle borderSize(ImGuiStyleVar_FrameBorderSize, 1.0f);
				
				if (Button("+ Add Script", ButtonVariant::Default, ButtonSize::Large, Size(-1, 35))) {
					// Drag & drop zone
				}
			}

			// Drag and drop
			void* payloadData = nullptr;
			if (ComponentDrawer::AcceptDropPayload("CONTENT_BROWSER_ITEM", &payloadData)) {
				ContentBrowserPayload* data = (ContentBrowserPayload*)payloadData;
				std::string ext = data->Extension;
				if (ext == ".cpp" || ext == ".h") {
					component.AddScript(data->RelativePath);
					LNX_LOG_INFO("Added script: {0}", data->RelativePath);
				}
				else {
					LNX_LOG_WARN("Only .cpp files are valid C++ scripts");
				}
			}

			ComponentDrawer::EndIndent();

			if (component.GetScriptCount() > 0) {
				ComponentDrawer::DrawSectionHeader("", "Script Properties");
				ComponentDrawer::BeginIndent();
				TextWrapped("Public variables will appear here when the reflection system is implemented.", TextVariant::Muted);
				ComponentDrawer::EndIndent();
			}
		});
	}

	// ============================================================================
	// CAMERA COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawCameraComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<CameraComponent>("Camera", entity, [](auto& component) {
			auto& camera = component.Camera;

			PropertyCheckbox("Primary", component.Primary, "This camera will be used for rendering");

			ComponentDrawer::DrawSectionHeader("", "Projection");
			ComponentDrawer::BeginIndent();

			const char* projectionTypes[] = { "Perspective", "Orthographic" };
			int currentType = (int)camera.GetProjectionType();

			if (PropertyDropdown("Type", currentType, projectionTypes, 2)) {
				camera.SetProjectionType((SceneCamera::ProjectionType)currentType);
			}

			AddSpacing(SpacingValues::XS);

			if (camera.GetProjectionType() == SceneCamera::ProjectionType::Perspective) {
				float fov = glm::degrees(camera.GetPerspectiveVerticalFOV());
				if (PropertySlider("FOV", fov, 1.0f, 120.0f, "%.1f", "Field of View")) {
					camera.SetPerspectiveVerticalFOV(glm::radians(fov));
				}

				float nearClip = camera.GetPerspectiveNearClip();
				float farClip = camera.GetPerspectiveFarClip();

				if (PropertyFloat("Near", nearClip, 0.01f, 0.01f, farClip - 0.01f)) {
					camera.SetPerspectiveNearClip(nearClip);
				}

				if (PropertyFloat("Far", farClip, 0.1f, nearClip + 0.01f, 10000.0f)) {
					camera.SetPerspectiveFarClip(farClip);
				}
			}
			else {
				float orthoSize = camera.GetOrthographicSize();
				if (PropertyFloat("Size", orthoSize, 0.1f, 0.1f, 100.0f)) {
					camera.SetOrthographicSize(orthoSize);
				}

				float nearClip = camera.GetOrthographicNearClip();
				float farClip = camera.GetOrthographicFarClip();

				if (PropertyFloat("Near", nearClip, 0.1f, -1000.0f, farClip - 0.1f)) {
					camera.SetOrthographicNearClip(nearClip);
				}

				if (PropertyFloat("Far", farClip, 0.1f, nearClip + 0.1f, 1000.0f)) {
					camera.SetOrthographicFarClip(farClip);
				}

				PropertyCheckbox("Fixed Aspect", component.FixedAspectRatio);
			}

			ComponentDrawer::EndIndent();
		});
	}

	// ============================================================================
	// SPRITE RENDERER COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawSpriteRendererComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<SpriteRendererComponent>("Sprite Renderer", entity, [](auto& component) {
			ComponentDrawer::DrawSectionHeader("", "Appearance");
			ComponentDrawer::BeginIndent();
			Color color(component.Color);
			if (PropertyColor4("Color", color)) {
				component.Color = glm::vec4(color);
			}
			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Texture");
			ComponentDrawer::BeginIndent();

			if (component.Texture && component.Texture->IsLoaded()) {
				if (ComponentDrawer::BeginInfoCard("##TextureInfo", 90.0f)) {
					Image(component.Texture, Size(70, 70));
					
					SameLine();
					ImGui::BeginGroup();
					TextStyled("Loaded Texture", TextVariant::Primary);
					{
						ScopedColor hintColor(ImGuiCol_Text, ComponentStyle::HintColor());
						Text("Size: %dx%d", component.Texture->GetWidth(), component.Texture->GetHeight());
					}
					AddSpacing(SpacingValues::XS);
					if (Button("Remove", ButtonVariant::Danger, ButtonSize::Small, Size(80, 0))) {
						component.Texture.reset();
					}
					ImGui::EndGroup();
				}
				ComponentDrawer::EndInfoCard();
			}
			else {
				ComponentDrawer::DrawDropZone("Drop Texture Here\n(.png, .jpg, .bmp, .tga, .hdr)", Size(-1, 70));
			}

			void* payloadData = nullptr;
			if (ComponentDrawer::AcceptDropPayload("CONTENT_BROWSER_ITEM", &payloadData)) {
				ContentBrowserPayload* data = (ContentBrowserPayload*)payloadData;
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

			PropertyFloat("Tiling Factor", component.TilingFactor, 0.1f, 0.0f, 100.0f, "Texture repeat multiplier");

			ComponentDrawer::EndIndent();
		});
	}

	// ============================================================================
	// CIRCLE RENDERER COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawCircleRendererComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<CircleRendererComponent>("Circle Renderer", entity, [](auto& component) {
			ComponentDrawer::DrawSectionHeader("", "Appearance");
			ComponentDrawer::BeginIndent();

			Color color(component.Color);
			if (PropertyColor4("Color", color)) {
				component.Color = glm::vec4(color);
			}
			PropertySlider("Thickness", component.Thickness, 0.0f, 1.0f, "%.3f", "0 = Filled, 1 = Outline");
			PropertySlider("Fade", component.Fade, 0.0f, 1.0f, "%.3f", "Edge softness");

			ComponentDrawer::EndIndent();
		});
	}

	// ============================================================================
	// MESH COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawMeshComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<MeshComponent>("Mesh Renderer", entity, [](auto& component) {
			ComponentDrawer::DrawSectionHeader("", "Model");
			ComponentDrawer::BeginIndent();

			const char* modelTypes[] = { "Cube", "Sphere", "Plane", "Cylinder", "Custom Model" };
			int currentType = (int)component.Type;

			if (PropertyDropdown("Type", currentType, modelTypes, 5)) {
				component.Type = (ModelType)currentType;
				if (component.Type != ModelType::FromFile) {
					component.ClearMeshAsset();
					component.CreatePrimitive(component.Type);
				}
			}

			if (component.Type == ModelType::FromFile) {
				AddSpacing(SpacingValues::XS);

				if (component.HasMeshAsset()) {
					auto meshAsset = component.GetMeshAsset();
					
					if (ComponentDrawer::BeginInfoCard("##MeshAssetInfo", 230.0f)) {
						TextStyled("MeshAsset", TextVariant::Success);
						
						SameLine(ImGui::GetContentRegionAvail().x - 50);
						TextStyled(".lumesh", TextVariant::Muted);

						std::filesystem::path assetPath(meshAsset->GetPath().string());
						{
							ScopedColor accentColor(ImGuiCol_Text, ComponentStyle::AccentColor());
							Text("%s", assetPath.filename().string().c_str());
						}

						AddSpacing(SpacingValues::XS);
						Separator();
						AddSpacing(SpacingValues::XS);

						const auto& metadata = meshAsset->GetMetadata();
						
						BeginColumns(2, false);
						SetColumnWidth(0, 100.0f);
						
						Label("Submeshes"); NextColumn();
						Text("%d", metadata.SubmeshCount); NextColumn();
						
						Label("Vertices"); NextColumn();
						Text("%d", metadata.VertexCount); NextColumn();
						
						Label("Triangles"); NextColumn();
						Text("%d", metadata.TriangleCount); NextColumn();
						
						Label("Source"); NextColumn();
						std::filesystem::path srcPath(meshAsset->GetSourcePath());
						Text("%s", srcPath.filename().string().c_str()); NextColumn();
						
						EndColumns();

						AddSpacing(SpacingValues::XS);
						
						if (Button("Remove Mesh", ButtonVariant::Danger, ButtonSize::Medium, Size(-1, 0))) {
							component.ClearMeshAsset();
						}
					}
					ComponentDrawer::EndInfoCard();
				}
				else if (component.MeshModel) {
					if (ComponentDrawer::BeginInfoCard("##ModelInfo", 140.0f)) {
						TextStyled("Legacy Model (not a MeshAsset)", TextVariant::Warning);
						
						std::filesystem::path modelPath(component.FilePath);
						{
							ScopedColor accentColor(ImGuiCol_Text, ComponentStyle::AccentColor());
							Text("%s", modelPath.filename().string().c_str());
						}

						AddSpacing(SpacingValues::XS);
						Separator();
						AddSpacing(SpacingValues::XS);

						uint32_t totalVertices = 0;
						uint32_t totalIndices = 0;
						for (const auto& mesh : component.MeshModel->GetMeshes()) {
							totalVertices += (uint32_t)mesh->GetVertices().size();
							totalIndices += (uint32_t)mesh->GetIndices().size();
						}

						BeginColumns(2, false);
						SetColumnWidth(0, 100.0f);
						
						Label("Submeshes"); NextColumn();
						Text("%zu", component.MeshModel->GetMeshes().size()); NextColumn();
						
						Label("Vertices"); NextColumn();
						Text("%d", totalVertices); NextColumn();
						
						Label("Triangles"); NextColumn();
						Text("%d", totalIndices / 3); NextColumn();
						
						EndColumns();

						AddSpacing(SpacingValues::XS);
						
						if (Button("Remove Model", ButtonVariant::Danger, ButtonSize::Medium, Size(-1, 0))) {
							component.FilePath.clear();
							component.MeshModel.reset();
						}
					}
					ComponentDrawer::EndInfoCard();
				}
				else {
					ComponentDrawer::DrawDropZone("Drop Mesh Asset Here\n(.lumesh, .obj, .fbx, .gltf, .glb, .dae)", Size(-1, 60));

					void* payloadData = nullptr;
					if (ComponentDrawer::AcceptDropPayload("CONTENT_BROWSER_ITEM", &payloadData)) {
						ContentBrowserPayload* data = (ContentBrowserPayload*)payloadData;
						std::string ext = data->Extension;
						
						if (ext == ".lumesh") {
							auto meshAsset = MeshAsset::LoadFromFile(data->FilePath);
							if (meshAsset) {
								component.SetMeshAsset(meshAsset);
								LNX_LOG_INFO("Loaded MeshAsset: {0}", data->FilePath);
							} else {
								LNX_LOG_ERROR("Failed to load MeshAsset: {0}", data->FilePath);
							}
						}
						else if (ext == ".obj" || ext == ".fbx" || ext == ".gltf" || ext == ".glb" || ext == ".dae") {
							component.LoadFromFile(data->FilePath);
							LNX_LOG_INFO("Loaded model (legacy): {0}", data->FilePath);
						}
						else {
							LNX_LOG_WARN("Unsupported model format: {0}", ext);
						}
					}
				}
			}

			ComponentDrawer::EndIndent();
		});
	}

	// ============================================================================
	// MATERIAL COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawMaterialComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<MaterialComponent>("Material", entity, [&](auto& component) {
			ComponentDrawer::DrawSectionHeader("", "Material Asset");
			ComponentDrawer::BeginIndent();

			if (ComponentDrawer::BeginInfoCard("##MaterialAssetCard", 150.0f)) {
				if (component.Instance && component.Instance->GetBaseAsset()) {
					auto asset = component.Instance->GetBaseAsset();

					ImGui::BeginGroup();

					// Thumbnail
					Ref<Texture2D> thumbnail = GetOrGenerateThumbnail(asset);
					
					if (thumbnail) {
						Image(thumbnail, Size(70, 70));
						
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Material Preview\nClick 'Edit Material' to modify");
						}
					}
					else {
						auto albedo = asset->GetAlbedo();
						ColorPreviewButton("##preview", Color(albedo.r, albedo.g, albedo.b, albedo.a), Size(70, 70));
						
						if (ImGui::IsItemHovered()) {
							ImGui::SetTooltip("Material Preview\n(Thumbnail generation failed)");
						}
					}

					ImGui::EndGroup();
					SameLine();

					ImGui::BeginGroup();

					{
						ScopedColor textColor(ImGuiCol_Text, ComponentStyle::HeaderColor());
						Text("%s", component.GetMaterialName().c_str());
					}

					{
						ScopedColor textColor(ImGuiCol_Text, ComponentStyle::HintColor());
						if (!component.GetAssetPath().empty()) {
							std::filesystem::path matPath(component.GetAssetPath());
							Text("%s", matPath.filename().string().c_str());
						}
						else {
							Text("Default Material");
						}
					}

					AddSpacing(SpacingValues::XS);

					if (component.HasLocalOverrides()) {
						TextStyled("Has local overrides", TextVariant::Warning);
					}
					else {
						TextStyled("Using base asset", TextVariant::Success);
					}

					ImGui::EndGroup();

					AddSpacing(SpacingValues::XS);
					Separator();
					AddSpacing(SpacingValues::XS);

					ImGui::BeginGroup();

					if (Button("Edit Material", ButtonVariant::Primary, ButtonSize::Medium, Size(120, 0))) {
						if (m_OnMaterialEditCallback && asset) {
							m_OnMaterialEditCallback(asset);
						}
						else {
							LNX_LOG_WARN("Material editor not connected or asset is null");
						}
					}

					SameLine();

					if (component.HasLocalOverrides()) {
						if (Button("Reset Overrides", ButtonVariant::Warning, ButtonSize::Medium, Size(120, 0))) {
							component.ResetOverrides();
						}
					}

					ImGui::EndGroup();
				}
				else {
					TextWrapped("No material assigned. Drop a .lumat file here.", TextVariant::Muted);
				}
			}
			ComponentDrawer::EndInfoCard();

			void* payloadData = nullptr;
			if (ComponentDrawer::AcceptDropPayload("CONTENT_BROWSER_ITEM", &payloadData)) {
				ContentBrowserPayload* data = (ContentBrowserPayload*)payloadData;
				std::string ext = data->Extension;
				if (ext == ".lumat") {
					component.SetMaterialAsset(data->FilePath);
					LNX_LOG_INFO("Material assigned: {0}", data->FilePath);
				}
				else {
					LNX_LOG_WARN("Only .lumat files are valid materials");
				}
			}

			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Surface Properties");
			ComponentDrawer::BeginIndent();

			TextWrapped("Tip: Changes here create local overrides. Use 'Reset Overrides' to revert.", TextVariant::Muted);
			AddSpacing(SpacingValues::XS);

			glm::vec4 colorGlm = component.GetAlbedo();
			Color color(colorGlm);
			if (PropertyColor4("Base Color", color)) {
				component.SetAlbedo(glm::vec4(color), true);
			}

			float metallic = component.GetMetallic();
			if (PropertySlider("Metallic", metallic, 0.0f, 1.0f, "%.2f", "0 = Dielectric, 1 = Metal")) {
				component.SetMetallic(metallic, true);
			}

			float roughness = component.GetRoughness();
			if (PropertySlider("Roughness", roughness, 0.0f, 1.0f, "%.2f", "0 = Smooth, 1 = Rough")) {
				component.SetRoughness(roughness, true);
			}

			float specular = component.GetSpecular();
			if (PropertySlider("Specular", specular, 0.0f, 1.0f)) {
				component.SetSpecular(specular, true);
			}

			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Emission");
			ComponentDrawer::BeginIndent();

			glm::vec3 emissionColor = component.GetEmissionColor();
			if (PropertyColor("Color", emissionColor)) {
				component.SetEmissionColor(emissionColor, true);
			}

			float emissionIntensity = component.GetEmissionIntensity();
			if (PropertyFloat("Intensity", emissionIntensity, 0.1f, 0.0f, 100.0f)) {
				component.SetEmissionIntensity(emissionIntensity, true);
			}

			ComponentDrawer::EndIndent();

			if (component.Instance && component.Instance->GetBaseAsset()) {
				auto asset = component.Instance->GetBaseAsset();

				if (asset->HasAnyTexture()) {
					ComponentDrawer::DrawSectionHeader("", "Texture Maps");
					ComponentDrawer::BeginIndent();

					TextWrapped("Textures are managed in the Material Asset. Open the Material Editor to modify them.", TextVariant::Muted);
					AddSpacing(SpacingValues::XS);

					if (asset->HasAlbedoMap()) BulletText("Albedo Map");
					if (asset->HasNormalMap()) BulletText("Normal Map");
					if (asset->HasMetallicMap()) BulletText("Metallic Map");
					if (asset->HasRoughnessMap()) BulletText("Roughness Map");
					if (asset->HasSpecularMap()) BulletText("Specular Map");
					if (asset->HasEmissionMap()) BulletText("Emission Map");
					if (asset->HasAOMap()) BulletText("AO Map");

					ComponentDrawer::EndIndent();
				}
			}
		}, false); // Material cannot be removed independently
	}

	// ============================================================================
	// LIGHT COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawLightComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<LightComponent>("Light", entity, [](auto& component) {
			ComponentDrawer::DrawSectionHeader("", "Light Type");
			ComponentDrawer::BeginIndent();

			const char* lightTypes[] = { "Directional", "Point", "Spot" };
			int currentType = (int)component.GetType();

			if (PropertyDropdown("Type", currentType, lightTypes, 3, "Type of light source")) {
				component.SetType((LightType)currentType);
			}

			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Appearance");
			ComponentDrawer::BeginIndent();

			glm::vec3 color = component.GetColor();
			if (PropertyColor("Color", color, "Light color")) {
				component.SetColor(color);
			}

			float intensity = component.GetIntensity();
			if (PropertyFloat("Intensity", intensity, 0.1f, 0.0f, 100.0f, "Light brightness")) {
				component.SetIntensity(intensity);
			}

			ComponentDrawer::EndIndent();

			if (component.GetType() == LightType::Point || component.GetType() == LightType::Spot) {
				ComponentDrawer::DrawSectionHeader("", "Range & Attenuation");
				ComponentDrawer::BeginIndent();

				float range = component.GetRange();
				if (PropertyFloat("Range", range, 0.1f, 0.1f, 1000.0f, "Maximum light distance")) {
					component.SetRange(range);
				}

				glm::vec3 attenuation = component.GetAttenuation();
				if (PropertyVec3("Attenuation", attenuation, 0.01f, "Constant, Linear, Quadratic")) {
					component.SetAttenuation(attenuation);
				}

				ComponentDrawer::EndIndent();
			}

			if (component.GetType() == LightType::Spot) {
				ComponentDrawer::DrawSectionHeader("", "Spotlight Cone");
				ComponentDrawer::BeginIndent();

				float innerAngle = component.GetInnerConeAngle();
				if (PropertySlider("Inner Angle", innerAngle, 0.0f, 90.0f, "%.1f", "Inner cone angle (full brightness)")) {
					component.SetInnerConeAngle(innerAngle);
				}

				float outerAngle = component.GetOuterConeAngle();
				if (PropertySlider("Outer Angle", outerAngle, 0.0f, 90.0f, "%.1f", "Outer cone angle (fades to zero)")) {
					component.SetOuterConeAngle(outerAngle);
				}

				ComponentDrawer::EndIndent();
			}
			
			if (component.GetType() == LightType::Directional) {
				ComponentDrawer::DrawSectionHeader("", "Sun / Sky");
				ComponentDrawer::BeginIndent();
				
				bool isSunLight = component.IsSunLight();
				if (PropertyCheckbox("Is Sun Light", isSunLight, "Mark this light as the primary sun that controls the skybox")) {
					component.SetIsSunLight(isSunLight);
				}
				
				if (isSunLight) {
					AddSpacing(SpacingValues::XS);
					
					bool linkToSkybox = component.GetLinkToSkyboxRotation();
					if (PropertyCheckbox("Link to Skybox", linkToSkybox, "Skybox rotation follows this light's direction")) {
						component.SetLinkToSkyboxRotation(linkToSkybox);
					}
					
					float skyboxMult = component.GetSkyboxIntensityMultiplier();
					if (PropertyFloat("Skybox Intensity", skyboxMult, 0.01f, 0.0f, 10.0f, "Multiplier for skybox brightness")) {
						component.SetSkyboxIntensityMultiplier(skyboxMult);
					}
					
					AddSpacing(SpacingValues::XS);
					Separator();
					AddSpacing(SpacingValues::XS);
					
					bool contributeAmbient = component.GetContributeToAmbient();
					if (PropertyCheckbox("Contribute to Ambient", contributeAmbient, "Add ambient light from sky")) {
						component.SetContributeToAmbient(contributeAmbient);
					}
					
					if (contributeAmbient) {
						float ambientContrib = component.GetAmbientContribution();
						if (PropertySlider("Ambient Amount", ambientContrib, 0.0f, 1.0f, "%.2f", "Amount of ambient light from sky")) {
							component.SetAmbientContribution(ambientContrib);
						}
						
						glm::vec3 groundColor = component.GetGroundColor();
						if (PropertyColor("Ground Color", groundColor, "Color for hemisphere ambient (bottom)")) {
							component.SetGroundColor(groundColor);
						}
					}
					
					AddSpacing(SpacingValues::XS);
					Separator();
					AddSpacing(SpacingValues::XS);
					
					TextWrapped("Sun Disk (Procedural Sky - Coming Soon)", TextVariant::Muted);
					
					BeginDisabled(true);
					bool renderSunDisk = component.GetRenderSunDisk();
					PropertyCheckbox("Render Sun Disk", renderSunDisk, "Show sun disk in procedural sky");
					
					if (renderSunDisk) {
						float diskSize = component.GetSunDiskSize();
						PropertyFloat("Disk Size", diskSize, 0.1f, 0.1f, 10.0f, "Size of the sun disk");
						
						float diskIntensity = component.GetSunDiskIntensity();
						PropertyFloat("Disk Intensity", diskIntensity, 0.1f, 0.0f, 100.0f, "Brightness of the sun disk");
					}
					EndDisabled();
					
					AddSpacing(SpacingValues::XS);
					Separator();
					AddSpacing(SpacingValues::XS);
					
					TextWrapped("Atmosphere (Procedural Sky - Coming Soon)", TextVariant::Muted);
					
					BeginDisabled(true);
					bool affectAtmo = component.GetAffectAtmosphere();
					PropertyCheckbox("Affect Atmosphere", affectAtmo, "Light affects atmospheric scattering");
					
					if (affectAtmo) {
						float atmoDensity = component.GetAtmosphericDensity();
						PropertyFloat("Density", atmoDensity, 0.01f, 0.0f, 5.0f, "Atmospheric density");
					}
					EndDisabled();
					
					AddSpacing(SpacingValues::XS);
					Separator();
					AddSpacing(SpacingValues::XS);
					
					TextWrapped("Time of Day (Coming Soon)", TextVariant::Muted);
					
					BeginDisabled(true);
					bool useTimeOfDay = component.GetUseTimeOfDay();
					PropertyCheckbox("Use Time of Day", useTimeOfDay, "Animate sun position based on time");
					
					if (useTimeOfDay) {
						float timeOfDay = component.GetTimeOfDay();
						PropertySlider("Time", timeOfDay, 0.0f, 24.0f, "%.1f h", "Current time (0-24 hours)");
						
						float timeSpeed = component.GetTimeOfDaySpeed();
						PropertyFloat("Speed", timeSpeed, 0.1f, 0.0f, 100.0f, "Time speed multiplier");
					}
					EndDisabled();
				}
				
				ComponentDrawer::EndIndent();
			}

			ComponentDrawer::DrawSectionHeader("", "Shadows");
			ComponentDrawer::BeginIndent();

			bool castShadows = component.GetCastShadows();
			if (PropertyCheckbox("Cast Shadows", castShadows, "Enable shadow casting")) {
				component.SetCastShadows(castShadows);
			}

			ComponentDrawer::EndIndent();
		});
	}

	// ============================================================================
	// RIGIDBODY 2D COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawRigidbody2DComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<Rigidbody2DComponent>("Rigidbody 2D", entity, [](auto& component) {
			ComponentDrawer::DrawSectionHeader("", "Body Configuration");
			ComponentDrawer::BeginIndent();

			const char* bodyTypes[] = { "Static", "Dynamic", "Kinematic" };
			int currentType = (int)component.Type;

			if (PropertyDropdown("Type", currentType, bodyTypes, 3, "Defines how the body responds to physics")) {
				component.Type = (Rigidbody2DComponent::BodyType)currentType;
			}

			PropertyCheckbox("Fixed Rotation", component.FixedRotation, "Prevent rotation from physics");

			ComponentDrawer::EndIndent();
		});
	}

	// ============================================================================
	// BOX COLLIDER 2D COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawBoxCollider2DComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<BoxCollider2DComponent>("Box Collider 2D", entity, [](auto& component) {
			ComponentDrawer::DrawSectionHeader("", "Shape");
			ComponentDrawer::BeginIndent();

			PropertyVec2("Offset", component.Offset, 0.01f);
			PropertyVec2("Size", component.Size, 0.01f);

			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Physics Material");
			ComponentDrawer::BeginIndent();

			PropertyFloat("Density", component.Density, 0.01f, 0.0f, 100.0f, "Mass per unit area");
			PropertyFloat("Friction", component.Friction, 0.01f, 0.0f, 1.0f, "Surface friction coefficient");
			PropertyFloat("Restitution", component.Restitution, 0.01f, 0.0f, 1.0f, "Bounciness (0 = no bounce, 1 = perfect bounce)");
			PropertyFloat("Restitution Threshold", component.RestitutionThreshold, 0.01f, 0.0f, 10.0f, "Minimum velocity for bounce");

			ComponentDrawer::EndIndent();
		});
	}

	// ============================================================================
	// CIRCLE COLLIDER 2D COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawCircleCollider2DComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<CircleCollider2DComponent>("Circle Collider 2D", entity, [](auto& component) {
			ComponentDrawer::DrawSectionHeader("", "Shape");
			ComponentDrawer::BeginIndent();

			PropertyVec2("Offset", component.Offset, 0.01f);
			PropertyFloat("Radius", component.Radius, 0.01f, 0.01f);

			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Physics Material");
			ComponentDrawer::BeginIndent();

			PropertyFloat("Density", component.Density, 0.01f, 0.0f, 100.0f, "Mass per unit area");
			PropertyFloat("Friction", component.Friction, 0.01f, 0.0f, 1.0f, "Surface friction coefficient");
			PropertyFloat("Restitution", component.Restitution, 0.01f, 0.0f, 1.0f, "Bounciness");
			PropertyFloat("Restitution Threshold", component.RestitutionThreshold, 0.01f, 0.0f, 10.0f);

			ComponentDrawer::EndIndent();
		});
	}

	// ============================================================================
	// RIGIDBODY 3D COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawRigidbody3DComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<Rigidbody3DComponent>("Rigidbody 3D", entity, [](auto& component) {
			ComponentDrawer::DrawSectionHeader("", "Body Configuration");
			ComponentDrawer::BeginIndent();

			const char* bodyTypes[] = { "Static", "Dynamic", "Kinematic" };
			int currentType = (int)component.Type;

			if (PropertyDropdown("Type", currentType, bodyTypes, 3, "Defines how the body responds to physics")) {
				component.Type = (Rigidbody3DComponent::BodyType)currentType;
			}

			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Physics Material");
			ComponentDrawer::BeginIndent();

			PropertyFloat("Mass", component.Mass, 0.1f, 0.0f, 10000.0f, "Object mass (kg)");
			PropertyFloat("Friction", component.Friction, 0.01f, 0.0f, 1.0f, "Surface friction coefficient");
			PropertyFloat("Restitution", component.Restitution, 0.01f, 0.0f, 1.0f, "Bounciness (0 = no bounce, 1 = perfect bounce)");

			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Damping");
			ComponentDrawer::BeginIndent();

			PropertyFloat("Linear Damping", component.LinearDamping, 0.01f, 0.0f, 1.0f, "Velocity damping (air resistance)");
			PropertyFloat("Angular Damping", component.AngularDamping, 0.01f, 0.0f, 1.0f, "Rotation damping");

			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Constraints");
			ComponentDrawer::BeginIndent();

			PropertyVec3("Linear Factor", component.LinearFactor, 0.1f, "Lock movement on axes (0 = locked, 1 = free)");
			PropertyVec3("Angular Factor", component.AngularFactor, 0.1f, "Lock rotation on axes (0 = locked, 1 = free)");

			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Advanced");
			ComponentDrawer::BeginIndent();

			PropertyCheckbox("Is Trigger", component.IsTrigger, "Detect collisions without physical response");
			PropertyCheckbox("Use CCD", component.UseCCD, "Continuous Collision Detection (prevents tunneling)");

			if (component.UseCCD) {
				PropertyFloat("CCD Motion Threshold", component.CcdMotionThreshold, 0.01f, 0.0f, 10.0f, "Minimum motion to trigger CCD");
				PropertyFloat("CCD Swept Sphere Radius", component.CcdSweptSphereRadius, 0.01f, 0.0f, 10.0f, "Radius for swept sphere test");
			}

			ComponentDrawer::EndIndent();
		});
	}

	// ============================================================================
	// BOX COLLIDER 3D COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawBoxCollider3DComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<BoxCollider3DComponent>("Box Collider 3D", entity, [](auto& component) {
			ComponentDrawer::DrawSectionHeader("", "Shape");
			ComponentDrawer::BeginIndent();

			PropertyVec3("Offset", component.Offset, 0.01f, "Center offset from entity position");
			PropertyVec3("Half Extents", component.HalfExtents, 0.01f, "Half-size on each axis (full size = 2x this)");

			ComponentDrawer::EndIndent();
		});
	}

	// ============================================================================
	// SPHERE COLLIDER 3D COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawSphereCollider3DComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<SphereCollider3DComponent>("Sphere Collider 3D", entity, [](auto& component) {
			ComponentDrawer::DrawSectionHeader("", "Shape");
			ComponentDrawer::BeginIndent();

			PropertyVec3("Offset", component.Offset, 0.01f, "Center offset from entity position");
			PropertyFloat("Radius", component.Radius, 0.01f, 0.01f, 100.0f, "Sphere radius");

			ComponentDrawer::EndIndent();
		});
	}

	// ============================================================================
	// CAPSULE COLLIDER 3D COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawCapsuleCollider3DComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<CapsuleCollider3DComponent>("Capsule Collider 3D", entity, [](auto& component) {
			ComponentDrawer::DrawSectionHeader("", "Shape");
			ComponentDrawer::BeginIndent();

			PropertyVec3("Offset", component.Offset, 0.01f, "Center offset from entity position");
			PropertyFloat("Radius", component.Radius, 0.01f, 0.01f, 100.0f, "Capsule radius");
			PropertyFloat("Height", component.Height, 0.01f, 0.01f, 100.0f, "Capsule cylinder height (excluding caps)");

			ComponentDrawer::EndIndent();
		});
	}

	// ============================================================================
	// MESH COLLIDER 3D COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawMeshCollider3DComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<MeshCollider3DComponent>("Mesh Collider 3D", entity, [](auto& component) {
			ComponentDrawer::DrawSectionHeader("", "Warning");
			ComponentDrawer::BeginIndent();

			TextWrapped("Mesh colliders are expensive! Use for static geometry only.", TextVariant::Warning);

			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Shape");
			ComponentDrawer::BeginIndent();

			const char* collisionTypes[] = { "Convex", "Concave" };
			int currentType = (int)component.Type;

			if (PropertyDropdown("Type", currentType, collisionTypes, 2, "Convex = faster but simplified, Concave = exact but slower")) {
				component.Type = (MeshCollider3DComponent::CollisionType)currentType;
			}

			if (component.Type == MeshCollider3DComponent::CollisionType::Concave) {
				TextWrapped("Concave meshes can only be used with static rigidbodies.", TextVariant::Muted);
			}

			PropertyCheckbox("Use Entity Mesh", component.UseEntityMesh, "Automatically use mesh from MeshComponent");

			ComponentDrawer::EndIndent();
		});
	}

	// ============================================================================
	// ADD COMPONENT ENTRY
	// ============================================================================

	template<typename T>
	void PropertiesPanel::DisplayAddComponentEntry(const std::string& entryName) {
		if (!m_SelectedEntity.HasComponent<T>()) {
			if (UI::MenuItem(entryName.c_str())) {
				m_SelectedEntity.AddComponent<T>();
				// Auto-add MaterialComponent when MeshComponent is created via UI
				if constexpr (std::is_same_v<T, MeshComponent>) {
					if (!m_SelectedEntity.HasComponent<MaterialComponent>()) {
						m_SelectedEntity.AddComponent<MaterialComponent>();
					}
				}
				ImGui::CloseCurrentPopup();
			}
		}
	}
}