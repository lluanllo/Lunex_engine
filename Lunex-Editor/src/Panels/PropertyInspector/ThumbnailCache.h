#pragma once

#include "Core/Core.h"
#include "Renderer/Texture.h"
#include "Renderer/MaterialPreviewRenderer.h"
#include "Assets/Materials/MaterialAsset.h"
#include "Core/UUID.h"
#include <unordered_map>

namespace Lunex {

	// ============================================================================
	// THUMBNAIL CACHE - Manages material preview thumbnails for the Properties panel
	// ============================================================================
	class PropertyThumbnailCache {
	public:
		PropertyThumbnailCache();
		~PropertyThumbnailCache() = default;

		// Get or generate thumbnail for a material
		Ref<Texture2D> GetOrGenerateThumbnail(const Ref<MaterialAsset>& asset);

		// Cache management
		void InvalidateThumbnail(UUID assetID);
		void ClearCache();

		// Check if thumbnail exists
		bool HasThumbnail(UUID assetID) const;

	private:
		void InitializeRenderer();

	private:
		std::unordered_map<UUID, Ref<Texture2D>> m_ThumbnailCache;
		Scope<MaterialPreviewRenderer> m_PreviewRenderer;
	};

} // namespace Lunex
