#include "stpch.h"
#include "ThumbnailManager.h"
#include "Assets/Materials/MaterialRegistry.h"
#include "Assets/Mesh/MeshAsset.h"
#include "Asset/Prefab.h"

#include <algorithm>
#include <sstream>

// Include ImGui for IM_COL32 macro
#include <imgui.h>

namespace Lunex {

	ThumbnailManager::ThumbnailManager() {
		LoadIcons("Resources/Icons/ContentBrowser");
	}

	void ThumbnailManager::LoadIcons(const std::filesystem::path& iconDirectory) {
		auto loadIcon = [&](const std::string& name) -> Ref<Texture2D> {
			std::filesystem::path iconPath = iconDirectory / (name + ".png");
			if (std::filesystem::exists(iconPath)) {
				return Texture2D::Create(iconPath.string());
			}
			return nullptr;
		};

		m_DirectoryIcon = loadIcon("FolderIcon");
		m_FileIcon = loadIcon("FileIcon");
		m_BackIcon = loadIcon("BackIcon");
		m_ForwardIcon = loadIcon("ForwardIcon");
		m_SceneIcon = loadIcon("SceneIcon");
		m_TextureIcon = loadIcon("ImageIcon");
		m_ShaderIcon = loadIcon("ShaderIcon");
		m_AudioIcon = loadIcon("AudioIcon");
		m_ScriptIcon = loadIcon("ScriptIcon");
		m_MaterialIcon = loadIcon("MaterialIcon");
		m_MeshIcon = loadIcon("MeshIcon");
		m_PrefabIcon = loadIcon("PrefabIcon");
		m_AnimationIcon = loadIcon("AnimationIcon");
		m_SkeletonIcon = loadIcon("SkeletonIcon");
	}

	void ThumbnailManager::InitializePreviewRenderer() {
		if (!m_PreviewRenderer) {
			m_PreviewRenderer = CreateScope<MaterialPreviewRenderer>();
			m_PreviewRenderer->SetResolution(160, 160);
			m_PreviewRenderer->SetAutoRotate(false);
			m_PreviewRenderer->SetBackgroundColor(glm::vec4(0.432f, 0.757f, 1.0f, 1.0f)); // #6EC1FF
			m_PreviewRenderer->SetCameraPosition(glm::vec3(0.0f, -0.3f, 2.5f));
		}
	}

	Ref<Texture2D> ThumbnailManager::GetIconForFile(const std::filesystem::path& path) {
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

		if (extension == ".lunex") return m_SceneIcon ? m_SceneIcon : m_FileIcon;
		if (extension == ".lumat") return m_MaterialIcon ? m_MaterialIcon : m_FileIcon;
		if (extension == ".lumesh") return m_MeshIcon ? m_MeshIcon : m_FileIcon;
		if (extension == ".luprefab") return m_PrefabIcon ? m_PrefabIcon : m_FileIcon;
		if (extension == ".luanim") return m_AnimationIcon ? m_AnimationIcon : m_FileIcon;
		if (extension == ".luskel") return m_SkeletonIcon ? m_SkeletonIcon : m_FileIcon;
		if (extension == ".png" || extension == ".jpg" || extension == ".jpeg")
			return m_TextureIcon ? m_TextureIcon : m_FileIcon;
		if (extension == ".glsl" || extension == ".shader")
			return m_ShaderIcon ? m_ShaderIcon : m_FileIcon;
		if (extension == ".wav" || extension == ".mp3" || extension == ".ogg")
			return m_AudioIcon ? m_AudioIcon : m_FileIcon;
		if (extension == ".cpp" || extension == ".h" || extension == ".cs")
			return m_ScriptIcon ? m_ScriptIcon : m_FileIcon;

		return m_FileIcon;
	}

	Ref<Texture2D> ThumbnailManager::GetThumbnailForFile(const std::filesystem::path& path) {
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

		// HDR files
		if (extension == ".hdr" || extension == ".hdri") {
			return LoadTextureThumbnail(path);
		}

		// Animation files
		if (extension == ".luanim") {
			return m_AnimationIcon ? m_AnimationIcon : m_FileIcon;
		}

		// Skeleton files
		if (extension == ".luskel") {
			return m_SkeletonIcon ? m_SkeletonIcon : m_FileIcon;
		}

		// Material files
		if (extension == ".lumat") {
			auto it = m_MaterialThumbnailCache.find(path.string());
			if (it != m_MaterialThumbnailCache.end() && it->second) {
				return it->second;
			}
			// Material thumbnails require material to be loaded separately
			return m_MaterialIcon ? m_MaterialIcon : m_FileIcon;
		}

		// Mesh files
		if (extension == ".lumesh") {
			return GetMeshThumbnail(path);
		}

		// Image files
		if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" ||
			extension == ".bmp" || extension == ".tga") {
			return LoadTextureThumbnail(path);
		}

		return GetIconForFile(path);
	}

	Ref<Texture2D> ThumbnailManager::LoadTextureThumbnail(const std::filesystem::path& path) {
		std::string pathStr = path.string();

		auto it = m_TextureCache.find(pathStr);
		if (it != m_TextureCache.end()) {
			return it->second;
		}

		try {
			Ref<Texture2D> texture = Texture2D::Create(pathStr);
			if (texture && texture->IsLoaded()) {
				m_TextureCache[pathStr] = texture;
				return texture;
			}
		}
		catch (...) {
			LNX_LOG_WARN("Failed to load texture thumbnail: {0}", path.filename().string());
		}

		return m_TextureIcon ? m_TextureIcon : m_FileIcon;
	}

	Ref<Texture2D> ThumbnailManager::GetMaterialThumbnail(const std::filesystem::path& path,
		const Ref<MaterialAsset>& material) {
		std::string pathStr = path.string();

		auto it = m_MaterialThumbnailCache.find(pathStr);
		if (it != m_MaterialThumbnailCache.end() && it->second) {
			return it->second;
		}

		InitializePreviewRenderer();

		try {
			if (material && m_PreviewRenderer) {
				Ref<Texture2D> thumbnail = m_PreviewRenderer->GetOrGenerateCachedThumbnail(path, material);
				if (thumbnail) {
					m_MaterialThumbnailCache[pathStr] = thumbnail;
					return thumbnail;
				}
			}
		}
		catch (...) {
			LNX_LOG_WARN("Failed to generate material thumbnail: {0}", path.filename().string());
		}

		return m_MaterialIcon ? m_MaterialIcon : m_FileIcon;
	}

	Ref<Texture2D> ThumbnailManager::GetMeshThumbnail(const std::filesystem::path& path) {
		std::string pathStr = path.string();

		auto it = m_MeshThumbnailCache.find(pathStr);
		if (it != m_MeshThumbnailCache.end() && it->second) {
			return it->second;
		}

		InitializePreviewRenderer();

		try {
			auto meshAsset = MeshAsset::LoadFromFile(path);
			if (meshAsset && meshAsset->GetModel() && m_PreviewRenderer) {
				Ref<Model> originalModel = m_PreviewRenderer->GetPreviewModel();
				m_PreviewRenderer->SetPreviewModel(meshAsset->GetModel());

				auto originalCameraPos = m_PreviewRenderer->GetCameraPosition();
				auto originalBgColor = m_PreviewRenderer->GetBackgroundColor();

				m_PreviewRenderer->SetCameraPosition(glm::vec3(2.0f, 1.2f, 2.5f));
				m_PreviewRenderer->SetBackgroundColor(glm::vec4(0.447f, 0.592f, 0.761f, 1.0f)); // #7297C2

				auto defaultMaterial = CreateRef<MaterialAsset>("MeshPreview");
				defaultMaterial->SetAlbedo(glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
				defaultMaterial->SetMetallic(0.0f);
				defaultMaterial->SetRoughness(0.5f);

				Ref<Texture2D> thumbnail = m_PreviewRenderer->RenderToTexture(defaultMaterial);

				m_PreviewRenderer->SetCameraPosition(originalCameraPos);
				m_PreviewRenderer->SetBackgroundColor(originalBgColor);
				m_PreviewRenderer->SetPreviewModel(originalModel);

				if (thumbnail) {
					m_MeshThumbnailCache[pathStr] = thumbnail;
					return thumbnail;
				}
			}
		}
		catch (...) {
			LNX_LOG_WARN("Failed to generate mesh thumbnail: {0}", path.filename().string());
		}

		return m_MeshIcon ? m_MeshIcon : m_FileIcon;
	}

	Ref<Texture2D> ThumbnailManager::GetPrefabThumbnail(const std::filesystem::path& path,
		const std::filesystem::path& baseDirectory) {
		std::string pathStr = path.string();

		auto it = m_MeshThumbnailCache.find(pathStr);
		if (it != m_MeshThumbnailCache.end() && it->second) {
			return it->second;
		}

		InitializePreviewRenderer();

		try {
			auto prefab = Prefab::LoadFromFile(path);
			if (prefab && m_PreviewRenderer) {
				for (const auto& entityData : prefab->GetEntityData()) {
					for (const auto& compData : entityData.Components) {
						if (compData.ComponentType == "MeshComponent") {
							std::istringstream iss(compData.SerializedData);
							std::string token;

							std::getline(iss, token, ';'); // type
							std::getline(iss, token, ';'); // color
							std::getline(iss, token, ';'); // meshAssetID
							std::getline(iss, token, ';'); // meshAssetPath
							std::string meshAssetPath = token;

							if (!meshAssetPath.empty()) {
								std::filesystem::path fullMeshPath = baseDirectory / meshAssetPath;
								if (std::filesystem::exists(fullMeshPath)) {
									auto meshAsset = MeshAsset::LoadFromFile(fullMeshPath);
									if (meshAsset && meshAsset->GetModel()) {
										Ref<Model> originalModel = m_PreviewRenderer->GetPreviewModel();
										m_PreviewRenderer->SetPreviewModel(meshAsset->GetModel());

										auto originalCameraPos = m_PreviewRenderer->GetCameraPosition();
										auto originalBgColor = m_PreviewRenderer->GetBackgroundColor();

										m_PreviewRenderer->SetCameraPosition(glm::vec3(2.2f, 1.5f, 2.8f));
										m_PreviewRenderer->SetBackgroundColor(glm::vec4(0.447f, 0.592f, 0.761f, 1.0f));

										auto defaultMaterial = CreateRef<MaterialAsset>("PrefabPreview");
										defaultMaterial->SetAlbedo(glm::vec4(0.6f, 0.65f, 0.7f, 1.0f));
										defaultMaterial->SetMetallic(0.0f);
										defaultMaterial->SetRoughness(0.4f);

										Ref<Texture2D> thumbnail = m_PreviewRenderer->RenderToTexture(defaultMaterial);

										m_PreviewRenderer->SetCameraPosition(originalCameraPos);
										m_PreviewRenderer->SetBackgroundColor(originalBgColor);
										m_PreviewRenderer->SetPreviewModel(originalModel);

										if (thumbnail) {
											m_MeshThumbnailCache[pathStr] = thumbnail;
											return thumbnail;
										}
									}
								}
							}
							break;
						}
					}
				}
			}
		}
		catch (...) {
			LNX_LOG_WARN("Failed to generate prefab thumbnail: {0}", path.filename().string());
		}

		return m_PrefabIcon ? m_PrefabIcon : m_FileIcon;
	}

	std::string ThumbnailManager::GetAssetTypeLabel(const std::filesystem::path& path) {
		if (std::filesystem::is_directory(path)) {
			return "FOLDER";
		}

		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

		if (extension == ".lumat") return "MATERIAL";
		if (extension == ".lumesh") return "STATIC MESH";
		if (extension == ".luprefab") return "PREFAB";
		if (extension == ".lunex") return "SCENE";
		if (extension == ".hdr" || extension == ".hdri") return "HDRI";
		if (extension == ".png" || extension == ".jpg" || extension == ".jpeg") return "TEXTURE";
		if (extension == ".glsl" || extension == ".shader") return "SHADER";
		if (extension == ".wav" || extension == ".mp3" || extension == ".ogg") return "AUDIO";
		if (extension == ".cpp" || extension == ".h" || extension == ".cs") return "SCRIPT";
		if (extension == ".luanim") return "ANIMATION";
		if (extension == ".luskel") return "SKELETON";
		if (extension == ".gltf" || extension == ".glb" || extension == ".fbx") return "3D MODEL";

		return "FILE";
	}

	ImU32 ThumbnailManager::GetAssetTypeBorderColor(const std::filesystem::path& path) {
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);

		if (extension == ".lumesh") return IM_COL32(100, 180, 100, 255);   // Green
		if (extension == ".luanim") return IM_COL32(180, 100, 220, 255);   // Purple
		if (extension == ".luskel") return IM_COL32(100, 150, 220, 255);   // Blue
		if (extension == ".lumat") return IM_COL32(220, 150, 50, 255);     // Orange
		if (extension == ".luprefab") return IM_COL32(100, 200, 220, 255); // Cyan

		return IM_COL32(45, 45, 48, 255); // Default: no visible border
	}

	bool ThumbnailManager::IsHDRFile(const std::filesystem::path& path) {
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
		return extension == ".hdr" || extension == ".hdri";
	}

	void ThumbnailManager::InvalidateThumbnail(const std::filesystem::path& path) {
		std::string pathStr = path.string();
		m_TextureCache.erase(pathStr);
		m_MaterialThumbnailCache.erase(pathStr);
		m_MeshThumbnailCache.erase(pathStr);
	}

	void ThumbnailManager::InvalidateMaterialDiskCache(const std::filesystem::path& path) {
		if (m_PreviewRenderer) {
			m_PreviewRenderer->InvalidateCachedThumbnail(path);
		}
	}

	void ThumbnailManager::ClearAllCaches() {
		m_TextureCache.clear();
		m_MaterialThumbnailCache.clear();
		m_MeshThumbnailCache.clear();
		LNX_LOG_INFO("Cleared all thumbnail caches");
	}

} // namespace Lunex
