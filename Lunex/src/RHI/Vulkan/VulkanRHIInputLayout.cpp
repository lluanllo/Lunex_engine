/**
 * @file VulkanRHIInputLayout.cpp
 * @brief Vulkan implementation of RHI Input Layout
 */

#include "stpch.h"
#include "VulkanRHIInputLayout.h"
#include "Log/Log.h"

namespace Lunex {
namespace RHI {

	VulkanRHIInputLayout::VulkanRHIInputLayout(const InputLayoutDesc& desc)
		: m_Desc(desc)
	{
		// Calculate number of slots and strides
		m_NumSlots = 0;
		for (const auto& element : desc.Elements) {
			m_NumSlots = std::max(m_NumSlots, element.InputSlot + 1);
		}

		m_Strides.resize(m_NumSlots, 0);
		for (uint32_t slot = 0; slot < m_NumSlots; slot++) {
			m_Strides[slot] = desc.GetStride(slot);
		}

		// Build Vulkan binding descriptions (one per slot)
		m_BindingDescriptions.resize(m_NumSlots);
		for (uint32_t slot = 0; slot < m_NumSlots; slot++) {
			m_BindingDescriptions[slot].binding = slot;
			m_BindingDescriptions[slot].stride = m_Strides[slot];
			m_BindingDescriptions[slot].inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
		}

		// Check for per-instance slots
		for (const auto& element : desc.Elements) {
			if (element.IsPerInstance) {
				m_BindingDescriptions[element.InputSlot].inputRate = VK_VERTEX_INPUT_RATE_INSTANCE;
			}
		}

		// Build Vulkan attribute descriptions
		m_AttributeDescriptions.resize(desc.Elements.size());
		for (size_t i = 0; i < desc.Elements.size(); i++) {
			const auto& element = desc.Elements[i];
			m_AttributeDescriptions[i].location = static_cast<uint32_t>(i);
			m_AttributeDescriptions[i].binding = element.InputSlot;
			m_AttributeDescriptions[i].format = DataTypeToVkFormat(element.Format);
			m_AttributeDescriptions[i].offset = element.AlignedByteOffset;
		}

		LNX_LOG_INFO("Created Vulkan InputLayout with {0} attributes across {1} slots",
			desc.Elements.size(), m_NumSlots);
	}

	uint32_t VulkanRHIInputLayout::GetStride(uint32_t slot) const {
		if (slot < m_Strides.size()) {
			return m_Strides[slot];
		}
		return 0;
	}

	VkFormat VulkanRHIInputLayout::DataTypeToVkFormat(DataType type) {
		switch (type) {
			case DataType::Float:  return VK_FORMAT_R32_SFLOAT;
			case DataType::Float2: return VK_FORMAT_R32G32_SFLOAT;
			case DataType::Float3: return VK_FORMAT_R32G32B32_SFLOAT;
			case DataType::Float4: return VK_FORMAT_R32G32B32A32_SFLOAT;
			case DataType::Int:    return VK_FORMAT_R32_SINT;
			case DataType::Int2:   return VK_FORMAT_R32G32_SINT;
			case DataType::Int3:   return VK_FORMAT_R32G32B32_SINT;
			case DataType::Int4:   return VK_FORMAT_R32G32B32A32_SINT;
			case DataType::UInt:   return VK_FORMAT_R32_UINT;
			case DataType::UInt2:  return VK_FORMAT_R32G32_UINT;
			case DataType::UInt3:  return VK_FORMAT_R32G32B32_UINT;
			case DataType::UInt4:  return VK_FORMAT_R32G32B32A32_UINT;
			case DataType::Bool:   return VK_FORMAT_R32_UINT;
			default:               return VK_FORMAT_R32G32B32_SFLOAT;
		}
	}

} // namespace RHI
} // namespace Lunex
