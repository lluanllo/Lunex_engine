#include "stpch.h"
#include "MaterialGPUTable.h"
#include "TextureAtlas.h"

#include "Assets/Materials/MaterialRegistry.h"
#include "Log/Log.h"

#include <cstring>

namespace Lunex {

	void MaterialGPUTable::Initialize() {
		// Create with room for 256 materials initially
		m_SSBO = StorageBuffer::Create(256 * sizeof(RTMaterialGPU), 0);
		m_Materials.reserve(256);
	}

	void MaterialGPUTable::Shutdown() {
		m_SSBO.reset();
		m_Materials.clear();
		m_LookupMap.clear();
	}

	uint32_t MaterialGPUTable::GetOrAddMaterial(const Ref<MaterialInstance>& instance,
	                                             TextureAtlas* atlas) {
		if (!instance) return 0;

		auto it = m_LookupMap.find(instance.get());
		if (it != m_LookupMap.end())
			return it->second;

		auto data = instance->GetUniformData();

		RTMaterialGPU gpu{};
		gpu.Albedo          = data.Albedo;
		gpu.EmissionAndMeta = glm::vec4(data.EmissionColor, data.Metallic);
		gpu.RoughSpecAO     = glm::vec4(data.Roughness, data.Specular, 1.0f, data.EmissionIntensity);

		// Texture indices — default to -1 (no texture)
		gpu.TexIndices  = glm::ivec4(RT_TEXTURE_NONE);
		gpu.TexIndices2 = glm::ivec4(RT_TEXTURE_NONE);

		if (atlas) {
			if (instance->HasAlbedoMap())
				gpu.TexIndices.x = atlas->GetOrAddTexture(instance->GetAlbedoMap());
			if (instance->HasNormalMap())
				gpu.TexIndices.y = atlas->GetOrAddTexture(instance->GetNormalMap());
			if (instance->HasMetallicMap())
				gpu.TexIndices.z = atlas->GetOrAddTexture(instance->GetMetallicMap());
			if (instance->HasRoughnessMap())
				gpu.TexIndices.w = atlas->GetOrAddTexture(instance->GetRoughnessMap());
			if (instance->HasSpecularMap())
				gpu.TexIndices2.x = atlas->GetOrAddTexture(instance->GetSpecularMap());
			if (instance->HasEmissionMap())
				gpu.TexIndices2.y = atlas->GetOrAddTexture(instance->GetEmissionMap());
			if (instance->HasAOMap())
				gpu.TexIndices2.z = atlas->GetOrAddTexture(instance->GetAOMap());
		}

		// Pack normalIntensity into .w as floatBitsToInt
		float normalIntensity = data.NormalIntensity;
		int normalIntBits;
		std::memcpy(&normalIntBits, &normalIntensity, sizeof(float));
		gpu.TexIndices2.w = normalIntBits;

		uint32_t index = static_cast<uint32_t>(m_Materials.size());
		m_Materials.push_back(gpu);
		m_LookupMap[instance.get()] = index;
		m_Dirty = true;
		return index;
	}

	void MaterialGPUTable::UploadToGPU() {
		if (!m_Dirty || m_Materials.empty()) return;

		uint32_t requiredSize = static_cast<uint32_t>(m_Materials.size() * sizeof(RTMaterialGPU));

		// Recreate if the buffer is too small
		if (!m_SSBO || requiredSize > static_cast<uint32_t>(m_Materials.capacity() * sizeof(RTMaterialGPU))) {
			uint32_t newCap = std::max(requiredSize * 2, 256u * (uint32_t)sizeof(RTMaterialGPU));
			m_SSBO = StorageBuffer::Create(newCap, 0);
		}

		m_SSBO->SetData(m_Materials.data(), requiredSize);
		m_Dirty = false;
	}

	void MaterialGPUTable::Bind(uint32_t binding) const {
		if (m_SSBO)
			m_SSBO->BindForCompute(binding);
	}

	void MaterialGPUTable::Clear() {
		m_Materials.clear();
		m_LookupMap.clear();
		m_Dirty = true;
	}

} // namespace Lunex
