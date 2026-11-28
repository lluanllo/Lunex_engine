#pragma once

/**
 * @file VulkanState.h
 * @brief GL-style binding state for the Vulkan backend.
 *
 * The engine binds resources OpenGL style (texture units, uniform buffer binding points,
 * "current" shader / framebuffer). The Vulkan command list reads this state at draw time
 * to build descriptor sets and pick pipelines.
 */

#include <cstdint>

namespace Lunex::RHI {
	class RHITexture;
	class RHIBuffer;
}

namespace Lunex::RHI::Vk {

	constexpr uint32_t kMaxTextureUnits = 48;
	constexpr uint32_t kMaxBufferBindings = 32;

	struct BufferBinding {
		const RHIBuffer* Buffer = nullptr;
		uint64_t Offset = 0;
		uint64_t Range = 0;   // 0 = whole buffer
	};

	struct BindingState {
		const RHITexture* Textures[kMaxTextureUnits] = {};
		const RHITexture* StorageImages[kMaxTextureUnits] = {};
		uint32_t StorageImageMips[kMaxTextureUnits] = {};
		BufferBinding UniformBuffers[kMaxBufferBindings];
		BufferBinding StorageBuffers[kMaxBufferBindings];
	};

	BindingState& GetBindingState();

}
