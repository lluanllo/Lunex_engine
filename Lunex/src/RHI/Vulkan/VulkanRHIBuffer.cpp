/**
 * @file VulkanRHIBuffer.cpp
 * @brief Vulkan implementation of RHI buffer types
 */

#include "stpch.h"
#include "VulkanRHIBuffer.h"
#include "VulkanRHIDevice.h"
#include "Log/Log.h"

namespace Lunex {
namespace RHI {

	// ============================================================================
	// VULKAN BUFFER HELPERS
	// ============================================================================

	namespace VulkanBufferUtils {

		static VkBufferUsageFlags GetBufferUsage(BufferType type) {
			switch (type) {
				case BufferType::Vertex:   return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
				case BufferType::Index:    return VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
				case BufferType::Uniform:  return VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT;
				case BufferType::Storage:  return VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
				case BufferType::Indirect: return VK_BUFFER_USAGE_INDIRECT_BUFFER_BIT;
				case BufferType::Staging:  return VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
				default: return VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
			}
		}

		static VkMemoryPropertyFlags GetMemoryProperties(BufferUsage usage) {
			switch (usage) {
				case BufferUsage::Static:
					return VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT;
				case BufferUsage::Dynamic:
				case BufferUsage::Stream:
				case BufferUsage::Staging:
					return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
				default:
					return VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
			}
		}

		static void CreateVkBuffer(VulkanRHIDevice* device, VkDeviceSize size, VkBufferUsageFlags usage,
								   VkMemoryPropertyFlags properties, VkBuffer& buffer, VkDeviceMemory& memory)
		{
			VkDevice vkDevice = device->GetVkDevice();

			VkBufferCreateInfo bufferInfo{};
			bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
			bufferInfo.size = size;
			bufferInfo.usage = usage;
			bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;

			if (vkCreateBuffer(vkDevice, &bufferInfo, nullptr, &buffer) != VK_SUCCESS) {
				LNX_LOG_ERROR("Failed to create Vulkan buffer!");
				return;
			}

			VkMemoryRequirements memRequirements;
			vkGetBufferMemoryRequirements(vkDevice, buffer, &memRequirements);

			VkMemoryAllocateInfo allocInfo{};
			allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
			allocInfo.allocationSize = memRequirements.size;
			allocInfo.memoryTypeIndex = device->FindMemoryType(memRequirements.memoryTypeBits, properties);

			if (vkAllocateMemory(vkDevice, &allocInfo, nullptr, &memory) != VK_SUCCESS) {
				LNX_LOG_ERROR("Failed to allocate Vulkan buffer memory!");
				vkDestroyBuffer(vkDevice, buffer, nullptr);
				buffer = VK_NULL_HANDLE;
				return;
			}

			vkBindBufferMemory(vkDevice, buffer, memory, 0);
			device->TrackAllocation(memRequirements.size);
		}

		static void DestroyVkBuffer(VulkanRHIDevice* device, VkBuffer& buffer, VkDeviceMemory& memory, uint64_t size) {
			VkDevice vkDevice = device->GetVkDevice();
			if (buffer != VK_NULL_HANDLE) {
				vkDestroyBuffer(vkDevice, buffer, nullptr);
				buffer = VK_NULL_HANDLE;
			}
			if (memory != VK_NULL_HANDLE) {
				vkFreeMemory(vkDevice, memory, nullptr);
				memory = VK_NULL_HANDLE;
				device->TrackDeallocation(size);
			}
		}

	} // namespace VulkanBufferUtils

	// ============================================================================
	// VULKAN RHI BUFFER
	// ============================================================================

	VulkanRHIBuffer::VulkanRHIBuffer(VulkanRHIDevice* device, const BufferDesc& desc, const void* initialData)
		: m_Device(device)
	{
		m_Desc = desc;
		CreateBuffer(initialData);
	}

	VulkanRHIBuffer::~VulkanRHIBuffer() {
		DestroyBuffer();
	}

	void VulkanRHIBuffer::CreateBuffer(const void* initialData) {
		VkBufferUsageFlags usage = VulkanBufferUtils::GetBufferUsage(m_Desc.Type);
		VkMemoryPropertyFlags memProps = VulkanBufferUtils::GetMemoryProperties(m_Desc.Usage);

		// If static buffer with initial data, we need transfer dst
		if (m_Desc.Usage == BufferUsage::Static && initialData) {
			usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}

		VulkanBufferUtils::CreateVkBuffer(m_Device, m_Desc.Size, usage, memProps, m_Buffer, m_Memory);

		if (initialData && m_Buffer != VK_NULL_HANDLE) {
			if (m_Desc.Usage == BufferUsage::Static) {
				// Use staging buffer for static data
				VkBuffer stagingBuffer = VK_NULL_HANDLE;
				VkDeviceMemory stagingMemory = VK_NULL_HANDLE;

				VulkanBufferUtils::CreateVkBuffer(m_Device, m_Desc.Size,
					VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
					VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
					stagingBuffer, stagingMemory);

				if (stagingBuffer != VK_NULL_HANDLE) {
					void* data = nullptr;
					vkMapMemory(m_Device->GetVkDevice(), stagingMemory, 0, m_Desc.Size, 0, &data);
					memcpy(data, initialData, static_cast<size_t>(m_Desc.Size));
					vkUnmapMemory(m_Device->GetVkDevice(), stagingMemory);

					// Copy staging -> device local
					VulkanRHIContext* context = m_Device->GetVulkanContext();
					VkCommandBuffer cmdBuf = context->BeginSingleTimeCommands();

					VkBufferCopy copyRegion{};
					copyRegion.size = m_Desc.Size;
					vkCmdCopyBuffer(cmdBuf, stagingBuffer, m_Buffer, 1, &copyRegion);

					context->EndSingleTimeCommands(cmdBuf);

					VulkanBufferUtils::DestroyVkBuffer(m_Device, stagingBuffer, stagingMemory, m_Desc.Size);
				}
			} else {
				// Direct map for host-visible buffers
				void* data = nullptr;
				vkMapMemory(m_Device->GetVkDevice(), m_Memory, 0, m_Desc.Size, 0, &data);
				memcpy(data, initialData, static_cast<size_t>(m_Desc.Size));
				vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
			}
		}
	}

	void VulkanRHIBuffer::DestroyBuffer() {
		if (m_MappedData) {
			Unmap();
		}
		VulkanBufferUtils::DestroyVkBuffer(m_Device, m_Buffer, m_Memory, m_Desc.Size);
	}

	void VulkanRHIBuffer::SetData(const void* data, uint64_t size, uint64_t offset) {
		if (!data || !m_Buffer) return;

		void* mapped = nullptr;
		vkMapMemory(m_Device->GetVkDevice(), m_Memory, offset, size, 0, &mapped);
		memcpy(mapped, data, static_cast<size_t>(size));
		vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
	}

	void VulkanRHIBuffer::GetData(void* data, uint64_t size, uint64_t offset) const {
		if (!data || !m_Buffer) return;

		void* mapped = nullptr;
		vkMapMemory(m_Device->GetVkDevice(), m_Memory, offset, size, 0, &mapped);
		memcpy(data, mapped, static_cast<size_t>(size));
		vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
	}

	MappedBufferRange VulkanRHIBuffer::Map(BufferAccess access) {
		return MapRange(0, m_Desc.Size, access);
	}

	MappedBufferRange VulkanRHIBuffer::MapRange(uint64_t offset, uint64_t size, BufferAccess access) {
		MappedBufferRange range{};
		if (!m_Buffer) return range;

		VkResult result = vkMapMemory(m_Device->GetVkDevice(), m_Memory, offset, size, 0, &m_MappedData);
		if (result == VK_SUCCESS) {
			range.Data = m_MappedData;
			range.Offset = offset;
			range.Size = size;
			range.Valid = true;
		}
		return range;
	}

	void VulkanRHIBuffer::Unmap() {
		if (m_MappedData) {
			vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
			m_MappedData = nullptr;
		}
	}

	void VulkanRHIBuffer::FlushMappedRange(uint64_t offset, uint64_t size) {
		VkMappedMemoryRange memoryRange{};
		memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		memoryRange.memory = m_Memory;
		memoryRange.offset = offset;
		memoryRange.size = size;
		vkFlushMappedMemoryRanges(m_Device->GetVkDevice(), 1, &memoryRange);
	}

	void VulkanRHIBuffer::Bind() const {
		// In Vulkan, buffers are bound via command buffers, not globally
	}

	void VulkanRHIBuffer::Unbind() const {
		// No-op for Vulkan
	}

	void VulkanRHIBuffer::BindToPoint(uint32_t bindingPoint) const {
		// In Vulkan, this is done through descriptor sets
	}

	void VulkanRHIBuffer::OnDebugNameChanged() {
		// TODO: Set VK_EXT_debug_utils object name
	}

	// ============================================================================
	// VULKAN RHI VERTEX BUFFER
	// ============================================================================

	VulkanRHIVertexBuffer::VulkanRHIVertexBuffer(VulkanRHIDevice* device, const BufferDesc& desc,
												 const VertexLayout& layout, const void* initialData)
		: m_Device(device), m_Layout(layout)
	{
		m_Desc = desc;

		VkBufferUsageFlags usage = VK_BUFFER_USAGE_VERTEX_BUFFER_BIT;
		VkMemoryPropertyFlags memProps = VulkanBufferUtils::GetMemoryProperties(desc.Usage);

		if (desc.Usage == BufferUsage::Static && initialData) {
			usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}

		VulkanBufferUtils::CreateVkBuffer(m_Device, desc.Size, usage, memProps, m_Buffer, m_Memory);

		if (initialData && m_Buffer != VK_NULL_HANDLE) {
			void* data = nullptr;
			vkMapMemory(m_Device->GetVkDevice(), m_Memory, 0, desc.Size, 0, &data);
			memcpy(data, initialData, static_cast<size_t>(desc.Size));
			vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
		}
	}

	VulkanRHIVertexBuffer::~VulkanRHIVertexBuffer() {
		if (m_MappedData) {
			vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
			m_MappedData = nullptr;
		}
		VulkanBufferUtils::DestroyVkBuffer(m_Device, m_Buffer, m_Memory, m_Desc.Size);
	}

	void VulkanRHIVertexBuffer::SetData(const void* data, uint64_t size, uint64_t offset) {
		if (!data || !m_Buffer) return;
		void* mapped = nullptr;
		vkMapMemory(m_Device->GetVkDevice(), m_Memory, offset, size, 0, &mapped);
		memcpy(mapped, data, static_cast<size_t>(size));
		vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
	}

	void VulkanRHIVertexBuffer::GetData(void* data, uint64_t size, uint64_t offset) const {
		if (!data || !m_Buffer) return;
		void* mapped = nullptr;
		vkMapMemory(m_Device->GetVkDevice(), m_Memory, offset, size, 0, &mapped);
		memcpy(data, mapped, static_cast<size_t>(size));
		vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
	}

	MappedBufferRange VulkanRHIVertexBuffer::Map(BufferAccess access) {
		return MapRange(0, m_Desc.Size, access);
	}

	MappedBufferRange VulkanRHIVertexBuffer::MapRange(uint64_t offset, uint64_t size, BufferAccess access) {
		MappedBufferRange range{};
		if (!m_Buffer) return range;
		VkResult result = vkMapMemory(m_Device->GetVkDevice(), m_Memory, offset, size, 0, &m_MappedData);
		if (result == VK_SUCCESS) {
			range.Data = m_MappedData;
			range.Offset = offset;
			range.Size = size;
			range.Valid = true;
		}
		return range;
	}

	void VulkanRHIVertexBuffer::Unmap() {
		if (m_MappedData) {
			vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
			m_MappedData = nullptr;
		}
	}

	void VulkanRHIVertexBuffer::FlushMappedRange(uint64_t offset, uint64_t size) {
		VkMappedMemoryRange memoryRange{};
		memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		memoryRange.memory = m_Memory;
		memoryRange.offset = offset;
		memoryRange.size = size;
		vkFlushMappedMemoryRanges(m_Device->GetVkDevice(), 1, &memoryRange);
	}

	void VulkanRHIVertexBuffer::Bind() const {}
	void VulkanRHIVertexBuffer::Unbind() const {}
	void VulkanRHIVertexBuffer::BindToPoint(uint32_t) const {}
	void VulkanRHIVertexBuffer::OnDebugNameChanged() {}

	// ============================================================================
	// VULKAN RHI INDEX BUFFER
	// ============================================================================

	VulkanRHIIndexBuffer::VulkanRHIIndexBuffer(VulkanRHIDevice* device, const BufferDesc& desc, const void* initialData)
		: m_Device(device)
	{
		m_Desc = desc;

		VkBufferUsageFlags usage = VK_BUFFER_USAGE_INDEX_BUFFER_BIT;
		VkMemoryPropertyFlags memProps = VulkanBufferUtils::GetMemoryProperties(desc.Usage);

		if (desc.Usage == BufferUsage::Static && initialData) {
			usage |= VK_BUFFER_USAGE_TRANSFER_DST_BIT;
		}

		VulkanBufferUtils::CreateVkBuffer(m_Device, desc.Size, usage, memProps, m_Buffer, m_Memory);

		if (initialData && m_Buffer != VK_NULL_HANDLE) {
			void* data = nullptr;
			vkMapMemory(m_Device->GetVkDevice(), m_Memory, 0, desc.Size, 0, &data);
			memcpy(data, initialData, static_cast<size_t>(desc.Size));
			vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
		}
	}

	VulkanRHIIndexBuffer::~VulkanRHIIndexBuffer() {
		if (m_MappedData) {
			vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
			m_MappedData = nullptr;
		}
		VulkanBufferUtils::DestroyVkBuffer(m_Device, m_Buffer, m_Memory, m_Desc.Size);
	}

	void VulkanRHIIndexBuffer::SetData(const void* data, uint64_t size, uint64_t offset) {
		if (!data || !m_Buffer) return;
		void* mapped = nullptr;
		vkMapMemory(m_Device->GetVkDevice(), m_Memory, offset, size, 0, &mapped);
		memcpy(mapped, data, static_cast<size_t>(size));
		vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
	}

	void VulkanRHIIndexBuffer::GetData(void* data, uint64_t size, uint64_t offset) const {
		if (!data || !m_Buffer) return;
		void* mapped = nullptr;
		vkMapMemory(m_Device->GetVkDevice(), m_Memory, offset, size, 0, &mapped);
		memcpy(data, mapped, static_cast<size_t>(size));
		vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
	}

	MappedBufferRange VulkanRHIIndexBuffer::Map(BufferAccess access) {
		return MapRange(0, m_Desc.Size, access);
	}

	MappedBufferRange VulkanRHIIndexBuffer::MapRange(uint64_t offset, uint64_t size, BufferAccess access) {
		MappedBufferRange range{};
		if (!m_Buffer) return range;
		VkResult result = vkMapMemory(m_Device->GetVkDevice(), m_Memory, offset, size, 0, &m_MappedData);
		if (result == VK_SUCCESS) {
			range.Data = m_MappedData;
			range.Offset = offset;
			range.Size = size;
			range.Valid = true;
		}
		return range;
	}

	void VulkanRHIIndexBuffer::Unmap() {
		if (m_MappedData) {
			vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
			m_MappedData = nullptr;
		}
	}

	void VulkanRHIIndexBuffer::FlushMappedRange(uint64_t offset, uint64_t size) {
		VkMappedMemoryRange memoryRange{};
		memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		memoryRange.memory = m_Memory;
		memoryRange.offset = offset;
		memoryRange.size = size;
		vkFlushMappedMemoryRanges(m_Device->GetVkDevice(), 1, &memoryRange);
	}

	void VulkanRHIIndexBuffer::Bind() const {}
	void VulkanRHIIndexBuffer::Unbind() const {}
	void VulkanRHIIndexBuffer::BindToPoint(uint32_t) const {}
	void VulkanRHIIndexBuffer::OnDebugNameChanged() {}

	// ============================================================================
	// VULKAN RHI UNIFORM BUFFER
	// ============================================================================

	VulkanRHIUniformBuffer::VulkanRHIUniformBuffer(VulkanRHIDevice* device, const BufferDesc& desc, const void* initialData)
		: m_Device(device)
	{
		m_Desc = desc;

		VulkanBufferUtils::CreateVkBuffer(m_Device, desc.Size,
			VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_Buffer, m_Memory);

		if (initialData && m_Buffer != VK_NULL_HANDLE) {
			void* data = nullptr;
			vkMapMemory(m_Device->GetVkDevice(), m_Memory, 0, desc.Size, 0, &data);
			memcpy(data, initialData, static_cast<size_t>(desc.Size));
			vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
		}
	}

	VulkanRHIUniformBuffer::~VulkanRHIUniformBuffer() {
		if (m_MappedData) {
			vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
			m_MappedData = nullptr;
		}
		VulkanBufferUtils::DestroyVkBuffer(m_Device, m_Buffer, m_Memory, m_Desc.Size);
	}

	void VulkanRHIUniformBuffer::SetData(const void* data, uint64_t size, uint64_t offset) {
		if (!data || !m_Buffer) return;
		void* mapped = nullptr;
		vkMapMemory(m_Device->GetVkDevice(), m_Memory, offset, size, 0, &mapped);
		memcpy(mapped, data, static_cast<size_t>(size));
		vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
	}

	void VulkanRHIUniformBuffer::GetData(void* data, uint64_t size, uint64_t offset) const {
		if (!data || !m_Buffer) return;
		void* mapped = nullptr;
		vkMapMemory(m_Device->GetVkDevice(), m_Memory, offset, size, 0, &mapped);
		memcpy(data, mapped, static_cast<size_t>(size));
		vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
	}

	MappedBufferRange VulkanRHIUniformBuffer::Map(BufferAccess access) {
		return MapRange(0, m_Desc.Size, access);
	}

	MappedBufferRange VulkanRHIUniformBuffer::MapRange(uint64_t offset, uint64_t size, BufferAccess access) {
		MappedBufferRange range{};
		if (!m_Buffer) return range;
		VkResult result = vkMapMemory(m_Device->GetVkDevice(), m_Memory, offset, size, 0, &m_MappedData);
		if (result == VK_SUCCESS) {
			range.Data = m_MappedData;
			range.Offset = offset;
			range.Size = size;
			range.Valid = true;
		}
		return range;
	}

	void VulkanRHIUniformBuffer::Unmap() {
		if (m_MappedData) {
			vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
			m_MappedData = nullptr;
		}
	}

	void VulkanRHIUniformBuffer::FlushMappedRange(uint64_t offset, uint64_t size) {
		VkMappedMemoryRange memoryRange{};
		memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		memoryRange.memory = m_Memory;
		memoryRange.offset = offset;
		memoryRange.size = size;
		vkFlushMappedMemoryRanges(m_Device->GetVkDevice(), 1, &memoryRange);
	}

	void VulkanRHIUniformBuffer::Bind() const {}
	void VulkanRHIUniformBuffer::Unbind() const {}
	void VulkanRHIUniformBuffer::BindToPoint(uint32_t) const {}
	void VulkanRHIUniformBuffer::Bind(uint32_t) const {}
	void VulkanRHIUniformBuffer::OnDebugNameChanged() {}

	// ============================================================================
	// VULKAN RHI STORAGE BUFFER
	// ============================================================================

	VulkanRHIStorageBuffer::VulkanRHIStorageBuffer(VulkanRHIDevice* device, const BufferDesc& desc, const void* initialData)
		: m_Device(device)
	{
		m_Desc = desc;

		VulkanBufferUtils::CreateVkBuffer(m_Device, desc.Size,
			VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
			VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
			m_Buffer, m_Memory);

		if (initialData && m_Buffer != VK_NULL_HANDLE) {
			void* data = nullptr;
			vkMapMemory(m_Device->GetVkDevice(), m_Memory, 0, desc.Size, 0, &data);
			memcpy(data, initialData, static_cast<size_t>(desc.Size));
			vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
		}
	}

	VulkanRHIStorageBuffer::~VulkanRHIStorageBuffer() {
		if (m_MappedData) {
			vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
			m_MappedData = nullptr;
		}
		VulkanBufferUtils::DestroyVkBuffer(m_Device, m_Buffer, m_Memory, m_Desc.Size);
	}

	void VulkanRHIStorageBuffer::SetData(const void* data, uint64_t size, uint64_t offset) {
		if (!data || !m_Buffer) return;
		void* mapped = nullptr;
		vkMapMemory(m_Device->GetVkDevice(), m_Memory, offset, size, 0, &mapped);
		memcpy(mapped, data, static_cast<size_t>(size));
		vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
	}

	void VulkanRHIStorageBuffer::GetData(void* data, uint64_t size, uint64_t offset) const {
		if (!data || !m_Buffer) return;
		void* mapped = nullptr;
		vkMapMemory(m_Device->GetVkDevice(), m_Memory, offset, size, 0, &mapped);
		memcpy(data, mapped, static_cast<size_t>(size));
		vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
	}

	MappedBufferRange VulkanRHIStorageBuffer::Map(BufferAccess access) {
		return MapRange(0, m_Desc.Size, access);
	}

	MappedBufferRange VulkanRHIStorageBuffer::MapRange(uint64_t offset, uint64_t size, BufferAccess access) {
		MappedBufferRange range{};
		if (!m_Buffer) return range;
		VkResult result = vkMapMemory(m_Device->GetVkDevice(), m_Memory, offset, size, 0, &m_MappedData);
		if (result == VK_SUCCESS) {
			range.Data = m_MappedData;
			range.Offset = offset;
			range.Size = size;
			range.Valid = true;
		}
		return range;
	}

	void VulkanRHIStorageBuffer::Unmap() {
		if (m_MappedData) {
			vkUnmapMemory(m_Device->GetVkDevice(), m_Memory);
			m_MappedData = nullptr;
		}
	}

	void VulkanRHIStorageBuffer::FlushMappedRange(uint64_t offset, uint64_t size) {
		VkMappedMemoryRange memoryRange{};
		memoryRange.sType = VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE;
		memoryRange.memory = m_Memory;
		memoryRange.offset = offset;
		memoryRange.size = size;
		vkFlushMappedMemoryRanges(m_Device->GetVkDevice(), 1, &memoryRange);
	}

	void VulkanRHIStorageBuffer::Bind() const {}
	void VulkanRHIStorageBuffer::Unbind() const {}
	void VulkanRHIStorageBuffer::BindToPoint(uint32_t) const {}
	void VulkanRHIStorageBuffer::BindForCompute(uint32_t) const {}
	void VulkanRHIStorageBuffer::BindForRead(uint32_t) const {}
	void VulkanRHIStorageBuffer::OnDebugNameChanged() {}

} // namespace RHI
} // namespace Lunex
