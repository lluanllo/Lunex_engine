/**
 * @file VulkanRHIShader.h
 * @brief Vulkan implementation of RHI shader (VkShaderModule + VkPipeline)
 *
 * Consumes SPIR-V directly since shaders are already compiled to SPIR-V
 * via shaderc in the OpenGL path. Vulkan uses the SPIR-V bytecode as-is.
 */

#pragma once

#include "RHI/RHIShader.h"
#include "RHI/RHIPipeline.h"
#include "VulkanRHIContext.h"

#include <unordered_map>
#include <vector>
#include <filesystem>

namespace Lunex {
namespace RHI {

	class VulkanRHIDevice;

	// ============================================================================
	// VULKAN RHI SHADER
	// ============================================================================

	class VulkanRHIShader : public RHIShader {
	public:
		VulkanRHIShader(VulkanRHIDevice* device, const std::string& filePath);
		VulkanRHIShader(VulkanRHIDevice* device, const std::string& name,
						const std::string& vertexSrc, const std::string& fragmentSrc);
		virtual ~VulkanRHIShader();

		// RHIResource
		RHIHandle GetNativeHandle() const override { return 0; }
		bool IsValid() const override { return !m_ShaderModules.empty(); }

		// Shader info
		const std::string& GetName() const override { return m_Name; }
		const std::string& GetFilePath() const override { return m_FilePath; }
		ShaderStage GetStages() const override { return m_Stages; }
		bool IsCompute() const override { return m_IsCompute; }
		const ShaderReflection& GetReflection() const override { return m_Reflection; }

		// Binding (no-op for Vulkan - pipeline-based)
		void Bind() const override;
		void Unbind() const override;

		// Uniforms (no-op for Vulkan - descriptor-based)
		void SetInt(const std::string& name, int value) override;
		void SetIntArray(const std::string& name, const int* values, uint32_t count) override;
		void SetFloat(const std::string& name, float value) override;
		void SetFloat2(const std::string& name, const glm::vec2& value) override;
		void SetFloat3(const std::string& name, const glm::vec3& value) override;
		void SetFloat4(const std::string& name, const glm::vec4& value) override;
		void SetMat3(const std::string& name, const glm::mat3& value) override;
		void SetMat4(const std::string& name, const glm::mat4& value) override;
		int GetUniformLocation(const std::string& name) const override;

		// Compute
		void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) override;
		void GetWorkGroupSize(uint32_t& x, uint32_t& y, uint32_t& z) const override;

		// Hot reload
		bool Reload() override;

		// Vulkan specific
		const std::vector<VkPipelineShaderStageCreateInfo>& GetShaderStages() const { return m_ShaderStageInfos; }
		VkShaderModule GetVertexModule() const;
		VkShaderModule GetFragmentModule() const;

	protected:
		void OnDebugNameChanged() override;

	private:
		std::string ReadFile(const std::string& filePath);
		void CompileGLSLToSPIRV(const std::string& source, VkShaderStageFlagBits stage);
		void CreateShaderModules();
		void DestroyShaderModules();
		void ReflectSPIRV(VkShaderStageFlagBits stage, const std::vector<uint32_t>& spirvData);

		static std::filesystem::path GetCacheDirectory();
		static void CreateCacheDirectoryIfNeeded();

		VulkanRHIDevice* m_Device = nullptr;
		std::string m_Name;
		std::string m_FilePath;
		ShaderStage m_Stages = ShaderStage::None;
		ShaderReflection m_Reflection;
		bool m_IsCompute = false;

		// SPIR-V bytecode per stage
		std::unordered_map<VkShaderStageFlagBits, std::vector<uint32_t>> m_SPIRVData;

		// Vulkan shader modules
		std::unordered_map<VkShaderStageFlagBits, VkShaderModule> m_ShaderModules;
		std::vector<VkPipelineShaderStageCreateInfo> m_ShaderStageInfos;

		// Compute work group size
		uint32_t m_WorkGroupSize[3] = { 1, 1, 1 };
	};

	// ============================================================================
	// VULKAN RHI GRAPHICS PIPELINE
	// ============================================================================

	class VulkanRHIGraphicsPipeline : public RHIGraphicsPipeline {
	public:
		VulkanRHIGraphicsPipeline(VulkanRHIDevice* device, const GraphicsPipelineDesc& desc);
		virtual ~VulkanRHIGraphicsPipeline();

		// RHIResource
		RHIHandle GetNativeHandle() const override { return reinterpret_cast<RHIHandle>(m_Pipeline); }
		bool IsValid() const override { return m_Pipeline != VK_NULL_HANDLE; }

		// Binding
		void Bind() const override;
		void Unbind() const override;

		// Vulkan specific
		VkPipeline GetVkPipeline() const { return m_Pipeline; }
		VkPipelineLayout GetVkPipelineLayout() const { return m_PipelineLayout; }
		VkRenderPass GetVkRenderPass() const { return m_RenderPass; }

	protected:
		void OnDebugNameChanged() override;

	private:
		void CreatePipeline();
		void CreateRenderPass();
		void DestroyPipeline();

		VulkanRHIDevice* m_Device = nullptr;
		VkPipeline m_Pipeline = VK_NULL_HANDLE;
		VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
		VkRenderPass m_RenderPass = VK_NULL_HANDLE;
	};

	// ============================================================================
	// VULKAN RHI COMPUTE PIPELINE
	// ============================================================================

	class VulkanRHIComputePipeline : public RHIComputePipeline {
	public:
		VulkanRHIComputePipeline(VulkanRHIDevice* device, const ComputePipelineDesc& desc);
		virtual ~VulkanRHIComputePipeline();

		// RHIResource
		RHIHandle GetNativeHandle() const override { return reinterpret_cast<RHIHandle>(m_Pipeline); }
		bool IsValid() const override { return m_Pipeline != VK_NULL_HANDLE; }

		// Binding and dispatch
		void Bind() const override;
		void Unbind() const override;
		void Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) const override;
		void GetWorkGroupSize(uint32_t& x, uint32_t& y, uint32_t& z) const override;

		// Vulkan specific
		VkPipeline GetVkPipeline() const { return m_Pipeline; }
		VkPipelineLayout GetVkPipelineLayout() const { return m_PipelineLayout; }

	protected:
		void OnDebugNameChanged() override;

	private:
		VulkanRHIDevice* m_Device = nullptr;
		VkPipeline m_Pipeline = VK_NULL_HANDLE;
		VkPipelineLayout m_PipelineLayout = VK_NULL_HANDLE;
	};

} // namespace RHI
} // namespace Lunex
