/**
 * @file VulkanRHIInputLayout.h
 * @brief Vulkan implementation of RHI Input Layout
 *
 * In Vulkan, vertex input layout is part of the pipeline state.
 * This class stores the description and provides VkVertexInput* structures
 * for pipeline creation.
 */

#pragma once

#include "RHI/RHIInputLayout.h"
#include "VulkanRHIContext.h"

namespace Lunex {
namespace RHI {

	class VulkanRHIInputLayout : public RHIInputLayout {
	public:
		VulkanRHIInputLayout(const InputLayoutDesc& desc);
		virtual ~VulkanRHIInputLayout() = default;

		// RHIResource
		ResourceType GetResourceType() const override { return ResourceType::InputLayout; }
		RHIHandle GetNativeHandle() const override { return 0; }
		bool IsValid() const override { return !m_Desc.Elements.empty(); }

		// RHIInputLayout
		const InputLayoutDesc& GetDescription() const override { return m_Desc; }
		uint32_t GetNumInputSlots() const override { return m_NumSlots; }
		uint32_t GetStride(uint32_t slot) const override;

		// Vulkan specific - provides pipeline create info structures
		const std::vector<VkVertexInputBindingDescription>& GetBindingDescriptions() const { return m_BindingDescriptions; }
		const std::vector<VkVertexInputAttributeDescription>& GetAttributeDescriptions() const { return m_AttributeDescriptions; }

	protected:
		void OnDebugNameChanged() override {}

	private:
		static VkFormat DataTypeToVkFormat(DataType type);

		InputLayoutDesc m_Desc;
		uint32_t m_NumSlots = 0;
		std::vector<uint32_t> m_Strides;

		std::vector<VkVertexInputBindingDescription> m_BindingDescriptions;
		std::vector<VkVertexInputAttributeDescription> m_AttributeDescriptions;
	};

} // namespace RHI
} // namespace Lunex
