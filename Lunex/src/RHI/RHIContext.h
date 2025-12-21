#pragma once

/**
 * @file RHIContext.h
 * @brief Graphics context management and swapchain handling
 * 
 * The RHIContext manages:
 * - Window surface binding
 * - Swapchain presentation
 * - Context switching (for multi-window rendering)
 */

#include "RHITypes.h"
#include "RHIResource.h"

namespace Lunex {
namespace RHI {

	// Forward declarations
	class RHITexture2D;
	class RHIFramebuffer;

	// ============================================================================
	// SWAPCHAIN CONFIGURATION
	// ============================================================================
	
	struct SwapchainCreateInfo {
		void* WindowHandle = nullptr;
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t BufferCount = 2;           // Double/triple buffering
		TextureFormat Format = TextureFormat::RGBA8;
		bool VSync = true;
		bool Fullscreen = false;
		uint32_t SampleCount = 1;           // For MSAA
	};

	// ============================================================================
	// PRESENT MODE
	// ============================================================================
	
	enum class PresentMode : uint8_t {
		Immediate = 0,    // No vsync, may tear
		VSync,            // Wait for vertical blank
		Mailbox,          // Triple buffering (lowest latency vsync)
		FIFO              // Queue frames (Vulkan FIFO)
	};

	// ============================================================================
	// SWAPCHAIN INTERFACE
	// ============================================================================
	
	/**
	 * @class RHISwapchain
	 * @brief Manages the swapchain for presenting rendered frames to the screen
	 */
	class RHISwapchain : public RHIResource {
	public:
		virtual ~RHISwapchain() = default;
		
		// Resource type
		ResourceType GetResourceType() const override { return ResourceType::Framebuffer; }
		
		// ============================================
		// SWAPCHAIN OPERATIONS
		// ============================================
		
		/**
		 * @brief Acquire the next backbuffer for rendering
		 * @return Index of the acquired buffer (0 to BufferCount-1)
		 */
		virtual uint32_t AcquireNextImage() = 0;
		
		/**
		 * @brief Present the current backbuffer to the screen
		 */
		virtual void Present() = 0;
		
		/**
		 * @brief Resize the swapchain (call when window resizes)
		 * @param width New width
		 * @param height New height
		 */
		virtual void Resize(uint32_t width, uint32_t height) = 0;
		
		// ============================================
		// SWAPCHAIN PROPERTIES
		// ============================================
		
		/**
		 * @brief Get current swapchain width
		 */
		virtual uint32_t GetWidth() const = 0;
		
		/**
		 * @brief Get current swapchain height
		 */
		virtual uint32_t GetHeight() const = 0;
		
		/**
		 * @brief Get the swapchain format
		 */
		virtual TextureFormat GetFormat() const = 0;
		
		/**
		 * @brief Get the number of backbuffers
		 */
		virtual uint32_t GetBufferCount() const = 0;
		
		/**
		 * @brief Get the current backbuffer index
		 */
		virtual uint32_t GetCurrentBufferIndex() const = 0;
		
		/**
		 * @brief Get a backbuffer texture
		 * @param index Buffer index
		 */
		virtual Ref<RHITexture2D> GetBackbuffer(uint32_t index) const = 0;
		
		/**
		 * @brief Get the current backbuffer as a render target
		 */
		virtual Ref<RHIFramebuffer> GetCurrentFramebuffer() const = 0;
		
		// ============================================
		// VSYNC CONTROL
		// ============================================
		
		/**
		 * @brief Set VSync mode
		 */
		virtual void SetVSync(bool enabled) = 0;
		
		/**
		 * @brief Check if VSync is enabled
		 */
		virtual bool IsVSyncEnabled() const = 0;
		
		/**
		 * @brief Set present mode
		 */
		virtual void SetPresentMode(PresentMode mode) = 0;
		
		/**
		 * @brief Get current present mode
		 */
		virtual PresentMode GetPresentMode() const = 0;
	};

	// ============================================================================
	// CONTEXT INTERFACE
	// ============================================================================
	
	/**
	 * @class RHIContext
	 * @brief Graphics context management
	 * 
	 * In OpenGL: Manages the OpenGL context
	 * In Vulkan: Manages VkInstance, VkDevice
	 * In DX12: Manages ID3D12Device
	 */
	class RHIContext {
	public:
		virtual ~RHIContext() = default;
		
		// ============================================
		// CONTEXT LIFECYCLE
		// ============================================
		
		/**
		 * @brief Initialize the graphics context
		 * @return true on success
		 */
		virtual bool Initialize() = 0;
		
		/**
		 * @brief Shutdown the context and release resources
		 */
		virtual void Shutdown() = 0;
		
		/**
		 * @brief Make this context current (OpenGL specific, no-op for others)
		 */
		virtual void MakeCurrent() = 0;
		
		// ============================================
		// SWAPCHAIN MANAGEMENT
		// ============================================
		
		/**
		 * @brief Create a swapchain for a window
		 * @param info Swapchain configuration
		 * @return New swapchain, or nullptr on failure
		 */
		virtual Ref<RHISwapchain> CreateSwapchain(const SwapchainCreateInfo& info) = 0;
		
		// ============================================
		// CONTEXT INFO
		// ============================================
		
		/**
		 * @brief Get the graphics API this context uses
		 */
		virtual GraphicsAPI GetAPI() const = 0;
		
		/**
		 * @brief Get the API version string
		 */
		virtual std::string GetAPIVersion() const = 0;
		
		/**
		 * @brief Check if the context is valid and ready
		 */
		virtual bool IsValid() const = 0;
		
		// ============================================
		// DEBUG FEATURES
		// ============================================
		
		/**
		 * @brief Enable debug output/validation layers
		 */
		virtual void EnableDebugOutput(bool enable) = 0;
		
		/**
		 * @brief Push a debug group (for GPU profilers)
		 */
		virtual void PushDebugGroup(const std::string& name) = 0;
		
		/**
		 * @brief Pop the current debug group
		 */
		virtual void PopDebugGroup() = 0;
		
		/**
		 * @brief Insert a debug marker
		 */
		virtual void InsertDebugMarker(const std::string& name) = 0;
		
		// ============================================
		// STATIC CREATION
		// ============================================
		
		/**
		 * @brief Create a graphics context for the specified API
		 * @param api Graphics API to use
		 * @param windowHandle Native window handle
		 * @return The created context, or nullptr on failure
		 */
		static Scope<RHIContext> Create(GraphicsAPI api, void* windowHandle);
		
		/**
		 * @brief Get the global context instance
		 * @return The current active context
		 */
		static RHIContext* Get() { return s_Instance; }  
		
	protected:
		static RHIContext* s_Instance;
		RHIContext() = default;
		
		// Allow RHI module to access s_Instance
		friend void Shutdown();
		friend bool Initialize(GraphicsAPI api, void* windowHandle);
	};

} // namespace RHI
} // namespace Lunex

// ============================================================================
// SCOPED DEBUG GROUP
// ============================================================================
	
/**
 * @class ScopedDebugGroup
 * @brief RAII helper for GPU debug groups
 * 
 * Usage:
 *   {
 *       ScopedDebugGroup group("Shadow Pass");
 *       // ... rendering code ...
 *   } // Automatically pops the group
 */
class ScopedDebugGroup {
public:
	explicit ScopedDebugGroup(const std::string& name) {
		if (Lunex::RHI::RHIContext::Get()) {
			Lunex::RHI::RHIContext::Get()->PushDebugGroup(name);
		}
	}
	
	~ScopedDebugGroup() {
		if (Lunex::RHI::RHIContext::Get()) {
			Lunex::RHI::RHIContext::Get()->PopDebugGroup();
		}
	}
	
	// Non-copyable
	ScopedDebugGroup(const ScopedDebugGroup&) = delete;
	ScopedDebugGroup& operator=(const ScopedDebugGroup&) = delete;
};

// Macro for easy debug group usage
#define RHI_DEBUG_GROUP(name) ::ScopedDebugGroup _debugGroup##__LINE__(name)
