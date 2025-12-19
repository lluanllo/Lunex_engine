#pragma once

/**
 * @file RHISampler.h
 * @brief Texture sampler state object interface
 * 
 * Samplers define how textures are sampled in shaders:
 * - Filtering modes
 * - Wrap modes
 * - Anisotropy
 * - Comparison functions (for shadow maps)
 */

#include "RHIResource.h"
#include "RHITypes.h"

namespace Lunex {
namespace RHI {

	// ============================================================================
	// RHI SAMPLER
	// ============================================================================
	
	/**
	 * @class RHISampler
	 * @brief Texture sampler state object
	 * 
	 * Samplers are immutable after creation. Create multiple samplers
	 * for different sampling configurations.
	 */
	class RHISampler : public RHIResource {
	public:
		virtual ~RHISampler() = default;
		
		ResourceType GetResourceType() const override { return ResourceType::Sampler; }
		
		// ============================================
		// SAMPLER PROPERTIES
		// ============================================
		
		/**
		 * @brief Get the sampler state
		 */
		const SamplerState& GetState() const { return m_State; }
		
		/**
		 * @brief Get min filter
		 */
		FilterMode GetMinFilter() const { return m_State.MinFilter; }
		
		/**
		 * @brief Get mag filter
		 */
		FilterMode GetMagFilter() const { return m_State.MagFilter; }
		
		/**
		 * @brief Get wrap mode U
		 */
		WrapMode GetWrapU() const { return m_State.WrapU; }
		
		/**
		 * @brief Get wrap mode V
		 */
		WrapMode GetWrapV() const { return m_State.WrapV; }
		
		/**
		 * @brief Get wrap mode W
		 */
		WrapMode GetWrapW() const { return m_State.WrapW; }
		
		/**
		 * @brief Get max anisotropy
		 */
		float GetMaxAnisotropy() const { return m_State.MaxAnisotropy; }
		
		/**
		 * @brief Check if comparison sampling is enabled
		 */
		bool IsComparisonSampler() const { return m_State.ComparisonFunc != CompareFunc::Never; }
		
		// ============================================
		// BINDING
		// ============================================
		
		/**
		 * @brief Bind sampler to a texture unit
		 * @param slot Sampler slot
		 */
		virtual void Bind(uint32_t slot) const = 0;
		
		/**
		 * @brief Unbind sampler from slot
		 */
		virtual void Unbind(uint32_t slot) const = 0;
		
		// ============================================
		// GPU MEMORY
		// ============================================
		
		uint64_t GetGPUMemorySize() const override { return 0; }  // Samplers use negligible memory
		
		// ============================================
		// FACTORY
		// ============================================
		
		/**
		 * @brief Create a sampler with the given state
		 */
		static Ref<RHISampler> Create(const SamplerState& state);
		
		/**
		 * @brief Create a linear sampler
		 */
		static Ref<RHISampler> CreateLinear();
		
		/**
		 * @brief Create a point/nearest sampler
		 */
		static Ref<RHISampler> CreatePoint();
		
		/**
		 * @brief Create an anisotropic sampler
		 * @param anisotropy Max anisotropy level
		 */
		static Ref<RHISampler> CreateAnisotropic(float anisotropy = 16.0f);
		
		/**
		 * @brief Create a shadow map sampler
		 */
		static Ref<RHISampler> CreateShadow();
		
		/**
		 * @brief Create a clamp-to-edge sampler
		 */
		static Ref<RHISampler> CreateClamp();
		
	protected:
		SamplerState m_State;
	};

	// ============================================================================
	// COMMON SAMPLER CACHE
	// ============================================================================
	
	/**
	 * @class SamplerCache
	 * @brief Caches commonly used samplers to avoid redundant creation
	 */
	class SamplerCache {
	public:
		static SamplerCache& Get();
		
		/**
		 * @brief Get or create a sampler with the given state
		 * @param state Sampler configuration
		 * @return Cached or newly created sampler
		 */
		Ref<RHISampler> GetSampler(const SamplerState& state);
		
		/**
		 * @brief Get the default linear sampler
		 */
		Ref<RHISampler> GetLinear();
		
		/**
		 * @brief Get the default point sampler
		 */
		Ref<RHISampler> GetPoint();
		
		/**
		 * @brief Get the default anisotropic sampler
		 */
		Ref<RHISampler> GetAnisotropic();
		
		/**
		 * @brief Get the shadow map sampler
		 */
		Ref<RHISampler> GetShadow();
		
		/**
		 * @brief Clear all cached samplers
		 */
		void Clear();
		
	private:
		SamplerCache() = default;
		
		std::unordered_map<size_t, Ref<RHISampler>> m_Cache;
		Ref<RHISampler> m_LinearSampler;
		Ref<RHISampler> m_PointSampler;
		Ref<RHISampler> m_AnisotropicSampler;
		Ref<RHISampler> m_ShadowSampler;
		
		size_t HashSamplerState(const SamplerState& state) const;
	};

} // namespace RHI
} // namespace Lunex
