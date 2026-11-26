/**
 * @file PropertiesPanel.cpp
 * @brief Properties Panel Implementation using Lunex UI Framework
 */

#include "stpch.h"
#include "Scripting/ScriptingEngine.h"
#include "Scripting/ScriptBuild.h"
#include "Scripting/ScriptReflection.h"
#include "PropertiesPanel.h"
#include "ContentBrowserPanel.h"
#include "AnimationEditorPanel.h"
#include "BlueprintEditorPanel.h"
#include "Assets/Animation/AnimationAsset.h"
#include "Project/Project.h"

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
				if (InputText("##Tag", buffer, sizeof(buffer))) {
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
				Text("Add Component");
			}
			Separator();

			DisplayAddComponentEntry<CameraComponent>("Camera");
			DisplayAddComponentEntry<ScriptComponent>("C++ Script");
			DisplayAddComponentEntry<SpriteRendererComponent>("Sprite Renderer");
			DisplayAddComponentEntry<CircleRendererComponent>("Circle Renderer");
			DisplayAddComponentEntry<MeshComponent>("Mesh Renderer");
			DisplayAddComponentEntry<LightComponent>("Light");
			DisplayAddComponentEntry<AnimatorComponent>("Animator");
			DisplayAddComponentEntry<ParticleSystemComponent>("Particle System");
			DisplayAddComponentEntry<BlueprintComponent>("Blueprint");

			Separator();
			{
				ScopedColor textColor(ImGuiCol_Text, ComponentStyle::SubheaderColor());
				Text("Physics 2D");
			}

			DisplayAddComponentEntry<Rigidbody2DComponent>("Rigidbody 2D");
			DisplayAddComponentEntry<BoxCollider2DComponent>("Box Collider 2D");
			DisplayAddComponentEntry<CircleCollider2DComponent>("Circle Collider 2D");

			Separator();
			{
				ScopedColor textColor(ImGuiCol_Text, ComponentStyle::SubheaderColor());
				Text("Physics 3D");
			}

			DisplayAddComponentEntry<Rigidbody3DComponent>("Rigidbody 3D");
			DisplayAddComponentEntry<BoxCollider3DComponent>("Box Collider 3D");
			DisplayAddComponentEntry<SphereCollider3DComponent>("Sphere Collider 3D");
			DisplayAddComponentEntry<CapsuleCollider3DComponent>("Capsule Collider 3D");
			DisplayAddComponentEntry<CylinderCollider3DComponent>("Cylinder Collider 3D");
			DisplayAddComponentEntry<ConeCollider3DComponent>("Cone Collider 3D");
			DisplayAddComponentEntry<MeshCollider3DComponent>("Mesh Collider 3D");
			DisplayAddComponentEntry<CharacterController3DComponent>("Character Controller 3D");

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
		DrawAnimatorComponent(entity);
		DrawParticleSystemComponent(entity);
		DrawBlueprintComponent(entity);
		DrawMaterialComponent(entity);
		DrawLightComponent(entity);
		DrawRigidbody2DComponent(entity);
		DrawBoxCollider2DComponent(entity);
		DrawCircleCollider2DComponent(entity);
		DrawRigidbody3DComponent(entity);
		DrawBoxCollider3DComponent(entity);
		DrawSphereCollider3DComponent(entity);
		DrawCapsuleCollider3DComponent(entity);
		DrawCylinderCollider3DComponent(entity);
		DrawConeCollider3DComponent(entity);
		DrawMeshCollider3DComponent(entity);
		DrawCharacterController3DComponent(entity);
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
	

	// ============================================================================
	// SCRIPT PUBLIC VARIABLES (Unity-style inspector)
	// ============================================================================

	namespace {

		// Edit-mode field: value = inspector override, or the default parsed from the source
		bool DrawScriptField(const ScriptFieldInfo& field, std::unordered_map<std::string, ScriptFieldValue>& overrides) {
			using namespace UI;
			auto it = overrides.find(field.Name);
			const bool overridden = (it != overrides.end() && it->second.Type == field.Default.Type);
			ScriptFieldValue value = overridden ? it->second : field.Default;
			bool changed = false;
			const char* tip = field.Tooltip.empty() ? nullptr : field.Tooltip.c_str();

			ImGui::PushID(field.Name.c_str());
			switch (field.Default.Type) {
				case ScriptFieldType::Float:
					changed = field.HasRange
						? PropertySlider(field.Name, value.Number.x, field.Min, field.Max, "%.3f", tip)
						: PropertyFloat(field.Name, value.Number.x, 0.05f, 0.0f, 0.0f, tip);
					break;
				case ScriptFieldType::Int: {
					int v = static_cast<int>(value.Number.x);
					BeginPropertyRow(field.Name, tip);
					ImGui::SetNextItemWidth(-1);
					if (ImGui::DragInt("##int", &v)) { value.Number.x = static_cast<float>(v); changed = true; }
					EndPropertyRow();
					break;
				}
				case ScriptFieldType::Bool: {
					bool b = value.Number.x != 0.0f;
					if (PropertyCheckbox(field.Name, b, tip)) { value.Number.x = b ? 1.0f : 0.0f; changed = true; }
					break;
				}
				case ScriptFieldType::Vec2: {
					glm::vec2 v(value.Number.x, value.Number.y);
					if (PropertyVec2(field.Name, v, 0.05f, tip)) { value.Number.x = v.x; value.Number.y = v.y; changed = true; }
					break;
				}
				case ScriptFieldType::Vec3: {
					glm::vec3 v(value.Number);
					if (PropertyVec3(field.Name, v, 0.05f, tip)) { value.Number = glm::vec4(v, 0.0f); changed = true; }
					break;
				}
				case ScriptFieldType::String: {
					char buf[512];
					strncpy_s(buf, value.String.c_str(), _TRUNCATE);
					BeginPropertyRow(field.Name, tip);
					ImGui::SetNextItemWidth(-1);
					if (ImGui::InputText("##str", buf, sizeof(buf))) { value.String = buf; changed = true; }
					EndPropertyRow();
					break;
				}
			}

			// Overridden values are marked and can be reset to the script's default
			if (overridden) {
				if (ImGui::BeginPopupContextItem("##fieldctx")) {
					if (ImGui::MenuItem("Reset to script default")) { overrides.erase(field.Name); ImGui::EndPopup(); ImGui::PopID(); return true; }
					ImGui::EndPopup();
				}
			}
			ImGui::PopID();

			if (changed) overrides[field.Name] = value;
			return changed;
		}

		// Play-mode field: edits the live variable inside the running script
		void DrawLiveScriptVariable(const VarMetadata& var) {
			using namespace UI;
			if (!var.DataPtr) return;
			const char* tip = var.Tooltip.empty() ? nullptr : var.Tooltip.c_str();
			ImGui::PushID(var.Name.c_str());
			switch (var.Type) {
				case VarType::Float: {
					float& f = *static_cast<float*>(var.DataPtr);
					if (var.HasRange) PropertySlider(var.Name, f, var.MinValue, var.MaxValue, "%.3f", tip);
					else PropertyFloat(var.Name, f, 0.05f, 0.0f, 0.0f, tip);
					break;
				}
				case VarType::Int: {
					BeginPropertyRow(var.Name, tip);
					ImGui::SetNextItemWidth(-1);
					ImGui::DragInt("##int", static_cast<int*>(var.DataPtr));
					EndPropertyRow();
					break;
				}
				case VarType::Bool: PropertyCheckbox(var.Name, *static_cast<bool*>(var.DataPtr), tip); break;
				case VarType::Vec2: {
					auto* v = static_cast<Lunex::Vec2*>(var.DataPtr);
					glm::vec2 g(v->x, v->y);
					if (PropertyVec2(var.Name, g, 0.05f, tip)) { v->x = g.x; v->y = g.y; }
					break;
				}
				case VarType::Vec3: {
					auto* v = static_cast<Lunex::Vec3*>(var.DataPtr);
					glm::vec3 g(v->x, v->y, v->z);
					if (PropertyVec3(var.Name, g, 0.05f, tip)) { v->x = g.x; v->y = g.y; v->z = g.z; }
					break;
				}
				case VarType::String: {
					auto* s = static_cast<std::string*>(var.DataPtr);
					char buf[512];
					strncpy_s(buf, s->c_str(), _TRUNCATE);
					BeginPropertyRow(var.Name, tip);
					ImGui::SetNextItemWidth(-1);
					if (ImGui::InputText("##str", buf, sizeof(buf))) *s = buf;
					EndPropertyRow();
					break;
				}
			}
			ImGui::PopID();
		}
	}

	void PropertiesPanel::DrawBlueprintComponent(Entity entity) {
		using namespace UI;
		ComponentDrawer::Draw<BlueprintComponent>("Blueprint", entity, [entity](auto& component) mutable {
			ComponentDrawer::BeginIndent();
			{
				auto resolve = [](const std::string& stored) {
					std::filesystem::path p(stored);
					if (p.is_relative() && Project::GetActive()) p = Project::GetActiveAssetDirectory() / p;
					return p;
				};
				if (component.BlueprintPath.empty()) {
					TextWrapped("Script this entity with nodes (events, flow, transform, physics, materials, particles...).", TextVariant::Muted);
					if (ButtonBlock("Create Blueprint", ButtonVariant::Primary)) {
						std::filesystem::path path = BlueprintEditorPanel::CreateForEntity(entity.GetComponent<TagComponent>().Tag);
						std::filesystem::path stored = path;
						if (Project::GetActive()) {
							std::error_code ec;
							auto rel = std::filesystem::relative(path, Project::GetActiveAssetDirectory(), ec);
							if (!ec && !rel.empty()) stored = rel;
						}
						component.BlueprintPath = stored.generic_string();
						if (auto* editor = BlueprintEditorPanel::Get()) editor->Open(path);
					}
				}
				else {
					TextStyled(std::filesystem::path(component.BlueprintPath).filename().string(), TextVariant::Primary);
					if (ButtonBlock("Edit Blueprint", ButtonVariant::Primary)) {
						if (auto* editor = BlueprintEditorPanel::Get()) editor->Open(resolve(component.BlueprintPath));
					}
					if (Button("Remove Blueprint", ButtonVariant::Default, ButtonSize::Small)) component.BlueprintPath.clear();
				}
				// Drop an existing .lublueprint from the Content Browser
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
						auto* data = static_cast<const ContentBrowserPayload*>(payload->Data);
						if (std::string(data->Extension) == ".lublueprint") {
							std::filesystem::path p(data->FilePath);
							if (Project::GetActive()) {
								std::error_code ec;
								auto rel = std::filesystem::relative(p, Project::GetActiveAssetDirectory(), ec);
								if (!ec && !rel.empty()) p = rel;
							}
							component.BlueprintPath = p.generic_string();
						}
					}
					ImGui::EndDragDropTarget();
				}
			}
			ComponentDrawer::EndIndent();

		});
	}

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
					
					const auto resolved = ScriptBuild::ResolveScriptPath(scriptPath);
					const auto build = ScriptBuild::GetStatus(resolved);

					Text("Status:");
					SameLine();
					if (isLoaded) {
						TextStyled("Running", TextVariant::Success);
					}
					else if (build.State == ScriptBuildState::Compiling) {
						TextStyled("Compiling...", TextVariant::Warning);
					}
					else if (build.State == ScriptBuildState::Failed) {
						TextStyled(std::to_string(build.Errors.size()) + " error(s)", TextVariant::Danger);
					}
					else if (!ScriptBuild::NeedsCompile(resolved)) {
						TextStyled(build.Seconds > 0.0f ? "Compiled (" + std::to_string((int)(build.Seconds * 10) / 10.0f).substr(0, 4) + " s)" : "Up to date",
							TextVariant::Success);
					}
					else {
						TextStyled("Needs compile", TextVariant::Warning);
					}

					SameLine(ImGui::GetContentRegionAvail().x - 70);
					ImGui::BeginDisabled(build.State == ScriptBuildState::Compiling);
					if (Button("Compile", ButtonVariant::Default, ButtonSize::Small, Size(70, 0))) {
						ScriptBuild::CompileAsync(resolved);
					}
					ImGui::EndDisabled();
				}
				ComponentDrawer::EndInfoCard();

				// Compiler errors (first few lines; the full output is in the Console)
				{
					const auto build = ScriptBuild::GetStatus(ScriptBuild::ResolveScriptPath(scriptPath));
					if (build.State == ScriptBuildState::Failed) {
						ScopedColor errColor(ImGuiCol_Text, Colors::Danger());
						for (size_t e = 0; e < build.Errors.size() && e < 6; e++) {
							std::string line = build.Errors[e];
							auto pos = line.find_last_of("\\/");
							if (pos != std::string::npos && pos < line.find(':', 3)) line = line.substr(pos + 1);
							TextWrapped(line, TextVariant::Primary);
						}
					}
				}

				// Public variables
				if (i >= component.FieldOverrides.size()) component.FieldOverrides.resize(component.GetScriptCount());
				ScriptPlugin* plugin = (i < component.ScriptPluginInstances.size())
					? static_cast<ScriptPlugin*>(component.ScriptPluginInstances[i]) : nullptr;

				if (plugin && plugin->IsLoaded() && plugin->GetModule()) {
					auto vars = plugin->GetModule()->GetPublicVariables();
					if (!vars.empty()) {
						ComponentDrawer::DrawSectionHeader("", "Variables (live)");
						for (const auto& var : vars) DrawLiveScriptVariable(var);
					}
				}
				else {
					const auto& fields = ScriptReflection::GetFields(ScriptBuild::ResolveScriptPath(scriptPath));
					if (!fields.empty()) {
						ComponentDrawer::DrawSectionHeader("", "Variables");
						for (const auto& field : fields) DrawScriptField(field, component.FieldOverrides[i]);
					}
				}
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
				AddSpacing(SpacingValues::XS);
				TextWrapped("Expose variables with RegisterVar(\"Name\", &member) in Start(). Right-click an edited value to reset it.", TextVariant::Muted);
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
	
	// ============================================================================
	// PARTICLE SYSTEM COMPONENT
	// ============================================================================

	namespace {
		// Asset slot: shows the file name, accepts a drop from the Content Browser
		bool ParticleAssetField(const char* label, std::string& path, std::initializer_list<const char*> extensions) {
			using namespace UI;
			bool changed = false;
			BeginPropertyRow(label);
			const std::string shown = path.empty() ? std::string("(drop asset)") : std::filesystem::path(path).filename().string();
			ImGui::PushID(label);
			const float clearW = path.empty() ? 0.0f : ImGui::GetFrameHeight();
			ImGui::Button(shown.c_str(), ImVec2(ImGui::GetContentRegionAvail().x - clearW - (clearW > 0 ? 4.0f : 0.0f), 0));
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
					auto* data = static_cast<const ContentBrowserPayload*>(payload->Data);
					std::string ext = data->Extension;
					std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
					for (const char* e : extensions) {
						if (ext != e) continue;
						std::filesystem::path p(data->FilePath);
						if (Project::GetActive()) {
							std::error_code ec;
							auto rel = std::filesystem::relative(p, Project::GetActiveAssetDirectory(), ec);
							if (!ec && !rel.empty() && rel.native()[0] != '.') p = rel;
						}
						path = p.generic_string();
						changed = true;
					}
				}
				ImGui::EndDragDropTarget();
			}
			if (!path.empty()) {
				ImGui::SameLine(0, 4);
				if (ImGui::Button("x", ImVec2(clearW, 0))) { path.clear(); changed = true; }
			}
			ImGui::PopID();
			EndPropertyRow();
			return changed;
		}

		bool PropertyRange(const char* label, glm::vec2& range, float speed, const char* tooltip) {
			using namespace UI;
			glm::vec2 v = range;
			if (PropertyVec2(label, v, speed, tooltip)) { range = glm::vec2(std::min(v.x, v.y), std::max(v.x, v.y)); return true; }
			return false;
		}

		bool PropertyRGBA(const char* label, glm::vec4& color) {
			UI::Color c(color);
			if (UI::PropertyColor4(label, c)) { color = glm::vec4(c.r, c.g, c.b, c.a); return true; }
			return false;
		}
	}

	void PropertiesPanel::DrawParticleSystemComponent(Entity entity) {
		using namespace UI;
		using PSC = ParticleSystemComponent;

		ComponentDrawer::Draw<PSC>("Particle System", entity, [](auto& ps) {
			// Live controls
			{
				if (Button(ps.Playing ? "Stop" : "Play", ButtonVariant::Primary, ButtonSize::Small)) { if (ps.Playing) ps.Stop(); else ps.Play(); }
				SameLine();
				if (Button("Burst x10", ButtonVariant::Default, ButtonSize::Small)) ps.Emit(10);
				SameLine();
				if (Button("Clear", ButtonVariant::Default, ButtonSize::Small)) ps.Clear();
				SameLine();
				TextStyled(std::to_string(ps.Particles.size()) + " alive", TextVariant::Muted);
			}

			ComponentDrawer::DrawSectionHeader("", "Emission");
			ComponentDrawer::BeginIndent();
			PropertyCheckbox("Play On Start", ps.PlayOnStart);
			PropertyCheckbox("Preview In Editor", ps.PreviewInEditor, "Simulate in Edit mode (simple simulation, no entities)");
			PropertyCheckbox("Looping", ps.Looping);
			PropertyFloat("Duration", ps.Duration, 0.05f, 0.1f, 600.0f, "Seconds per emission cycle");
			PropertyFloat("Rate", ps.Rate, 0.5f, 0.0f, 10000.0f, "Particles per second");
			float burst = static_cast<float>(ps.Burst);
			if (PropertyFloat("Burst", burst, 1.0f, 0.0f, 10000.0f, "Particles emitted when each cycle starts")) ps.Burst = static_cast<int>(burst);
			float maxP = static_cast<float>(ps.MaxParticles);
			if (PropertyFloat("Max Particles", maxP, 10.0f, 1.0f, 100000.0f)) ps.MaxParticles = std::max(1, static_cast<int>(maxP));
			PropertyCheckbox("World Space", ps.WorldSpace, "Particles stay where they were born when the emitter moves");
			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Shape");
			ComponentDrawer::BeginIndent();
			static const char* shapes[] = { "Point", "Sphere", "Box", "Cone" };
			int shape = static_cast<int>(ps.Shape);
			if (PropertyDropdown("Shape", shape, shapes, 4)) ps.Shape = static_cast<PSC::EmitterShape>(shape);
			if (ps.Shape == PSC::EmitterShape::Box) PropertyVec3("Size", ps.BoxSize, 0.05f);
			else if (ps.Shape != PSC::EmitterShape::Point) PropertyFloat("Radius", ps.ShapeRadius, 0.01f, 0.0f, 1000.0f);
			if (ps.Shape == PSC::EmitterShape::Cone) PropertySlider("Angle", ps.ConeAngle, 0.0f, 90.0f, "%.0f deg");
			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Particles");
			ComponentDrawer::BeginIndent();
			PropertyRange("Lifetime", ps.Lifetime, 0.05f, "Min / max seconds");
			PropertyRange("Speed", ps.Speed, 0.05f, "Min / max initial speed");
			PropertyRange("Start Size", ps.StartSize, 0.01f, "Min / max size");
			PropertySlider("End Size", ps.EndSizeScale, 0.0f, 3.0f, "%.2fx", "Size multiplier at the end of the life");
			PropertyRange("Spin", ps.AngularSpeed, 1.0f, "Min / max degrees per second");
			PropertyRGBA("Start Color", ps.StartColor);
			PropertyRGBA("End Color", ps.EndColor);
			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Forces");
			ComponentDrawer::BeginIndent();
			PropertyFloat("Gravity Scale", ps.GravityScale, 0.01f, -10.0f, 10.0f);
			PropertyFloat("Drag", ps.Drag, 0.01f, 0.0f, 20.0f);
			PropertyCheckbox("Collide With Ground", ps.CollideWithGround, "Simple simulation: bounce on a horizontal plane");
			if (ps.CollideWithGround) {
				PropertyFloat("Ground Height", ps.GroundHeight, 0.05f, -10000.0f, 10000.0f);
				PropertySlider("Bounce", ps.Bounce, 0.0f, 1.0f);
			}
			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Render");
			ComponentDrawer::BeginIndent();
			static const char* modes[] = { "Billboard", "Mesh (.lumesh)", "Prefab (.luprefab)" };
			int mode = static_cast<int>(ps.Mode);
			if (PropertyDropdown("Particle", mode, modes, 3, "What every particle is")) ps.Mode = static_cast<PSC::RenderMode>(mode);
			static const char* sims[] = { "Simple (CPU)", "Physics (Rigidbody)" };
			int sim = static_cast<int>(ps.Simulation);
			if (PropertyDropdown("Simulation", sim, sims, 2, "Physics spawns real Rigidbody entities (Play mode)")) ps.Simulation = static_cast<PSC::SimulationMode>(sim);
			if (ps.Mode == PSC::RenderMode::Billboard) {
				ParticleAssetField("Texture", ps.TexturePath, { ".png", ".jpg", ".jpeg", ".tga" });
				PropertyCheckbox("Additive", ps.AdditiveBlend, "Additive blending (fire, sparks, magic)");
			}
			else if (ps.Mode == PSC::RenderMode::Mesh) {
				ParticleAssetField("Mesh", ps.MeshPath, { ".lumesh" });
				ParticleAssetField("Material", ps.MaterialPath, { ".lumat" });
			}
			else {
				ParticleAssetField("Prefab", ps.PrefabPath, { ".luprefab" });
				TextWrapped("Each particle instantiates the prefab (with its rigidbodies, colliders and scripts) in Play mode.", TextVariant::Muted);
			}
			if (ps.Simulation == PSC::SimulationMode::Physics) PropertyFloat("Particle Mass", ps.ParticleMass, 0.01f, 0.001f, 1000.0f);
			ComponentDrawer::EndIndent();
		});
	}

	void PropertiesPanel::DrawAnimatorComponent(Entity entity) {
		using namespace UI;

		ComponentDrawer::Draw<AnimatorComponent>("Animator", entity, [entity](auto& component) mutable {
			Ref<Model> model;
			if (entity.HasComponent<MeshComponent>())
				model = entity.GetComponent<MeshComponent>().MeshModel;

			if (!model || !model->IsSkinned()) {
				TextWrapped("The Mesh Renderer on this entity has no skeleton. Import a rigged model (FBX/glTF with bones) to animate it.", TextVariant::Muted);
				return;
			}

			const auto& clips = model->GetAnimations();
			ComponentDrawer::DrawSectionHeader("", "Clip");
			ComponentDrawer::BeginIndent();

			// Combined list: model clips, then authored .luanim clips
			auto resolveAsset = [](const std::string& stored) {
				std::filesystem::path p(stored);
				if (p.is_relative() && Project::GetActive()) p = Project::GetActiveAssetDirectory() / p;
				return p;
			};
			std::vector<std::string> names;
			std::vector<float> durations;
			for (const auto& clip : clips) { names.push_back(clip->Name); durations.push_back(clip->GetDurationSeconds()); }
			for (const auto& stored : component.AnimationAssets) {
				auto asset = AnimationAsset::GetOrLoad(resolveAsset(stored));
				names.push_back(std::filesystem::path(stored).stem().string() + " (.luanim)");
				durations.push_back(asset ? asset->Duration : 0.0f);
			}

			if (names.empty()) {
				TextWrapped("No clips yet. Open the Animation Editor to author a .luanim clip.", TextVariant::Muted);
			}
			else {
				int index = glm::clamp(component.ClipIndex, 0, static_cast<int>(names.size()) - 1);
				if (PropertyDropdown("Animation", index, names, "Clip to play (crossfades while playing)")) {
					if (component.Playing) component.Play(index);   // smooth transition
					else { component.ClipIndex = index; component.Time = 0.0f; }
				}

				const float duration = durations[index];
				float time = duration > 0.0f ? std::fmod(component.Time, duration) : 0.0f;
				if (PropertySlider("Time", time, 0.0f, std::max(duration, 0.001f), "%.2f s", "Scrub the animation")) {
					component.Time = time;
				}
				Text("Bones: %d   Duration: %.2f s", model->GetSkeleton()->GetBoneCount(), duration);

				// Remove an authored clip from the list (the file stays on disk)
				int assetIndex = index - static_cast<int>(clips.size());
				if (assetIndex >= 0 && Button("Remove Clip From Animator", ButtonVariant::Default, ButtonSize::Small)) {
					component.AnimationAssets.erase(component.AnimationAssets.begin() + assetIndex);
					component.ClipIndex = 0;
				}
			}
			if (ButtonBlock("Edit Animation", ButtonVariant::Primary)) {
				AnimationEditorPanel::OpenFor(entity);
			}
			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Playback");
			ComponentDrawer::BeginIndent();
			PropertyFloat("Speed", component.Speed, 0.01f, -5.0f, 5.0f, "Playback speed multiplier");
			PropertyCheckbox("Loop", component.Loop);
			PropertyCheckbox("Play On Start", component.PlayOnStart, "Start playing when entering Play mode");
			PropertyCheckbox("Preview In Editor", component.PreviewInEditor, "Animate in Edit mode");
			PropertyFloat("Blend Time", component.BlendDuration, 0.01f, 0.0f, 2.0f, "Crossfade duration when switching clips (seconds)");
			ComponentDrawer::EndIndent();

			// ---- Rig debugging ----
			ComponentDrawer::DrawSectionHeader("", "Rig");
			ComponentDrawer::BeginIndent();
			PropertyCheckbox("Show Skeleton", component.ShowSkeleton, "Draw the bones in the viewport");

			auto skeleton = model->GetSkeleton();
			std::vector<std::string> boneNames(skeleton->GetBoneCount() + 1);
			boneNames[0] = "None";
			for (const auto& [name, idx] : skeleton->BoneIndexByName)
				if (idx >= 0 && idx + 1 < static_cast<int>(boneNames.size())) boneNames[idx + 1] = name;
			int selected = component.DebugWeightsBone + 1;
			if (PropertyDropdown("Weights View", selected, boneNames,
				"Heatmap of the selected bone's vertex weights (blue = 0, red = 1)")) {
				component.DebugWeightsBone = selected - 1;
			}
			ComponentDrawer::EndIndent();

			// ---- Runtime IK ----
			ComponentDrawer::DrawSectionHeader("", "IK Chains");
			ComponentDrawer::BeginIndent();
			TextWrapped("Each chain bends its bones so the end bone reaches the target entity (applied after the animation).", TextVariant::Muted);

			std::vector<std::string> nodeNames;
			for (const auto& n : skeleton->Nodes)
				if (n.BoneIndex >= 0) nodeNames.push_back(n.Name);

			// Target candidates: every entity in the scene except this one
			std::vector<std::string> targetNames{ "None" };
			std::vector<uint64_t> targetIDs{ 0 };
			if (Scene* scene = entity.GetScene()) {
				auto view = scene->GetAllEntitiesWith<IDComponent, TagComponent>();
				for (auto e : view) {
					if (e == static_cast<entt::entity>(entity)) continue;
					targetIDs.push_back(static_cast<uint64_t>(view.template get<IDComponent>(e).ID));
					targetNames.push_back(view.template get<TagComponent>(e).Tag);
				}
			}

			int removeChain = -1;
			for (size_t i = 0; i < component.IKChains.size(); i++) {
				auto& chain = component.IKChains[i];
				ImGui::PushID(static_cast<int>(i));
				ImGui::Separator();
				PropertyCheckbox("Enabled", chain.Enabled);

				int boneIdx = 0;
				for (size_t b = 0; b < nodeNames.size(); b++) if (nodeNames[b] == chain.EndBone) boneIdx = static_cast<int>(b);
				if (!nodeNames.empty() && PropertyDropdown("End Bone", boneIdx, nodeNames, "Bone that reaches the target (hand, foot...)"))
					chain.EndBone = nodeNames[boneIdx];

				int targetIdx = 0;
				for (size_t t = 0; t < targetIDs.size(); t++) if (targetIDs[t] == chain.TargetEntity) targetIdx = static_cast<int>(t);
				if (PropertyDropdown("Target", targetIdx, targetNames, "Entity whose position the end bone follows"))
					chain.TargetEntity = targetIDs[targetIdx];

				float chainLen = static_cast<float>(chain.ChainLength);
				if (PropertyFloat("Chain Length", chainLen, 0.05f, 1.0f, 10.0f, "Joints above the end bone that bend (2 = arm / leg)"))
					chain.ChainLength = std::max(1, static_cast<int>(chainLen + 0.5f));
				PropertySlider("Weight", chain.Weight, 0.0f, 1.0f, "%.2f");
				if (Button("Remove Chain", ButtonVariant::Default, ButtonSize::Small)) removeChain = static_cast<int>(i);
				ImGui::PopID();
			}
			if (removeChain >= 0) component.IKChains.erase(component.IKChains.begin() + removeChain);

			if (ButtonBlock("+ Add IK Chain")) {
				AnimatorComponent::IKChain chain;
				auto it = std::find_if(nodeNames.begin(), nodeNames.end(), [](const std::string& n) { return n.find("Hand") != std::string::npos; });
				if (it != nodeNames.end()) chain.EndBone = *it;
				else if (!nodeNames.empty()) chain.EndBone = nodeNames.back();
				component.IKChains.push_back(chain);
			}
			ComponentDrawer::EndIndent();
		});
	}

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
					
					if (ComponentDrawer::BeginInfoCard("##MeshAssetInfo", 240.0f)) {
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
					if (ComponentDrawer::BeginInfoCard("##ModelInfo", 220.0f)) {
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
					ComponentDrawer::DrawDropZone("Drop Mesh Asset Here\n(.lumesh, .obj, .fbx, .gltf, .glb, .dae)", Size(-1, 70));

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
						else if (ext == ".luprefab" || ext == ".lunex" || ext == ".lumat") {
							// Silently ignore non-mesh asset types dropped on mesh slot
						}
						else {
							LNX_LOG_WARN("Unsupported model format for Mesh Renderer: {0}", ext);
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
			if (PropertySlider("Specular", specular, 0.0f, 1.0f, "%.2f", "Specular reflection intensity")) {
				component.SetSpecular(specular, true);
			}

			float normalIntensity = component.GetNormalIntensity();
			if (PropertySlider("Normal Intensity", normalIntensity, 0.0f, 2.0f, "%.2f", "Strength of the normal map effect")) {
				component.SetNormalIntensity(normalIntensity, true);
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
	// CYLINDER COLLIDER 3D COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawCylinderCollider3DComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<CylinderCollider3DComponent>("Cylinder Collider 3D", entity, [](auto& component) {
			ComponentDrawer::DrawSectionHeader("", "Shape");
			ComponentDrawer::BeginIndent();

			PropertyVec3("Offset", component.Offset, 0.01f, "Center offset from entity position");
			PropertyVec3("Half Extents", component.HalfExtents, 0.01f, "Half-size on each axis (x=radius, y=halfHeight, z=radius)");

			ComponentDrawer::EndIndent();
		});
	}

	// ============================================================================
	// CONE COLLIDER 3D COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawConeCollider3DComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<ConeCollider3DComponent>("Cone Collider 3D", entity, [](auto& component) {
			ComponentDrawer::DrawSectionHeader("", "Shape");
			ComponentDrawer::BeginIndent();

			PropertyVec3("Offset", component.Offset, 0.01f, "Center offset from entity position");
			PropertyFloat("Radius", component.Radius, 0.01f, 0.01f, 100.0f, "Cone base radius");
			PropertyFloat("Height", component.Height, 0.01f, 0.01f, 100.0f, "Cone height");

			ComponentDrawer::EndIndent();
		});
	}

	// ============================================================================
	// MESH COLLIDER 3D COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawMeshCollider3DComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<MeshCollider3DComponent>("Mesh Collider 3D", entity, [](auto& component) {
			ComponentDrawer::DrawSectionHeader("", "Collision Type");
			ComponentDrawer::BeginIndent();

			const char* collisionTypes[] = { "Convex", "Triangle Mesh" };
			int currentType = (int)component.Type;

			if (PropertyDropdown("Type", currentType, collisionTypes, 2, "Convex = fast, Triangle Mesh = accurate (static only)")) {
				component.Type = (MeshCollider3DComponent::CollisionType)currentType;
			}

			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Mesh Source");
			ComponentDrawer::BeginIndent();

			PropertyCheckbox("Use Entity Mesh", component.UseEntityMesh, "Use the MeshComponent attached to this entity");

			if (!component.UseEntityMesh) {
				AddSpacing(SpacingValues::XS);
				TextWrapped("Custom mesh data can be assigned via script.", TextVariant::Muted);
			}

			ComponentDrawer::EndIndent();
		});
	}

	// ============================================================================
	// CHARACTER CONTROLLER 3D COMPONENT
	// ============================================================================
	
	void PropertiesPanel::DrawCharacterController3DComponent(Entity entity) {
		using namespace UI;
		
		ComponentDrawer::Draw<CharacterController3DComponent>("Character Controller 3D", entity, [](auto& component) {
			ComponentDrawer::DrawSectionHeader("", "Capsule Shape");
			ComponentDrawer::BeginIndent();

			PropertyFloat("Radius", component.Radius, 0.01f, 0.01f, 10.0f, "Character capsule radius");
			PropertyFloat("Height", component.Height, 0.01f, 0.1f, 10.0f, "Character total height");

			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Movement");
			ComponentDrawer::BeginIndent();

			PropertyFloat("Move Speed", component.MoveSpeed, 0.1f, 0.0f, 100.0f, "Default movement speed (m/s)");
			PropertyFloat("Jump Force", component.JumpForce, 0.1f, 0.0f, 50.0f, "Initial jump velocity (m/s)");
			PropertyFloat("Gravity Scale", component.GravityScale, 0.01f, 0.0f, 10.0f, "Multiplier for gravity");

			ComponentDrawer::EndIndent();

			ComponentDrawer::DrawSectionHeader("", "Ground Detection");
			ComponentDrawer::BeginIndent();

			PropertyFloat("Step Height", component.StepHeight, 0.01f, 0.0f, 2.0f, "Max step the character can climb");
			PropertyFloat("Max Slope Angle", component.MaxSlopeAngle, 1.0f, 0.0f, 90.0f, "Max walkable slope in degrees");
			PropertyFloat("Skin Width", component.SkinWidth, 0.01f, 0.0f, 1.0f, "Small offset to prevent getting stuck");

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