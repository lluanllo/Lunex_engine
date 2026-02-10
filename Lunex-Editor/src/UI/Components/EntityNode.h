/**
 * @file EntityNode.h
 * @brief Entity Node Component for Hierarchy Panel
 */

#pragma once

#include "../UICore.h"
#include "../UIDragDrop.h"
#include "Renderer/Texture.h"

namespace Lunex::UI {

	// ============================================================================
	// ENTITY NODE COMPONENT
	// ============================================================================
	
	struct EntityNodeStyle {
		Color backgroundColor = Color(0, 0, 0, 0);
		Color selectedColor = Colors::Selected();
		Color hoverColor = Color(0.17f, 0.17f, 0.18f, 0.60f);
		float iconSize = SpacingValues::IconMD;
		float indentPerLevel = 16.0f;
	};
	
	struct EntityNodeResult {
		bool clicked = false;
		bool doubleClicked = false;
		bool rightClicked = false;
		bool expanded = false;
		bool dragStarted = false;
		bool dropReceived = false;
		uint64_t droppedEntityID = 0;
	};
	
	/**
	 * @class EntityNode
	 * @brief Renders an entity node in the scene hierarchy
	 * 
	 * Features:
	 * - Tree node with expand/collapse
	 * - Entity icon
	 * - Selection highlight
	 * - Drag & drop (both source and target)
	 * - Depth-based indentation
	 */
	class EntityNode {
	public:
		EntityNode() = default;
		~EntityNode() = default;
		
		/**
		 * @brief Render the entity node
		 * @param label Entity name
		 * @param entityID Unique entity identifier
		 * @param depth Tree depth level
		 * @param isSelected Whether the entity is selected
		 * @param hasChildren Whether the entity has children
		 * @param isExpanded Whether the node is expanded
		 * @param icon Entity type icon
		 * @return Interaction result
		 */
		EntityNodeResult Render(const std::string& label,
								uint64_t entityID,
								int depth,
								bool isSelected,
								bool hasChildren,
								bool isExpanded,
								Ref<Texture2D> icon = nullptr);
		
		// Style configuration
		void SetStyle(const EntityNodeStyle& style) { m_Style = style; }
		EntityNodeStyle& GetStyle() { return m_Style; }
		const EntityNodeStyle& GetStyle() const { return m_Style; }
		
	private:
		EntityNodeStyle m_Style;
		
		void RenderBackground(ImDrawList* drawList, ImVec2 pos, ImVec2 size, bool isSelected);
		void RenderIcon(Ref<Texture2D> icon);
		ImGuiTreeNodeFlags GetTreeNodeFlags(bool hasChildren, bool isSelected) const;
	};
	
	// ============================================================================
	// STATIC HELPER FUNCTION
	// ============================================================================
	
	EntityNodeResult RenderEntityNode(const std::string& label,
									  uint64_t entityID,
									  int depth,
									  bool isSelected,
									  bool hasChildren,
									  bool isExpanded,
									  Ref<Texture2D> icon = nullptr,
									  const EntityNodeStyle& style = EntityNodeStyle());

} // namespace Lunex::UI
