#include "stpch.h"
#include "MaterialRegistry.h"
#include "Log/Log.h"

#include <algorithm>

namespace Lunex {

	MaterialRegistry::MaterialRegistry() {
		LNX_LOG_INFO("MaterialRegistry initialized");
	}

	// ========== GESTIÓN DE ASSETS ==========

	Ref<MaterialAsset> MaterialRegistry::LoadMaterial(const std::filesystem::path& path) {
		// Normalizar path
		std::string normalizedPath = NormalizePath(path);

		// Verificar si ya está en cache
		auto it = m_PathToUUID.find(normalizedPath);
		if (it != m_PathToUUID.end()) {
			UUID id = it->second;
			auto materialIt = m_MaterialCache.find(id);
			if (materialIt != m_MaterialCache.end()) {
				LNX_LOG_INFO("Material loaded from cache: {0}", normalizedPath);
				return materialIt->second;
			}
		}

		// Cargar desde disco
		Ref<MaterialAsset> material = MaterialAsset::LoadFromFile(path);
		if (!material) {
			LNX_LOG_ERROR("Failed to load material from: {0}", normalizedPath);
			return GetDefaultMaterial();
		}

		// Registrar en cache
		UUID id = material->GetID();
		m_MaterialCache[id] = material;
		m_PathToUUID[normalizedPath] = id;

		// Configurar file watcher
		AddFileWatcher(path, id);

		LNX_LOG_INFO("Material loaded and cached: {0} (ID: {1})", normalizedPath, (uint64_t)id);
		return material;
	}

	void MaterialRegistry::RegisterMaterial(Ref<MaterialAsset> material) {
		if (!material) {
			LNX_LOG_ERROR("MaterialRegistry::RegisterMaterial - Material is null");
			return;
		}

		UUID id = material->GetID();
		m_MaterialCache[id] = material;

		if (!material->GetPath().empty()) {
			std::string normalizedPath = NormalizePath(material->GetPath());
			m_PathToUUID[normalizedPath] = id;

			// Si tiene path, configurar file watcher
			AddFileWatcher(material->GetPath(), id);
		}

		LNX_LOG_INFO("Material registered: {0} (ID: {1})", material->GetName(), (uint64_t)id);
	}

	void MaterialRegistry::UnregisterMaterial(UUID id) {
		auto it = m_MaterialCache.find(id);
		if (it != m_MaterialCache.end()) {
			// Remover de path lookup
			if (!it->second->GetPath().empty()) {
				std::string normalizedPath = NormalizePath(it->second->GetPath());
				m_PathToUUID.erase(normalizedPath);
				RemoveFileWatcher(it->second->GetPath());
			}

			LNX_LOG_INFO("Material unregistered: {0} (ID: {1})", it->second->GetName(), (uint64_t)id);
			m_MaterialCache.erase(it);
		}
	}

	Ref<MaterialAsset> MaterialRegistry::GetMaterial(UUID id) const {
		auto it = m_MaterialCache.find(id);
		if (it != m_MaterialCache.end()) {
			return it->second;
		}
		return nullptr;
	}

	Ref<MaterialAsset> MaterialRegistry::GetMaterialByPath(const std::filesystem::path& path) const {
		std::string normalizedPath = NormalizePath(path);
		auto it = m_PathToUUID.find(normalizedPath);
		if (it != m_PathToUUID.end()) {
			return GetMaterial(it->second);
		}
		return nullptr;
	}

	bool MaterialRegistry::IsMaterialLoaded(UUID id) const {
		return m_MaterialCache.find(id) != m_MaterialCache.end();
	}

	bool MaterialRegistry::IsMaterialLoadedByPath(const std::filesystem::path& path) const {
		std::string normalizedPath = NormalizePath(path);
		return m_PathToUUID.find(normalizedPath) != m_PathToUUID.end();
	}

	// ========== MATERIALES POR DEFECTO ==========

	Ref<MaterialAsset> MaterialRegistry::GetDefaultMaterial() {
		if (!m_DefaultMaterial) {
			m_DefaultMaterial = CreateDefaultMaterial();
		}
		return m_DefaultMaterial;
	}

	Ref<MaterialAsset> MaterialRegistry::CreateNewMaterial(const std::string& name) {
		Ref<MaterialAsset> material = CreateRef<MaterialAsset>(name);
		// No registrar automáticamente - esperar a que se guarde
		return material;
	}

	// ========== HOT-RELOAD ==========

	void MaterialRegistry::ReloadMaterial(UUID id) {
		auto it = m_MaterialCache.find(id);
		if (it == m_MaterialCache.end()) {
			LNX_LOG_WARN("MaterialRegistry::ReloadMaterial - Material not found: {0}", (uint64_t)id);
			return;
		}

		Ref<MaterialAsset> material = it->second;
		std::filesystem::path path = material->GetPath();

		if (path.empty() || !std::filesystem::exists(path)) {
			LNX_LOG_WARN("MaterialRegistry::ReloadMaterial - Cannot reload, invalid path: {0}", path.string());
			return;
		}

		// Cargar desde disco
		Ref<MaterialAsset> reloaded = MaterialAsset::LoadFromFile(path);
		if (!reloaded) {
			LNX_LOG_ERROR("MaterialRegistry::ReloadMaterial - Failed to reload: {0}", path.string());
			return;
		}

		// Reemplazar en cache (mantener el mismo UUID)
		m_MaterialCache[id] = reloaded;
		LNX_LOG_INFO("Material reloaded: {0}", material->GetName());
	}

	void MaterialRegistry::ReloadMaterialByPath(const std::filesystem::path& path) {
		std::string normalizedPath = NormalizePath(path);
		auto it = m_PathToUUID.find(normalizedPath);
		if (it != m_PathToUUID.end()) {
			ReloadMaterial(it->second);
		}
	}

	void MaterialRegistry::ReloadModifiedMaterials() {
		UpdateFileTimestamps();
	}

	void MaterialRegistry::Update() {
		// Actualizar file watchers periódicamente
		static float timeSinceLastCheck = 0.0f;
		timeSinceLastCheck += 0.016f; // Aproximadamente 60 FPS

		if (timeSinceLastCheck >= 1.0f) { // Verificar cada segundo
			ReloadModifiedMaterials();
			timeSinceLastCheck = 0.0f;
		}
	}

	// ========== BÚSQUEDA Y LISTADO ==========

	std::vector<Ref<MaterialAsset>> MaterialRegistry::GetAllMaterials() const {
		std::vector<Ref<MaterialAsset>> materials;
		materials.reserve(m_MaterialCache.size());

		for (const auto& [id, material] : m_MaterialCache) {
			materials.push_back(material);
		}

		return materials;
	}

	std::vector<Ref<MaterialAsset>> MaterialRegistry::SearchMaterialsByName(const std::string& query) const {
		std::vector<Ref<MaterialAsset>> results;

		std::string lowerQuery = query;
		std::transform(lowerQuery.begin(), lowerQuery.end(), lowerQuery.begin(), ::tolower);

		for (const auto& [id, material] : m_MaterialCache) {
			std::string materialName = material->GetName();
			std::transform(materialName.begin(), materialName.end(), materialName.begin(), ::tolower);

			if (materialName.find(lowerQuery) != std::string::npos) {
				results.push_back(material);
			}
		}

		return results;
	}

	// ========== LIMPIEZA ==========

	void MaterialRegistry::ClearUnusedMaterials() {
		std::vector<UUID> toRemove;

		for (const auto& [id, material] : m_MaterialCache) {
			// Verificar si solo hay 1 referencia (la del cache)
			if (material.use_count() == 1 && material != m_DefaultMaterial) {
				toRemove.push_back(id);
			}
		}

		for (UUID id : toRemove) {
			UnregisterMaterial(id);
		}

		if (!toRemove.empty()) {
			LNX_LOG_INFO("Cleared {0} unused materials", toRemove.size());
		}
	}

	void MaterialRegistry::ClearAll() {
		m_MaterialCache.clear();
		m_PathToUUID.clear();
		m_FileWatchers.clear();
		m_DefaultMaterial.reset();
		LNX_LOG_INFO("MaterialRegistry cleared");
	}

	// ========== FILE WATCHING ==========

	void MaterialRegistry::AddFileWatcher(const std::filesystem::path& path, UUID materialID) {
		if (!std::filesystem::exists(path)) {
			return;
		}

		std::string normalizedPath = NormalizePath(path);
		FileWatchData data;
		data.Path = path;
		data.LastModified = std::filesystem::last_write_time(path);
		data.MaterialID = materialID;

		m_FileWatchers[normalizedPath] = data;
	}

	void MaterialRegistry::RemoveFileWatcher(const std::filesystem::path& path) {
		std::string normalizedPath = NormalizePath(path);
		m_FileWatchers.erase(normalizedPath);
	}

	void MaterialRegistry::UpdateFileTimestamps() {
		std::vector<UUID> materialsToReload;

		for (auto& [pathStr, watchData] : m_FileWatchers) {
			if (!std::filesystem::exists(watchData.Path)) {
				continue;
			}

			try {
				auto currentModified = std::filesystem::last_write_time(watchData.Path);
				if (currentModified != watchData.LastModified) {
					watchData.LastModified = currentModified;
					materialsToReload.push_back(watchData.MaterialID);
				}
			}
			catch (const std::exception& e) {
				LNX_LOG_ERROR("Failed to check file modification time: {0}", e.what());
			}
		}

		for (UUID id : materialsToReload) {
			ReloadMaterial(id);
		}
	}

	// ========== HELPERS PRIVADOS ==========

	Ref<MaterialAsset> MaterialRegistry::CreateDefaultMaterial() {
		Ref<MaterialAsset> material = CreateRef<MaterialAsset>("Default Material");
		material->SetAlbedo(glm::vec4(1.0f, 1.0f, 1.0f, 1.0f));
		material->SetMetallic(0.0f);
		material->SetRoughness(0.5f);
		material->SetSpecular(0.5f);

		// Registrar pero sin path (no se guardará)
		RegisterMaterial(material);

		LNX_LOG_INFO("Default material created");
		return material;
	}

	std::string MaterialRegistry::NormalizePath(const std::filesystem::path& path) const {
		try {
			// Convertir a absolute y normalizar separadores
			std::filesystem::path normalized = std::filesystem::absolute(path);
			std::string pathStr = normalized.string();

			// Convertir separadores a forward slashes para consistency
			std::replace(pathStr.begin(), pathStr.end(), '\\', '/');

			// Convertir a lowercase para comparaciones case-insensitive en Windows
			#ifdef _WIN32
			std::transform(pathStr.begin(), pathStr.end(), pathStr.begin(), ::tolower);
			#endif

			return pathStr;
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Failed to normalize path: {0}", e.what());
			return path.string();
		}
	}

}
