#include "stpch.h"
#include "MaterialImporter.h"
#include "Log/Log.h"

namespace Lunex {

	Ref<MaterialAsset> MaterialImporter::Import(const std::filesystem::path& path) {
		if (!std::filesystem::exists(path)) {
			LNX_LOG_ERROR("MaterialImporter: File not found: {0}", path.string());
			return nullptr;
		}
		
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		
		if (ext == ".lumat") {
			return MaterialAsset::LoadFromFile(path);
		}
		else if (ext == ".mtl") {
			return ImportMTL(path);
		}
		else if (ext == ".gltf" || ext == ".glb") {
			LNX_LOG_WARN("MaterialImporter: GLTF import requires material name parameter");
			return nullptr;
		}
		
		LNX_LOG_ERROR("MaterialImporter: Unsupported format: {0}", ext);
		return nullptr;
	}
	
	Ref<MaterialAsset> MaterialImporter::ImportMTL(const std::filesystem::path& path) {
		// Basic MTL parsing - can be extended
		LNX_LOG_WARN("MaterialImporter: MTL import not yet fully implemented");
		
		Ref<MaterialAsset> material = CreateRef<MaterialAsset>();
		material->SetName(path.stem().string());
		
		// TODO: Parse MTL file format
		// Kd = diffuse color
		// Ks = specular color
		// Ns = specular exponent
		// map_Kd = diffuse texture
		// etc.
		
		return material;
	}
	
	Ref<MaterialAsset> MaterialImporter::ImportGLTF(const std::filesystem::path& path, const std::string& materialName) {
		// GLTF material import
		LNX_LOG_WARN("MaterialImporter: GLTF import not yet fully implemented");
		
		Ref<MaterialAsset> material = CreateRef<MaterialAsset>();
		material->SetName(materialName);
		
		// TODO: Parse GLTF/GLB and extract material data
		// pbrMetallicRoughness.baseColorFactor
		// pbrMetallicRoughness.metallicFactor
		// pbrMetallicRoughness.roughnessFactor
		// etc.
		
		return material;
	}
	
	std::vector<std::string> MaterialImporter::GetSupportedExtensions() {
		return { ".lumat", ".mtl", ".gltf", ".glb" };
	}
	
	bool MaterialImporter::IsSupported(const std::filesystem::path& path) {
		std::string ext = path.extension().string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		
		auto supported = GetSupportedExtensions();
		return std::find(supported.begin(), supported.end(), ext) != supported.end();
	}

} // namespace Lunex
