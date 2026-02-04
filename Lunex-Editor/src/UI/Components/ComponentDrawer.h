/**
 * @file ComponentDrawer.h
 * @brief ECS Component UI Drawer - Abstracts component rendering in Properties Panel
 */

#pragma once

#include "../LunexUI.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Renderer/Texture.h"
#include "Assets/Materials/MaterialAsset.h"
#include <functional>

namespace Lunex::UI {

	// ============================================================================
	// COMPONENT SECTION STYLES
	// ============================================================================
	
	struct ComponentStyle {
		static constexpr float INDENT_SIZE = 12.0f;
		static constexpr float COLUMN_WIDTH = 120.0f;
		
		static Color HeaderColor() { return Color(0.85f, 0.85f, 0.85f, 1.0f); }
		static Color SubheaderColor() { return Color(0.7f, 0.7f, 0.7f, 1.0f); }
		static Color HintColor() { return Color(0.5f, 0.5f, 0.5f, 1.0f); }
		static Color AccentColor() { return Color(0.26f, 0.59f, 0.98f, 1.0f); }
		static Color SuccessColor() { return Color(0.3f, 0.8f, 0.3f, 1.0f); }
		static Color WarningColor() { return Color(0.8f, 0.6f, 0.2f, 1.0f); }
		static Color DangerColor() { return Color(0.8f, 0.3f, 0.3f, 1.0f); }
		static Color BgDark() { return Color(0.16f, 0.16f, 0.17f, 1.0f); }
		static Color BgMedium() { return Color(0.22f, 0.22f, 0.24f, 1.0f); }
	};
	
	// ============================================================================
	// COMPONENT DRAWER RESULT
	// ============================================================================
	
	struct ComponentDrawResult {
		bool isOpen = false;
		bool removeClicked = false;
	};
	
	// ============================================================================
	// COMPONENT DRAWER CLASS
	// ============================================================================
	
	/**
	 * @brief Helper class for drawing ECS component UI
	 * 
	 * Provides a consistent way to render component sections with
	 * collapsible headers and remove buttons.
	 */
	class ComponentDrawer {
	public:
		/**
		 * @brief Begin drawing a component section
		 * @param name Display name of the component
		 * @param canRemove Whether the component can be removed
		 * @return Result containing isOpen and removeClicked states
		 */
		template<typename T>
		static ComponentDrawResult Begin(const std::string& name, Entity entity, bool canRemove = true);
		
		/**
		 * @brief End the component section (must call if Begin returned isOpen=true)
		 */
		static void End();
		
		/**
		 * @brief Draw a complete component with a lambda for the content
		 */
		template<typename T, typename UIFunction>
		static void Draw(const std::string& name, Entity entity, UIFunction uiFunction, bool canRemove = true);
		
		// ============================================================================
		// HELPER METHODS FOR COMMON UI PATTERNS
		// ============================================================================
		
		/**
		 * @brief Draw a section header with icon
		 */
		static void DrawSectionHeader(const char* icon, const std::string& title);
		
		/**
		 * @brief Begin indented section
		 */
		static void BeginIndent();
		
		/**
		 * @brief End indented section
		 */
		static void EndIndent();
		
		/**
		 * @brief Draw an info card with child window
		 */
		static bool BeginInfoCard(const std::string& id, float height = 100.0f);
		static void EndInfoCard();
		
		/**
		 * @brief Draw a drop zone button
		 */
		static void DrawDropZone(const std::string& text, const Size& size = Size(-1, 60));
		
		/**
		 * @brief Check if item was dropped and get payload
		 */
		static bool AcceptDropPayload(const char* payloadType, void** outData);
		
	private:
		static void BeginPropertyGrid();
		static void EndPropertyGrid();
	};
	
	// ============================================================================
	// TEMPLATE IMPLEMENTATIONS
	// ============================================================================
	
	template<typename T>
	ComponentDrawResult ComponentDrawer::Begin(const std::string& name, Entity entity, bool canRemove) {
		ComponentDrawResult result;
		
		if (!entity.HasComponent<T>()) {
			return result;
		}
		
		const ImGuiTreeNodeFlags treeNodeFlags = 
			ImGuiTreeNodeFlags_DefaultOpen | 
			ImGuiTreeNodeFlags_Framed | 
			ImGuiTreeNodeFlags_SpanAvailWidth | 
			ImGuiTreeNodeFlags_AllowOverlap | 
			ImGuiTreeNodeFlags_FramePadding;
		
		ImVec2 contentRegionAvailable = ImGui::GetContentRegionAvail();
		
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 6));
		float lineHeight = ImGui::GetTextLineHeight() + ImGui::GetStyle().FramePadding.y * 2.0f;
		
		ImGui::PushStyleColor(ImGuiCol_Header, ToImVec4(ComponentStyle::BgMedium()));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.26f, 0.26f, 0.28f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.30f, 0.30f, 0.32f, 1.0f));
		
		result.isOpen = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, "%s", name.c_str());
		
		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
		
		// Settings button
		ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
		
		ImGui::PushID((int)(intptr_t)(void*)typeid(T).hash_code());
		
		if (!canRemove) {
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
		}
		
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.3f, 0.3f, 0.32f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.4f, 0.4f, 0.42f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.35f, 0.35f, 0.37f, 1.0f));
		
		if (ImGui::Button("+", ImVec2(lineHeight, lineHeight))) {
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
		
		if (canRemove && ImGui::BeginPopup("ComponentSettings")) {
			if (ImGui::MenuItem("Remove component")) {
				result.removeClicked = true;
			}
			ImGui::EndPopup();
		}
		
		ImGui::PopID();
		
		if (result.isOpen) {
			ImGui::Indent(ComponentStyle::INDENT_SIZE);
			BeginPropertyGrid();
		}
		
		return result;
	}
	
	template<typename T, typename UIFunction>
	void ComponentDrawer::Draw(const std::string& name, Entity entity, UIFunction uiFunction, bool canRemove) {
		if (!entity.HasComponent<T>()) {
			return;
		}
		
		auto& component = entity.GetComponent<T>();
		auto result = Begin<T>(name, entity, canRemove);
		
		if (result.isOpen) {
			uiFunction(component);
			End();
		}
		
		if (result.removeClicked) {
			// Special handling for MeshComponent - also remove MaterialComponent
			if constexpr (std::is_same_v<T, MeshComponent>) {
				if (entity.HasComponent<MaterialComponent>()) {
					entity.RemoveComponent<MaterialComponent>();
				}
			}
			entity.RemoveComponent<T>();
		}
	}

} // namespace Lunex::UI
