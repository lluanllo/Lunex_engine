/**
 * @file UIWidgets.h
 * @brief Lunex UI Framework - Widget Components (Legacy Compatibility)
 * 
 * @deprecated This file is kept for backwards compatibility.
 * New code should include Components/Components.h directly or
 * use LunexUI.h for the complete framework.
 * 
 * All widgets have been refactored into individual component files
 * in the Components/ directory for better maintainability.
 */

#pragma once

// Include all components for backwards compatibility
#include "Components/Components.h"

// Legacy type aliases for backwards compatibility
namespace Lunex::UI {
	
	// Re-export from Components for legacy code
	using AssetCardStyle = ::Lunex::UI::AssetCardStyle;
	using AssetCardResult = ::Lunex::UI::AssetCardResult;
	using EntityNodeStyle = ::Lunex::UI::EntityNodeStyle;
	using EntityNodeResult = ::Lunex::UI::EntityNodeResult;
	using MaterialPreviewStyle = ::Lunex::UI::MaterialPreviewStyle;
	using MaterialPreviewResult = ::Lunex::UI::MaterialPreviewResult;
	using ScriptEntryResult = ::Lunex::UI::ScriptEntryResult;
	using ToolbarStyle = ::Lunex::UI::ToolbarStyle;
	using StatusBarStyle = ::Lunex::UI::StatusBarStyle;
	using BreadcrumbItem = ::Lunex::UI::BreadcrumbItem;
	using InputDialogResult = ::Lunex::UI::InputDialogResult;
	using ConfirmDialogResult = ::Lunex::UI::ConfirmDialogResult;
	
	// Legacy function aliases
	inline AssetCardResult AssetCard(const std::string& id, const std::string& name,
									 const std::string& typeLabel, Ref<Texture2D> thumbnail,
									 bool isSelected = false, bool isDirectory = false,
									 bool isWideAspect = false,
									 const AssetCardStyle& style = AssetCardStyle()) {
		return RenderAssetCard(id, name, typeLabel, thumbnail, isSelected, isDirectory, isWideAspect, style);
	}
	
	inline EntityNodeResult EntityNode(const std::string& label, uint64_t entityID, int depth,
									   bool isSelected, bool hasChildren, bool isExpanded,
									   Ref<Texture2D> icon = nullptr,
									   const EntityNodeStyle& style = EntityNodeStyle()) {
		return RenderEntityNode(label, entityID, depth, isSelected, hasChildren, isExpanded, icon, style);
	}
	
	inline MaterialPreviewResult MaterialPreview(const std::string& id, const std::string& materialName,
												 Ref<Texture2D> thumbnail, bool hasOverrides = false,
												 const std::string& assetPath = "",
												 const MaterialPreviewStyle& style = MaterialPreviewStyle()) {
		return RenderMaterialPreview(id, materialName, thumbnail, hasOverrides, assetPath, style);
	}
	
	inline ScriptEntryResult ScriptEntry(const std::string& id, const std::string& scriptPath,
										 int index, bool isLoaded) {
		return RenderScriptEntry(id, scriptPath, index, isLoaded);
	}
	
	inline bool SearchBar(const std::string& id, char* buffer, size_t bufferSize,
						  const char* placeholder = "Search...", float width = 200.0f) {
		return RenderSearchBar(id, buffer, bufferSize, placeholder, width);
	}
	
	inline int Breadcrumb(const std::string& id, const std::vector<BreadcrumbItem>& items) {
		return RenderBreadcrumb(id, items);
	}
	
	// Progress indicators (kept here as simple functions)
	void ProgressBar(float progress, const Size& size = Size(-1, 0), const char* overlay = nullptr);
	void Spinner(const std::string& id, float radius = 10.0f, const Color& color = Colors::Primary());
	
	// Keyboard shortcut display
	void KeyboardShortcut(const std::string& shortcut);

} // namespace Lunex::UI
