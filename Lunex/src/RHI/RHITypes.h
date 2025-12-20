#pragma once

/**
 * @file RHITypes.h
 * @brief Core types, enums, and structures for the Render Hardware Interface (RHI)
 * 
 * This file defines the fundamental types used across all RHI implementations.
 * It is backend-agnostic and should not contain any OpenGL/Vulkan/DX12 specific code.
 */

#include "Core/Core.h"
#include <cstdint>
#include <string>
#include <vector>
#include <optional>

namespace Lunex {
namespace RHI {

	// ============================================================================
	// FORWARD DECLARATIONS
	// ============================================================================
	
	class RHIResource;
	class RHIBuffer;
	class RHITexture;
	class RHIShader;
	class RHIPipeline;
	class RHIFramebuffer;
	class RHICommandList;
	class RHIDevice;
	class RHIContext;

	// ============================================================================
	// HANDLES & IDS
	// ============================================================================
	
	using RHIHandle = uint64_t;
	constexpr RHIHandle InvalidRHIHandle = 0;

	struct ResourceHandle {
		RHIHandle Handle = InvalidRHIHandle;
		uint32_t Generation = 0;  // For detecting stale handles
		
		bool IsValid() const { return Handle != InvalidRHIHandle; }
		operator bool() const { return IsValid(); }
		
		bool operator==(const ResourceHandle& other) const {
			return Handle == other.Handle && Generation == other.Generation;
		}
		bool operator!=(const ResourceHandle& other) const { return !(*this == other); }
	};

	// ============================================================================
	// GRAPHICS API SELECTION
	// ============================================================================
	
	enum class GraphicsAPI : uint8_t {
		None = 0,
		OpenGL,
		Vulkan,      // Future
		DirectX12,   // Future
		Metal        // Future (macOS/iOS)
	};

	// ============================================================================
	// SHADER TYPES
	// ============================================================================
	
	enum class ShaderStage : uint8_t {
		None = 0,
		Vertex      = 1 << 0,
		Fragment    = 1 << 1,  // Pixel shader in DX terminology
		Geometry    = 1 << 2,
		TessControl = 1 << 3,  // Hull shader in DX
		TessEval    = 1 << 4,  // Domain shader in DX
		Compute     = 1 << 5,
		
		// Common combinations
		VertexFragment = Vertex | Fragment,
		AllGraphics = Vertex | Fragment | Geometry | TessControl | TessEval
	};
	
	inline ShaderStage operator|(ShaderStage a, ShaderStage b) {
		return static_cast<ShaderStage>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
	}
	inline ShaderStage operator&(ShaderStage a, ShaderStage b) {
		return static_cast<ShaderStage>(static_cast<uint8_t>(a) & static_cast<uint8_t>(b));
	}
	inline bool HasFlag(ShaderStage flags, ShaderStage flag) {
		return (static_cast<uint8_t>(flags) & static_cast<uint8_t>(flag)) != 0;
	}

	// ============================================================================
	// DATA TYPES
	// ============================================================================
	
	enum class DataType : uint8_t {
		None = 0,
		Float, Float2, Float3, Float4,
		Int, Int2, Int3, Int4,
		UInt, UInt2, UInt3, UInt4,
		Mat3, Mat4,
		Bool
	};

	constexpr uint32_t GetDataTypeSize(DataType type) {
		switch (type) {
			case DataType::None:   return 0;
			case DataType::Float:  return 4;
			case DataType::Float2: return 8;
			case DataType::Float3: return 12;
			case DataType::Float4: return 16;
			case DataType::Int:    return 4;
			case DataType::Int2:   return 8;
			case DataType::Int3:   return 12;
			case DataType::Int4:   return 16;
			case DataType::UInt:   return 4;
			case DataType::UInt2:  return 8;
			case DataType::UInt3:  return 12;
			case DataType::UInt4:  return 16;
			case DataType::Mat3:   return 36;
			case DataType::Mat4:   return 64;
			case DataType::Bool:   return 1;
		}
		return 0;
	}

	constexpr uint32_t GetDataTypeComponentCount(DataType type) {
		switch (type) {
			case DataType::Float: case DataType::Int: case DataType::UInt: case DataType::Bool:
				return 1;
			case DataType::Float2: case DataType::Int2: case DataType::UInt2:
				return 2;
			case DataType::Float3: case DataType::Int3: case DataType::UInt3: case DataType::Mat3:
				return 3;
			case DataType::Float4: case DataType::Int4: case DataType::UInt4: case DataType::Mat4:
				return 4;
			default: return 0;
		}
	}

	// ============================================================================
	// TEXTURE FORMATS
	// ============================================================================
	
	enum class TextureFormat : uint8_t {
		None = 0,
		
		// Color formats (8-bit)
		R8,
		RG8,
		RGB8,
		RGBA8,
		SRGB8,
		SRGBA8,
		
		// Color formats (16-bit)
		R16F,
		RG16F,
		RGB16F,
		RGBA16F,
		
		// Color formats (32-bit)
		R32F,
		RG32F,
		RGB32F,
		RGBA32F,
		
		// Integer formats
		R32I,
		RG32I,
		RGBA32I,
		R32UI,
		
		// Depth/Stencil
		Depth16,
		Depth24,
		Depth32F,
		Depth24Stencil8,
		Depth32FStencil8,
		
		// Compressed formats (BC/DXT)
		BC1,      // DXT1 - RGB
		BC1_SRGB,
		BC3,      // DXT5 - RGBA
		BC3_SRGB,
		BC4,      // Single channel
		BC5,      // Two channels (normal maps)
		BC6H,     // HDR
		BC7,      // High quality RGBA
		BC7_SRGB,
		
		// Compressed formats (ETC/ASTC for mobile)
		ETC2_RGB,
		ETC2_RGBA,
		ASTC_4x4,
		ASTC_6x6,
		ASTC_8x8
	};

	constexpr bool IsDepthFormat(TextureFormat format) {
		return format >= TextureFormat::Depth16 && format <= TextureFormat::Depth32FStencil8;
	}

	constexpr bool IsCompressedFormat(TextureFormat format) {
		return format >= TextureFormat::BC1 && format <= TextureFormat::ASTC_8x8;
	}

	constexpr bool IsSRGBFormat(TextureFormat format) {
		return format == TextureFormat::SRGB8 || 
			   format == TextureFormat::SRGBA8 ||
			   format == TextureFormat::BC1_SRGB ||
			   format == TextureFormat::BC3_SRGB ||
			   format == TextureFormat::BC7_SRGB;
	}

	// ============================================================================
	// BUFFER TYPES
	// ============================================================================
	
	enum class BufferType : uint8_t {
		None = 0,
		Vertex,
		Index,
		Uniform,      // Constant buffer in DX
		Storage,      // SSBO in OpenGL, StructuredBuffer in DX
		Indirect,     // For indirect draw commands
		Staging       // For CPU-GPU transfers
	};

	enum class BufferUsage : uint8_t {
		Static = 0,   // GPU only, immutable after creation
		Dynamic,      // CPU write, GPU read (updated frequently)
		Stream,       // CPU write once, GPU read once per frame
		Staging       // CPU read/write for transfers
	};

	enum class BufferAccess : uint8_t {
		None = 0,
		Read         = 1 << 0,
		Write        = 1 << 1,
		ReadWrite    = Read | Write,
		Persistent   = 1 << 2,  // Buffer stays mapped
		Coherent     = 1 << 3   // No explicit flush needed
	};
	
	inline BufferAccess operator|(BufferAccess a, BufferAccess b) {
		return static_cast<BufferAccess>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
	}

	// ============================================================================
	// PRIMITIVE TOPOLOGY
	// ============================================================================
	
	enum class PrimitiveTopology : uint8_t {
		Points = 0,
		Lines,
		LineStrip,
		LineLoop,
		Triangles,
		TriangleStrip,
		TriangleFan,
		Patches         // For tessellation
	};

	// ============================================================================
	// INDEX TYPE
	// ============================================================================
	
	enum class IndexType : uint8_t {
		UInt16 = 0,
		UInt32
	};

	constexpr uint32_t GetIndexTypeSize(IndexType type) {
		return type == IndexType::UInt16 ? 2 : 4;
	}

	// ============================================================================
	// BLEND STATES
	// ============================================================================
	
	enum class BlendFactor : uint8_t {
		Zero = 0,
		One,
		SrcColor,
		OneMinusSrcColor,
		DstColor,
		OneMinusDstColor,
		SrcAlpha,
		OneMinusSrcAlpha,
		DstAlpha,
		OneMinusDstAlpha,
		ConstantColor,
		OneMinusConstantColor,
		SrcAlphaSaturate
	};

	enum class BlendOp : uint8_t {
		Add = 0,
		Subtract,
		ReverseSubtract,
		Min,
		Max
	};

	struct BlendState {
		bool Enabled = false;
		BlendFactor SrcColor = BlendFactor::SrcAlpha;
		BlendFactor DstColor = BlendFactor::OneMinusSrcAlpha;
		BlendOp ColorOp = BlendOp::Add;
		BlendFactor SrcAlpha = BlendFactor::One;
		BlendFactor DstAlpha = BlendFactor::OneMinusSrcAlpha;
		BlendOp AlphaOp = BlendOp::Add;
		
		static BlendState Opaque() { return BlendState{}; }
		static BlendState AlphaBlend() { 
			BlendState state;
			state.Enabled = true;
			return state;
		}
		static BlendState Additive() {
			BlendState state;
			state.Enabled = true;
			state.SrcColor = BlendFactor::SrcAlpha;
			state.DstColor = BlendFactor::One;
			return state;
		}
	};

	// ============================================================================
	// DEPTH/STENCIL STATES
	// ============================================================================
	
	enum class CompareFunc : uint8_t {
		Never = 0,
		Less,
		Equal,
		LessEqual,
		Greater,
		NotEqual,
		GreaterEqual,
		Always
	};

	enum class StencilOp : uint8_t {
		Keep = 0,
		Zero,
		Replace,
		IncrementClamp,
		DecrementClamp,
		Invert,
		IncrementWrap,
		DecrementWrap
	};

	struct StencilState {
		StencilOp FailOp = StencilOp::Keep;
		StencilOp DepthFailOp = StencilOp::Keep;
		StencilOp PassOp = StencilOp::Keep;
		CompareFunc CompareFunc = CompareFunc::Always;
	};

	struct DepthStencilState {
		bool DepthTestEnabled = true;
		bool DepthWriteEnabled = true;
		CompareFunc DepthCompareFunc = CompareFunc::Less;
		
		bool StencilTestEnabled = false;
		uint8_t StencilReadMask = 0xFF;
		uint8_t StencilWriteMask = 0xFF;
		StencilState FrontFace;
		StencilState BackFace;
		
		static DepthStencilState Default() { return DepthStencilState{}; }
		static DepthStencilState ReadOnly() {
			DepthStencilState state;
			state.DepthWriteEnabled = false;
			return state;
		}
		static DepthStencilState Disabled() {
			DepthStencilState state;
			state.DepthTestEnabled = false;
			state.DepthWriteEnabled = false;
			return state;
		}
	};

	// ============================================================================
	// RASTERIZER STATES
	// ============================================================================
	
	enum class CullMode : uint8_t {
		None = 0,
		Front,
		Back
	};

	enum class FillMode : uint8_t {
		Solid = 0,
		Wireframe
	};

	enum class FrontFace : uint8_t {
		CounterClockwise = 0,
		Clockwise
	};

	struct RasterizerState {
		CullMode Culling = CullMode::Back;
		FillMode Fill = FillMode::Solid;
		FrontFace WindingOrder = FrontFace::CounterClockwise;
		float DepthBias = 0.0f;
		float SlopeScaledDepthBias = 0.0f;
		bool DepthClipEnabled = true;
		bool ScissorEnabled = false;
		bool MultisampleEnabled = false;
		bool AntialiasedLineEnabled = false;
		
		static RasterizerState Default() { return RasterizerState{}; }
		static RasterizerState NoCull() {
			RasterizerState state;
			state.Culling = CullMode::None;
			return state;
		}
		static RasterizerState Wireframe() {
			RasterizerState state;
			state.Fill = FillMode::Wireframe;
			return state;
		}
	};

	// ============================================================================
	// SAMPLER STATES
	// ============================================================================
	
	enum class FilterMode : uint8_t {
		Nearest = 0,
		Linear,
		NearestMipmapNearest,
		LinearMipmapNearest,
		NearestMipmapLinear,
		LinearMipmapLinear    // Trilinear
	};

	enum class WrapMode : uint8_t {
		Repeat = 0,
		MirroredRepeat,
		ClampToEdge,
		ClampToBorder
	};

	struct SamplerState {
		FilterMode MinFilter = FilterMode::LinearMipmapLinear;
		FilterMode MagFilter = FilterMode::Linear;
		WrapMode WrapU = WrapMode::Repeat;
		WrapMode WrapV = WrapMode::Repeat;
		WrapMode WrapW = WrapMode::Repeat;
		float MipLODBias = 0.0f;
		float MaxAnisotropy = 1.0f;
		CompareFunc ComparisonFunc = CompareFunc::Never;  // For shadow maps
		float BorderColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };
		float MinLOD = 0.0f;
		float MaxLOD = 1000.0f;
		
		static SamplerState Linear() { return SamplerState{}; }
		static SamplerState Point() {
			SamplerState state;
			state.MinFilter = FilterMode::Nearest;
			state.MagFilter = FilterMode::Nearest;
			return state;
		}
		static SamplerState Anisotropic(float anisotropy = 16.0f) {
			SamplerState state;
			state.MaxAnisotropy = anisotropy;
			return state;
		}
		static SamplerState Shadow() {
			SamplerState state;
			state.MinFilter = FilterMode::Linear;
			state.MagFilter = FilterMode::Linear;
			state.WrapU = WrapMode::ClampToBorder;
			state.WrapV = WrapMode::ClampToBorder;
			state.BorderColor[0] = 1.0f;
			state.BorderColor[1] = 1.0f;
			state.BorderColor[2] = 1.0f;
			state.BorderColor[3] = 1.0f;
			state.ComparisonFunc = CompareFunc::LessEqual;
			return state;
		}
	};

	// ============================================================================
	// VIEWPORT & SCISSOR
	// ============================================================================
	
	struct Viewport {
		float X = 0.0f;
		float Y = 0.0f;
		float Width = 0.0f;
		float Height = 0.0f;
		float MinDepth = 0.0f;
		float MaxDepth = 1.0f;
	};

	struct ScissorRect {
		int32_t X = 0;
		int32_t Y = 0;
		uint32_t Width = 0;
		uint32_t Height = 0;
	};

	// ============================================================================
	// CLEAR VALUES
	// ============================================================================
	
	struct ClearValue {
		union {
			float Color[4];
			struct {
				float Depth;
				uint8_t Stencil;
			} DepthStencil;
		};
		
		ClearValue() : Color{ 0.0f, 0.0f, 0.0f, 1.0f } {}
		
		static ClearValue ColorValue(float r, float g, float b, float a = 1.0f) {
			ClearValue v;
			v.Color[0] = r; v.Color[1] = g; v.Color[2] = b; v.Color[3] = a;
			return v;
		}
		static ClearValue DepthValue(float depth = 1.0f, uint8_t stencil = 0) {
			ClearValue v;
			v.DepthStencil.Depth = depth;
			v.DepthStencil.Stencil = stencil;
			return v;
		}
	};

	// ============================================================================
	// TEXTURE REGION (for copy operations)
	// ============================================================================
	
	struct TextureRegion {
		uint32_t MipLevel = 0;
		uint32_t ArrayLayer = 0;
		
		// Offset
		int32_t X = 0;
		int32_t Y = 0;
		int32_t Z = 0;
		
		// Size (0 = entire dimension)
		uint32_t Width = 0;
		uint32_t Height = 0;
		uint32_t Depth = 1;
	};

	// ============================================================================
	// RESOURCE STATES (for barriers)
	// ============================================================================
	
	enum class ResourceState : uint16_t {
		Undefined = 0,
		Common,
		VertexBuffer,
		IndexBuffer,
		ConstantBuffer,
		ShaderResource,
		UnorderedAccess,
		RenderTarget,
		DepthWrite,
		DepthRead,
		CopySource,
		CopyDest,
		Present
	};

	// ============================================================================
	// DEVICE CAPABILITIES
	// ============================================================================
	
	struct DeviceCapabilities {
		// Limits
		uint32_t MaxTextureSize = 0;
		uint32_t MaxCubeMapSize = 0;
		uint32_t Max3DTextureSize = 0;
		uint32_t MaxArrayTextureLayers = 0;
		uint32_t MaxFramebufferColorAttachments = 0;
		uint32_t MaxUniformBufferSize = 0;
		uint32_t MaxStorageBufferSize = 0;
		uint32_t MaxVertexAttributes = 0;
		uint32_t MaxComputeWorkGroupCount[3] = { 0, 0, 0 };
		uint32_t MaxComputeWorkGroupSize[3] = { 0, 0, 0 };
		float MaxAnisotropy = 0.0f;
		
		// Features
		bool SupportsCompute = false;
		bool SupportsTessellation = false;
		bool SupportsGeometryShader = false;
		bool SupportsMultiDrawIndirect = false;
		bool SupportsBindlessTextures = false;
		bool SupportsRayTracing = false;
		bool SupportsMeshShaders = false;
		bool SupportsVariableRateShading = false;
		
		// Compression formats
		bool SupportsBCCompression = false;
		bool SupportsETCCompression = false;
		bool SupportsASTCCompression = false;
		
		// Memory
		uint64_t DedicatedVideoMemory = 0;
		uint64_t SharedSystemMemory = 0;
		
		// Device info
		std::string DeviceName;
		std::string VendorName;
		std::string DriverVersion;
		GraphicsAPI API = GraphicsAPI::None;
	};

	// ============================================================================
	// STATISTICS
	// ============================================================================
	
	struct RenderStatistics {
		uint32_t DrawCalls = 0;
		uint32_t TrianglesDrawn = 0;
		uint32_t VerticesProcessed = 0;
		uint32_t TextureBinds = 0;
		uint32_t ShaderBinds = 0;
		uint32_t PipelineStateChanges = 0;
		uint32_t BufferUploads = 0;
		uint64_t BufferBytesUploaded = 0;
		
		void Reset() {
			DrawCalls = 0;
			TrianglesDrawn = 0;
			VerticesProcessed = 0;
			TextureBinds = 0;
			ShaderBinds = 0;
			PipelineStateChanges = 0;
			BufferUploads = 0;
			BufferBytesUploaded = 0;
		}
	};

} // namespace RHI
} // namespace Lunex
