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
		
		static Color HeaderColor() { return Colors::TextPrimary(); }
		static Color SubheaderColor() { return Colors::TextSecondary(); }
		static Color HintColor() { return Colors::TextMuted(); }
		static Color AccentColor() { return Colors::Primary(); }
		static Color SuccessColor() { return Colors::Success(); }
		static Color WarningColor() { return Colors::Warning(); }
		static Color DangerColor() { return Colors::Danger(); }
		static Color BgDark() { return Colors::BgDark(); }
		static Color BgMedium() { return Colors::BgLight(); }
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
		
		ImGui::PushStyleColor(ImGuiCol_Header, ToImVec4(Colors::BgDark()));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ToImVec4(Colors::BgHover()));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ToImVec4(Colors::BgLight()));
		
		result.isOpen = ImGui::TreeNodeEx((void*)typeid(T).hash_code(), treeNodeFlags, "%s", name.c_str());
		
		ImGui::PopStyleColor(3);
		ImGui::PopStyleVar();
		
		// Settings button
		ImGui::SameLine(contentRegionAvailable.x - lineHeight * 0.5f);
		
		ImGui::PushID((int)(intptr_t)(void*)typeid(T).hash_code());
		
		if (!canRemove) {
			ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.5f);
		}
		
		ImGui::PushStyleColor(ImGuiCol_Button, ToImVec4(Colors::BgLight()));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ToImVec4(Colors::BgHover()));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ToImVec4(Colors::BgDark()));
		
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
