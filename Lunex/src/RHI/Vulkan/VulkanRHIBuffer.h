/**
 * @file VulkanRHIBuffer.h
 * @brief Vulkan implementation of RHI buffer types
 *
 * Wraps VkBuffer + VkDeviceMemory for vertex, index, uniform, and storage buffers.
 */

#pragma once

#include "RHI/RHIBuffer.h"
#include "VulkanRHIContext.h"

namespace Lunex {
namespace RHI {

	class VulkanRHIDevice;

	// ============================================================================
	// VULKAN RHI BUFFER
	// ============================================================================

	class VulkanRHIBuffer : public RHIBuffer {
	public:
		VulkanRHIBuffer(VulkanRHIDevice* device, const BufferDesc& desc, const void* initialData = nullptr);
		virtual ~VulkanRHIBuffer();

		// RHIResource
		RHIHandle GetNativeHandle() const override { return reinterpret_cast<RHIHandle>(m_Buffer); }
		bool IsValid() const override { return m_Buffer != VK_NULL_HANDLE; }

		// Data operations
		void SetData(const void* data, uint64_t size, uint64_t offset = 0) override;
		void GetData(void* data, uint64_t size, uint64_t offset = 0) const override;

		// Memory mapping
		MappedBufferRange Map(BufferAccess access = BufferAccess::Write) override;
		MappedBufferRange MapRange(uint64_t offset, uint64_t size, BufferAccess access = BufferAccess::Write) override;
		void Unmap() override;
		void FlushMappedRange(uint64_t offset, uint64_t size) override;
		bool IsMapped() const override { return m_MappedData != nullptr; }

		// Binding
		void Bind() const override;
		void Unbind() const override;
		void BindToPoint(uint32_t bindingPoint) const override;

		// Vulkan specific
		VkBuffer GetVkBuffer() const { return m_Buffer; }
		VkDeviceMemory GetVkMemory() const { return m_Memory; }

	protected:
		void OnDebugNameChanged() override;

	private:
		void CreateBuffer(const void* initialData);
		void DestroyBuffer();

		VulkanRHIDevice* m_Device = nullptr;
		VkBuffer m_Buffer = VK_NULL_HANDLE;
		VkDeviceMemory m_Memory = VK_NULL_HANDLE;
		void* m_MappedData = nullptr;
	};

	// ============================================================================
	// VULKAN RHI VERTEX BUFFER
	// ============================================================================

	class VulkanRHIVertexBuffer : public RHIVertexBuffer {
	public:
		VulkanRHIVertexBuffer(VulkanRHIDevice* device, const BufferDesc& desc, const VertexLayout& layout, const void* initialData = nullptr);
		virtual ~VulkanRHIVertexBuffer();

		// RHIResource
		RHIHandle GetNativeHandle() const override { return reinterpret_cast<RHIHandle>(m_Buffer); }
		bool IsValid() const override { return m_Buffer != VK_NULL_HANDLE; }

		// Data operations
		void SetData(const void* data, uint64_t size, uint64_t offset = 0) override;
		void GetData(void* data, uint64_t size, uint64_t offset = 0) const override;

		// Memory mapping
		MappedBufferRange Map(BufferAccess access = BufferAccess::Write) override;
		MappedBufferRange MapRange(uint64_t offset, uint64_t size, BufferAccess access = BufferAccess::Write) override;
		void Unmap() override;
		void FlushMappedRange(uint64_t offset, uint64_t size) override;
		bool IsMapped() const override { return m_MappedData != nullptr; }

		// Binding
		void Bind() const override;
		void Unbind() const override;
		void BindToPoint(uint32_t bindingPoint) const override;

		// Vertex layout
		void SetLayout(const VertexLayout& layout) override { m_Layout = layout; }
		const VertexLayout& GetLayout() const override { return m_Layout; }

		// Vulkan specific
		VkBuffer GetVkBuffer() const { return m_Buffer; }

	protected:
		void OnDebugNameChanged() override;

	private:
		VulkanRHIDevice* m_Device = nullptr;
		VkBuffer m_Buffer = VK_NULL_HANDLE;
		VkDeviceMemory m_Memory = VK_NULL_HANDLE;
		VertexLayout m_Layout;
		void* m_MappedData = nullptr;
	};

	// ============================================================================
	// VULKAN RHI INDEX BUFFER
	// ============================================================================

	class VulkanRHIIndexBuffer : public RHIIndexBuffer {
	public:
		VulkanRHIIndexBuffer(VulkanRHIDevice* device, const BufferDesc& desc, const void* initialData = nullptr);
		virtual ~VulkanRHIIndexBuffer();

		// RHIResource
		RHIHandle GetNativeHandle() const override { return reinterpret_cast<RHIHandle>(m_Buffer); }
		bool IsValid() const override { return m_Buffer != VK_NULL_HANDLE; }

		// Data operations
		void SetData(const void* data, uint64_t size, uint64_t offset = 0) override;
		void GetData(void* data, uint64_t size, uint64_t offset = 0) const override;

		// Memory mapping
		MappedBufferRange Map(BufferAccess access = BufferAccess::Write) override;
		MappedBufferRange MapRange(uint64_t offset, uint64_t size, BufferAccess access = BufferAccess::Write) override;
		void Unmap() override;
		void FlushMappedRange(uint64_t offset, uint64_t size) override;
		bool IsMapped() const override { return m_MappedData != nullptr; }

		// Binding
		void Bind() const override;
		void Unbind() const override;
		void BindToPoint(uint32_t bindingPoint) const override;

		// Vulkan specific
		VkBuffer GetVkBuffer() const { return m_Buffer; }
		VkIndexType GetVkIndexType() const {
			return m_Desc.IndexFormat == IndexType::UInt16 ? VK_INDEX_TYPE_UINT16 : VK_INDEX_TYPE_UINT32;
		}

	protected:
		void OnDebugNameChanged() override;

	private:
		VulkanRHIDevice* m_Device = nullptr;
		VkBuffer m_Buffer = VK_NULL_HANDLE;
		VkDeviceMemory m_Memory = VK_NULL_HANDLE;
		void* m_MappedData = nullptr;
	};

	// ============================================================================
	// VULKAN RHI UNIFORM BUFFER
	// ============================================================================

	class VulkanRHIUniformBuffer : public RHIUniformBuffer {
	public:
		VulkanRHIUniformBuffer(VulkanRHIDevice* device, const BufferDesc& desc, const void* initialData = nullptr);
		virtual ~VulkanRHIUniformBuffer();

		// RHIResource
		RHIHandle GetNativeHandle() const override { return reinterpret_cast<RHIHandle>(m_Buffer); }
		bool IsValid() const override { return m_Buffer != VK_NULL_HANDLE; }

		// Data operations
		void SetData(const void* data, uint64_t size, uint64_t offset = 0) override;
		void GetData(void* data, uint64_t size, uint64_t offset = 0) const override;

		// Memory mapping
		MappedBufferRange Map(BufferAccess access = BufferAccess::Write) override;
		MappedBufferRange MapRange(uint64_t offset, uint64_t size, BufferAccess access = BufferAccess::Write) override;
		void Unmap() override;
		void FlushMappedRange(uint64_t offset, uint64_t size) override;
		bool IsMapped() const override { return m_MappedData != nullptr; }

		// Binding
		void Bind() const override;
		void Unbind() const override;
		void BindToPoint(uint32_t bindingPoint) const override;

		// Uniform buffer specific
		void Bind(uint32_t bindingPoint) const override;

		// Vulkan specific
		VkBuffer GetVkBuffer() const { return m_Buffer; }

	protected:
		void OnDebugNameChanged() override;

	private:
		VulkanRHIDevice* m_Device = nullptr;
		VkBuffer m_Buffer = VK_NULL_HANDLE;
		VkDeviceMemory m_Memory = VK_NULL_HANDLE;
		void* m_MappedData = nullptr;
	};

	// ============================================================================
	// VULKAN RHI STORAGE BUFFER (SSBO)
	// ============================================================================

	class VulkanRHIStorageBuffer : public RHIStorageBuffer {
	public:
		VulkanRHIStorageBuffer(VulkanRHIDevice* device, const BufferDesc& desc, const void* initialData = nullptr);
		virtual ~VulkanRHIStorageBuffer();

		// RHIResource
		RHIHandle GetNativeHandle() const override { return reinterpret_cast<RHIHandle>(m_Buffer); }
		bool IsValid() const override { return m_Buffer != VK_NULL_HANDLE; }

		// Data operations
		void SetData(const void* data, uint64_t size, uint64_t offset = 0) override;
		void GetData(void* data, uint64_t size, uint64_t offset = 0) const override;

		// Memory mapping
		MappedBufferRange Map(BufferAccess access = BufferAccess::Write) override;
		MappedBufferRange MapRange(uint64_t offset, uint64_t size, BufferAccess access = BufferAccess::Write) override;
		void Unmap() override;
		void FlushMappedRange(uint64_t offset, uint64_t size) override;
		bool IsMapped() const override { return m_MappedData != nullptr; }

		// Binding
		void Bind() const override;
		void Unbind() const override;
		void BindToPoint(uint32_t bindingPoint) const override;

		// Storage buffer specific
		void BindForCompute(uint32_t bindingPoint) const override;
		void BindForRead(uint32_t bindingPoint) const override;

		// Vulkan specific
		VkBuffer GetVkBuffer() const { return m_Buffer; }

	protected:
		void OnDebugNameChanged() override;

	private:
		VulkanRHIDevice* m_Device = nullptr;
		VkBuffer m_Buffer = VK_NULL_HANDLE;
		VkDeviceMemory m_Memory = VK_NULL_HANDLE;
		void* m_MappedData = nullptr;
	};

} // namespace RHI
} // namespace Lunex
