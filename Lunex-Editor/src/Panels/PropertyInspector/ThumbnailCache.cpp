#include "stpch.h"
#include "ThumbnailCache.h"

namespace Lunex {

	PropertyThumbnailCache::PropertyThumbnailCache() {
		// Lazy initialization - renderer will be created on first use
	}

	void PropertyThumbnailCache::InitializeRenderer() {
		if (!m_PreviewRenderer) {
			LNX_LOG_INFO("PropertyThumbnailCache: Initializing MaterialPreviewRenderer...");
			try {
				m_PreviewRenderer = CreateScope<MaterialPreviewRenderer>();
				m_PreviewRenderer->SetResolution(128, 128);
				m_PreviewRenderer->SetAutoRotate(false);
			}
			catch (const std::exception& e) {
				LNX_LOG_ERROR("Failed to initialize MaterialPreviewRenderer: {0}", e.what());
			}
		}
	}

	Ref<Texture2D> PropertyThumbnailCache::GetOrGenerateThumbnail(const Ref<MaterialAsset>& asset) {
		if (!asset) return nullptr;

		UUID assetID = asset->GetID();

		// Check cache first
		auto it = m_ThumbnailCache.find(assetID);
		if (it != m_ThumbnailCache.end() && it->second) {
			return it->second;
		}

		// Initialize renderer if needed
		InitializeRenderer();

		if (!m_PreviewRenderer) {
			return nullptr;
		}

		// Generate new thumbnail
		try {
			Ref<Texture2D> thumbnail = m_PreviewRenderer->RenderToTexture(asset);
			if (thumbnail) {
				m_ThumbnailCache[assetID] = thumbnail;
				return thumbnail;
			}
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Failed to generate thumbnail for material {0}: {1}", 
				asset->GetName(), e.what());
		}

		return nullptr;
	}

	void PropertyThumbnailCache::InvalidateThumbnail(UUID assetID) {
		auto it = m_ThumbnailCache.find(assetID);
		if (it != m_ThumbnailCache.end()) {
			m_ThumbnailCache.erase(it);
		}
	}

	void PropertyThumbnailCache::ClearCache() {
		m_ThumbnailCache.clear();
		LNX_LOG_TRACE("Cleared property thumbnail cache");
	}

	bool PropertyThumbnailCache::HasThumbnail(UUID assetID) const {
		return m_ThumbnailCache.find(assetID) != m_ThumbnailCache.end();
	}

} // namespace Lunex
