#include "stpch.h"
#include "DrawCommand.h"
#include "Log/Log.h"
#include <algorithm>

namespace Lunex {

	// ============================================================================
	// DRAW COMMAND IMPLEMENTATION
	// ============================================================================
	
	void DrawCommand::Execute(RHI::RHICommandList* cmdList) const {
		if (!IsValid() || !cmdList) return;
		
		// Bind pipeline
		cmdList->SetPipeline(Material.Pipeline.get());
		
		// Bind vertex and index buffers
		cmdList->SetVertexBuffer(Mesh.VertexBuffer.get(), 0, 0);
		cmdList->SetIndexBuffer(Mesh.IndexBuffer.get(), 0);
		
		// Bind textures
		for (size_t i = 0; i < Material.Textures.size(); i++) {
			if (Material.Textures[i]) {
				auto sampler = i < Material.Samplers.size() ? Material.Samplers[i] : nullptr;
				cmdList->SetTextureAndSampler(Material.Textures[i].get(), sampler.get(), static_cast<uint32_t>(i));
			}
		}
		
		// Bind uniform buffers
		for (size_t i = 0; i < Material.UniformBuffers.size(); i++) {
			if (Material.UniformBuffers[i]) {
				cmdList->SetUniformBuffer(Material.UniformBuffers[i].get(), static_cast<uint32_t>(i), 
										  RHI::ShaderStage::AllGraphics);
			}
		}
		
		// Execute draw call
		RHI::DrawArgs args;
		args.IndexCount = Mesh.IndexCount;
		args.InstanceCount = Mesh.InstanceCount;
		args.FirstIndex = Mesh.FirstIndex;
		args.VertexOffset = Mesh.VertexOffset;
		args.FirstInstance = Mesh.FirstInstance;
		
		cmdList->DrawIndexed(args);
	}

	// ============================================================================
	// DRAW LIST IMPLEMENTATION
	// ============================================================================
	
	void DrawList::Sort(bool reverse) {
		if (m_Sorted) return;
		
		if (reverse) {
			// Back-to-front (for transparency)
			std::sort(m_Commands.begin(), m_Commands.end(), 
					 [](const DrawCommand& a, const DrawCommand& b) {
						 return a.SortKey > b.SortKey;
					 });
		} else {
			// Front-to-back (for opaque)
			std::sort(m_Commands.begin(), m_Commands.end(), 
					 [](const DrawCommand& a, const DrawCommand& b) {
						 return a.SortKey < b.SortKey;
					 });
		}
		
		m_Sorted = true;
	}
	
	uint32_t DrawList::CullAgainstFrustum(const glm::vec4 frustumPlanes[6]) {
		uint32_t culledCount = 0;
		
		auto it = std::remove_if(m_Commands.begin(), m_Commands.end(),
			[&frustumPlanes, &culledCount](const DrawCommand& cmd) {
				// Skip if no bounds set
				if (cmd.BoundsRadius <= 0.0f) return false;
				
				// Test sphere against all 6 frustum planes
				for (int i = 0; i < 6; i++) {
					float distance = glm::dot(glm::vec3(frustumPlanes[i]), cmd.BoundsCenter) + frustumPlanes[i].w;
					if (distance < -cmd.BoundsRadius) {
						// Sphere is completely outside this plane
						culledCount++;
						return true;
					}
				}
				return false;
			});
		
		m_Commands.erase(it, m_Commands.end());
		return culledCount;
	}
	
	uint32_t DrawList::BatchCommands() {
		if (m_Commands.empty()) return 0;
		
		// Sort first to group compatible commands
		Sort();
		
		uint32_t batchedCount = 0;
		m_BatchedCommands.clear();
		m_BatchedCommands.reserve(m_Commands.size());
		
		// Group commands by mesh + material combination
		// For now, just identify batches - actual instancing requires uniform buffers
		DrawCommand* currentBatch = nullptr;
		
		for (auto& cmd : m_Commands) {
			if (currentBatch && 
				cmd.Mesh.VertexBuffer == currentBatch->Mesh.VertexBuffer &&
				cmd.Mesh.IndexBuffer == currentBatch->Mesh.IndexBuffer &&
				cmd.Material.Pipeline == currentBatch->Material.Pipeline) {
				// Compatible with current batch - increment instance count
				currentBatch->Mesh.InstanceCount++;
				batchedCount++;
				// Note: In a full implementation, we'd also store instance transforms
				// in a per-instance buffer for GPU instancing
			} else {
				// Start new batch
				m_BatchedCommands.push_back(cmd);
				currentBatch = &m_BatchedCommands.back();
			}
		}
		
		// Swap batched commands back
		if (batchedCount > 0) {
			m_Commands = std::move(m_BatchedCommands);
			m_BatchedCommands.clear();
		}
		
		return batchedCount;
	}
	
	void DrawList::Execute(RHI::RHICommandList* cmdList) {
		if (!cmdList) return;
		
		// Sort if needed
		if (!m_Sorted) {
			Sort();
		}
		
		// Track state to minimize state changes
		RHI::RHIGraphicsPipeline* lastPipeline = nullptr;
		
		for (const auto& cmd : m_Commands) {
			if (!cmd.IsValid()) continue;
			
			// Only bind pipeline if it changed
			if (cmd.Material.Pipeline.get() != lastPipeline) {
				cmd.Execute(cmdList);
				lastPipeline = cmd.Material.Pipeline.get();
			} else {
				// Pipeline already bound, just execute geometry bindings and draw
				cmdList->SetVertexBuffer(cmd.Mesh.VertexBuffer.get(), 0, 0);
				cmdList->SetIndexBuffer(cmd.Mesh.IndexBuffer.get(), 0);
				
				// Textures (could be batched better)
				for (size_t i = 0; i < cmd.Material.Textures.size(); i++) {
					if (cmd.Material.Textures[i]) {
						auto sampler = i < cmd.Material.Samplers.size() ? cmd.Material.Samplers[i] : nullptr;
						cmdList->SetTextureAndSampler(cmd.Material.Textures[i].get(), sampler.get(), static_cast<uint32_t>(i));
					}
				}
				
				RHI::DrawArgs args;
				args.IndexCount = cmd.Mesh.IndexCount;
				args.InstanceCount = cmd.Mesh.InstanceCount;
				args.FirstIndex = cmd.Mesh.FirstIndex;
				args.VertexOffset = cmd.Mesh.VertexOffset;
				args.FirstInstance = cmd.Mesh.FirstInstance;
				
				cmdList->DrawIndexed(args);
			}
		}
	}
	
	void DrawList::Execute(RHI::RHICommandList* cmdList, DrawStatistics& stats) {
		if (!cmdList) return;
		
		stats.TotalDrawCalls = static_cast<uint32_t>(m_Commands.size());
		
		// Sort if needed
		if (!m_Sorted) {
			Sort();
		}
		
		// Track state to minimize state changes
		RHI::RHIGraphicsPipeline* lastPipeline = nullptr;
		
		for (const auto& cmd : m_Commands) {
			if (!cmd.IsValid()) {
				stats.DrawCallsCulled++;
				continue;
			}
			
			// Only bind pipeline if it changed
			if (cmd.Material.Pipeline.get() != lastPipeline) {
				stats.PipelineChanges++;
				cmdList->SetPipeline(cmd.Material.Pipeline.get());
				lastPipeline = cmd.Material.Pipeline.get();
			}
			
			// Bind buffers
			cmdList->SetVertexBuffer(cmd.Mesh.VertexBuffer.get(), 0, 0);
			cmdList->SetIndexBuffer(cmd.Mesh.IndexBuffer.get(), 0);
			stats.BufferBinds += 2;
			
			// Bind textures
			for (size_t i = 0; i < cmd.Material.Textures.size(); i++) {
				if (cmd.Material.Textures[i]) {
					auto sampler = i < cmd.Material.Samplers.size() ? cmd.Material.Samplers[i] : nullptr;
					cmdList->SetTextureAndSampler(cmd.Material.Textures[i].get(), sampler.get(), static_cast<uint32_t>(i));
					stats.TextureBinds++;
				}
			}
			
			// Draw
			RHI::DrawArgs args;
			args.IndexCount = cmd.Mesh.IndexCount;
			args.InstanceCount = cmd.Mesh.InstanceCount;
			args.FirstIndex = cmd.Mesh.FirstIndex;
			args.VertexOffset = cmd.Mesh.VertexOffset;
			args.FirstInstance = cmd.Mesh.FirstInstance;
			
			cmdList->DrawIndexed(args);
			
			stats.DrawCallsExecuted++;
			stats.TrianglesDrawn += (cmd.Mesh.IndexCount / 3) * cmd.Mesh.InstanceCount;
			
			if (cmd.Mesh.InstanceCount > 1) {
				stats.DrawCallsBatched++;
			}
		}
	}

	// ============================================================================
	// DRAW LIST BUILDER IMPLEMENTATION
	// ============================================================================
	
	void DrawListBuilder::Begin() {
		m_DrawList.Clear();
		m_DrawCallCounter = 0;
		m_TranslucencyType = TranslucencyType::Opaque;
		m_CurrentPipeline = nullptr;
		m_CurrentTextures.clear();
		m_CurrentSamplers.clear();
		m_CurrentUniformBuffers.clear();
	}
	
	void DrawListBuilder::AddMesh(
		const glm::mat4& transform,
		Ref<RHI::RHIBuffer> vertexBuffer,
		Ref<RHI::RHIBuffer> indexBuffer,
		uint32_t indexCount,
		Ref<RHI::RHIGraphicsPipeline> pipeline,
		int entityID)
	{
		if (!vertexBuffer || !indexBuffer || !pipeline) {
			LNX_LOG_WARN("DrawListBuilder::AddMesh - Invalid mesh data");
			return;
		}
		
		DrawCommand cmd;
		
		// Mesh data
		cmd.Mesh.VertexBuffer = vertexBuffer;
		cmd.Mesh.IndexBuffer = indexBuffer;
		cmd.Mesh.IndexCount = indexCount;
		cmd.Mesh.InstanceCount = 1;
		cmd.Mesh.IndexType = RHI::IndexType::UInt32;
		
		// Material data
		cmd.Material.Pipeline = pipeline;
		cmd.Material.Textures = m_CurrentTextures;
		cmd.Material.Samplers = m_CurrentSamplers;
		cmd.Material.UniformBuffers = m_CurrentUniformBuffers;
		
		// Instance data
		cmd.Transform = transform;
		cmd.EntityID = entityID;
		
		// Generate sort key
		// For now, simple material-based sorting
		uint16_t materialID = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(pipeline.get()) & 0xFFFF);
		uint16_t meshID = static_cast<uint16_t>(reinterpret_cast<uintptr_t>(vertexBuffer.get()) & 0xFFFF);
		
		// Depth could be calculated from camera distance
		uint16_t depth = 0;  // TODO: Calculate from transform and camera
		
		cmd.SortKey = DrawKey::Make(0, m_TranslucencyType, materialID, meshID, depth);
		cmd.DrawCallIndex = m_DrawCallCounter++;
		
		m_DrawList.AddDrawCommand(cmd);
	}
	
	void DrawListBuilder::SetTexture(uint32_t slot, Ref<RHI::RHITexture> texture, Ref<RHI::RHISampler> sampler) {
		// Resize if needed
		if (slot >= m_CurrentTextures.size()) {
			m_CurrentTextures.resize(slot + 1);
			m_CurrentSamplers.resize(slot + 1);
		}
		
		m_CurrentTextures[slot] = texture;
		m_CurrentSamplers[slot] = sampler;
	}
	
	void DrawListBuilder::SetUniformBuffer(uint32_t binding, Ref<RHI::RHIBuffer> buffer) {
		if (binding >= m_CurrentUniformBuffers.size()) {
			m_CurrentUniformBuffers.resize(binding + 1);
		}
		
		m_CurrentUniformBuffers[binding] = buffer;
	}
	
	DrawList DrawListBuilder::End() {
		return std::move(m_DrawList);
	}

} // namespace Lunex
