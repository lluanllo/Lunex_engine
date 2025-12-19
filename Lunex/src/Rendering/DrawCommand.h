#pragma once

/**
 * @file DrawCommand.h
 * @brief Draw command and draw list system for efficient rendering
 * 
 * Inspired by:
 * - Unreal's FMeshDrawCommand
 * - Unity's DrawCommandBatch
 * - Destiny's Command Buffer system
 */

#include "Core/Core.h"
#include "RHI/RHI.h"
#include <glm/glm.hpp>

namespace Lunex {

	// ============================================================================
	// DRAW KEY (for sorting)
	// ============================================================================
	
	/**
	 * @struct DrawKey
	 * @brief 64-bit key for sorting draw commands
	 * 
	 * Layout:
	 * - Bits 63-56: View layer (for multi-view rendering)
	 * - Bits 55-48: Translucency type (opaque, transparent, etc.)
	 * - Bits 47-32: Material ID (shader + pipeline)
	 * - Bits 31-16: Mesh ID
	 * - Bits 15-0:  Depth (for front-to-back or back-to-front sorting)
	 */
	union DrawKey {
		uint64_t Key = 0;
		
		struct {
			uint16_t Depth;
			uint16_t MeshID;
			uint16_t MaterialID;
			uint8_t TranslucencyType;
			uint8_t ViewLayer;
		};
		
		DrawKey() = default;
		DrawKey(uint64_t key) : Key(key) {}
		
		static DrawKey Make(uint8_t viewLayer, uint8_t translucency, uint16_t material, uint16_t mesh, uint16_t depth) {
			DrawKey key;
			key.ViewLayer = viewLayer;
			key.TranslucencyType = translucency;
			key.MaterialID = material;
			key.MeshID = mesh;
			key.Depth = depth;
			return key;
		}
		
		bool operator<(const DrawKey& other) const { return Key < other.Key; }
		bool operator>(const DrawKey& other) const { return Key > other.Key; }
	};

	// ============================================================================
	// MESH DRAW DATA
	// ============================================================================
	
	/**
	 * @struct MeshDrawData
	 * @brief Geometry data for a single draw call
	 */
	struct MeshDrawData {
		Ref<RHI::RHIBuffer> VertexBuffer;
		Ref<RHI::RHIBuffer> IndexBuffer;
		
		uint32_t IndexCount = 0;
		uint32_t InstanceCount = 1;
		uint32_t FirstIndex = 0;
		int32_t VertexOffset = 0;
		uint32_t FirstInstance = 0;
		
		RHI::IndexType IndexType = RHI::IndexType::UInt32;
	};

	// ============================================================================
	// MATERIAL DRAW DATA
	// ============================================================================
	
	/**
	 * @struct MaterialDrawData
	 * @brief Material/shader data for a draw call
	 */
	struct MaterialDrawData {
		Ref<RHI::RHIGraphicsPipeline> Pipeline;
		
		// Textures
		std::vector<Ref<RHI::RHITexture>> Textures;
		std::vector<Ref<RHI::RHISampler>> Samplers;
		
		// Uniform buffers
		std::vector<Ref<RHI::RHIBuffer>> UniformBuffers;
		
		// Push constants (small immediate data)
		struct PushConstant {
			std::string Name;
			glm::vec4 Value;
		};
		std::vector<PushConstant> PushConstants;
	};

	// ============================================================================
	// DRAW COMMAND
	// ============================================================================
	
	/**
	 * @struct DrawCommand
	 * @brief Complete description of a single draw call
	 * 
	 * This is a "fat" draw command that contains everything needed to execute.
	 * Larger than a typical command buffer command, but easier to sort and batch.
	 */
	struct DrawCommand {
		DrawKey SortKey;
		
		// Geometry
		MeshDrawData Mesh;
		
		// Material
		MaterialDrawData Material;
		
		// Per-instance data
		glm::mat4 Transform = glm::mat4(1.0f);
		int EntityID = -1;
		
		// Bounding info (for culling)
		glm::vec3 BoundsCenter = glm::vec3(0.0f);
		float BoundsRadius = 0.0f;
		
		// Metadata
		uint32_t DrawCallIndex = 0;  // For debugging
		
		/**
		 * @brief Check if this command is valid
		 */
		bool IsValid() const {
			return Mesh.VertexBuffer && Mesh.IndexBuffer && Material.Pipeline;
		}
		
		/**
		 * @brief Execute this draw command
		 */
		void Execute(RHI::RHICommandList* cmdList) const;
	};

	// ============================================================================
	// DRAW LIST
	// ============================================================================
	
	/**
	 * @class DrawList
	 * @brief Collection of draw commands with sorting and batching
	 */
	class DrawList {
	public:
		DrawList() = default;
		
		/**
		 * @brief Add a draw command to the list
		 */
		void AddDrawCommand(const DrawCommand& command) {
			m_Commands.push_back(command);
			m_Sorted = false;
		}
		
		/**
		 * @brief Add multiple commands
		 */
		void AddDrawCommands(const std::vector<DrawCommand>& commands) {
			m_Commands.insert(m_Commands.end(), commands.begin(), commands.end());
			m_Sorted = false;
		}
		
		/**
		 * @brief Sort commands by their sort key
		 * @param reverse If true, sort back-to-front (for transparency)
		 */
		void Sort(bool reverse = false);
		
		/**
		 * @brief Execute all commands in order
		 */
		void Execute(RHI::RHICommandList* cmdList);
		
		/**
		 * @brief Clear all commands
		 */
		void Clear() {
			m_Commands.clear();
			m_Sorted = false;
		}
		
		/**
		 * @brief Get command count
		 */
		size_t GetCommandCount() const { return m_Commands.size(); }
		
		/**
		 * @brief Check if list is empty
		 */
		bool IsEmpty() const { return m_Commands.empty(); }
		
		/**
		 * @brief Get commands (for advanced use)
		 */
		std::vector<DrawCommand>& GetCommands() { return m_Commands; }
		const std::vector<DrawCommand>& GetCommands() const { return m_Commands; }
		
	private:
		std::vector<DrawCommand> m_Commands;
		bool m_Sorted = false;
	};

	// ============================================================================
	// DRAW LIST BUILDER
	// ============================================================================
	
	/**
	 * @class DrawListBuilder
	 * @brief Helper for building draw lists from scene data
	 */
	class DrawListBuilder {
	public:
		/**
		 * @brief Begin building a draw list
		 */
		void Begin();
		
		/**
		 * @brief Add a mesh with material
		 */
		void AddMesh(
			const glm::mat4& transform,
			Ref<RHI::RHIBuffer> vertexBuffer,
			Ref<RHI::RHIBuffer> indexBuffer,
			uint32_t indexCount,
			Ref<RHI::RHIGraphicsPipeline> pipeline,
			int entityID = -1
		);
		
		/**
		 * @brief Set current material/pipeline for subsequent draws
		 */
		void SetPipeline(Ref<RHI::RHIGraphicsPipeline> pipeline) {
			m_CurrentPipeline = pipeline;
		}
		
		/**
		 * @brief Add texture binding
		 */
		void SetTexture(uint32_t slot, Ref<RHI::RHITexture> texture, Ref<RHI::RHISampler> sampler = nullptr);
		
		/**
		 * @brief Add uniform buffer
		 */
		void SetUniformBuffer(uint32_t binding, Ref<RHI::RHIBuffer> buffer);
		
		/**
		 * @brief Set translucency type for sorting
		 */
		void SetTranslucencyType(uint8_t type) { m_TranslucencyType = type; }
		
		/**
		 * @brief End building and get the draw list
		 */
		DrawList End();
		
	private:
		DrawList m_DrawList;
		Ref<RHI::RHIGraphicsPipeline> m_CurrentPipeline;
		std::vector<Ref<RHI::RHITexture>> m_CurrentTextures;
		std::vector<Ref<RHI::RHISampler>> m_CurrentSamplers;
		std::vector<Ref<RHI::RHIBuffer>> m_CurrentUniformBuffers;
		uint8_t m_TranslucencyType = 0;  // 0 = opaque
		uint16_t m_DrawCallCounter = 0;
	};

	// ============================================================================
	// TRANSLUCENCY TYPES
	// ============================================================================
	
	namespace TranslucencyType {
		constexpr uint8_t Opaque = 0;
		constexpr uint8_t Masked = 1;
		constexpr uint8_t Translucent = 2;
		constexpr uint8_t Additive = 3;
	}

} // namespace Lunex
