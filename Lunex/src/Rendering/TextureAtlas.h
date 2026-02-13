#pragma once

/**
 * @file TextureAtlas.h
 * @brief Bindless texture table for ray-tracing backend
 *
 * Collects all unique scene textures, makes them GPU-resident via
 * GL_ARB_bindless_texture, and uploads their 64-bit handles into an SSBO
 * so the path-tracer compute shader can sample any texture by index.
 *
 * Fallback: if bindless textures are unavailable the atlas is empty and
 * materials fall back to scalar PBR values only.
 */

#include "Core/Core.h"
#include "Renderer/Texture.h"
#include "Renderer/StorageBuffer.h"

#include <vector>
#include <unordered_map>
#include <cstdint>

namespace Lunex {

	/// Invalid texture index — means "no texture, use scalar fallback"
	static constexpr int32_t RT_TEXTURE_NONE = -1;

	class TextureAtlas {
	public:
		TextureAtlas()  = default;
		~TextureAtlas() = default;

		void Initialize();
		void Shutdown();

		/**
		 * @brief Register a texture and return its index into the handle SSBO.
		 * @return Index (? 0) or RT_TEXTURE_NONE if the texture is null/invalid.
		 */
		int32_t GetOrAddTexture(const Ref<Texture2D>& texture);

		/** Upload all collected handles to the GPU SSBO. */
		void UploadToGPU();

		/** Bind the handle SSBO at the given binding point. */
		void Bind(uint32_t binding) const;

		/** Clear all entries (scene rebuild). */
		void Clear();

		uint32_t GetTextureCount() const { return static_cast<uint32_t>(m_Handles.size()); }

		/** @return true if bindless textures are supported on this GPU. */
		static bool IsBindlessSupported();

	private:
		/// 64-bit bindless GPU handles, indexed by the atlas slot
		std::vector<uint64_t>                         m_Handles;
		/// Map from GL renderer-ID ? atlas index (dedup)
		std::unordered_map<uint32_t, int32_t>         m_LookupMap;
		/// Keep textures alive so their GL objects stay valid
		std::vector<Ref<Texture2D>>                   m_TextureRefs;

		Ref<StorageBuffer>                            m_SSBO;
		bool                                          m_Dirty = true;
	};

} // namespace Lunex
