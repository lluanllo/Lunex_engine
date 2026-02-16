#pragma once

/**
 * @file NodeEditorStyle.h
 * @brief Lunex-themed styling for the imnodes node editor
 * 
 * Integrates imnodes visual appearance with the LunexUI theme system.
 * Call ApplyLunexStyle() after ImNodes::CreateContext().
 */

#include <imnodes.h>
#include "../UICore.h"

namespace Lunex::UI {

	inline void ApplyNodeEditorStyle(ImNodesStyle* style = nullptr) {
		if (!style)
			style = &ImNodes::GetStyle();

		// Grid
		style->GridSpacing = 32.0f;
		style->Colors[ImNodesCol_GridBackground] = Colors::BgDark().ToImU32();
		style->Colors[ImNodesCol_GridLine] = Color(0.15f, 0.18f, 0.22f, 0.40f).ToImU32();
		style->Colors[ImNodesCol_GridLinePrimary] = Color(0.15f, 0.18f, 0.22f, 0.70f).ToImU32();

		// Node
		style->NodeCornerRounding = 6.0f;
		style->NodePadding = ImVec2(8.0f, 4.0f);
		style->NodeBorderThickness = 1.0f;
		style->Colors[ImNodesCol_NodeBackground] = Colors::BgCard().ToImU32();
		style->Colors[ImNodesCol_NodeBackgroundHovered] = Colors::BgHover().ToImU32();
		style->Colors[ImNodesCol_NodeBackgroundSelected] = Color(0.14f, 0.20f, 0.28f, 1.0f).ToImU32();
		style->Colors[ImNodesCol_NodeOutline] = Colors::BorderLight().ToImU32();

		// Title bar
		style->Colors[ImNodesCol_TitleBar] = Color(0.16f, 0.22f, 0.30f, 1.0f).ToImU32();
		style->Colors[ImNodesCol_TitleBarHovered] = Color(0.20f, 0.28f, 0.36f, 1.0f).ToImU32();
		style->Colors[ImNodesCol_TitleBarSelected] = Colors::Primary().WithAlpha(0.60f).ToImU32();

		// Links
		style->LinkThickness = 3.0f;
		style->LinkLineSegmentsPerLength = 0.1f;
		style->LinkHoverDistance = 10.0f;
		style->Colors[ImNodesCol_Link] = Color(0.60f, 0.65f, 0.70f, 0.80f).ToImU32();
		style->Colors[ImNodesCol_LinkHovered] = Colors::Primary().ToImU32();
		style->Colors[ImNodesCol_LinkSelected] = Colors::Primary().Lighter(0.1f).ToImU32();

		// Pins
		style->PinCircleRadius = 4.5f;
		style->PinQuadSideLength = 7.0f;
		style->PinTriangleSideLength = 9.0f;
		style->PinLineThickness = 1.5f;
		style->PinHoverRadius = 10.0f;
		style->PinOffset = 0.0f;
		style->Colors[ImNodesCol_Pin] = Color(0.70f, 0.75f, 0.80f, 1.0f).ToImU32();
		style->Colors[ImNodesCol_PinHovered] = Colors::Primary().ToImU32();

		// Selection
		style->Colors[ImNodesCol_BoxSelector] = Colors::Primary().WithAlpha(0.10f).ToImU32();
		style->Colors[ImNodesCol_BoxSelectorOutline] = Colors::Primary().WithAlpha(0.60f).ToImU32();

		// Mini-map
		style->MiniMapPadding = ImVec2(8.0f, 8.0f);
		style->MiniMapOffset = ImVec2(8.0f, 8.0f);
		style->Colors[ImNodesCol_MiniMapBackground] = Colors::BgDark().WithAlpha(0.85f).ToImU32();
		style->Colors[ImNodesCol_MiniMapBackgroundHovered] = Colors::BgMedium().WithAlpha(0.90f).ToImU32();
		style->Colors[ImNodesCol_MiniMapOutline] = Colors::BorderLight().ToImU32();
		style->Colors[ImNodesCol_MiniMapOutlineHovered] = Colors::Primary().ToImU32();
		style->Colors[ImNodesCol_MiniMapNodeBackground] = Colors::BgLight().ToImU32();
		style->Colors[ImNodesCol_MiniMapNodeBackgroundHovered] = Colors::BgHover().ToImU32();
		style->Colors[ImNodesCol_MiniMapNodeBackgroundSelected] = Colors::Primary().WithAlpha(0.40f).ToImU32();
		style->Colors[ImNodesCol_MiniMapNodeOutline] = Colors::BorderLight().ToImU32();
		style->Colors[ImNodesCol_MiniMapLink] = Color(0.50f, 0.55f, 0.60f, 0.50f).ToImU32();
		style->Colors[ImNodesCol_MiniMapLinkSelected] = Colors::Primary().WithAlpha(0.70f).ToImU32();
		style->Colors[ImNodesCol_MiniMapCanvas] = Colors::BgDark().WithAlpha(0.30f).ToImU32();
		style->Colors[ImNodesCol_MiniMapCanvasOutline] = Colors::Primary().WithAlpha(0.40f).ToImU32();

		// Flags
		style->Flags = ImNodesStyleFlags_NodeOutline | ImNodesStyleFlags_GridLines | ImNodesStyleFlags_GridLinesPrimary;
	}

} // namespace Lunex::UI
