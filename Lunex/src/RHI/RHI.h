#pragma once

/**
 * @file RHI.h
 * @brief Main include file for the Render Hardware Interface
 *
 * Include this single header to get access to all RHI types and interfaces.
 */

 // Core types and enums
#include "RHITypes.h"

// Base resource class
#include "RHIResource.h"

// Device and context
#include "RHIDevice.h"
#include "RHIContext.h"

// Resources
#include "RHIBuffer.h"
#include "RHITexture.h"
#include "RHISampler.h"
#include "RHIShader.h"
#include "RHIPipeline.h"
#include "RHIFramebuffer.h"
#include "RHIInputLayout.h"  // NEW: API-agnostic vertex input layout

// Command system
#include "RHICommandList.h"
#include "RHICommandPool.h"  // NEW: Multithreaded command recording

namespace Lunex {
	namespace RHI {

		// ============================================================================
		// RHI INITIALIZATION
		// ============================================================================

		/**
		 * @brief Initialize the RHI system
		 * @param api Graphics API to use
		 * @param windowHandle Native window handle for context creation
		 * @return true on success
		 */
		bool Initialize(GraphicsAPI api, void* windowHandle);

		/**
		 * @brief Shutdown the RHI system and release all resources
		 */
		void Shutdown();

		/**
		 * @brief Check if the RHI is initialized
		 * @return true if RHI is ready
		 */
		bool IsInitialized();

		/**
		 * @brief Get the current graphics API
		 * @return The active graphics API
		 */
		GraphicsAPI GetCurrentAPI();

		// ============================================================================
		// GRAPHICS API SELECTION (decided before the window exists)
		// ============================================================================

		// API for this run: --vulkan / --opengl on the command line, else the saved preference
		GraphicsAPI ResolveStartupAPI(int argc, char** argv);
		// Saved preference (%APPDATA%/Lunex/GraphicsAPI.txt), applied on the next start
		GraphicsAPI GetPreferredAPI();
		void SetPreferredAPI(GraphicsAPI api);
		// API the window / context are being created for
		GraphicsAPI GetRequestedAPI();
		void SetRequestedAPI(GraphicsAPI api);
		const char* GraphicsAPIName(GraphicsAPI api);

		// ============================================================================
		// GLOBAL COMMAND LIST ACCESS
		// ============================================================================

		/**
		 * @brief Get the immediate command list for rendering
		 *
		 * This provides a global command list for immediate-mode rendering.
		 * For new code, prefer creating your own command lists per-thread.
		 *
		 * @return The global command list
		 */
		RHICommandList* GetImmediateCommandList();

		/**
		 * @brief Initialize OpenGL state (blend, depth test, etc.)
		 * Called during engine initialization.
		 */
		void InitializeRenderState();

	} // namespace RHI
} // namespace Lunex