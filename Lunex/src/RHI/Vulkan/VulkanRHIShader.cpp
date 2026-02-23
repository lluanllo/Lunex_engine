/**
 * @file VulkanRHIShader.cpp
 * @brief Vulkan implementation of RHI shader and pipeline
 */

#include "stpch.h"
#include "VulkanRHIShader.h"
#include "VulkanRHIDevice.h"
#include "Log/Log.h"

#include <fstream>

namespace Lunex {
namespace RHI {

	// ============================================================================
	// VULKAN RHI SHADER
	// ============================================================================

	VulkanRHIShader::VulkanRHIShader(VulkanRHIDevice* device, const std::string& filePath)
		: m_Device(device), m_FilePath(filePath)
	{
		// Extract name from file path
		auto lastSlash = filePath.find_last_of("/\\");
		auto lastDot = filePath.rfind('.');
		auto start = lastSlash == std::string::npos ? 0 : lastSlash + 1;
		m_Name = filePath.substr(start, lastDot == std::string::npos ? filePath.size() - start : lastDot - start);

		std::string source = ReadFile(filePath);
		if (source.empty()) {
			LNX_LOG_ERROR("VulkanRHIShader: Failed to read shader file: {0}", filePath);
			return;
		}

		// TODO: Parse shader source, compile to SPIR-V using shaderc, create modules
		// For now, look for cached SPIR-V from the OpenGL shader pipeline
		LNX_LOG_WARN("VulkanRHIShader: SPIR-V compilation pipeline not fully implemented for: {0}", m_Name);

		m_Stages = ShaderStage::VertexFragment;
	}

	VulkanRHIShader::VulkanRHIShader(VulkanRHIDevice* device, const std::string& name,
									 const std::string& vertexSrc, const std::string& fragmentSrc)
		: m_Device(device), m_Name(name)
	{
		// TODO: Compile GLSL sources to SPIR-V using shaderc
		LNX_LOG_WARN("VulkanRHIShader: Source compilation not fully implemented for: {0}", name);
		m_Stages = ShaderStage::VertexFragment;
	}

	VulkanRHIShader::~VulkanRHIShader() {
		DestroyShaderModules();
	}

	std::string VulkanRHIShader::ReadFile(const std::string& filePath) {
		std::ifstream file(filePath, std::ios::ate | std::ios::binary);
		if (!file.is_open()) {
			return "";
		}

		size_t fileSize = static_cast<size_t>(file.tellg());
		std::string buffer;
		buffer.resize(fileSize);

		file.seekg(0);
		file.read(buffer.data(), fileSize);
		file.close();

		return buffer;
	}

	void VulkanRHIShader::CreateShaderModules() {
		VkDevice vkDevice = m_Device->GetVkDevice();

		for (auto& [stage, spirv] : m_SPIRVData) {
			VkShaderModuleCreateInfo createInfo{};
			createInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
			createInfo.codeSize = spirv.size() * sizeof(uint32_t);
			createInfo.pCode = spirv.data();

			VkShaderModule shaderModule;
			if (vkCreateShaderModule(vkDevice, &createInfo, nullptr, &shaderModule) != VK_SUCCESS) {
				LNX_LOG_ERROR("Failed to create Vulkan shader module for: {0}", m_Name);
				continue;
			}

			m_ShaderModules[stage] = shaderModule;

			VkPipelineShaderStageCreateInfo stageInfo{};
			stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
			stageInfo.stage = stage;
			stageInfo.module = shaderModule;
			stageInfo.pName = "main";
			m_ShaderStageInfos.push_back(stageInfo);
		}
	}

	void VulkanRHIShader::DestroyShaderModules() {
		VkDevice vkDevice = m_Device->GetVkDevice();
		for (auto& [stage, module] : m_ShaderModules) {
			if (module != VK_NULL_HANDLE) {
				vkDestroyShaderModule(vkDevice, module, nullptr);
			}
		}
		m_ShaderModules.clear();
		m_ShaderStageInfos.clear();
	}

	void VulkanRHIShader::ReflectSPIRV(VkShaderStageFlagBits stage, const std::vector<uint32_t>& spirvData) {
		// TODO: Use spirv-cross or spirv-reflect for shader reflection
	}

	void VulkanRHIShader::Bind() const {
		// No-op: Vulkan shaders are bound as part of pipelines
	}

	void VulkanRHIShader::Unbind() const {
		// No-op
	}

	void VulkanRHIShader::SetInt(const std::string& name, int value) {
		// In Vulkan, uniforms are set via descriptor sets or push constants
		LNX_LOG_WARN("VulkanRHIShader::SetInt - Use descriptor sets instead");
	}

	void VulkanRHIShader::SetIntArray(const std::string& name, const int* values, uint32_t count) {
		LNX_LOG_WARN("VulkanRHIShader::SetIntArray - Use descriptor sets instead");
	}

	void VulkanRHIShader::SetFloat(const std::string& name, float value) {
		LNX_LOG_WARN("VulkanRHIShader::SetFloat - Use descriptor sets instead");
	}

	void VulkanRHIShader::SetFloat2(const std::string& name, const glm::vec2& value) {
		LNX_LOG_WARN("VulkanRHIShader::SetFloat2 - Use descriptor sets instead");
	}

	void VulkanRHIShader::SetFloat3(const std::string& name, const glm::vec3& value) {
		LNX_LOG_WARN("VulkanRHIShader::SetFloat3 - Use descriptor sets instead");
	}

	void VulkanRHIShader::SetFloat4(const std::string& name, const glm::vec4& value) {
		LNX_LOG_WARN("VulkanRHIShader::SetFloat4 - Use descriptor sets instead");
	}

	void VulkanRHIShader::SetMat3(const std::string& name, const glm::mat3& value) {
		LNX_LOG_WARN("VulkanRHIShader::SetMat3 - Use descriptor sets instead");
	}

	void VulkanRHIShader::SetMat4(const std::string& name, const glm::mat4& value) {
		LNX_LOG_WARN("VulkanRHIShader::SetMat4 - Use descriptor sets instead");
	}

	int VulkanRHIShader::GetUniformLocation(const std::string& name) const {
		// No uniform locations in Vulkan
		return -1;
	}

	void VulkanRHIShader::Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) {
		// Compute dispatch is done via command buffer in Vulkan
		LNX_LOG_WARN("VulkanRHIShader::Dispatch - Use command list instead");
	}

	void VulkanRHIShader::GetWorkGroupSize(uint32_t& x, uint32_t& y, uint32_t& z) const {
		x = m_WorkGroupSize[0];
		y = m_WorkGroupSize[1];
		z = m_WorkGroupSize[2];
	}

	bool VulkanRHIShader::Reload() {
		if (m_FilePath.empty()) return false;

		DestroyShaderModules();

		std::string source = ReadFile(m_FilePath);
		if (source.empty()) {
			LNX_LOG_ERROR("VulkanRHIShader::Reload - Failed to read: {0}", m_FilePath);
			return false;
		}

		// TODO: Recompile to SPIR-V
		LNX_LOG_WARN("VulkanRHIShader::Reload - Not fully implemented");
		return false;
	}

	VkShaderModule VulkanRHIShader::GetVertexModule() const {
		auto it = m_ShaderModules.find(VK_SHADER_STAGE_VERTEX_BIT);
		return it != m_ShaderModules.end() ? it->second : VK_NULL_HANDLE;
	}

	VkShaderModule VulkanRHIShader::GetFragmentModule() const {
		auto it = m_ShaderModules.find(VK_SHADER_STAGE_FRAGMENT_BIT);
		return it != m_ShaderModules.end() ? it->second : VK_NULL_HANDLE;
	}

	std::filesystem::path VulkanRHIShader::GetCacheDirectory() {
		return "assets/cache/shader/vulkan";
	}

	void VulkanRHIShader::CreateCacheDirectoryIfNeeded() {
		auto cacheDir = GetCacheDirectory();
		if (!std::filesystem::exists(cacheDir)) {
			std::filesystem::create_directories(cacheDir);
		}
	}

	void VulkanRHIShader::OnDebugNameChanged() {
		// TODO: Set VK_EXT_debug_utils object name
	}

	// ============================================================================
	// VULKAN RHI GRAPHICS PIPELINE
	// ============================================================================

	VulkanRHIGraphicsPipeline::VulkanRHIGraphicsPipeline(VulkanRHIDevice* device, const GraphicsPipelineDesc& desc)
		: m_Device(device)
	{
		m_Desc = desc;
		// TODO: Create VkPipeline from description
		LNX_LOG_WARN("VulkanRHIGraphicsPipeline: Pipeline creation not fully implemented");
	}

	VulkanRHIGraphicsPipeline::~VulkanRHIGraphicsPipeline() {
		DestroyPipeline();
	}

	void VulkanRHIGraphicsPipeline::CreatePipeline() {
		// TODO: Full VkGraphicsPipelineCreateInfo setup
	}

	void VulkanRHIGraphicsPipeline::CreateRenderPass() {
		// TODO: Create compatible VkRenderPass for this pipeline
	}

	void VulkanRHIGraphicsPipeline::DestroyPipeline() {
		VkDevice vkDevice = m_Device->GetVkDevice();
		if (m_Pipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(vkDevice, m_Pipeline, nullptr);
			m_Pipeline = VK_NULL_HANDLE;
		}
		if (m_PipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(vkDevice, m_PipelineLayout, nullptr);
			m_PipelineLayout = VK_NULL_HANDLE;
		}
		if (m_RenderPass != VK_NULL_HANDLE) {
			vkDestroyRenderPass(vkDevice, m_RenderPass, nullptr);
			m_RenderPass = VK_NULL_HANDLE;
		}
	}

	void VulkanRHIGraphicsPipeline::Bind() const {
		// Pipeline binding is done via vkCmdBindPipeline in command buffer
	}

	void VulkanRHIGraphicsPipeline::Unbind() const {
		// No-op for Vulkan
	}

	void VulkanRHIGraphicsPipeline::OnDebugNameChanged() {}

	// ============================================================================
	// VULKAN RHI COMPUTE PIPELINE
	// ============================================================================

	VulkanRHIComputePipeline::VulkanRHIComputePipeline(VulkanRHIDevice* device, const ComputePipelineDesc& desc)
		: m_Device(device)
	{
		m_Desc = desc;
		// TODO: Create VkComputePipeline
		LNX_LOG_WARN("VulkanRHIComputePipeline: Pipeline creation not fully implemented");
	}

	VulkanRHIComputePipeline::~VulkanRHIComputePipeline() {
		VkDevice vkDevice = m_Device->GetVkDevice();
		if (m_Pipeline != VK_NULL_HANDLE) {
			vkDestroyPipeline(vkDevice, m_Pipeline, nullptr);
			m_Pipeline = VK_NULL_HANDLE;
		}
		if (m_PipelineLayout != VK_NULL_HANDLE) {
			vkDestroyPipelineLayout(vkDevice, m_PipelineLayout, nullptr);
			m_PipelineLayout = VK_NULL_HANDLE;
		}
	}

	void VulkanRHIComputePipeline::Bind() const {}
	void VulkanRHIComputePipeline::Unbind() const {}

	void VulkanRHIComputePipeline::Dispatch(uint32_t groupsX, uint32_t groupsY, uint32_t groupsZ) const {
		// Dispatch is done via command buffer
	}

	void VulkanRHIComputePipeline::GetWorkGroupSize(uint32_t& x, uint32_t& y, uint32_t& z) const {
		if (m_Desc.Shader) {
			m_Desc.Shader->GetWorkGroupSize(x, y, z);
		} else {
			x = y = z = 1;
		}
	}

	void VulkanRHIComputePipeline::OnDebugNameChanged() {}

} // namespace RHI
} // namespace Lunex
