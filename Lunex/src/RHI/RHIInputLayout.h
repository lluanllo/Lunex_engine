#pragma once

/**
 * @file RHIInputLayout.h
 * @brief API-agnostic input layout abstraction
 * 
 * InputLayout describes how vertex data is interpreted by the GPU.
 * This replaces the OpenGL-specific VertexArray concept with a more
 * portable abstraction that works with Vulkan, DX12, Metal, etc.
 * 
 * Key differences from VertexArray:
 * - InputLayout is immutable after creation (part of pipeline state)
 * - Buffer bindings are separate from layout description
 * - Supports instancing natively
 */

#include "RHITypes.h"
#include "RHIResource.h"
#include <vector>
#include <string>

namespace Lunex {
namespace RHI {

	// ============================================================================
	// INPUT ELEMENT DESCRIPTION
	// ============================================================================
	
	/**
	 * @struct InputElementDesc
	 * @brief Describes a single vertex attribute
	 */
	struct InputElementDesc {
		std::string SemanticName;          // e.g., "POSITION", "NORMAL", "TEXCOORD"
		uint32_t SemanticIndex = 0;        // For multiple of same semantic (TEXCOORD0, TEXCOORD1)
		DataType Format = DataType::Float3;
		uint32_t InputSlot = 0;            // Which vertex buffer
		uint32_t AlignedByteOffset = 0;    // Offset within vertex
		bool IsPerInstance = false;        // Per-vertex or per-instance
		uint32_t InstanceDataStepRate = 0; // For instanced data
		
		InputElementDesc() = default;
		InputElementDesc(const std::string& semantic, DataType format, 
						 uint32_t slot = 0, uint32_t offset = 0)
			: SemanticName(semantic), Format(format), InputSlot(slot), AlignedByteOffset(offset) {}
		
		// Factory for common elements
		static InputElementDesc Position(uint32_t slot = 0, uint32_t offset = 0) {
			return InputElementDesc("POSITION", DataType::Float3, slot, offset);
		}
		
		static InputElementDesc Normal(uint32_t slot = 0, uint32_t offset = 0) {
			return InputElementDesc("NORMAL", DataType::Float3, slot, offset);
		}
		
		static InputElementDesc TexCoord(uint32_t index = 0, uint32_t slot = 0, uint32_t offset = 0) {
			InputElementDesc desc("TEXCOORD", DataType::Float2, slot, offset);
			desc.SemanticIndex = index;
			return desc;
		}
		
		static InputElementDesc Color(uint32_t slot = 0, uint32_t offset = 0) {
			return InputElementDesc("COLOR", DataType::Float4, slot, offset);
		}
		
		static InputElementDesc Tangent(uint32_t slot = 0, uint32_t offset = 0) {
			return InputElementDesc("TANGENT", DataType::Float4, slot, offset);
		}
	};

	// ============================================================================
	// INPUT LAYOUT DESCRIPTION
	// ============================================================================
	
	/**
	 * @struct InputLayoutDesc
	 * @brief Complete input layout description
	 */
	struct InputLayoutDesc {
		std::vector<InputElementDesc> Elements;
		std::string DebugName;
		
		InputLayoutDesc() = default;
		InputLayoutDesc(std::initializer_list<InputElementDesc> elements)
			: Elements(elements) {}
		
		/**
		 * @brief Add an element to the layout
		 */
		InputLayoutDesc& Add(const InputElementDesc& element) {
			Elements.push_back(element);
			return *this;
		}
		
		/**
		 * @brief Calculate stride for a specific input slot
		 */
		uint32_t GetStride(uint32_t slot) const {
			uint32_t stride = 0;
			for (const auto& element : Elements) {
				if (element.InputSlot == slot) {
					stride = std::max(stride, 
						element.AlignedByteOffset + GetDataTypeSize(element.Format));
				}
			}
			return stride;
		}
		
		/**
		 * @brief Factory for common layouts
		 */
		static InputLayoutDesc PositionOnly() {
			return InputLayoutDesc{
				InputElementDesc::Position()
			};
		}
		
		static InputLayoutDesc PositionNormal() {
			return InputLayoutDesc{
				InputElementDesc::Position(0, 0),
				InputElementDesc::Normal(0, 12)
			};
		}
		
		static InputLayoutDesc PositionNormalTexCoord() {
			return InputLayoutDesc{
				InputElementDesc::Position(0, 0),
				InputElementDesc::Normal(0, 12),
				InputElementDesc::TexCoord(0, 0, 24)
			};
		}
		
		static InputLayoutDesc PositionNormalTexCoordTangent() {
			return InputLayoutDesc{
				InputElementDesc::Position(0, 0),
				InputElementDesc::Normal(0, 12),
				InputElementDesc::TexCoord(0, 0, 24),
				InputElementDesc::Tangent(0, 32)
			};
		}
		
		static InputLayoutDesc PositionColorTexCoord() {
			return InputLayoutDesc{
				InputElementDesc::Position(0, 0),
				InputElementDesc::Color(0, 12),
				InputElementDesc::TexCoord(0, 0, 28)
			};
		}
	};

	// ============================================================================
	// RHI INPUT LAYOUT
	// ============================================================================
	
	/**
	 * @class RHIInputLayout
	 * @brief Abstract input layout (immutable once created)
	 * 
	 * This is API-agnostic and represents the vertex input configuration.
	 * In OpenGL, this will internally manage a VAO.
	 * In Vulkan/DX12, this becomes part of the pipeline state.
	 */
	class RHIInputLayout : public RHIResource {
	public:
		virtual ~RHIInputLayout() = default;
		
		/**
		 * @brief Get the input layout description
		 */
		virtual const InputLayoutDesc& GetDescription() const = 0;
		
		/**
		 * @brief Get number of input slots used
		 */
		virtual uint32_t GetNumInputSlots() const = 0;
		
		/**
		 * @brief Get stride for a specific slot
		 */
		virtual uint32_t GetStride(uint32_t slot) const = 0;
		
		/**
		 * @brief Create an input layout
		 */
		static Ref<RHIInputLayout> Create(const InputLayoutDesc& desc);
	};

	// ============================================================================
	// VERTEX BUFFER VIEW
	// ============================================================================
	
	/**
	 * @struct VertexBufferView
	 * @brief Describes a vertex buffer binding (API-agnostic)
	 */
	struct VertexBufferView {
		Ref<RHIBuffer> Buffer;
		uint32_t Stride = 0;
		uint32_t Offset = 0;
		
		VertexBufferView() = default;
		VertexBufferView(Ref<RHIBuffer> buffer, uint32_t stride, uint32_t offset = 0)
			: Buffer(buffer), Stride(stride), Offset(offset) {}
	};

	// ============================================================================
	// INDEX BUFFER VIEW
	// ============================================================================
	
	/**
	 * @struct IndexBufferView
	 * @brief Describes an index buffer binding (API-agnostic)
	 */
	struct IndexBufferView {
		Ref<RHIBuffer> Buffer;
		IndexType Format = IndexType::UInt32;
		uint32_t Offset = 0;
		
		IndexBufferView() = default;
		IndexBufferView(Ref<RHIBuffer> buffer, IndexType format = IndexType::UInt32, uint32_t offset = 0)
			: Buffer(buffer), Format(format), Offset(offset) {}
	};

} // namespace RHI
} // namespace Lunex
