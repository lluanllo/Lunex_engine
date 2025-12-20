#include "stpch.h"
#include "OpenGLRHIDevice.h"
#include "OpenGLRHIBuffer.h"
#include "OpenGLRHITexture.h"
#include "OpenGLRHIFramebuffer.h"
#include "OpenGLRHIShader.h"
#include "OpenGLRHICommandList.h"
#include "Log/Log.h"

// Fallback defines for missing GLAD extensions
#ifndef GLAD_GL_KHR_debug
#define GLAD_GL_KHR_debug 0
#endif

#ifndef GLAD_GL_EXT_texture_filter_anisotropic
#define GLAD_GL_EXT_texture_filter_anisotropic 0
#define GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT 0x84FF
#endif

#ifndef GLAD_GL_EXT_texture_compression_s3tc
#define GLAD_GL_EXT_texture_compression_s3tc 0
#endif

#ifndef GLAD_GL_ARB_ES3_compatibility
#define GLAD_GL_ARB_ES3_compatibility 0
#endif

#ifndef GLAD_GL_KHR_texture_compression_astc_ldr
#define GLAD_GL_KHR_texture_compression_astc_ldr 0
#endif

namespace Lunex {
namespace RHI {

	OpenGLRHIDevice::OpenGLRHIDevice() {
		QueryCapabilities();
		s_Instance = this;
		
		LNX_LOG_INFO("OpenGL RHI Device created");
		LNX_LOG_INFO("  Device: {0}", m_DeviceName);
		LNX_LOG_INFO("  Max Texture Size: {0}", m_Capabilities.MaxTextureSize);
		LNX_LOG_INFO("  Max Anisotropy: {0}", m_Capabilities.MaxAnisotropy);
		LNX_LOG_INFO("  Compute Support: {0}", m_Capabilities.SupportsCompute ? "Yes" : "No");
	}
	
	OpenGLRHIDevice::~OpenGLRHIDevice() {
		if (s_Instance == this) {
			s_Instance = nullptr;
		}
		LNX_LOG_INFO("OpenGL RHI Device destroyed");
	}
	
	void OpenGLRHIDevice::QueryCapabilities() {
		m_Capabilities.API = GraphicsAPI::OpenGL;
		
		// Device info
		m_DeviceName = std::string((const char*)glGetString(GL_RENDERER));
		m_Capabilities.DeviceName = m_DeviceName;
		m_Capabilities.VendorName = std::string((const char*)glGetString(GL_VENDOR));
		m_Capabilities.DriverVersion = std::string((const char*)glGetString(GL_VERSION));
		
		// Texture limits
		GLint maxTextureSize;
		glGetIntegerv(GL_MAX_TEXTURE_SIZE, &maxTextureSize);
		m_Capabilities.MaxTextureSize = static_cast<uint32_t>(maxTextureSize);
		
		GLint maxCubeMapSize;
		glGetIntegerv(GL_MAX_CUBE_MAP_TEXTURE_SIZE, &maxCubeMapSize);
		m_Capabilities.MaxCubeMapSize = static_cast<uint32_t>(maxCubeMapSize);
		
		GLint max3DTextureSize;
		glGetIntegerv(GL_MAX_3D_TEXTURE_SIZE, &max3DTextureSize);
		m_Capabilities.Max3DTextureSize = static_cast<uint32_t>(max3DTextureSize);
		
		GLint maxArrayLayers;
		glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &maxArrayLayers);
		m_Capabilities.MaxArrayTextureLayers = static_cast<uint32_t>(maxArrayLayers);
		
		// Framebuffer limits
		GLint maxColorAttachments;
		glGetIntegerv(GL_MAX_COLOR_ATTACHMENTS, &maxColorAttachments);
		m_Capabilities.MaxFramebufferColorAttachments = static_cast<uint32_t>(maxColorAttachments);
		
		// Buffer limits
		GLint maxUBOSize;
		glGetIntegerv(GL_MAX_UNIFORM_BLOCK_SIZE, &maxUBOSize);
		m_Capabilities.MaxUniformBufferSize = static_cast<uint32_t>(maxUBOSize);
		
		GLint maxSSBOSize;
		glGetIntegerv(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &maxSSBOSize);
		m_Capabilities.MaxStorageBufferSize = static_cast<uint32_t>(maxSSBOSize);
		
		// Vertex limits
		GLint maxVertexAttribs;
		glGetIntegerv(GL_MAX_VERTEX_ATTRIBS, &maxVertexAttribs);
		m_Capabilities.MaxVertexAttributes = static_cast<uint32_t>(maxVertexAttribs);
		
		// Anisotropic filtering
		if (GLAD_GL_EXT_texture_filter_anisotropic) {
			GLfloat maxAniso;
			glGetFloatv(GL_MAX_TEXTURE_MAX_ANISOTROPY_EXT, &maxAniso);
			m_Capabilities.MaxAnisotropy = maxAniso;
		} else {
			m_Capabilities.MaxAnisotropy = 1.0f;
		}
		
		// Compute shader support (OpenGL 4.3+)
		m_Capabilities.SupportsCompute = (GLVersion.major > 4) || 
										 (GLVersion.major == 4 && GLVersion.minor >= 3);
		
		if (m_Capabilities.SupportsCompute) {
			GLint workGroupCount[3];
			GLint workGroupSize[3];
			
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 0, &workGroupCount[0]);
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 1, &workGroupCount[1]);
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, 2, &workGroupCount[2]);
			
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 0, &workGroupSize[0]);
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 1, &workGroupSize[1]);
			glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, 2, &workGroupSize[2]);
			
			m_Capabilities.MaxComputeWorkGroupCount[0] = static_cast<uint32_t>(workGroupCount[0]);
			m_Capabilities.MaxComputeWorkGroupCount[1] = static_cast<uint32_t>(workGroupCount[1]);
			m_Capabilities.MaxComputeWorkGroupCount[2] = static_cast<uint32_t>(workGroupCount[2]);
			
			m_Capabilities.MaxComputeWorkGroupSize[0] = static_cast<uint32_t>(workGroupSize[0]);
			m_Capabilities.MaxComputeWorkGroupSize[1] = static_cast<uint32_t>(workGroupSize[1]);
			m_Capabilities.MaxComputeWorkGroupSize[2] = static_cast<uint32_t>(workGroupSize[2]);
		}
		
		// Tessellation support (OpenGL 4.0+)
		m_Capabilities.SupportsTessellation = (GLVersion.major > 4) || 
											  (GLVersion.major == 4 && GLVersion.minor >= 0);
		
		// Geometry shader support (OpenGL 3.2+)
		m_Capabilities.SupportsGeometryShader = (GLVersion.major > 3) || 
												(GLVersion.major == 3 && GLVersion.minor >= 2);
		
		// Multi-draw indirect (OpenGL 4.3+)
		m_Capabilities.SupportsMultiDrawIndirect = m_Capabilities.SupportsCompute;
		
		// Compression formats
		m_Capabilities.SupportsBCCompression = GLAD_GL_EXT_texture_compression_s3tc;
		m_Capabilities.SupportsETCCompression = GLAD_GL_ARB_ES3_compatibility;
		m_Capabilities.SupportsASTCCompression = GLAD_GL_KHR_texture_compression_astc_ldr;
		
		// No ray tracing or mesh shaders in OpenGL
		m_Capabilities.SupportsRayTracing = false;
		m_Capabilities.SupportsMeshShaders = false;
		m_Capabilities.SupportsVariableRateShading = false;
	}
	
	// ============================================================================
	// RESOURCE CREATION
	// ============================================================================
	
	Ref<RHIBuffer> OpenGLRHIDevice::CreateBuffer(const BufferCreateInfo& info) {
		BufferDesc desc;
		desc.Type = info.Type;
		desc.Usage = info.Usage;
		desc.Size = info.Size;
		desc.Stride = info.Stride;
		desc.IndexFormat = info.IndexFormat;
		
		return CreateRef<OpenGLRHIBuffer>(desc, info.InitialData);
	}
	
	Ref<RHITexture2D> OpenGLRHIDevice::CreateTexture2D(const TextureCreateInfo& info) {
		TextureDesc desc;
		desc.Width = info.Width;
		desc.Height = info.Height;
		desc.Depth = info.Depth;
		desc.ArrayLayers = info.ArrayLayers;
		desc.MipLevels = info.MipLevels;
		desc.SampleCount = info.SampleCount;
		desc.Format = info.Format;
		desc.IsRenderTarget = info.IsRenderTarget;
		desc.IsStorage = info.IsStorage;
		desc.GenerateMipmaps = info.GenerateMipmaps;
		
		return CreateRef<OpenGLRHITexture2D>(desc, info.InitialData);
	}
	
	Ref<RHITextureCube> OpenGLRHIDevice::CreateTextureCube(const TextureCreateInfo& info) {
		TextureDesc desc;
		desc.Width = info.Width;
		desc.Height = info.Height;
		desc.MipLevels = info.MipLevels;
		desc.Format = info.Format;
		desc.GenerateMipmaps = info.GenerateMipmaps;
		
		return CreateRef<OpenGLRHITextureCube>(desc);
	}
	
	Ref<RHISampler> OpenGLRHIDevice::CreateSampler(const SamplerCreateInfo& info) {
		return CreateRef<OpenGLRHISampler>(info.State);
	}
	
	Ref<RHIShader> OpenGLRHIDevice::CreateShader(const ShaderCreateInfo& info) {
		if (!info.FilePath.empty()) {
			return CreateRef<OpenGLRHIShader>(info.FilePath);
		}
		// TODO: Support creating from stages
		LNX_LOG_WARN("OpenGLRHIDevice::CreateShader - Only file-based shaders supported currently");
		return nullptr;
	}
	
	Ref<RHIPipeline> OpenGLRHIDevice::CreatePipeline(const PipelineCreateInfo& info) {
		// OpenGL doesn't have explicit pipeline objects
		// State is set at draw time
		LNX_LOG_WARN("OpenGLRHIDevice::CreatePipeline - Not yet implemented (OpenGL uses state at draw time)");
		return nullptr;
	}
	
	Ref<RHIFramebuffer> OpenGLRHIDevice::CreateFramebuffer(const FramebufferCreateInfo& info) {
		FramebufferDesc desc;
		desc.Width = info.Width;
		desc.Height = info.Height;
		
		// Convert color attachments
		for (const auto& colorAtt : info.ColorAttachments) {
			RenderTargetDesc rtDesc;
			rtDesc.Width = info.Width;
			rtDesc.Height = info.Height;
			rtDesc.ExistingTexture = colorAtt.Texture;
			rtDesc.MipLevel = colorAtt.MipLevel;
			rtDesc.ArrayLayer = colorAtt.ArrayLayer;
			if (colorAtt.Texture) {
				rtDesc.Format = colorAtt.Texture->GetFormat();
			}
			desc.ColorAttachments.push_back(rtDesc);
		}
		
		// Convert depth attachment
		if (info.DepthStencilAttachment.Texture) {
			desc.DepthAttachment.ExistingTexture = info.DepthStencilAttachment.Texture;
			desc.DepthAttachment.MipLevel = info.DepthStencilAttachment.MipLevel;
			desc.HasDepth = true;
		}
		
		return CreateRef<OpenGLRHIFramebuffer>(desc);
	}
	
	Ref<RHICommandList> OpenGLRHIDevice::CreateCommandList(const CommandListCreateInfo& info) {
		return CreateRef<OpenGLRHICommandList>();
	}
	
	Ref<RHIFence> OpenGLRHIDevice::CreateFence(bool signaled) {
		// TODO: Implement OpenGLRHIFence (using glFenceSync)
		LNX_LOG_WARN("OpenGLRHIDevice::CreateFence - Not yet implemented");
		return nullptr;
	}
	
	// ============================================================================
	// DEVICE OPERATIONS
	// ============================================================================
	
	void OpenGLRHIDevice::WaitIdle() {
		glFinish();
	}
	
	void OpenGLRHIDevice::BeginFrame() {
		ResetStatistics();
	}
	
	void OpenGLRHIDevice::EndFrame() {
		// Nothing special for OpenGL
	}
	
	// ============================================================================
	// FACTORY IMPLEMENTATION
	// ============================================================================
	
	Ref<RHIDevice> RHIDevice::Create(GraphicsAPI api, void* windowHandle) {
		switch (api) {
			case GraphicsAPI::OpenGL:
				return CreateRef<OpenGLRHIDevice>();
			default:
				LNX_LOG_ERROR("RHIDevice::Create: Unsupported graphics API!");
				return nullptr;
		}
	}

} // namespace RHI
} // namespace Lunex
