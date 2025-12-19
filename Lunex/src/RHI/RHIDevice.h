#pragma once

/**
 * @file RHIDevice.h
 * @brief GPU Device abstraction - Factory for creating all RHI resources
 * 
 * The RHIDevice represents a physical GPU and provides methods to:
 * - Create all GPU resources (buffers, textures, shaders, etc.)
 * - Query device capabilities
 * - Manage resource pools and caches
 */

#include "RHITypes.h"
#include "RHIResource.h"

// Forward declare GLM types to avoid header dependency
namespace glm {
	template<typename T, int N> struct vec;
	using vec2 = vec<float, 2>;
	using vec3 = vec<float, 3>;
	using vec4 = vec<float, 4>;
	template<typename T, int C, int R> struct mat;
	using mat3 = mat<float, 3, 3>;
	using mat4 = mat<float, 4, 4>;
}

namespace Lunex {
namespace RHI {

	// ============================================================================
	// FORWARD DECLARATIONS
	// ============================================================================
	
	class RHIBuffer;
	class RHITexture2D;
	class RHITextureCube;
	class RHISampler;
	class RHIShader;
	class RHIPipeline;
	class RHIFramebuffer;
	class RHICommandList;
	class RHIFence;
	
	// ============================================================================
	// BUFFER CREATION SPECS
	// ============================================================================
	
	struct BufferCreateInfo : ResourceCreationInfo {
		BufferType Type = BufferType::Vertex;
		BufferUsage Usage = BufferUsage::Static;
		uint64_t Size = 0;
		const void* InitialData = nullptr;
		
		// For vertex buffers
		uint32_t Stride = 0;
		
		// For index buffers
		IndexType IndexFormat = IndexType::UInt32;
	};

	// ============================================================================
	// TEXTURE CREATION SPECS
	// ============================================================================
	
	struct TextureCreateInfo : ResourceCreationInfo {
		uint32_t Width = 1;
		uint32_t Height = 1;
		uint32_t Depth = 1;           // For 3D textures
		uint32_t ArrayLayers = 1;     // For texture arrays
		uint32_t MipLevels = 1;       // 0 = auto-generate
		uint32_t SampleCount = 1;     // For MSAA
		TextureFormat Format = TextureFormat::RGBA8;
		
		bool GenerateMipmaps = false;
		bool IsRenderTarget = false;
		bool IsStorage = false;       // For compute shader access
		
		const void* InitialData = nullptr;
		uint64_t InitialDataSize = 0;
	};

	// ============================================================================
	// SAMPLER CREATION SPECS
	// ============================================================================
	
	struct SamplerCreateInfo : ResourceCreationInfo {
		SamplerState State;
	};

	// ============================================================================
	// SHADER CREATION SPECS
	// ============================================================================
	
	struct ShaderStageInfo {
		ShaderStage Stage = ShaderStage::None;
		std::string SourceCode;       // GLSL/HLSL source
		std::string EntryPoint = "main";
		std::vector<uint32_t> SPIRV;  // Pre-compiled SPIR-V (optional)
	};

	struct ShaderCreateInfo : ResourceCreationInfo {
		std::vector<ShaderStageInfo> Stages;
		std::string FilePath;         // Alternative: load from file
	};

	// ============================================================================
	// PIPELINE CREATION SPECS
	// ============================================================================
	
	struct VertexInputElement {
		std::string Name;
		DataType Type = DataType::Float3;
		uint32_t Offset = 0;
		uint32_t BufferSlot = 0;
		bool PerInstance = false;     // Instance data vs vertex data
	};

	struct VertexInputLayout {
		std::vector<VertexInputElement> Elements;
		uint32_t Stride = 0;
	};

	struct PipelineCreateInfo : ResourceCreationInfo {
		Ref<RHIShader> Shader;
		VertexInputLayout VertexLayout;
		PrimitiveTopology Topology = PrimitiveTopology::Triangles;
		RasterizerState Rasterizer;
		DepthStencilState DepthStencil;
		BlendState Blend;
		
		// Render target formats (for validation)
		std::vector<TextureFormat> ColorAttachmentFormats;
		TextureFormat DepthAttachmentFormat = TextureFormat::None;
		uint32_t SampleCount = 1;
	};

	// ============================================================================
	// FRAMEBUFFER CREATION SPECS
	// ============================================================================
	
	struct FramebufferAttachment {
		Ref<RHITexture2D> Texture;
		uint32_t MipLevel = 0;
		uint32_t ArrayLayer = 0;
		ClearValue ClearValue;
	};

	struct FramebufferCreateInfo : ResourceCreationInfo {
		uint32_t Width = 0;
		uint32_t Height = 0;
		std::vector<FramebufferAttachment> ColorAttachments;
		FramebufferAttachment DepthStencilAttachment;
	};

	// ============================================================================
	// COMMAND LIST CREATION SPECS
	// ============================================================================
	
	enum class CommandListType : uint8_t {
		Graphics = 0,
		Compute,
		Copy
	};

	struct CommandListCreateInfo : ResourceCreationInfo {
		CommandListType Type = CommandListType::Graphics;
	};

	// ============================================================================
	// RHI DEVICE INTERFACE
	// ============================================================================
	
	/**
	 * @class RHIDevice
	 * @brief Abstract GPU device interface - creates and manages all RHI resources
	 * 
	 * This is the main factory for creating GPU resources. Each graphics API
	 * (OpenGL, Vulkan, DX12) provides its own implementation.
	 */
	class RHIDevice {
	public:
		virtual ~RHIDevice() = default;
		
		// ============================================
		// DEVICE INFO
		// ============================================
		
		/**
		 * @brief Get the graphics API this device uses
		 */
		virtual GraphicsAPI GetAPI() const = 0;
		
		/**
		 * @brief Get device capabilities and limits
		 */
		virtual const DeviceCapabilities& GetCapabilities() const = 0;
		
		/**
		 * @brief Get the device name (e.g., "NVIDIA GeForce RTX 3080")
		 */
		virtual const std::string& GetDeviceName() const = 0;
		
		// ============================================
		// RESOURCE CREATION
		// ============================================
		
		/**
		 * @brief Create a GPU buffer
		 * @param info Buffer creation parameters
		 * @return New buffer resource, or nullptr on failure
		 */
		virtual Ref<RHIBuffer> CreateBuffer(const BufferCreateInfo& info) = 0;
		
		/**
		 * @brief Create a 2D texture
		 * @param info Texture creation parameters
		 * @return New texture resource, or nullptr on failure
		 */
		virtual Ref<RHITexture2D> CreateTexture2D(const TextureCreateInfo& info) = 0;
		
		/**
		 * @brief Create a cube texture (for skyboxes, environment maps)
		 * @param info Texture creation parameters (Width must equal Height)
		 * @return New cube texture resource, or nullptr on failure
		 */
		virtual Ref<RHITextureCube> CreateTextureCube(const TextureCreateInfo& info) = 0;
		
		/**
		 * @brief Create a texture sampler
		 * @param info Sampler state configuration
		 * @return New sampler resource, or nullptr on failure
		 */
		virtual Ref<RHISampler> CreateSampler(const SamplerCreateInfo& info) = 0;
		
		/**
		 * @brief Create a shader program
		 * @param info Shader stages and source code
		 * @return New shader resource, or nullptr on failure
		 */
		virtual Ref<RHIShader> CreateShader(const ShaderCreateInfo& info) = 0;
		
		/**
		 * @brief Create a render pipeline state
		 * @param info Pipeline configuration
		 * @return New pipeline resource, or nullptr on failure
		 */
		virtual Ref<RHIPipeline> CreatePipeline(const PipelineCreateInfo& info) = 0;
		
		/**
		 * @brief Create a framebuffer (render target)
		 * @param info Framebuffer attachments and size
		 * @return New framebuffer resource, or nullptr on failure
		 */
		virtual Ref<RHIFramebuffer> CreateFramebuffer(const FramebufferCreateInfo& info) = 0;
		
		/**
		 * @brief Create a command list for recording GPU commands
		 * @param info Command list configuration
		 * @return New command list, or nullptr on failure
		 */
		virtual Ref<RHICommandList> CreateCommandList(const CommandListCreateInfo& info) = 0;
		
		/**
		 * @brief Create a GPU fence for synchronization
		 * @param signaled Initial fence state
		 * @return New fence resource
		 */
		virtual Ref<RHIFence> CreateFence(bool signaled = false) = 0;
		
		// ============================================
		// CONVENIENCE METHODS
		// ============================================
		
		/**
		 * @brief Create a vertex buffer with initial data
		 */
		Ref<RHIBuffer> CreateVertexBuffer(const void* data, uint64_t size, uint32_t stride, 
										  BufferUsage usage = BufferUsage::Static);
		
		/**
		 * @brief Create an index buffer with initial data
		 */
		Ref<RHIBuffer> CreateIndexBuffer(const uint32_t* indices, uint32_t count,
										 BufferUsage usage = BufferUsage::Static);
		
		/**
		 * @brief Create a uniform buffer
		 */
		Ref<RHIBuffer> CreateUniformBuffer(uint64_t size, BufferUsage usage = BufferUsage::Dynamic);
		
		/**
		 * @brief Load a shader from file
		 */
		Ref<RHIShader> CreateShaderFromFile(const std::string& filePath);
		
		/**
		 * @brief Load a texture from file
		 */
		Ref<RHITexture2D> CreateTexture2DFromFile(const std::string& filePath, bool generateMips = true);
		
		// ============================================
		// MEMORY MANAGEMENT
		// ============================================
		
		/**
		 * @brief Get total GPU memory allocated by this device
		 * @return Bytes of GPU memory in use
		 */
		virtual uint64_t GetAllocatedMemory() const = 0;
		
		/**
		 * @brief Get current frame's render statistics
		 */
		virtual const RenderStatistics& GetStatistics() const = 0;
		
		/**
		 * @brief Reset frame statistics (call at start of frame)
		 */
		virtual void ResetStatistics() = 0;
		
		// ============================================
		// DEVICE LIFETIME
		// ============================================
		
		/**
		 * @brief Wait for all GPU operations to complete
		 */
		virtual void WaitIdle() = 0;
		
		/**
		 * @brief Begin a new frame
		 */
		virtual void BeginFrame() = 0;
		
		/**
		 * @brief End the current frame
		 */
		virtual void EndFrame() = 0;
		
		// ============================================
		// STATIC CREATION
		// ============================================
		
		/**
		 * @brief Create an RHI device for the specified API
		 * @param api Graphics API to use
		 * @param window Optional window handle for context creation
		 * @return The created device, or nullptr on failure
		 */
		static Ref<RHIDevice> Create(GraphicsAPI api, void* windowHandle = nullptr);
		
		/**
		 * @brief Get the global device instance
		 * @return The current active device
		 */
		static RHIDevice* Get();
		
	protected:
		static RHIDevice* s_Instance;
	};

} // namespace RHI
} // namespace Lunex
