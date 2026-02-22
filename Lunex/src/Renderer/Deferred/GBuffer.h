#pragma once

/**
 * @file GBuffer.h
 * @brief Geometry Buffer for Deferred Rendering Pipeline
 * 
 * G-Buffer Layout (MRT):
 *   RT0 (RGBA16F): Albedo.rgb   + Metallic    (a)
 *   RT1 (RGBA16F): Normal.xyz   + Roughness   (a) [world-space normals, octahedral-encoded or raw]
 *   RT2 (RGBA16F): Emission.rgb + AO          (a)
 *   RT3 (RGBA16F): Position.xyz + Specular    (a) [world-space position]
 *   RT4 (R32I):    EntityID                        [for editor picking]
 *   Depth:         Depth24Stencil8
 * 
 * All framebuffer operations use the engine's Framebuffer/RHI abstraction.
 * No raw graphics API calls in this file.
 */

#include "Core/Core.h"
#include "Renderer/FrameBuffer.h"

namespace Lunex {

	/**
	 * @enum GBufferAttachment
	 * @brief Named indices for G-Buffer render targets
	 */
	enum class GBufferAttachment : uint32_t {
		AlbedoMetallic  = 0,   // RT0: Albedo.rgb + Metallic
		NormalRoughness = 1,   // RT1: Normal.xyz + Roughness
		EmissionAO      = 2,   // RT2: Emission.rgb + AO
		PositionSpecular = 3,  // RT3: Position.xyz + Specular
		EntityID        = 4,   // RT4: EntityID (R32I)
		Count           = 5
	};

	/**
	 * @class GBuffer
	 * @brief Manages the G-Buffer framebuffer with multiple render targets
	 */
	class GBuffer {
	public:
		GBuffer() = default;
		~GBuffer() = default;

		/**
		 * @brief Initialize the G-Buffer with given dimensions
		 */
		void Initialize(uint32_t width, uint32_t height);

		/**
		 * @brief Resize the G-Buffer
		 */
		void Resize(uint32_t width, uint32_t height);

		/**
		 * @brief Bind G-Buffer for geometry pass (writing)
		 */
		void Bind();

		/**
		 * @brief Unbind G-Buffer
		 */
		void Unbind();

		/**
		 * @brief Clear all G-Buffer attachments
		 */
		void Clear();

		/**
		 * @brief Clear entity ID attachment to -1
		 */
		void ClearEntityID();

		/**
		 * @brief Read entity ID at pixel position (for picking)
		 */
		int ReadEntityID(int x, int y);

		/**
		 * @brief Get the OpenGL texture ID for a specific attachment (for sampling in lighting pass)
		 */
		uint32_t GetAttachmentRendererID(GBufferAttachment attachment) const;

		/**
		 * @brief Get the depth attachment texture ID
		 */
		uint32_t GetDepthAttachmentRendererID() const;

		/**
		 * @brief Get the underlying framebuffer
		 */
		Ref<Framebuffer> GetFramebuffer() const { return m_Framebuffer; }

		/**
		 * @brief Get the native framebuffer handle
		 */
		uint32_t GetRendererID() const;

		uint32_t GetWidth() const { return m_Width; }
		uint32_t GetHeight() const { return m_Height; }
		bool IsInitialized() const { return m_Framebuffer != nullptr; }

	private:
		Ref<Framebuffer> m_Framebuffer;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
	};

} // namespace Lunex
