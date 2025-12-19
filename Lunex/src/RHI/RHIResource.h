#pragma once

/**
 * @file RHIResource.h
 * @brief Base class for all GPU resources in the RHI
 * 
 * All RHI resources (buffers, textures, shaders, etc.) derive from this base class.
 * Provides common functionality for resource tracking, debugging, and lifecycle management.
 */

#include "RHITypes.h"
#include <string>
#include <atomic>

namespace Lunex {
namespace RHI {

	// ============================================================================
	// RESOURCE TYPE ENUM
	// ============================================================================
	
	enum class ResourceType : uint8_t {
		Unknown = 0,
		Buffer,
		Texture,
		Sampler,
		Shader,
		Pipeline,
		Framebuffer,
		CommandList,
		Fence,
		Query
	};

	// ============================================================================
	// RHI RESOURCE BASE CLASS
	// ============================================================================
	
	/**
	 * @class RHIResource
	 * @brief Abstract base class for all RHI resources
	 * 
	 * Features:
	 * - Unique resource ID generation
	 * - Debug naming support
	 * - Reference counting integration
	 * - Resource state tracking
	 */
	class RHIResource {
	public:
		RHIResource() : m_ResourceID(GenerateResourceID()) {}
		virtual ~RHIResource() = default;
		
		// Non-copyable, movable
		RHIResource(const RHIResource&) = delete;
		RHIResource& operator=(const RHIResource&) = delete;
		RHIResource(RHIResource&&) = default;
		RHIResource& operator=(RHIResource&&) = default;
		
		// ============================================
		// RESOURCE IDENTIFICATION
		// ============================================
		
		/**
		 * @brief Get the unique resource ID
		 * @return Unique 64-bit identifier for this resource
		 */
		RHIHandle GetResourceID() const { return m_ResourceID; }
		
		/**
		 * @brief Get the resource type
		 * @return Type enum identifying the resource category
		 */
		virtual ResourceType GetResourceType() const = 0;
		
		/**
		 * @brief Get the native API handle (OpenGL ID, VkBuffer, ID3D12Resource*, etc.)
		 * @return Platform-specific handle as uint64_t
		 */
		virtual RHIHandle GetNativeHandle() const = 0;
		
		// ============================================
		// DEBUG NAMING
		// ============================================
		
		/**
		 * @brief Set a debug name for this resource
		 * @param name Human-readable name for debugging
		 * 
		 * This name will be visible in graphics debuggers (RenderDoc, PIX, etc.)
		 */
		virtual void SetDebugName(const std::string& name) {
			m_DebugName = name;
			OnDebugNameChanged();
		}
		
		/**
		 * @brief Get the debug name
		 * @return The current debug name, or empty string if not set
		 */
		const std::string& GetDebugName() const { return m_DebugName; }
		
		// ============================================
		// RESOURCE STATE
		// ============================================
		
		/**
		 * @brief Check if the resource is in a valid state
		 * @return true if the resource is valid and usable
		 */
		virtual bool IsValid() const = 0;
		
		/**
		 * @brief Get the current resource state
		 * @return Current state for barrier tracking
		 */
		ResourceState GetCurrentState() const { return m_CurrentState; }
		
		/**
		 * @brief Transition the resource to a new state
		 * @param newState The target state
		 * 
		 * Note: This does NOT insert barriers - that's done by the command list.
		 * This only tracks the logical state.
		 */
		void SetCurrentState(ResourceState newState) { m_CurrentState = newState; }
		
		// ============================================
		// MEMORY INFO
		// ============================================
		
		/**
		 * @brief Get the GPU memory used by this resource
		 * @return Size in bytes
		 */
		virtual uint64_t GetGPUMemorySize() const { return 0; }
		
	protected:
		/**
		 * @brief Called when the debug name changes
		 * Override in backend to set API-specific debug labels
		 */
		virtual void OnDebugNameChanged() {}
		
	private:
		static RHIHandle GenerateResourceID() {
			static std::atomic<RHIHandle> s_NextID{ 1 };
			return s_NextID.fetch_add(1, std::memory_order_relaxed);
		}
		
	protected:
		RHIHandle m_ResourceID;
		std::string m_DebugName;
		ResourceState m_CurrentState = ResourceState::Undefined;
	};

	// ============================================================================
	// RESOURCE CREATION INFO (common fields)
	// ============================================================================
	
	struct ResourceCreationInfo {
		std::string DebugName;
		
		// Optional: initial state for resources that need it
		ResourceState InitialState = ResourceState::Undefined;
	};

} // namespace RHI
} // namespace Lunex
