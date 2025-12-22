#pragma once

/**
 * @file GPUSkeleton.h
 * @brief GPU resource for skeleton bone matrices
 * 
 * Part of the AAA Animation System
 * 
 * GPUSkeleton manages the GPU-side storage of bone matrices
 * for skeletal animation. It wraps a storage buffer that
 * can be bound to shaders for vertex skinning.
 */

#include "Core/Core.h"
#include "Renderer/StorageBuffer.h"

#include <glm/glm.hpp>
#include <vector>

namespace Lunex {

	/**
	 * @class GPUSkeleton
	 * @brief GPU resource for bone transformation matrices
	 * 
	 * This class manages the GPU buffer containing bone matrices
	 * that are used for skeletal mesh skinning in the vertex shader.
	 * 
	 * Usage:
	 *   1. Create with bone count
	 *   2. Upload bone matrices each frame after animation
	 *   3. Bind to shader before rendering skeletal mesh
	 */
	class GPUSkeleton {
	public:
		static constexpr uint32_t MAX_BONES = 256;
		static constexpr uint32_t DEFAULT_BINDING = 3; // Binding point for bone matrices
		
		GPUSkeleton();
		GPUSkeleton(uint32_t boneCount);
		~GPUSkeleton() = default;
		
		// ========== INITIALIZATION ==========
		
		/**
		 * @brief Initialize GPU buffer for given bone count
		 */
		void Initialize(uint32_t boneCount);
		
		/**
		 * @brief Release GPU resources
		 */
		void Release();
		
		// ========== DATA UPLOAD ==========
		
		/**
		 * @brief Upload all bone matrices to GPU
		 * @param matrices Bone matrices (model-space * inverse bind pose)
		 */
		void Upload(const std::vector<glm::mat4>& matrices);
		
		/**
		 * @brief Upload a single bone matrix
		 * @param boneIndex Index of the bone
		 * @param matrix The bone matrix
		 */
		void Upload(uint32_t boneIndex, const glm::mat4& matrix);
		
		/**
		 * @brief Reset all bones to identity
		 */
		void Reset();
		
		// ========== BINDING ==========
		
		/**
		 * @brief Bind the storage buffer for rendering
		 * @param binding Binding point (default is 3)
		 */
		void Bind(uint32_t binding = DEFAULT_BINDING) const;
		
		/**
		 * @brief Unbind the storage buffer
		 */
		void Unbind() const;
		
		// ========== QUERIES ==========
		
		uint32_t GetBoneCount() const { return m_BoneCount; }
		bool IsValid() const { return m_Buffer != nullptr && m_BoneCount > 0; }
		
		// Access underlying buffer
		Ref<StorageBuffer> GetBuffer() const { return m_Buffer; }
		
		// ========== FACTORY ==========
		
		static Ref<GPUSkeleton> Create(uint32_t boneCount);
		
	private:
		Ref<StorageBuffer> m_Buffer;
		uint32_t m_BoneCount = 0;
		
		// CPU-side cache for partial updates
		std::vector<glm::mat4> m_MatrixCache;
	};

}
