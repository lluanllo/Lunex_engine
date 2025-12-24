#pragma once

#include "Core/Core.h"
#include "Renderer/Texture.h"
#include "Renderer/MaterialPreviewRenderer.h"
#include "Assets/Materials/MaterialAsset.h"
#include <filesystem>
#include <unordered_map>
#include <string>
#include <cstdint>

namespace Lunex {

	// ============================================================================
	// THUMBNAIL MANAGER - Handles icon loading and thumbnail generation/caching
	// ============================================================================
	class ThumbnailManager {
	public:
		ThumbnailManager();
		~ThumbnailManager() = default;

		// Icon management
		void LoadIcons(const std::filesystem::path& iconDirectory);
		Ref<Texture2D> GetIconForFile(const std::filesystem::path& path);
		Ref<Texture2D> GetDirectoryIcon() const { return m_DirectoryIcon; }
		Ref<Texture2D> GetBackIcon() const { return m_BackIcon; }
		Ref<Texture2D> GetForwardIcon() const { return m_ForwardIcon; }

		// Thumbnail generation
		Ref<Texture2D> GetThumbnailForFile(const std::filesystem::path& path);
		
		// Material preview
		Ref<Texture2D> GetMaterialThumbnail(const std::filesystem::path& path, 
			const Ref<MaterialAsset>& material);
		
		// Mesh preview
		Ref<Texture2D> GetMeshThumbnail(const std::filesystem::path& path);
		
		// Prefab preview
		Ref<Texture2D> GetPrefabThumbnail(const std::filesystem::path& path,
			const std::filesystem::path& baseDirectory);

		// Asset type identification
		std::string GetAssetTypeLabel(const std::filesystem::path& path);
		uint32_t GetAssetTypeBorderColor(const std::filesystem::path& path);
		bool IsHDRFile(const std::filesystem::path& path);

		// Cache management
		void InvalidateThumbnail(const std::filesystem::path& path);
		void InvalidateMaterialDiskCache(const std::filesystem::path& path);
		void ClearAllCaches();
		void ClearTextureCache() { m_TextureCache.clear(); }
		void ClearMaterialCache() { m_MaterialThumbnailCache.clear(); }
		void ClearMeshCache() { m_MeshThumbnailCache.clear(); }

	private:
		void InitializePreviewRenderer();
		Ref<Texture2D> LoadTextureThumbnail(const std::filesystem::path& path);

	private:
		// Navigation icons
		Ref<Texture2D> m_DirectoryIcon;
		Ref<Texture2D> m_FileIcon;
		Ref<Texture2D> m_BackIcon;
		Ref<Texture2D> m_ForwardIcon;

		// File type icons
		Ref<Texture2D> m_SceneIcon;
		Ref<Texture2D> m_TextureIcon;
		Ref<Texture2D> m_ShaderIcon;
		Ref<Texture2D> m_AudioIcon;
		Ref<Texture2D> m_ScriptIcon;
		Ref<Texture2D> m_MaterialIcon;
		Ref<Texture2D> m_MeshIcon;
		Ref<Texture2D> m_PrefabIcon;
		Ref<Texture2D> m_AnimationIcon;
		Ref<Texture2D> m_SkeletonIcon;

		// Preview renderer for 3D thumbnails
		Scope<MaterialPreviewRenderer> m_PreviewRenderer;

		// Caches
		std::unordered_map<std::string, Ref<Texture2D>> m_TextureCache;
		std::unordered_map<std::string, Ref<Texture2D>> m_MaterialThumbnailCache;
		std::unordered_map<std::string, Ref<Texture2D>> m_MeshThumbnailCache;
	};

} // namespace Lunex
