#include "stpch.h"
#include "RHI.h"
#include "Log/Log.h"

namespace Lunex {
namespace RHI {

	// Static instance pointers
	RHIDevice* RHIDevice::s_Instance = nullptr;
	RHIContext* RHIContext::s_Instance = nullptr;
	
	// Module state
	static bool s_Initialized = false;
	static GraphicsAPI s_CurrentAPI = GraphicsAPI::None;
	static Scope<RHIContext> s_Context = nullptr;
	static Ref<RHIDevice> s_Device = nullptr;

	// ============================================================================
	// RHI INITIALIZATION
	// ============================================================================
	
	bool Initialize(GraphicsAPI api, void* windowHandle) {
		if (s_Initialized) {
			LNX_LOG_WARN("RHI already initialized!");
			return true;
		}
		
		LNX_LOG_INFO("Initializing RHI with API: {0}", 
			api == GraphicsAPI::OpenGL ? "OpenGL" :
			api == GraphicsAPI::Vulkan ? "Vulkan" :
			api == GraphicsAPI::DirectX12 ? "DirectX12" :
			api == GraphicsAPI::Metal ? "Metal" : "Unknown");
		
		// Create context
		s_Context = RHIContext::Create(api, windowHandle);
		if (!s_Context || !s_Context->Initialize()) {
			LNX_LOG_ERROR("Failed to create RHI context!");
			return false;
		}
		
		// Create device
		s_Device = RHIDevice::Create(api, windowHandle);
		if (!s_Device) {
			LNX_LOG_ERROR("Failed to create RHI device!");
			s_Context->Shutdown();
			s_Context = nullptr;
			return false;
		}
		
		s_CurrentAPI = api;
		s_Initialized = true;
		
		// Log device info
		const auto& caps = s_Device->GetCapabilities();
		LNX_LOG_INFO("RHI Device: {0}", caps.DeviceName);
		LNX_LOG_INFO("RHI Vendor: {0}", caps.VendorName);
		LNX_LOG_INFO("RHI Driver: {0}", caps.DriverVersion);
		LNX_LOG_INFO("RHI Max Texture Size: {0}", caps.MaxTextureSize);
		LNX_LOG_INFO("RHI Compute Support: {0}", caps.SupportsCompute ? "Yes" : "No");
		
		return true;
	}
	
	void Shutdown() {
		if (!s_Initialized) {
			return;
		}
		
		LNX_LOG_INFO("Shutting down RHI...");
		
		// Wait for GPU to finish
		if (s_Device) {
			s_Device->WaitIdle();
		}
		
		// Release device
		s_Device = nullptr;
		RHIDevice::s_Instance = nullptr;
		
		// Shutdown context
		if (s_Context) {
			s_Context->Shutdown();
			s_Context = nullptr;
		}
		RHIContext::s_Instance = nullptr;
		
		s_CurrentAPI = GraphicsAPI::None;
		s_Initialized = false;
		
		LNX_LOG_INFO("RHI shutdown complete");
	}
	
	bool IsInitialized() {
		return s_Initialized;
	}
	
	GraphicsAPI GetCurrentAPI() {
		return s_CurrentAPI;
	}

	// ============================================================================
	// RHI DEVICE CONVENIENCE METHODS
	// ============================================================================
	
	Ref<RHIBuffer> RHIDevice::CreateVertexBuffer(const void* data, uint64_t size, uint32_t stride, BufferUsage usage) {
		BufferCreateInfo info;
		info.Type = BufferType::Vertex;
		info.Usage = usage;
		info.Size = size;
		info.Stride = stride;
		info.InitialData = data;
		return CreateBuffer(info);
	}
	
	Ref<RHIBuffer> RHIDevice::CreateIndexBuffer(const uint32_t* indices, uint32_t count, BufferUsage usage) {
		BufferCreateInfo info;
		info.Type = BufferType::Index;
		info.Usage = usage;
		info.Size = count * sizeof(uint32_t);
		info.IndexFormat = IndexType::UInt32;
		info.InitialData = indices;
		return CreateBuffer(info);
	}
	
	Ref<RHIBuffer> RHIDevice::CreateUniformBuffer(uint64_t size, BufferUsage usage) {
		BufferCreateInfo info;
		info.Type = BufferType::Uniform;
		info.Usage = usage;
		info.Size = size;
		return CreateBuffer(info);
	}
	
	Ref<RHIShader> RHIDevice::CreateShaderFromFile(const std::string& filePath) {
		ShaderCreateInfo info;
		info.FilePath = filePath;
		info.DebugName = filePath;
		return CreateShader(info);
	}
	
	Ref<RHITexture2D> RHIDevice::CreateTexture2DFromFile(const std::string& filePath, bool generateMips) {
		// This will be implemented when we have texture loading utilities
		// For now, return nullptr - backends will implement their own loaders
		TextureCreateInfo info;
		info.DebugName = filePath;
		info.GenerateMipmaps = generateMips;
		// Note: Actual file loading happens in backend implementation
		return nullptr;
	}

} // namespace RHI
} // namespace Lunex
