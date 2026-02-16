#pragma once

/**
 * @file MaterialAsset.h
 * @brief Material asset for PBR rendering
 * 
 * AAA Architecture: MaterialAsset lives in Assets/Materials/
 * This is the serializable, CPU-side material definition.
 */

#include "Core/Core.h"
#include "Assets/Core/Asset.h"
#include "Renderer/Texture.h"
#include <glm/glm.hpp>
#include <string>
#include <filesystem>

// Forward declarations
namespace YAML {
	class Emitter;
	class Node;
}

namespace Lunex {

	// ============================================================================
	// ALPHA MODE
	// ============================================================================
	enum class AlphaMode : int {
		Opaque = 0,
		Cutoff = 1,
		Transparent = 2
	};

	inline const char* AlphaModeToString(AlphaMode mode) {
		switch (mode) {
			case AlphaMode::Opaque:      return "Opaque";
			case AlphaMode::Cutoff:      return "Cutoff";
			case AlphaMode::Transparent: return "Transparent";
		}
		return "Unknown";
	}

	inline AlphaMode StringToAlphaMode(const std::string& str) {
		if (str == "Cutoff")      return AlphaMode::Cutoff;
		if (str == "Transparent") return AlphaMode::Transparent;
		return AlphaMode::Opaque;
	}

	/**
	 * @class MaterialAsset
	 * @brief Serializable PBR material definition
	 * 
	 * This is an Asset that can be saved to .umat files.
	 * It defines material properties and texture references.
	 */
	class MaterialAsset : public Asset {
	public:
		MaterialAsset();
		MaterialAsset(const std::string& name);
		~MaterialAsset() = default;

		// ========== ASSET TYPE =========
		AssetType GetType() const override { return AssetType::Material; }
		static AssetType GetStaticType() { return AssetType::Material; }

		// ========== PBR PROPERTIES =========
		
		// Albedo (Base Color)
		void SetAlbedo(const glm::vec4& color) { m_Albedo = color; MarkDirty(); }
		const glm::vec4& GetAlbedo() const { return m_Albedo; }

		// Metallic (0 = dielectric, 1 = metal)
		void SetMetallic(float metallic) { m_Metallic = glm::clamp(metallic, 0.0f, 1.0f); MarkDirty(); }
		float GetMetallic() const { return m_Metallic; }

		// Roughness (0 = smooth, 1 = rough)
		void SetRoughness(float roughness) { m_Roughness = glm::clamp(roughness, 0.0f, 1.0f); MarkDirty(); }
		float GetRoughness() const { return m_Roughness; }

		// Specular
		void SetSpecular(float specular) { m_Specular = glm::clamp(specular, 0.0f, 1.0f); MarkDirty(); }
		float GetSpecular() const { return m_Specular; }

		// Emission
		void SetEmissionColor(const glm::vec3& color) { m_EmissionColor = color; MarkDirty(); }
		const glm::vec3& GetEmissionColor() const { return m_EmissionColor; }

		void SetEmissionIntensity(float intensity) { m_EmissionIntensity = glm::max(0.0f, intensity); MarkDirty(); }
		float GetEmissionIntensity() const { return m_EmissionIntensity; }

		// Normal intensity
		void SetNormalIntensity(float intensity) { m_NormalIntensity = glm::clamp(intensity, 0.0f, 2.0f); MarkDirty(); }
		float GetNormalIntensity() const { return m_NormalIntensity; }

		// ========== SURFACE SETTINGS ==========

		// Alpha mode
		void SetAlphaMode(AlphaMode mode) { m_AlphaMode = mode; MarkDirty(); }
		AlphaMode GetAlphaMode() const { return m_AlphaMode; }

		// Alpha cutoff (for AlphaMode::Cutoff)
		void SetAlphaCutoff(float cutoff) { m_AlphaCutoff = glm::clamp(cutoff, 0.0f, 1.0f); MarkDirty(); }
		float GetAlphaCutoff() const { return m_AlphaCutoff; }

		// Two-sided rendering
		void SetTwoSided(bool twoSided) { m_TwoSided = twoSided; MarkDirty(); }
		bool IsTwoSided() const { return m_TwoSided; }

		// UV Tiling
		void SetUVTiling(const glm::vec2& tiling) { m_UVTiling = tiling; MarkDirty(); }
		const glm::vec2& GetUVTiling() const { return m_UVTiling; }

		// UV Offset
		void SetUVOffset(const glm::vec2& offset) { m_UVOffset = offset; MarkDirty(); }
		const glm::vec2& GetUVOffset() const { return m_UVOffset; }

		// ========== PBR TEXTURES =========
		
		// Albedo Map
		void SetAlbedoMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetAlbedoMap() const { return m_AlbedoMap; }
		const std::string& GetAlbedoPath() const { return m_AlbedoPath; }
		bool HasAlbedoMap() const { return m_AlbedoMap != nullptr; }

		// Normal Map
		void SetNormalMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetNormalMap() const { return m_NormalMap; }
		const std::string& GetNormalPath() const { return m_NormalPath; }
		bool HasNormalMap() const { return m_NormalMap != nullptr; }

		// Metallic Map
		void SetMetallicMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetMetallicMap() const { return m_MetallicMap; }
		const std::string& GetMetallicPath() const { return m_MetallicPath; }
		bool HasMetallicMap() const { return m_MetallicMap != nullptr; }
		void SetMetallicMultiplier(float multiplier) { m_MetallicMultiplier = glm::clamp(multiplier, 0.0f, 2.0f); MarkDirty(); }
		float GetMetallicMultiplier() const { return m_MetallicMultiplier; }

		// Roughness Map
		void SetRoughnessMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetRoughnessMap() const { return m_RoughnessMap; }
		const std::string& GetRoughnessPath() const { return m_RoughnessPath; }
		bool HasRoughnessMap() const { return m_RoughnessMap != nullptr; }
		void SetRoughnessMultiplier(float multiplier) { m_RoughnessMultiplier = glm::clamp(multiplier, 0.0f, 2.0f); MarkDirty(); }
		float GetRoughnessMultiplier() const { return m_RoughnessMultiplier; }

		// Specular Map
		void SetSpecularMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetSpecularMap() const { return m_SpecularMap; }
		const std::string& GetSpecularPath() const { return m_SpecularPath; }
		bool HasSpecularMap() const { return m_SpecularMap != nullptr; }
		void SetSpecularMultiplier(float multiplier) { m_SpecularMultiplier = glm::clamp(multiplier, 0.0f, 2.0f); MarkDirty(); }
		float GetSpecularMultiplier() const { return m_SpecularMultiplier; }

		// Emission Map
		void SetEmissionMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetEmissionMap() const { return m_EmissionMap; }
		const std::string& GetEmissionPath() const { return m_EmissionPath; }
		bool HasEmissionMap() const { return m_EmissionMap != nullptr; }

		// Ambient Occlusion Map
		void SetAOMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetAOMap() const { return m_AOMap; }
		const std::string& GetAOPath() const { return m_AOPath; }
		bool HasAOMap() const { return m_AOMap != nullptr; }
		void SetAOMultiplier(float multiplier) { m_AOMultiplier = glm::clamp(multiplier, 0.0f, 2.0f); MarkDirty(); }
		float GetAOMultiplier() const { return m_AOMultiplier; }

		// ========== LAYERED (ORM) TEXTURE ==========
		// Packed texture: R=Metallic, G=Roughness, B=AO (game-optimized format)

		void SetLayeredMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetLayeredMap() const { return m_LayeredMap; }
		const std::string& GetLayeredPath() const { return m_LayeredPath; }
		bool HasLayeredMap() const { return m_LayeredMap != nullptr; }
		void SetUseLayeredMap(bool use) { m_UseLayeredMap = use; MarkDirty(); }
		bool GetUseLayeredMap() const { return m_UseLayeredMap; }

		// When layered map is active, these control which channels to use
		// Default: R=Metallic, G=Roughness, B=AO (ORM standard)
		void SetLayeredChannelMetallic(int channel) { m_LayeredChannelMetallic = glm::clamp(channel, 0, 2); MarkDirty(); }
		int GetLayeredChannelMetallic() const { return m_LayeredChannelMetallic; }
		void SetLayeredChannelRoughness(int channel) { m_LayeredChannelRoughness = glm::clamp(channel, 0, 2); MarkDirty(); }
		int GetLayeredChannelRoughness() const { return m_LayeredChannelRoughness; }
		void SetLayeredChannelAO(int channel) { m_LayeredChannelAO = glm::clamp(channel, 0, 2); MarkDirty(); }
		int GetLayeredChannelAO() const { return m_LayeredChannelAO; }

		// ========== HEIGHT / DISPLACEMENT MAP ==========

		void SetHeightMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetHeightMap() const { return m_HeightMap; }
		const std::string& GetHeightPath() const { return m_HeightPath; }
		bool HasHeightMap() const { return m_HeightMap != nullptr; }

		void SetHeightScale(float scale) { m_HeightScale = glm::clamp(scale, 0.0f, 0.5f); MarkDirty(); }
		float GetHeightScale() const { return m_HeightScale; }

		// ========== DETAIL NORMAL MAP ==========

		void SetDetailNormalMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetDetailNormalMap() const { return m_DetailNormalMap; }
		const std::string& GetDetailNormalPath() const { return m_DetailNormalPath; }
		bool HasDetailNormalMap() const { return m_DetailNormalMap != nullptr; }

		void SetDetailNormalScale(float scale) { m_DetailNormalScale = glm::clamp(scale, 0.0f, 2.0f); MarkDirty(); }
		float GetDetailNormalScale() const { return m_DetailNormalScale; }

		void SetDetailUVTiling(const glm::vec2& tiling) { m_DetailUVTiling = tiling; MarkDirty(); }
		const glm::vec2& GetDetailUVTiling() const { return m_DetailUVTiling; }

		// ========== SERIALIZATION ==========
		
		bool SaveToFile(const std::filesystem::path& path) override;
		static Ref<MaterialAsset> LoadFromFile(const std::filesystem::path& path);

		// ========== UTILITIES ==========

		bool HasAnyTexture() const {
			return HasAlbedoMap() || HasNormalMap() || HasMetallicMap() || 
				   HasRoughnessMap() || HasSpecularMap() || HasEmissionMap() || HasAOMap() ||
				   HasLayeredMap() || HasHeightMap() || HasDetailNormalMap();
		}

		int GetTextureCount() const {
			int count = 0;
			if (HasAlbedoMap()) count++;
			if (HasNormalMap()) count++;
			if (HasMetallicMap()) count++;
			if (HasRoughnessMap()) count++;
			if (HasSpecularMap()) count++;
			if (HasEmissionMap()) count++;
			if (HasAOMap()) count++;
			if (HasLayeredMap()) count++;
			if (HasHeightMap()) count++;
			if (HasDetailNormalMap()) count++;
			return count;
		}

		Ref<MaterialAsset> Clone() const;

		// ========== GPU DATA ==========
		
		struct MaterialUniformData {
			glm::vec4 Albedo;                // 16
			float Metallic;                   // 4
			float Roughness;                  // 4
			float Specular;                   // 4
			float EmissionIntensity;          // 4  = 32
			glm::vec3 EmissionColor;          // 12
			float NormalIntensity;            // 4  = 48

			int UseAlbedoMap;                 // 4
			int UseNormalMap;                 // 4
			int UseMetallicMap;               // 4
			int UseRoughnessMap;              // 4  = 64
			int UseSpecularMap;               // 4
			int UseEmissionMap;               // 4
			int UseAOMap;                     // 4
			int UseLayeredMap;                // 4  = 80

			float MetallicMultiplier;         // 4
			float RoughnessMultiplier;        // 4
			float SpecularMultiplier;         // 4
			float AOMultiplier;               // 4  = 96

			glm::vec2 UVTiling;               // 8
			glm::vec2 UVOffset;               // 8  = 112

			int LayeredChannelMetallic;       // 4
			int LayeredChannelRoughness;      // 4
			int LayeredChannelAO;             // 4
			int UseHeightMap;                 // 4  = 128

			float HeightScale;                // 4
			int UseDetailNormalMap;            // 4
			float DetailNormalScale;           // 4
			float AlphaCutoff;                // 4  = 144

			glm::vec2 DetailUVTiling;         // 8
			int AlphaMode;                    // 4
			float _padding3;                  // 4  = 160
		};

		MaterialUniformData GetUniformData() const;

	private:
		// PBR Properties
		glm::vec4 m_Albedo = { 1.0f, 1.0f, 1.0f, 1.0f };
		float m_Metallic = 0.0f;
		float m_Roughness = 0.5f;
		float m_Specular = 0.5f;
		glm::vec3 m_EmissionColor = { 0.0f, 0.0f, 0.0f };
		float m_EmissionIntensity = 0.0f;
		float m_NormalIntensity = 1.0f;

		// Surface settings
		AlphaMode m_AlphaMode = AlphaMode::Opaque;
		float m_AlphaCutoff = 0.5f;
		bool m_TwoSided = false;
		glm::vec2 m_UVTiling = { 1.0f, 1.0f };
		glm::vec2 m_UVOffset = { 0.0f, 0.0f };

		// Textures
		Ref<Texture2D> m_AlbedoMap;
		std::string m_AlbedoPath;

		Ref<Texture2D> m_NormalMap;
		std::string m_NormalPath;

		Ref<Texture2D> m_MetallicMap;
		std::string m_MetallicPath;
		float m_MetallicMultiplier = 1.0f;

		Ref<Texture2D> m_RoughnessMap;
		std::string m_RoughnessPath;
		float m_RoughnessMultiplier = 1.0f;

		Ref<Texture2D> m_SpecularMap;
		std::string m_SpecularPath;
		float m_SpecularMultiplier = 1.0f;

		Ref<Texture2D> m_EmissionMap;
		std::string m_EmissionPath;

		Ref<Texture2D> m_AOMap;
		std::string m_AOPath;
		float m_AOMultiplier = 1.0f;

		// Layered (ORM) texture
		Ref<Texture2D> m_LayeredMap;
		std::string m_LayeredPath;
		bool m_UseLayeredMap = false;
		int m_LayeredChannelMetallic = 0;  // R
		int m_LayeredChannelRoughness = 1; // G
		int m_LayeredChannelAO = 2;        // B

		// Height / Displacement
		Ref<Texture2D> m_HeightMap;
		std::string m_HeightPath;
		float m_HeightScale = 0.05f;

		// Detail Normal
		Ref<Texture2D> m_DetailNormalMap;
		std::string m_DetailNormalPath;
		float m_DetailNormalScale = 1.0f;
		glm::vec2 m_DetailUVTiling = { 4.0f, 4.0f };

		// Serialization helpers
		void SerializeTexture(YAML::Emitter& out, const std::string& key, const std::string& path) const;
		void DeserializeTexture(const YAML::Node& node, const std::string& key, 
								Ref<Texture2D>& texture, std::string& path);
	};

} // namespace Lunex
