#include "stpch.h"
#include "MaterialGPUTable.h"

#include "Assets/Materials/MaterialRegistry.h"
#include "Log/Log.h"

namespace Lunex {

	void MaterialGPUTable::Initialize() {
		// Create with room for 256 materials initially (~16 KB)
		m_SSBO = StorageBuffer::Create(256 * sizeof(RTMaterialGPU), 0);
		m_Materials.reserve(256);
	}

	void MaterialGPUTable::Shutdown() {
		m_SSBO.reset();
		m_Materials.clear();
		m_LookupMap.clear();
	}

	uint32_t MaterialGPUTable::GetOrAddMaterial(const Ref<MaterialInstance>& instance) {
		if (!instance) return 0;

		auto it = m_LookupMap.find(instance.get());
		if (it != m_LookupMap.end())
			return it->second;

		auto data = instance->GetUniformData();

		RTMaterialGPU gpu{};
		gpu.Albedo          = data.Albedo;
		gpu.EmissionAndMeta = glm::vec4(data.EmissionColor, data.Metallic);
		gpu.RoughSpecAO     = glm::vec4(data.Roughness, data.Specular, 1.0f, data.EmissionIntensity);
		gpu._reserved       = glm::vec4(0.0f);

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
