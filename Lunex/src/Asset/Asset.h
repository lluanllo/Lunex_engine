#pragma once

#include "Core/Core.h"
#include "Core/UUID.h"

#include <string>
#include <filesystem>

// Forward declarations
namespace YAML {
	class Emitter;
	class Node;
}

namespace Lunex {

	// ============================================================================
	// ASSET TYPE ENUMERATION
	// Defines all supported asset types in the engine
	// ============================================================================
	enum class AssetType : uint16_t {
		None = 0,
		Scene,
		Material,
		Mesh,
		Texture,
		Shader,
		Audio,
		Script,
		Prefab,
		Animation,
		Font
	};

	// Convert AssetType to string for serialization/debugging
	inline const char* AssetTypeToString(AssetType type) {
		switch (type) {
			case AssetType::None:      return "None";
			case AssetType::Scene:     return "Scene";
			case AssetType::Material:  return "Material";
			case AssetType::Mesh:      return "Mesh";
			case AssetType::Texture:   return "Texture";
			case AssetType::Shader:    return "Shader";
			case AssetType::Audio:     return "Audio";
			case AssetType::Script:    return "Script";
			case AssetType::Prefab:    return "Prefab";
			case AssetType::Animation: return "Animation";
			case AssetType::Font:      return "Font";
		}
		return "Unknown";
	}

	// Convert string to AssetType for deserialization
	inline AssetType StringToAssetType(const std::string& str) {
		if (str == "Scene")     return AssetType::Scene;
		if (str == "Material")  return AssetType::Material;
		if (str == "Mesh")      return AssetType::Mesh;
		if (str == "Texture")   return AssetType::Texture;
		if (str == "Shader")    return AssetType::Shader;
		if (str == "Audio")     return AssetType::Audio;
		if (str == "Script")    return AssetType::Script;
		if (str == "Prefab")    return AssetType::Prefab;
		if (str == "Animation") return AssetType::Animation;
		if (str == "Font")      return AssetType::Font;
		return AssetType::None;
	}

	// Get file extension for asset type
	inline const char* AssetTypeToExtension(AssetType type) {
		switch (type) {
			case AssetType::Scene:     return ".lunex";
			case AssetType::Material:  return ".lumat";
			case AssetType::Mesh:      return ".lumesh";
			case AssetType::Texture:   return ".lutex";   // Future: processed textures
			case AssetType::Shader:    return ".glsl";
			case AssetType::Audio:     return ".luaudio"; // Future: audio clips
			case AssetType::Script:    return ".cpp";
			case AssetType::Prefab:    return ".luprefab";
			case AssetType::Animation: return ".luanim";
			case AssetType::Font:      return ".lufont";
			default:                   return "";
		}
	}

	// Get asset type from file extension
	inline AssetType ExtensionToAssetType(const std::string& ext) {
		if (ext == ".lunex")    return AssetType::Scene;
		if (ext == ".lumat")    return AssetType::Material;
		if (ext == ".lumesh")   return AssetType::Mesh;
		if (ext == ".lutex")    return AssetType::Texture;
		if (ext == ".glsl")     return AssetType::Shader;
		if (ext == ".luaudio")  return AssetType::Audio;
		if (ext == ".cpp")      return AssetType::Script;
		if (ext == ".luprefab") return AssetType::Prefab;
		if (ext == ".luanim")   return AssetType::Animation;
		if (ext == ".lufont")   return AssetType::Font;
		return AssetType::None;
	}

	// ============================================================================
	// ASSET FLAGS
	// Bit flags for asset state
	// ============================================================================
	enum class AssetFlags : uint8_t {
		None        = 0,
		Dirty       = 1 << 0,  // Has unsaved changes
		Loaded      = 1 << 1,  // GPU/Runtime data loaded
		ReadOnly    = 1 << 2,  // Cannot be modified
		Embedded    = 1 << 3,  // Embedded in scene (not external file)
		Procedural  = 1 << 4   // Generated at runtime
	};

	inline AssetFlags operator|(AssetFlags a, AssetFlags b) {
		return static_cast<AssetFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
	}

	inline AssetFlags operator&(AssetFlags a, AssetFlags b) {
		return static_cast<AssetFlags>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
	}

	inline AssetFlags& operator|=(AssetFlags& a, AssetFlags b) {
		return a = a | b;
	}

	inline AssetFlags& operator&=(AssetFlags& a, AssetFlags b) {
		return a = a & b;
	}

	inline bool HasFlag(AssetFlags flags, AssetFlags flag) {
		return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(flag)) != 0;
	}

	// ============================================================================
	// ASSET METADATA
	// Lightweight structure for asset registry entries
	// ============================================================================
	struct AssetMetadata {
		UUID ID;
		AssetType Type = AssetType::None;
		std::filesystem::path FilePath;
		std::string Name;
		bool IsLoaded = false;
		
		// Source file info (for imported assets like meshes)
		std::filesystem::path SourcePath;
		std::filesystem::file_time_type LastModified;
	};

	// ============================================================================
	// ASSET BASE CLASS
	// Abstract base class for all assets in the engine
	// Provides common functionality for identification, serialization, and state
	// ============================================================================
	class Asset {
	public:
		Asset();
		Asset(const std::string& name);
		virtual ~Asset() = default;

		// ========== IDENTIFICATION ==========
		
		UUID GetID() const { return m_ID; }
		void SetID(UUID id) { m_ID = id; }
		
		const std::string& GetName() const { return m_Name; }
		void SetName(const std::string& name) { m_Name = name; MarkDirty(); }

		const std::filesystem::path& GetPath() const { return m_FilePath; }
		void SetPath(const std::filesystem::path& path) { m_FilePath = path; }

		// ========== TYPE INFORMATION ==========
		
		virtual AssetType GetType() const = 0;
		virtual const char* GetTypeName() const { return AssetTypeToString(GetType()); }
		virtual const char* GetExtension() const { return AssetTypeToExtension(GetType()); }

		// ========== STATE FLAGS ==========
		
		bool IsDirty() const { return HasFlag(m_Flags, AssetFlags::Dirty); }
		void MarkDirty() { m_Flags |= AssetFlags::Dirty; }
		void ClearDirty() { m_Flags = static_cast<AssetFlags>(static_cast<uint8_t>(m_Flags) & ~static_cast<uint8_t>(AssetFlags::Dirty)); }

		bool IsLoaded() const { return HasFlag(m_Flags, AssetFlags::Loaded); }
		void SetLoaded(bool loaded) {
			if (loaded) m_Flags |= AssetFlags::Loaded;
			else m_Flags = static_cast<AssetFlags>(static_cast<uint8_t>(m_Flags) & ~static_cast<uint8_t>(AssetFlags::Loaded));
		}

		bool IsReadOnly() const { return HasFlag(m_Flags, AssetFlags::ReadOnly); }
		void SetReadOnly(bool readOnly) {
			if (readOnly) m_Flags |= AssetFlags::ReadOnly;
			else m_Flags = static_cast<AssetFlags>(static_cast<uint8_t>(m_Flags) & ~static_cast<uint8_t>(AssetFlags::ReadOnly));
		}

		AssetFlags GetFlags() const { return m_Flags; }

		// ========== SERIALIZATION ==========
		
		// Save asset to file (uses m_FilePath if path not specified)
		virtual bool Save();
		virtual bool SaveToFile(const std::filesystem::path& path) = 0;

		// Get asset metadata for registry
		AssetMetadata GetMetadata() const;

		// ========== SOURCE FILE (for imported assets) ==========
		
		const std::filesystem::path& GetSourcePath() const { return m_SourcePath; }
		void SetSourcePath(const std::filesystem::path& path) { m_SourcePath = path; }
		bool HasSourceFile() const { return !m_SourcePath.empty(); }

	protected:
		// Generate new UUID (called in constructor)
		void GenerateID();

	protected:
		UUID m_ID;
		std::string m_Name = "New Asset";
		std::filesystem::path m_FilePath;
		std::filesystem::path m_SourcePath;  // Original source file (for imported assets)
		AssetFlags m_Flags = AssetFlags::None;
	};

}
