#include "stpch.h"
#include "GPUSkeleton.h"

namespace Lunex {

	// ============================================================================
	// CONSTRUCTORS
	// ============================================================================
	
	GPUSkeleton::GPUSkeleton()
		: m_BoneCount(0)
	{
	}
	
	GPUSkeleton::GPUSkeleton(uint32_t boneCount)
		: m_BoneCount(0)
	{
		Initialize(boneCount);
	}

	// ============================================================================
	// INITIALIZATION
	// ============================================================================
	
	void GPUSkeleton::Initialize(uint32_t boneCount) {
		if (boneCount == 0 || boneCount > MAX_BONES) {
			LNX_LOG_ERROR("GPUSkeleton: Invalid bone count: {0} (max: {1})", boneCount, MAX_BONES);
			return;
		}
		
		m_BoneCount = boneCount;
		
		// Calculate buffer size (mat4 = 64 bytes)
		uint32_t bufferSize = boneCount * sizeof(glm::mat4);
		
		// Create storage buffer
		m_Buffer = StorageBuffer::Create(bufferSize, DEFAULT_BINDING);
		
		// Initialize cache with identity matrices
		m_MatrixCache.resize(boneCount, glm::mat4(1.0f));
		
		// Upload initial identity matrices
		if (m_Buffer) {
			m_Buffer->SetData(m_MatrixCache.data(), bufferSize);
		}
		
		LNX_LOG_INFO("GPUSkeleton: Initialized with {0} bones ({1} bytes)", boneCount, bufferSize);
	}
	
	void GPUSkeleton::Release() {
		m_Buffer = nullptr;
		m_MatrixCache.clear();
		m_BoneCount = 0;
	}

	// ============================================================================
	// DATA UPLOAD
	// ============================================================================
	
	void GPUSkeleton::Upload(const std::vector<glm::mat4>& matrices) {
		if (!m_Buffer) {
			LNX_LOG_WARN("GPUSkeleton: Cannot upload - buffer not initialized");
			return;
		}
		
		if (matrices.size() != m_BoneCount) {
			LNX_LOG_WARN("GPUSkeleton: Matrix count mismatch (expected {0}, got {1})", 
				m_BoneCount, matrices.size());
		}
		
		// Update cache
		size_t copyCount = std::min(matrices.size(), static_cast<size_t>(m_BoneCount));
		for (size_t i = 0; i < copyCount; i++) {
			m_MatrixCache[i] = matrices[i];
		}
		
		// Upload to GPU
		m_Buffer->SetData(m_MatrixCache.data(), m_BoneCount * sizeof(glm::mat4));
	}
	
	void GPUSkeleton::Upload(uint32_t boneIndex, const glm::mat4& matrix) {
		if (!m_Buffer || boneIndex >= m_BoneCount) {
			return;
		}
		
		m_MatrixCache[boneIndex] = matrix;
		
		// Upload single matrix (partial update)
		m_Buffer->SetData(&matrix, sizeof(glm::mat4), boneIndex * sizeof(glm::mat4));
	}
	
	void GPUSkeleton::Reset() {
		if (!m_Buffer) return;
		
		// Reset cache to identity
		for (auto& mat : m_MatrixCache) {
			mat = glm::mat4(1.0f);
		}
		
		// Upload to GPU
		m_Buffer->SetData(m_MatrixCache.data(), m_BoneCount * sizeof(glm::mat4));
	}

	// ============================================================================
	// BINDING
	// ============================================================================
	
	void GPUSkeleton::Bind(uint32_t binding) const {
		if (m_Buffer) {
			m_Buffer->BindForRead(binding);
		}
	}
	
	void GPUSkeleton::Unbind() const {
		// Storage buffers don't typically need explicit unbinding
		// but we could add that functionality if needed
	}

	// ============================================================================
	// FACTORY
	// ============================================================================
	
	Ref<GPUSkeleton> GPUSkeleton::Create(uint32_t boneCount) {
		return CreateRef<GPUSkeleton>(boneCount);
	}

}
