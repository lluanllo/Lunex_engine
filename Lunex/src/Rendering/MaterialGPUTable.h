#pragma once

/**
 * @file MaterialGPUTable.h
 * @brief Flat GPU table of materials for ray-tracing backend
 *
 * Converts MaterialInstance properties into a flat SSBO-friendly
 * array so the compute shader can index materials by ID.
 * For now: PBR scalars only (no textures). Bindless textures later.
 */

#include "Core/Core.h"
#include "Renderer/StorageBuffer.h"
#include "Resources/Render/MaterialInstance.h"

#include <glm/glm.hpp>
#include <vector>
#include <unordered_map>

namespace Lunex {

	// GPU-ready, 64 bytes, std430-aligned
	struct RTMaterialGPU {
		glm::vec4 Albedo;              // rgba
		glm::vec4 EmissionAndMeta;     // rgb=emission, a=metallic
		glm::vec4 RoughSpecAO;         // x=roughness, y=specular, z=ao, w=emissionIntensity
		glm::vec4 _reserved;           // future: texture IDs for bindless
	};

	class MaterialGPUTable {
	public:
		MaterialGPUTable() = default;
		~MaterialGPUTable() = default;

		void Initialize();
		void Shutdown();

		/**
		 * @brief  Register a material and return its GPU index.
		 * @return Index into the SSBO array.
		 */
		uint32_t GetOrAddMaterial(const Ref<MaterialInstance>& instance);

		/** Upload any dirty data to the GPU SSBO. */
		void UploadToGPU();

		/** Bind the SSBO at the given binding point. */
		void Bind(uint32_t binding) const;

		/** Clear all entries (scene change). */
		void Clear();

		uint32_t GetMaterialCount() const { return static_cast<uint32_t>(m_Materials.size()); }

	private:
		std::vector<RTMaterialGPU>                           m_Materials;
		std::unordered_map<MaterialInstance*, uint32_t>      m_LookupMap;
		Ref<StorageBuffer>                                    m_SSBO;
		bool                                                  m_Dirty = true;
	};

} // namespace Lunex
