#pragma once

/**
 * @file MaterialAsset.h
 * @brief Material asset for PBR rendering
 * 
 * AAA Architecture: MaterialAsset lives in Assets/Materials/
 * This is the serializable, CPU-side material definition.
 * 
 * Features:
 * - Standard PBR properties (albedo, metallic, roughness, specular, emission, AO)
 * - Detail normal maps (unlimited)
 * - Layered (channel-packed) textures with configurable channel mapping
 * - Color space annotations (sRGB / Non-Color)
 * - Alpha channel packing
 */

#include "Core/Core.h"
#include "Assets/Core/Asset.h"
#include "Renderer/Texture.h"
#include <glm/glm.hpp>
#include <string>
#include <filesystem>
#include <vector>

// Forward declarations
namespace YAML {
	class Emitter;
	class Node;
}

namespace Lunex {

	// ============================================================================
	// ENUMS & STRUCTS
	// ============================================================================

	/**
	 * @brief Color space for texture interpretation
	 */
	enum class TextureColorSpace : uint8_t {
		sRGB = 0,       // Color textures (albedo, emission)
		NonColor = 1     // Data textures (normal, metallic, roughness, AO)
	};

	/**
	 * @brief Which channel of a texture to read from
	 */
	enum class TextureChannel : uint8_t {
		Red = 0,
		Green = 1,
		Blue = 2,
		Alpha = 3,
		RGB = 4          // Use full RGB (for color textures)
	};

	/**
	 * @brief A single detail normal map entry
	 */
	struct DetailNormalMap {
		Ref<Texture2D> Texture;
		std::string Path;
		float Intensity = 1.0f;
		float TilingX = 1.0f;
		float TilingY = 1.0f;
		float OffsetX = 0.0f;
		float OffsetY = 0.0f;
	};

	/**
	 * @brief Configuration for a layered (channel-packed) texture
	 * Default: R=Metallic, G=Roughness, B=AO
	 */
	struct LayeredTextureConfig {
		Ref<Texture2D> Texture;
		std::string Path;
		bool Enabled = false;

		// Channel assignments
		TextureChannel MetallicChannel = TextureChannel::Red;
		TextureChannel RoughnessChannel = TextureChannel::Green;
		TextureChannel AOChannel = TextureChannel::Blue;

		// Per-channel enable
		bool UseForMetallic = true;
		bool UseForRoughness = true;
		bool UseForAO = true;
	};

	// ============================================================================
	// MATERIAL ASSET
	// ============================================================================

	/**
	 * @class MaterialAsset
	 * @brief Serializable PBR material definition
	 */
	class MaterialAsset : public Asset {
	public:
		MaterialAsset();
		MaterialAsset(const std::string& name);
		~MaterialAsset() = default;

		// ========== ASSET TYPE ==========
		AssetType GetType() const override { return AssetType::Material; }
		static AssetType GetStaticType() { return AssetType::Material; }

		// ========== PBR PROPERTIES ==========
		
		void SetAlbedo(const glm::vec4& color) { m_Albedo = color; MarkDirty(); }
		const glm::vec4& GetAlbedo() const { return m_Albedo; }

		void SetMetallic(float metallic) { m_Metallic = glm::clamp(metallic, 0.0f, 1.0f); MarkDirty(); }
		float GetMetallic() const { return m_Metallic; }

		void SetRoughness(float roughness) { m_Roughness = glm::clamp(roughness, 0.0f, 1.0f); MarkDirty(); }
		float GetRoughness() const { return m_Roughness; }

		void SetSpecular(float specular) { m_Specular = glm::clamp(specular, 0.0f, 1.0f); MarkDirty(); }
		float GetSpecular() const { return m_Specular; }

		void SetEmissionColor(const glm::vec3& color) { m_EmissionColor = color; MarkDirty(); }
		const glm::vec3& GetEmissionColor() const { return m_EmissionColor; }

		void SetEmissionIntensity(float intensity) { m_EmissionIntensity = glm::max(0.0f, intensity); MarkDirty(); }
		float GetEmissionIntensity() const { return m_EmissionIntensity; }

		void SetNormalIntensity(float intensity) { m_NormalIntensity = glm::clamp(intensity, 0.0f, 2.0f); MarkDirty(); }
		float GetNormalIntensity() const { return m_NormalIntensity; }

		// ========== PBR TEXTURES ==========
		
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
		void SetMetallicMultiplier(float multiplier) { m_MetallicMultiplier = glm::clamp(multiplier, 0.0f, 10.0f); MarkDirty(); }
		float GetMetallicMultiplier() const { return m_MetallicMultiplier; }

		// Roughness Map
		void SetRoughnessMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetRoughnessMap() const { return m_RoughnessMap; }
		const std::string& GetRoughnessPath() const { return m_RoughnessPath; }
		bool HasRoughnessMap() const { return m_RoughnessMap != nullptr; }
		void SetRoughnessMultiplier(float multiplier) { m_RoughnessMultiplier = glm::clamp(multiplier, 0.0f, 10.0f); MarkDirty(); }
		float GetRoughnessMultiplier() const { return m_RoughnessMultiplier; }

		// Specular Map
		void SetSpecularMap(Ref<Texture2D> texture);
		Ref<Texture2D> GetSpecularMap() const { return m_SpecularMap; }
		const std::string& GetSpecularPath() const { return m_SpecularPath; }
		bool HasSpecularMap() const { return m_SpecularMap != nullptr; }
		void SetSpecularMultiplier(float multiplier) { m_SpecularMultiplier = glm::clamp(multiplier, 0.0f, 10.0f); MarkDirty(); }
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
		void SetAOMultiplier(float multiplier) { m_AOMultiplier = glm::clamp(multiplier, 0.0f, 10.0f); MarkDirty(); }
		float GetAOMultiplier() const { return m_AOMultiplier; }

		// ========== COLOR SPACE ==========

		void SetAlbedoColorSpace(TextureColorSpace cs) { m_AlbedoColorSpace = cs; MarkDirty(); }
		TextureColorSpace GetAlbedoColorSpace() const { return m_AlbedoColorSpace; }

		void SetEmissionColorSpace(TextureColorSpace cs) { m_EmissionColorSpace = cs; MarkDirty(); }
		TextureColorSpace GetEmissionColorSpace() const { return m_EmissionColorSpace; }

		void SetNormalColorSpace(TextureColorSpace cs) { m_NormalColorSpace = cs; MarkDirty(); }
		TextureColorSpace GetNormalColorSpace() const { return m_NormalColorSpace; }

		void SetMetallicColorSpace(TextureColorSpace cs) { m_MetallicColorSpace = cs; MarkDirty(); }
		TextureColorSpace GetMetallicColorSpace() const { return m_MetallicColorSpace; }

		void SetRoughnessColorSpace(TextureColorSpace cs) { m_RoughnessColorSpace = cs; MarkDirty(); }
		TextureColorSpace GetRoughnessColorSpace() const { return m_RoughnessColorSpace; }

		void SetAOColorSpace(TextureColorSpace cs) { m_AOColorSpace = cs; MarkDirty(); }
		TextureColorSpace GetAOColorSpace() const { return m_AOColorSpace; }

		// ========== ALPHA CHANNEL PACKING ==========

		void SetAlphaIsPacked(bool packed) { m_AlphaIsPacked = packed; MarkDirty(); }
		bool IsAlphaChannelPacked() const { return m_AlphaIsPacked; }

		void SetAlphaPackedChannel(TextureChannel ch) { m_AlphaPackedChannel = ch; MarkDirty(); }
		TextureChannel GetAlphaPackedChannel() const { return m_AlphaPackedChannel; }

		// ========== DETAIL NORMAL MAPS ==========

		void AddDetailNormalMap(const DetailNormalMap& detail);
		void RemoveDetailNormalMap(size_t index);
		void SetDetailNormalMap(size_t index, const DetailNormalMap& detail);
		const std::vector<DetailNormalMap>& GetDetailNormalMaps() const { return m_DetailNormalMaps; }
		std::vector<DetailNormalMap>& GetDetailNormalMaps() { return m_DetailNormalMaps; }
		size_t GetDetailNormalMapCount() const { return m_DetailNormalMaps.size(); }
		bool HasDetailNormalMaps() const { return !m_DetailNormalMaps.empty(); }

		// ========== LAYERED (CHANNEL-PACKED) TEXTURE ==========

		void SetLayeredTextureConfig(const LayeredTextureConfig& config) { m_LayeredConfig = config; MarkDirty(); }
		const LayeredTextureConfig& GetLayeredTextureConfig() const { return m_LayeredConfig; }
		LayeredTextureConfig& GetLayeredTextureConfig() { return m_LayeredConfig; }
		bool HasLayeredTexture() const { return m_LayeredConfig.Enabled && m_LayeredConfig.Texture != nullptr; }

		void SetLayeredTexture(Ref<Texture2D> texture);

		// ========== SERIALIZATION ==========
		
		bool SaveToFile(const std::filesystem::path& path) override;
		static Ref<MaterialAsset> LoadFromFile(const std::filesystem::path& path);

		// ========== UTILITIES ==========

		bool HasAnyTexture() const {
			return HasAlbedoMap() || HasNormalMap() || HasMetallicMap() || 
				   HasRoughnessMap() || HasSpecularMap() || HasEmissionMap() || HasAOMap() ||
				   HasDetailNormalMaps() || HasLayeredTexture();
		}

		Ref<MaterialAsset> Clone() const;

		// ========== GPU DATA ==========
		
		// Max detail normal maps the shader supports
		static constexpr int MAX_DETAIL_NORMALS = 4;

		struct MaterialUniformData {
			glm::vec4 Albedo;
			float Metallic;
			float Roughness;
			float Specular;
			float EmissionIntensity;
			glm::vec3 EmissionColor;
			float NormalIntensity;

			int UseAlbedoMap;
			int UseNormalMap;
			int UseMetallicMap;
			int UseRoughnessMap;
			int UseSpecularMap;
			int UseEmissionMap;
			int UseAOMap;
			float _padding;

			float MetallicMultiplier;
			float RoughnessMultiplier;
			float SpecularMultiplier;
			float AOMultiplier;

			// New: detail normals
			int DetailNormalCount;
			int UseLayeredTexture;
			int LayeredMetallicChannel;   // 0=R, 1=G, 2=B, 3=A
			int LayeredRoughnessChannel;  // 0=R, 1=G, 2=B, 3=A

			int LayeredAOChannel;         // 0=R, 1=G, 2=B, 3=A
			int LayeredUseMetallic;
			int LayeredUseRoughness;
			int LayeredUseAO;

			// Detail normal intensities & tiling (packed)
			glm::vec4 DetailNormalIntensities;  // .x = detail0, .y = detail1, .z = detail2, .w = detail3
			glm::vec4 DetailNormalTilingX;
			glm::vec4 DetailNormalTilingY;
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

		// Color space per texture
		TextureColorSpace m_AlbedoColorSpace = TextureColorSpace::sRGB;
		TextureColorSpace m_EmissionColorSpace = TextureColorSpace::sRGB;
		TextureColorSpace m_NormalColorSpace = TextureColorSpace::NonColor;
		TextureColorSpace m_MetallicColorSpace = TextureColorSpace::NonColor;
		TextureColorSpace m_RoughnessColorSpace = TextureColorSpace::NonColor;
		TextureColorSpace m_AOColorSpace = TextureColorSpace::NonColor;

		// Alpha channel packing
		bool m_AlphaIsPacked = false;
		TextureChannel m_AlphaPackedChannel = TextureChannel::Alpha;

		// Detail normal maps
		std::vector<DetailNormalMap> m_DetailNormalMaps;

		// Layered (channel-packed) texture
		LayeredTextureConfig m_LayeredConfig;

		// Serialization helpers
		void SerializeTexture(YAML::Emitter& out, const std::string& key, const std::string& path) const;
		void DeserializeTexture(const YAML::Node& node, const std::string& key, 
								Ref<Texture2D>& texture, std::string& path);
	};

} // namespace Lunex
