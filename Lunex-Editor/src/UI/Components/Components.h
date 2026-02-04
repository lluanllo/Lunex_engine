/**
 * @file UIComponents.h
 * @brief Lunex UI Framework - All Components Include
 * 
 * Include this file to access all UI components.
 * Components are self-contained, reusable UI elements
 * that can be used to build complex interfaces.
 * 
 * ## Component Architecture
 * 
 * Each component follows the same pattern:
 * 
 * 1. **Style Struct**: Defines appearance options
 * 2. **Result Struct**: Returns interaction state
 * 3. **Component Class**: Encapsulates rendering logic
 * 4. **Static Helper**: Provides quick-use functions
 * 
 * ## Example Usage
 * 
 * ### Using Component Class (Recommended for repeated use):
 * ```cpp
 * UI::AssetCard card;
 * card.SetWidth(120.0f);
 * 
 * for (const auto& asset : assets) {
 *     auto result = card.Render(asset.id, asset.name, asset.type, asset.thumbnail);
 *     if (result.doubleClicked) {
 *         OpenAsset(asset);
 *     }
 * }
 * ```
 * 
 * ### Using Static Helper (Quick one-off use):
 * ```cpp
 * auto result = UI::RenderAssetCard("myAsset", "MyTexture", "Texture", thumbnail);
 * if (result.clicked) {
 *     SelectAsset();
 * }
 * ```
 * 
 * ## Available Components
 * 
 * | Component       | Description                           |
 * |-----------------|---------------------------------------|
 * | AssetCard       | Content browser asset cards           |
 * | EntityNode      | Scene hierarchy entity nodes          |
 * | MaterialPreview | Material preview with actions         |
 * | ScriptEntry     | Script component entries              |
 * | SearchBar       | Search input with icon                |
 * | Toolbar         | Floating toolbar with buttons         |
 * | StatusBar       | Bottom status bar                     |
 * | Toast           | Toast notifications                   |
 * | Breadcrumb      | Path navigation                       |
 * | Dialog          | Input and confirmation dialogs        |
 */

#pragma once

// Base components
#include "AssetCard.h"
#include "EntityNode.h"
#include "MaterialPreview.h"
#include "ScriptEntry.h"

// Navigation
#include "SearchBar.h"
#include "Breadcrumb.h"

// Bars
#include "Toolbar.h"
#include "StatusBar.h"

// Feedback
#include "Toast.h"
#include "Dialog.h"
