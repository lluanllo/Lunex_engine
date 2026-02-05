/**
 * @file UIImage.h
 * @brief Image UI Components
 */

#pragma once

#include "../UICore.h"
#include "Renderer/Texture.h"
#include <filesystem>

namespace Lunex::UI {

	// ============================================================================
	// IMAGE & TEXTURE COMPONENTS
	// ============================================================================
	
	/**
	 * @brief Display an image/texture
	 */
	void Image(Ref<Texture2D> texture,
			   const Size& size,
			   bool flipY = true,
			   const Color& tint = Color(1, 1, 1, 1));
	
	void Image(uint32_t textureID,
			   const Size& size,
			   bool flipY = true,
			   const Color& tint = Color(1, 1, 1, 1));
	
	/**
	 * @brief Image that acts as a button
	 */
	bool ImageButton(const std::string& id,
					 Ref<Texture2D> texture,
					 const Size& size,
					 bool flipY = true,
					 const char* tooltip = nullptr);
	
	/**
	 * @brief Texture slot for material editors etc.
	 */
	struct TextureSlotResult {
		bool textureChanged = false;
		bool removeClicked = false;
		std::string droppedPath;
	};
	
	TextureSlotResult TextureSlot(const std::string& label,
								  const char* icon,
								  Ref<Texture2D> currentTexture,
								  const std::string& currentPath);
	
	/**
	 * @brief Thumbnail preview
	 */
	void Thumbnail(Ref<Texture2D> texture,
				   const Size& size,
				   bool selected = false,
				   bool hovered = false);
	
	/**
	 * @brief Color preview button
	 */
	bool ColorPreviewButton(const std::string& id, const Color& color, const Size& size);

} // namespace Lunex::UI
