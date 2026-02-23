/**
 * @file VulkanRHIDevice.cpp
 * @brief Vulkan implementation of RHIDevice
 */

#include "stpch.h"
#include "VulkanRHIDevice.h"
#include "VulkanRHICommandList.h"
#include "VulkanRHIBuffer.h"
#include "VulkanRHITexture.h"
#include "VulkanRHIShader.h"
#include "VulkanRHIFramebuffer.h"
#include "Log/Log.h"

namespace Lunex {
namespace RHI {

	VulkanRHIDevice::VulkanRHIDevice(VulkanRHIContext* context)
		: m_Context(context)
	{
		m_VkDevice = context->GetDevice();
		m_VkPhysicalDevice = context->GetPhysicalDevice();

		vkGetPhysicalDeviceProperties(m_VkPhysicalDevice, &m_DeviceProperties);
		vkGetPhysicalDeviceFeatures(m_VkPhysicalDevice, &m_DeviceFeatures);
		vkGetPhysicalDeviceMemoryProperties(m_VkPhysicalDevice, &m_MemoryProperties);

		QueryCapabilities();
		s_Instance = this;

		LNX_LOG_INFO("Vulkan RHI Device created");
		LNX_LOG_INFO("  Device: {0}", m_DeviceName);
		LNX_LOG_INFO("  Max Texture Size: {0}", m_Capabilities.MaxTextureSize);
		LNX_LOG_INFO("  Compute Support: {0}", m_Capabilities.SupportsCompute ? "Yes" : "No");
	}

	VulkanRHIDevice::~VulkanRHIDevice() {
		if (s_Instance == this) {
			s_Instance = nullptr;
		}
		LNX_LOG_INFO("Vulkan RHI Device destroyed");
	}

	void VulkanRHIDevice::QueryCapabilities() {
		m_Capabilities.API = GraphicsAPI::Vulkan;

		m_DeviceName = std::string(m_DeviceProperties.deviceName);
		m_Capabilities.DeviceName = m_DeviceName;
		m_Capabilities.VendorName = "Vulkan Vendor";  // TODO: Resolve vendor ID

		uint32_t driverMajor = VK_VERSION_MAJOR(m_DeviceProperties.driverVersion);
		uint32_t driverMinor = VK_VERSION_MINOR(m_DeviceProperties.driverVersion);
		uint32_t driverPatch = VK_VERSION_PATCH(m_DeviceProperties.driverVersion);
		m_Capabilities.DriverVersion = std::to_string(driverMajor) + "." +
									   std::to_string(driverMinor) + "." +
									   std::to_string(driverPatch);

		// Texture limits
		m_Capabilities.MaxTextureSize = m_DeviceProperties.limits.maxImageDimension2D;
		m_Capabilities.MaxCubeMapSize = m_DeviceProperties.limits.maxImageDimensionCube;
		m_Capabilities.Max3DTextureSize = m_DeviceProperties.limits.maxImageDimension3D;
		m_Capabilities.MaxArrayTextureLayers = m_DeviceProperties.limits.maxImageArrayLayers;

		// Framebuffer
		m_Capabilities.MaxFramebufferColorAttachments = m_DeviceProperties.limits.maxColorAttachments;

		// Buffers
		m_Capabilities.MaxUniformBufferSize = static_cast<uint32_t>(
			std::min<uint64_t>(m_DeviceProperties.limits.maxUniformBufferRange, UINT32_MAX));
		m_Capabilities.MaxStorageBufferSize = static_cast<uint32_t>(
			std::min<uint64_t>(m_DeviceProperties.limits.maxStorageBufferRange, UINT32_MAX));

		// Vertex
		m_Capabilities.MaxVertexAttributes = m_DeviceProperties.limits.maxVertexInputAttributes;

		// Anisotropy
		if (m_DeviceFeatures.samplerAnisotropy) {
			m_Capabilities.MaxAnisotropy = m_DeviceProperties.limits.maxSamplerAnisotropy;
		} else {
			m_Capabilities.MaxAnisotropy = 1.0f;
		}

		// Compute (always available in Vulkan 1.0+)
		m_Capabilities.SupportsCompute = true;
		m_Capabilities.MaxComputeWorkGroupCount[0] = m_DeviceProperties.limits.maxComputeWorkGroupCount[0];
		m_Capabilities.MaxComputeWorkGroupCount[1] = m_DeviceProperties.limits.maxComputeWorkGroupCount[1];
		m_Capabilities.MaxComputeWorkGroupCount[2] = m_DeviceProperties.limits.maxComputeWorkGroupCount[2];
		m_Capabilities.MaxComputeWorkGroupSize[0] = m_DeviceProperties.limits.maxComputeWorkGroupSize[0];
		m_Capabilities.MaxComputeWorkGroupSize[1] = m_DeviceProperties.limits.maxComputeWorkGroupSize[1];
		m_Capabilities.MaxComputeWorkGroupSize[2] = m_DeviceProperties.limits.maxComputeWorkGroupSize[2];

		// Features
		m_Capabilities.SupportsTessellation = m_DeviceFeatures.tessellationShader;
		m_Capabilities.SupportsGeometryShader = m_DeviceFeatures.geometryShader;
		m_Capabilities.SupportsMultiDrawIndirect = m_DeviceFeatures.multiDrawIndirect;
		m_Capabilities.SupportsBindlessTextures = false; // Needs VK_EXT_descriptor_indexing check
		m_Capabilities.SupportsRayTracing = false;       // Needs VK_KHR_ray_tracing_pipeline check
		m_Capabilities.SupportsMeshShaders = false;      // Needs VK_EXT_mesh_shader check
		m_Capabilities.SupportsVariableRateShading = false;

		// Compression (always available in Vulkan with proper format support)
		m_Capabilities.SupportsBCCompression = m_DeviceFeatures.textureCompressionBC;
		m_Capabilities.SupportsETCCompression = m_DeviceFeatures.textureCompressionETC2;
		m_Capabilities.SupportsASTCCompression = m_DeviceFeatures.textureCompressionASTC_LDR;

		// Memory
		for (uint32_t i = 0; i < m_MemoryProperties.memoryHeapCount; i++) {
			if (m_MemoryProperties.memoryHeaps[i].flags & VK_MEMORY_HEAP_DEVICE_LOCAL_BIT) {
				m_Capabilities.DedicatedVideoMemory += m_MemoryProperties.memoryHeaps[i].size;
			} else {
				m_Capabilities.SharedSystemMemory += m_MemoryProperties.memoryHeaps[i].size;
			}
		}
	}

	// ============================================================================
	// RESOURCE CREATION
	// ============================================================================

	Ref<RHIBuffer> VulkanRHIDevice::CreateBuffer(const BufferCreateInfo& info) {
		BufferDesc desc;
		desc.Type = info.Type;
		desc.Usage = info.Usage;
		desc.Size = info.Size;
		desc.Stride = info.Stride;
		desc.IndexFormat = info.IndexFormat;

		return CreateRef<VulkanRHIBuffer>(this, desc, info.InitialData);
	}

	Ref<RHITexture2D> VulkanRHIDevice::CreateTexture2D(const TextureCreateInfo& info) {
		TextureDesc desc;
		desc.Width = info.Width;
		desc.Height = info.Height;
		desc.MipLevels = info.MipLevels;
		desc.SampleCount = info.SampleCount;
		desc.Format = info.Format;
		desc.IsRenderTarget = info.IsRenderTarget;
		desc.IsStorage = info.IsStorage;
		desc.GenerateMipmaps = info.GenerateMipmaps;

		return CreateRef<VulkanRHITexture2D>(this, desc, info.InitialData);
	}

	Ref<RHITextureCube> VulkanRHIDevice::CreateTextureCube(const TextureCreateInfo& info) {
		TextureDesc desc;
		desc.Width = info.Width;
		desc.Height = info.Height;
		desc.MipLevels = info.MipLevels;
		desc.Format = info.Format;
		desc.ArrayLayers = 6;

		return CreateRef<VulkanRHITextureCube>(this, desc);
	}

	Ref<RHISampler> VulkanRHIDevice::CreateSampler(const SamplerCreateInfo& info) {
		return CreateRef<VulkanRHISampler>(this, info.State);
	}

	Ref<RHIShader> VulkanRHIDevice::CreateShader(const ShaderCreateInfo& info) {
		if (!info.FilePath.empty()) {
			return CreateRef<VulkanRHIShader>(this, info.FilePath);
		}

		// Create from source stages
		if (info.Stages.size() >= 2) {
			std::string vertSrc, fragSrc;
			for (const auto& stage : info.Stages) {
				if (stage.Stage == ShaderStage::Vertex)
					vertSrc = stage.SourceCode;
				else if (stage.Stage == ShaderStage::Fragment)
					fragSrc = stage.SourceCode;
			}
			if (!vertSrc.empty() && !fragSrc.empty()) {
				return CreateRef<VulkanRHIShader>(this, info.DebugName, vertSrc, fragSrc);
			}
		}

		LNX_LOG_WARN("VulkanRHIDevice::CreateShader - Insufficient shader info");
		return nullptr;
	}

	Ref<RHIPipeline> VulkanRHIDevice::CreatePipeline(const PipelineCreateInfo& info) {
		// TODO: Map PipelineCreateInfo to GraphicsPipelineDesc and create VulkanRHIGraphicsPipeline
		LNX_LOG_WARN("VulkanRHIDevice::CreatePipeline - Not yet implemented");
		return nullptr;
	}

	Ref<RHIFramebuffer> VulkanRHIDevice::CreateFramebuffer(const FramebufferCreateInfo& info) {
		FramebufferDesc desc;
		desc.Width = info.Width;
		desc.Height = info.Height;

		for (const auto& attachment : info.ColorAttachments) {
			RenderTargetDesc rtDesc;
			rtDesc.Width = info.Width;
			rtDesc.Height = info.Height;
			rtDesc.ExistingTexture = attachment.Texture;
			rtDesc.MipLevel = attachment.MipLevel;
			rtDesc.ArrayLayer = attachment.ArrayLayer;
			if (attachment.Texture) {
				rtDesc.Format = attachment.Texture->GetFormat();
			}
			desc.ColorAttachments.push_back(rtDesc);
		}

		if (info.DepthStencilAttachment.Texture) {
			desc.HasDepth = true;
			desc.DepthAttachment.ExistingTexture = info.DepthStencilAttachment.Texture;
			desc.DepthAttachment.Width = info.Width;
			desc.DepthAttachment.Height = info.Height;
			desc.DepthAttachment.Format = info.DepthStencilAttachment.Texture->GetFormat();
		}

		return CreateRef<VulkanRHIFramebuffer>(this, desc);
	}

	Ref<RHICommandList> VulkanRHIDevice::CreateCommandList(const CommandListCreateInfo& info) {
		return CreateRef<VulkanRHICommandList>(m_Context);
	}

	Ref<RHIFence> VulkanRHIDevice::CreateFence(bool signaled) {
		// TODO: Implement VulkanRHIFence
		LNX_LOG_WARN("VulkanRHIDevice::CreateFence - Not yet implemented");
		return nullptr;
	}

	// ============================================================================
	// DEVICE OPERATIONS
	// ============================================================================

	void VulkanRHIDevice::WaitIdle() {
		if (m_VkDevice != VK_NULL_HANDLE) {
			vkDeviceWaitIdle(m_VkDevice);
		}
	}

	void VulkanRHIDevice::BeginFrame() {
		ResetStatistics();
	}

	void VulkanRHIDevice::EndFrame() {
		// Nothing special yet
	}

	uint32_t VulkanRHIDevice::FindMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) const {
		for (uint32_t i = 0; i < m_MemoryProperties.memoryTypeCount; i++) {
			if ((typeFilter & (1 << i)) &&
				(m_MemoryProperties.memoryTypes[i].propertyFlags & properties) == properties) {
				return i;
			}
		}

		LNX_LOG_ERROR("Failed to find suitable Vulkan memory type!");
		return 0;
	}

} // namespace RHI
} // namespace Lunex
