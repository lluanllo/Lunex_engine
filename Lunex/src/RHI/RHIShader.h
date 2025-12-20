#pragma once

/**
 * @file RHIShader.h
 * @brief Shader program interface for vertex, fragment, compute, and other shader stages
 * 
 * Shaders in the RHI are compiled programs that can be attached to pipelines.
 * This interface abstracts the differences between:
 * - OpenGL: glCreateProgram + glAttachShader
 * - Vulkan: VkShaderModule + VkPipeline
 * - DX12: ID3D12PipelineState with bytecode
 */

#include "RHIResource.h"
#include "RHITypes.h"
#include <unordered_map>
#include <variant>
#include <glm/glm.hpp>

namespace Lunex {
namespace RHI {

	// ============================================================================
	// SHADER REFLECTION DATA
	// ============================================================================
	
	/**
	 * @struct ShaderUniform
	 * @brief Information about a shader uniform variable
	 */
	struct ShaderUniform {
		std::string Name;
		DataType Type = DataType::None;
		uint32_t Location = 0;       // Uniform location
		uint32_t Offset = 0;         // Offset in uniform buffer
		uint32_t Size = 0;           // Size in bytes
		uint32_t ArraySize = 1;      // For arrays
		ShaderStage Stage = ShaderStage::None;
	};

	/**
	 * @struct ShaderUniformBlock
	 * @brief Information about a uniform buffer block
	 */
	struct ShaderUniformBlock {
		std::string Name;
		uint32_t Binding = 0;
		uint32_t Size = 0;
		std::vector<ShaderUniform> Members;
		ShaderStage Stage = ShaderStage::None;
	};

	/**
	 * @struct ShaderStorageBlock
	 * @brief Information about a storage buffer block (SSBO)
	 */
	struct ShaderStorageBlock {
		std::string Name;
		uint32_t Binding = 0;
		ShaderStage Stage = ShaderStage::None;
	};

	/**
	 * @struct ShaderSampler
	 * @brief Information about a texture sampler
	 */
	struct ShaderSampler {
		std::string Name;
		uint32_t Binding = 0;
		uint32_t ArraySize = 1;
		ShaderStage Stage = ShaderStage::None;
		bool IsShadow = false;       // Shadow sampler (sampler2DShadow)
		bool IsCube = false;         // Cube sampler (samplerCube)
	};

	/**
	 * @struct ShaderVertexInput
	 * @brief Information about a vertex attribute input
	 */
	struct ShaderVertexInput {
		std::string Name;
		uint32_t Location = 0;
		DataType Type = DataType::Float3;
	};

	/**
	 * @struct ShaderReflection
	 * @brief Complete reflection data for a shader program
	 */
	struct ShaderReflection {
		std::vector<ShaderVertexInput> VertexInputs;
		std::vector<ShaderUniform> Uniforms;
		std::vector<ShaderUniformBlock> UniformBlocks;
		std::vector<ShaderStorageBlock> StorageBlocks;
		std::vector<ShaderSampler> Samplers;
		
		// Compute shader specific
		uint32_t ComputeWorkGroupSize[3] = { 0, 0, 0 };
		
		// Output info
		uint32_t ColorOutputCount = 0;
		bool HasDepthOutput = false;
		
		/**
		 * @brief Find a uniform by name
		 */
		const ShaderUniform* FindUniform(const std::string& name) const {
			for (const auto& u : Uniforms) {
				if (u.Name == name) return &u;
			}
			return nullptr;
		}
		
		/**
		 * @brief Find a uniform block by name
		 */
		const ShaderUniformBlock* FindUniformBlock(const std::string& name) const {
			for (const auto& b : UniformBlocks) {
				if (b.Name == name) return &b;
			}
			return nullptr;
		}
		
		/**
		 * @brief Find a sampler by name
		 */
		const ShaderSampler* FindSampler(const std::string& name) const {
			for (const auto& s : Samplers) {
				if (s.Name == name) return &s;
			}
			return nullptr;
		}
	};

	// ============================================================================
	// RHI SHADER
	// ============================================================================
	
	/**
	 * @class RHIShader
	 * @brief Compiled shader program
	 * 
	 * Represents a complete shader program with one or more stages.
	 * Graphics shaders typically have vertex + fragment stages.
	 * Compute shaders have a single compute stage.
	 */
	class RHIShader : public RHIResource {
	public:
		virtual ~RHIShader() = default;
		
		ResourceType GetResourceType() const override { return ResourceType::Shader; }
		
		// ============================================
		// SHADER INFO
		// ============================================
		
		/**
		 * @brief Get the shader name (from source file or creation)
		 */
		virtual const std::string& GetName() const = 0;
		
		/**
		 * @brief Get the source file path (if loaded from file)
		 */
		virtual const std::string& GetFilePath() const = 0;
		
		/**
		 * @brief Get which stages are present
		 */
		virtual ShaderStage GetStages() const = 0;
		
		/**
		 * @brief Check if this is a compute shader
		 */
		virtual bool IsCompute() const = 0;
		
		/**
		 * @brief Check if compilation was successful
		 */
		virtual bool IsValid() const override = 0;
		
		// ============================================
		// REFLECTION
		// ============================================
		
		/**
		 * @brief Get shader reflection data
		 */
		virtual const ShaderReflection& GetReflection() const = 0;
		
		// ============================================
		// BINDING (OpenGL-style, for compatibility)
		// ============================================
		
		/**
		 * @brief Bind the shader program
		 */
		virtual void Bind() const = 0;
		
		/**
		 * @brief Unbind the shader program
		 */
		virtual void Unbind() const = 0;
		
		// ============================================
		// UNIFORM SETTING (Direct, for OpenGL compatibility)
		// Prefer using uniform buffers for new code
		// ============================================
		
		virtual void SetInt(const std::string& name, int value) = 0;
		virtual void SetIntArray(const std::string& name, const int* values, uint32_t count) = 0;
		virtual void SetFloat(const std::string& name, float value) = 0;
		virtual void SetFloat2(const std::string& name, const glm::vec2& value) = 0;
		virtual void SetFloat3(const std::string& name, const glm::vec3& value) = 0;
		virtual void SetFloat4(const std::string& name, const glm::vec4& value) = 0;
		virtual void SetMat3(const std::string& name, const glm::mat3& value) = 0;
		virtual void SetMat4(const std::string& name, const glm::mat4& value) = 0;
		
		/**
		 * @brief Get uniform location (OpenGL specific)
		 * @return Location or -1 if not found
		 */
		virtual int GetUniformLocation(const std::string& name) const = 0;
		
		// ============================================
		// COMPUTE SHADER
		// ============================================
		
		/**
		 * @brief Dispatch compute shader (only for compute shaders)
		 * @param groupsX Number of work groups in X
		 * @param groupsY Number of work groups in Y
		 * @param groupsZ Number of work groups in Z
		 */
		virtual void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) = 0;
		
		/**
		 * @brief Get compute work group size
		 */
		virtual void GetWorkGroupSize(uint32_t& x, uint32_t& y, uint32_t& z) const = 0;
		
		// ============================================
		// HOT RELOAD
		// ============================================
		
		/**
		 * @brief Reload shader from source file
		 * @return true if reload was successful
		 */
		virtual bool Reload() = 0;
		
		// ============================================
		// FACTORY
		// ============================================
		
		/**
		 * @brief Create shader from file
		 * @param filePath Path to shader file (.glsl)
		 */
		static Ref<RHIShader> CreateFromFile(const std::string& filePath);
		
		/**
		 * @brief Create shader from source strings
		 * @param name Shader name
		 * @param vertexSource Vertex shader source
		 * @param fragmentSource Fragment shader source
		 */
		static Ref<RHIShader> CreateFromSource(const std::string& name, 
											   const std::string& vertexSource,
											   const std::string& fragmentSource);
		
		/**
		 * @brief Create compute shader from file
		 * @param filePath Path to compute shader file
		 */
		static Ref<RHIShader> CreateComputeFromFile(const std::string& filePath);
		
		/**
		 * @brief Create compute shader from source
		 * @param name Shader name
		 * @param source Compute shader source
		 */
		static Ref<RHIShader> CreateComputeFromSource(const std::string& name, const std::string& source);
	};

	// ============================================================================
	// SHADER LIBRARY
	// ============================================================================
	
	/**
	 * @class ShaderLibrary
	 * @brief Manages a collection of shaders for the application
	 */
	class RHIShaderLibrary {
	public:
		/**
		 * @brief Add a shader to the library
		 * @param name Name to register the shader under
		 * @param shader The shader to add
		 */
		void Add(const std::string& name, Ref<RHIShader> shader);
		
		/**
		 * @brief Add a shader using its internal name
		 */
		void Add(Ref<RHIShader> shader);
		
		/**
		 * @brief Load a shader from file and add to library
		 * @param filePath Shader file path
		 * @return The loaded shader
		 */
		Ref<RHIShader> Load(const std::string& filePath);
		
		/**
		 * @brief Load with custom name
		 */
		Ref<RHIShader> Load(const std::string& name, const std::string& filePath);
		
		/**
		 * @brief Get a shader by name
		 * @return The shader or nullptr if not found
		 */
		Ref<RHIShader> Get(const std::string& name);
		
		/**
		 * @brief Check if a shader exists
		 */
		bool Exists(const std::string& name) const;
		
		/**
		 * @brief Reload all shaders from their source files
		 */
		void ReloadAll();
		
		/**
		 * @brief Get all shader names
		 */
		std::vector<std::string> GetNames() const;
		
		/**
		 * @brief Clear all shaders
		 */
		void Clear();
		
	private:
		std::unordered_map<std::string, Ref<RHIShader>> m_Shaders;
	};

} // namespace RHI
} // namespace Lunex
