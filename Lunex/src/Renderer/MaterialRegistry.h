#pragma once

#include "Core/Core.h"
#include "Core/UUID.h"
#include "MaterialAsset.h"
#include "MaterialInstance.h"

#include <unordered_map>
#include <filesystem>
#include <string>

namespace Lunex {

	// MaterialRegistry - Sistema centralizado de gestión de materiales
	// Implementa:
	// - Cache de MaterialAssets cargados
	// - Hot-reload cuando cambian archivos .lumat
	// - Creación y gestión de materiales por defecto
	// - Lookup rápido por UUID o Path
	// Singleton pattern para acceso global
	class MaterialRegistry {
	public:
		static MaterialRegistry& Get() {
			static MaterialRegistry instance;
			return instance;
		}

		MaterialRegistry(const MaterialRegistry&) = delete;
		MaterialRegistry& operator=(const MaterialRegistry&) = delete;

		// ========== GESTIÓN DE ASSETS ==========

		// Cargar material desde archivo (con cache)
		Ref<MaterialAsset> LoadMaterial(const std::filesystem::path& path);

		// Registrar material en cache (cuando se crea desde código)
		void RegisterMaterial(Ref<MaterialAsset> material);

		// Desregistrar material (cuando ya no se usa)
		void UnregisterMaterial(UUID id);

		// Obtener material por UUID
		Ref<MaterialAsset> GetMaterial(UUID id) const;

		// Obtener material por path (busca en cache primero)
		Ref<MaterialAsset> GetMaterialByPath(const std::filesystem::path& path) const;

		// Verificar si un material está cargado
		bool IsMaterialLoaded(UUID id) const;
		bool IsMaterialLoadedByPath(const std::filesystem::path& path) const;

		// ========== MATERIALES POR DEFECTO ==========

		// Obtener o crear material por defecto (blanco, sin texturas)
		Ref<MaterialAsset> GetDefaultMaterial();

		// Crear nuevo material con valores por defecto
		Ref<MaterialAsset> CreateNewMaterial(const std::string& name = "New Material");

		// ========== HOT-RELOAD ==========

		// Recargar un material específico desde disco
		void ReloadMaterial(UUID id);
		void ReloadMaterialByPath(const std::filesystem::path& path);

		// Recargar todos los materiales modificados
		void ReloadModifiedMaterials();

		// Actualizar file watchers (llamar en update loop)
		void Update();

		// ========== BÚSQUEDA Y LISTADO ==========

		// Obtener todos los materiales cargados
		std::vector<Ref<MaterialAsset>> GetAllMaterials() const;

		// Buscar materiales por nombre (partial match)
		std::vector<Ref<MaterialAsset>> SearchMaterialsByName(const std::string& query) const;

		// Obtener estadísticas
		size_t GetLoadedMaterialCount() const { return m_MaterialCache.size(); }

		// ========== LIMPIEZA ==========

		// Limpiar materiales no usados (sin referencias externas)
		void ClearUnusedMaterials();

		// Limpiar toda la cache
		void ClearAll();

	private:
		MaterialRegistry();
		~MaterialRegistry() = default;

		// Cache principal: UUID -> MaterialAsset
		std::unordered_map<UUID, Ref<MaterialAsset>> m_MaterialCache;

		// Lookup table: Path -> UUID (para búsqueda rápida por path)
		std::unordered_map<std::string, UUID> m_PathToUUID;

		// Material por defecto (lazy initialization)
		Ref<MaterialAsset> m_DefaultMaterial;

		// ========== FILE WATCHING ==========
		struct FileWatchData {
			std::filesystem::path Path;
			std::filesystem::file_time_type LastModified;
			UUID MaterialID;
		};

		std::unordered_map<std::string, FileWatchData> m_FileWatchers;

		// Inicializar file watcher para un material
		void AddFileWatcher(const std::filesystem::path& path, UUID materialID);

		// Remover file watcher
		void RemoveFileWatcher(const std::filesystem::path& path);

		// Helper para actualizar timestamps
		void UpdateFileTimestamps();

		// ========== HELPERS PRIVADOS ==========

		// Crear material por defecto
		Ref<MaterialAsset> CreateDefaultMaterial();

		// Normalizar path para comparaciones
		std::string NormalizePath(const std::filesystem::path& path) const;
	};

}
