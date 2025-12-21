#include "stpch.h"
#include "RenderGraph.h"
#include "Log/Log.h"
#include <algorithm>
#include <queue>
#include <sstream>

namespace Lunex {

	// ============================================================================
	// INTERNAL DATA STRUCTURES
	// ============================================================================
	
	struct RenderGraph::ResourceNode {
		uint32_t ID;
		std::string Name;
		RenderGraphResourceType Type;
		
		// For transient resources
		RenderGraphTextureDesc TextureDesc;
		RenderGraphBufferDesc BufferDesc;
		
		// For imported resources
		Ref<RHI::RHITexture2D> ImportedTexture;
		Ref<RHI::RHIBuffer> ImportedBuffer;
		
		// Actual allocated resource (after compilation)
		Ref<RHI::RHITexture2D> AllocatedTexture;
		Ref<RHI::RHIBuffer> AllocatedBuffer;
		
		// Dependency tracking
		std::vector<uint32_t> ProducerPasses;  // Passes that write to this
		std::vector<uint32_t> ConsumerPasses;  // Passes that read from this
		
		bool IsImported = false;
		bool IsBackbuffer = false;
		uint32_t FirstUse = UINT32_MAX;
		uint32_t LastUse = 0;
	};

	struct RenderGraph::PassNode {
		uint32_t ID;
		std::string Name;
		PassType Type;
		
		RenderPassSetupFunc SetupFunc;
		RenderPassExecuteFunc ExecuteFunc;
		
		// Resources
		std::vector<RenderGraphResource> ColorTargets;
		RenderGraphResource DepthTarget;
		std::vector<RenderGraphResource> ReadResources;
		std::vector<RenderGraphResource> WriteResources;
		
		// Dependency tracking
		std::vector<uint32_t> DependsOn;  // Passes this depends on
		
		bool Culled = false;
		uint32_t ExecutionOrder = UINT32_MAX;
	};

	// ============================================================================
	// RENDER GRAPH IMPLEMENTATION
	// ============================================================================
	
	RenderGraph::RenderGraph() {
		m_CommandList = RHI::RHICommandList::CreateGraphics();
	}
	
	RenderGraph::~RenderGraph() = default;
	
	RenderGraphResource RenderGraph::ImportTexture(const std::string& name, Ref<RHI::RHITexture2D> texture) {
		auto node = CreateScope<ResourceNode>();
		node->ID = static_cast<uint32_t>(m_Resources.size());
		node->Name = name;
		node->Type = RenderGraphResourceType::External;
		node->IsImported = true;
		node->ImportedTexture = texture;
		node->AllocatedTexture = texture;
		
		m_ResourceNameMap[name] = node->ID;
		
		RenderGraphResource handle(node->ID, RenderGraphResourceType::Texture);
		m_Resources.push_back(std::move(node));
		
		return handle;
	}
	
	RenderGraphResource RenderGraph::ImportBuffer(const std::string& name, Ref<RHI::RHIBuffer> buffer) {
		auto node = CreateScope<ResourceNode>();
		node->ID = static_cast<uint32_t>(m_Resources.size());
		node->Name = name;
		node->Type = RenderGraphResourceType::External;
		node->IsImported = true;
		node->ImportedBuffer = buffer;
		node->AllocatedBuffer = buffer;
		
		m_ResourceNameMap[name] = node->ID;
		
		RenderGraphResource handle(node->ID, RenderGraphResourceType::Buffer);
		m_Resources.push_back(std::move(node));
		
		return handle;
	}
	
	void RenderGraph::SetBackbufferSource(RenderGraphResource handle) {
		m_BackbufferSource = handle;
		if (handle.IsValid() && handle.GetID() < m_Resources.size()) {
			m_Resources[handle.GetID()]->IsBackbuffer = true;
		}
	}
	
	void RenderGraph::AddPass(const std::string& name,
							  const RenderPassSetupFunc& setup,
							  const RenderPassExecuteFunc& execute) {
		auto pass = CreateScope<PassNode>();
		pass->ID = static_cast<uint32_t>(m_Passes.size());
		pass->Name = name;
		pass->Type = PassType::Graphics;
		pass->SetupFunc = setup;
		pass->ExecuteFunc = execute;
		
		// Run setup to declare resources
		RenderPassBuilder builder;
		builder.m_Graph = this;
		builder.m_Pass = static_cast<void*>(pass.get());
		builder.SetName(name);
		
		if (setup) {
			setup(builder);
		}
		
		// Store builder results
		pass->ColorTargets = builder.m_ColorTargets;
		pass->DepthTarget = builder.m_DepthTarget;
		pass->ReadResources = builder.m_Reads;
		pass->WriteResources = builder.m_Writes;
		
		m_Passes.push_back(std::move(pass));
	}
	
	void RenderGraph::AddComputePass(const std::string& name,
									const RenderPassSetupFunc& setup,
									const RenderPassExecuteFunc& execute) {
		auto pass = CreateScope<PassNode>();
		pass->ID = static_cast<uint32_t>(m_Passes.size());
		pass->Name = name;
		pass->Type = PassType::Compute;
		pass->SetupFunc = setup;
		pass->ExecuteFunc = execute;
		
		RenderPassBuilder builder;
		builder.m_Graph = this;
		builder.m_Pass = static_cast<void*>(pass.get());
		builder.SetName(name);
		
		if (setup) {
			setup(builder);
		}
		
		pass->ReadResources = builder.m_Reads;
		pass->WriteResources = builder.m_Writes;
		
		m_Passes.push_back(std::move(pass));
	}
	
	void RenderGraph::AddCopyPass(const std::string& name,
								  const RenderPassSetupFunc& setup,
								  const RenderPassExecuteFunc& execute) {
		AddComputePass(name, setup, execute);  // Same logic for now
		m_Passes.back()->Type = PassType::Copy;
	}
	
	void RenderGraph::Compile() {
		LNX_LOG_INFO("RenderGraph: Compiling graph with {0} passes and {1} resources",
					m_Passes.size(), m_Resources.size());
		
		CompilePasses();
		
		if (m_EnablePassCulling) {
			CullUnusedPasses();
		}
		
		AllocateResources();
		
		m_Compiled = true;
		
		LNX_LOG_INFO("RenderGraph: Compilation complete - {0} passes, {1} culled, {2} resources",
					m_Stats.ExecutedPasses, m_Stats.CulledPasses, m_Stats.TransientResources);
	}
	
	void RenderGraph::CompilePasses() {
		// Build dependency graph
		for (auto& pass : m_Passes) {
			// Check reads - depend on passes that write these resources
			for (auto& readRes : pass->ReadResources) {
				if (readRes.IsValid() && readRes.GetID() < m_Resources.size()) {
					auto& resNode = m_Resources[readRes.GetID()];
					resNode->ConsumerPasses.push_back(pass->ID);
					
					// Add dependency on producer passes
					for (auto producerID : resNode->ProducerPasses) {
						if (std::find(pass->DependsOn.begin(), pass->DependsOn.end(), producerID) == pass->DependsOn.end()) {
							pass->DependsOn.push_back(producerID);
						}
					}
				}
			}
			
			// Check writes - this pass produces these resources
			for (auto& writeRes : pass->WriteResources) {
				if (writeRes.IsValid() && writeRes.GetID() < m_Resources.size()) {
					auto& resNode = m_Resources[writeRes.GetID()];
					resNode->ProducerPasses.push_back(pass->ID);
				}
			}
			
			// Color targets are both read and write
			for (auto& rt : pass->ColorTargets) {
				if (rt.IsValid() && rt.GetID() < m_Resources.size()) {
					auto& resNode = m_Resources[rt.GetID()];
					resNode->ProducerPasses.push_back(pass->ID);
					resNode->ConsumerPasses.push_back(pass->ID);
				}
			}
			
			// Depth target
			if (pass->DepthTarget.IsValid()) {
				auto& resNode = m_Resources[pass->DepthTarget.GetID()];
				resNode->ProducerPasses.push_back(pass->ID);
				resNode->ConsumerPasses.push_back(pass->ID);
			}
		}
		
		// Topological sort for execution order
		std::vector<uint32_t> inDegree(m_Passes.size(), 0);
		for (auto& pass : m_Passes) {
			inDegree[pass->ID] = static_cast<uint32_t>(pass->DependsOn.size());
		}
		
		std::queue<uint32_t> ready;
		for (size_t i = 0; i < m_Passes.size(); i++) {
			if (inDegree[i] == 0) {
				ready.push(static_cast<uint32_t>(i));
			}
		}
		
		uint32_t order = 0;
		while (!ready.empty()) {
			uint32_t passID = ready.front();
			ready.pop();
			
			m_Passes[passID]->ExecutionOrder = order++;
			
			// Update dependent passes
			for (auto& otherPass : m_Passes) {
				if (std::find(otherPass->DependsOn.begin(), otherPass->DependsOn.end(), passID) != otherPass->DependsOn.end()) {
					inDegree[otherPass->ID]--;
					if (inDegree[otherPass->ID] == 0) {
						ready.push(otherPass->ID);
					}
				}
			}
		}
	}
	
	void RenderGraph::CullUnusedPasses() {
		// Mark all passes as potentially cullable
		for (auto& pass : m_Passes) {
			pass->Culled = true;
		}
		
		// Traverse backwards from backbuffer source
		if (m_BackbufferSource.IsValid()) {
			std::queue<uint32_t> toVisit;
			auto& backbufferRes = m_Resources[m_BackbufferSource.GetID()];
			
			for (auto producerID : backbufferRes->ProducerPasses) {
				toVisit.push(producerID);
			}
			
			while (!toVisit.empty()) {
				uint32_t passID = toVisit.front();
				toVisit.pop();
				
				if (passID >= m_Passes.size()) continue;
				
				auto& pass = m_Passes[passID];
				if (!pass->Culled) continue;  // Already visited
				
				pass->Culled = false;  // Keep this pass
				
				// Visit dependencies
				for (auto depID : pass->DependsOn) {
					if (depID < m_Passes.size() && m_Passes[depID]->Culled) {
						toVisit.push(depID);
					}
				}
			}
		}
		
		// Count statistics
		m_Stats.TotalPasses = static_cast<uint32_t>(m_Passes.size());
		m_Stats.CulledPasses = 0;
		m_Stats.ExecutedPasses = 0;
		
		for (auto& pass : m_Passes) {
			if (pass->Culled) {
				m_Stats.CulledPasses++;
			} else {
				m_Stats.ExecutedPasses++;
			}
		}
	}
	
	// ============================================================================
	// RESOURCE LOOKUP
	// ============================================================================
	
	RenderGraphResource RenderGraph::GetResource(const std::string& name) const {
		auto it = m_ResourceNameMap.find(name);
		if (it != m_ResourceNameMap.end()) {
			auto& node = m_Resources[it->second];
			return RenderGraphResource(it->second, node->Type);
		}
		return RenderGraphResource();
	}
	
	bool RenderGraph::HasResource(const std::string& name) const {
		return m_ResourceNameMap.find(name) != m_ResourceNameMap.end();
	}
	
	// ============================================================================
	// RESOURCE POOLING (AAA-level memory management)
	// ============================================================================
	
	Ref<RHI::RHITexture2D> RenderGraph::AcquirePooledTexture(const RenderGraphTextureDesc& desc) {
		if (!m_EnableResourcePooling) {
			// Create new texture directly
			RHI::TextureDesc texDesc;
			if (desc.Policy == RenderGraphTextureDesc::SizePolicy::ScaleToSwapchain) {
				texDesc.Width = static_cast<uint32_t>(m_SwapchainWidth * desc.ScaleX);
				texDesc.Height = static_cast<uint32_t>(m_SwapchainHeight * desc.ScaleY);
			} else {
				texDesc.Width = desc.Width;
				texDesc.Height = desc.Height;
			}
			texDesc.Format = desc.Format;
			texDesc.MipLevels = desc.MipLevels;
			texDesc.IsRenderTarget = desc.IsRenderTarget;
			texDesc.DebugName = desc.DebugName;
			
			m_PoolStats.PoolMisses++;
			return RHI::RHITexture2D::Create(texDesc);
		}
		
		// Calculate target dimensions
		uint32_t targetWidth, targetHeight;
		if (desc.Policy == RenderGraphTextureDesc::SizePolicy::ScaleToSwapchain) {
			targetWidth = static_cast<uint32_t>(m_SwapchainWidth * desc.ScaleX);
			targetHeight = static_cast<uint32_t>(m_SwapchainHeight * desc.ScaleY);
		} else {
			targetWidth = desc.Width;
			targetHeight = desc.Height;
		}
		
		// Look for compatible texture in pool
		for (auto& pooled : m_TexturePool) {
			if (pooled.Texture && 
				pooled.Desc.Format == desc.Format &&
				pooled.Desc.MipLevels == desc.MipLevels &&
				pooled.Desc.IsRenderTarget == desc.IsRenderTarget) {
				
				// Check size match (allow exact or larger)
				uint32_t pooledWidth, pooledHeight;
				if (pooled.Desc.Policy == RenderGraphTextureDesc::SizePolicy::ScaleToSwapchain) {
					pooledWidth = static_cast<uint32_t>(m_SwapchainWidth * pooled.Desc.ScaleX);
					pooledHeight = static_cast<uint32_t>(m_SwapchainHeight * pooled.Desc.ScaleY);
				} else {
					pooledWidth = pooled.Desc.Width;
					pooledHeight = pooled.Desc.Height;
				}
				
				if (pooledWidth == targetWidth && pooledHeight == targetHeight) {
					// Found compatible texture
					Ref<RHI::RHITexture2D> result = pooled.Texture;
					pooled.Texture = nullptr;  // Mark as used
					pooled.LastUsedFrame = m_CurrentFrame;
					m_PoolStats.PoolHits++;
					return result;
				}
			}
		}
		
		// No compatible texture found, create new one
		RHI::TextureDesc texDesc;
		texDesc.Width = targetWidth;
		texDesc.Height = targetHeight;
		texDesc.Format = desc.Format;
		texDesc.MipLevels = desc.MipLevels;
		texDesc.IsRenderTarget = desc.IsRenderTarget;
		texDesc.DebugName = desc.DebugName;
		
		m_PoolStats.PoolMisses++;
		return RHI::RHITexture2D::Create(texDesc);
	}
	
	void RenderGraph::ReleasePooledTextures() {
		// Return allocated textures to pool for next frame
		for (auto& resNode : m_Resources) {
			if (resNode->IsImported) continue;
			
			if (resNode->AllocatedTexture) {
				// Add to pool
				PooledTexture pooled;
				pooled.Texture = resNode->AllocatedTexture;
				pooled.Desc = resNode->TextureDesc;
				pooled.LastUsedFrame = m_CurrentFrame;
				
				m_TexturePool.push_back(pooled);
				resNode->AllocatedTexture = nullptr;
			}
		}
		
		// Update pool statistics
		m_PoolStats.PooledTextures = 0;
		m_PoolStats.PooledMemory = 0;
		for (const auto& pooled : m_TexturePool) {
			if (pooled.Texture) {
				m_PoolStats.PooledTextures++;
				m_PoolStats.PooledMemory += pooled.Texture->GetGPUMemorySize();
			}
		}
		
		// Cleanup old textures (not used for 3 frames)
		const uint64_t maxAge = 3;
		m_TexturePool.erase(
			std::remove_if(m_TexturePool.begin(), m_TexturePool.end(),
				[this, maxAge](const PooledTexture& p) {
					return p.Texture == nullptr || 
						   (m_CurrentFrame - p.LastUsedFrame) > maxAge;
				}),
			m_TexturePool.end()
		);
	}
	
	void RenderGraph::ClearResourcePool() {
		m_TexturePool.clear();
		m_BufferPool.clear();
		m_PoolStats = PoolStatistics{};
		LNX_LOG_INFO("RenderGraph: Resource pool cleared");
	}
	
	void RenderGraph::AllocateResources() {
		m_Stats.TransientResources = 0;
		m_Stats.TransientMemoryUsage = 0;
		
		for (auto& resNode : m_Resources) {
			if (resNode->IsImported) continue;  // Already allocated
			
			if (resNode->Type == RenderGraphResourceType::Texture) {
				// Use pooled allocation
				resNode->AllocatedTexture = AcquirePooledTexture(resNode->TextureDesc);
				
				if (resNode->AllocatedTexture) {
					m_Stats.TransientResources++;
					m_Stats.TransientMemoryUsage += resNode->AllocatedTexture->GetGPUMemorySize();
				}
				
			} else if (resNode->Type == RenderGraphResourceType::Buffer) {
				RHI::BufferDesc bufDesc;
				bufDesc.Type = resNode->BufferDesc.Type;
				bufDesc.Usage = resNode->BufferDesc.Usage;
				bufDesc.Size = resNode->BufferDesc.Size;
				
				// TODO: Implement buffer pooling similar to textures
				
				m_Stats.TransientResources++;
			}
		}
	}
	
	void RenderGraph::Execute() {
		if (!m_Compiled) {
			LNX_LOG_ERROR("RenderGraph: Cannot execute - graph not compiled!");
			return;
		}
		
		// Sort passes by execution order
		std::vector<PassNode*> sortedPasses;
		for (auto& pass : m_Passes) {
			if (!pass->Culled) {
				sortedPasses.push_back(pass.get());
			}
		}
		
		std::sort(sortedPasses.begin(), sortedPasses.end(), 
				 [](PassNode* a, PassNode* b) { return a->ExecutionOrder < b->ExecutionOrder; });
		
		// Execute passes
		m_CommandList->Begin();
		
		for (auto* pass : sortedPasses) {
			ExecutePass(pass);
		}
		
		m_CommandList->End();
	}
	
	void RenderGraph::ExecutePass(PassNode* pass) {
		if (!pass || !pass->ExecuteFunc) return;
		
		RHI_SCOPED_EVENT(m_CommandList.get(), pass->Name);
		
		// Build resources for this pass
		RenderPassResources resources;
		resources.m_CommandList = m_CommandList.get();
		
		// Collect textures
		for (auto& res : pass->ReadResources) {
			if (res.IsValid() && res.GetID() < m_Resources.size()) {
				auto& node = m_Resources[res.GetID()];
				if (node->AllocatedTexture) {
					resources.m_Textures[res.GetID()] = node->AllocatedTexture;
				}
			}
		}
		
		for (auto& res : pass->WriteResources) {
			if (res.IsValid() && res.GetID() < m_Resources.size()) {
				auto& node = m_Resources[res.GetID()];
				if (node->AllocatedTexture) {
					resources.m_Textures[res.GetID()] = node->AllocatedTexture;
				}
			}
		}
		
		// Create framebuffer for graphics passes
		if (pass->Type == PassType::Graphics && !pass->ColorTargets.empty()) {
			RHI::FramebufferDesc fbDesc;
			
			for (auto& rt : pass->ColorTargets) {
				if (rt.IsValid() && rt.GetID() < m_Resources.size()) {
					auto& node = m_Resources[rt.GetID()];
					if (node->AllocatedTexture) {
						fbDesc.AddColorTexture(node->AllocatedTexture);
						resources.m_Textures[rt.GetID()] = node->AllocatedTexture;
					}
				}
			}
			
			if (pass->DepthTarget.IsValid()) {
				auto& node = m_Resources[pass->DepthTarget.GetID()];
				if (node->AllocatedTexture) {
					fbDesc.SetDepthTexture(node->AllocatedTexture);
					resources.m_Textures[pass->DepthTarget.GetID()] = node->AllocatedTexture;
				}
			}
			
			if (!fbDesc.ColorAttachments.empty()) {
				fbDesc.SetName(pass->Name + "_Framebuffer");
				resources.m_RenderTarget = RHI::RHIFramebuffer::Create(fbDesc);
			}
		}
		
		// Execute pass
		pass->ExecuteFunc(resources);
	}
	
	void RenderGraph::Reset() {
		// Release textures back to pool before clearing
		if (m_EnableResourcePooling) {
			ReleasePooledTextures();
		}
		
		m_Passes.clear();
		m_Resources.clear();
		m_ResourceNameMap.clear();
		m_Compiled = false;
		m_BackbufferSource = RenderGraphResource();
		m_Stats = Statistics{};
		
		// Increment frame counter for pool management
		m_CurrentFrame++;
	}
	
	std::string RenderGraph::ExportGraphViz() const {
		std::stringstream ss;
		ss << "digraph RenderGraph {\n";
		ss << "  rankdir=LR;\n";
		ss << "  node [shape=box];\n\n";
		
		// Passes
		for (auto& pass : m_Passes) {
			std::string color = pass->Culled ? "gray" : "lightblue";
			ss << "  Pass" << pass->ID << " [label=\"" << pass->Name << "\", fillcolor=" << color << ", style=filled];\n";
		}
		
		// Resources
		for (auto& res : m_Resources) {
			std::string color = res->IsImported ? "lightgreen" : "lightyellow";
			std::string shape = "ellipse";
			ss << "  Res" << res->ID << " [label=\"" << res->Name << "\", fillcolor=" << color << ", style=filled, shape=" << shape << "];\n";
		}
		
		ss << "\n";
		
		// Edges
		for (auto& pass : m_Passes) {
			for (auto& read : pass->ReadResources) {
				if (read.IsValid()) {
					ss << "  Res" << read.GetID() << " -> Pass" << pass->ID << " [label=\"read\"];\n";
				}
			}
			for (auto& write : pass->WriteResources) {
				if (write.IsValid()) {
					ss << "  Pass" << pass->ID << " -> Res" << write.GetID() << " [label=\"write\"];\n";
				}
			}
		}
		
		ss << "}\n";
		return ss.str();
	}

	// ============================================================================
	// RENDER PASS BUILDER IMPLEMENTATION
	// ============================================================================
	
	RenderGraphResource RenderPassBuilder::CreateTexture(const RenderGraphTextureDesc& desc) {
		auto node = CreateScope<RenderGraph::ResourceNode>();
		node->ID = static_cast<uint32_t>(m_Graph->m_Resources.size());
		node->Name = desc.DebugName;
		node->Type = RenderGraphResourceType::Texture;
		node->TextureDesc = desc;
		
		RenderGraphResource handle(node->ID, RenderGraphResourceType::Texture);
		m_Graph->m_Resources.push_back(std::move(node));
		
		return handle;
	}
	
	RenderGraphResource RenderPassBuilder::CreateBuffer(const RenderGraphBufferDesc& desc) {
		auto node = CreateScope<RenderGraph::ResourceNode>();
		node->ID = static_cast<uint32_t>(m_Graph->m_Resources.size());
		node->Name = desc.DebugName;
		node->Type = RenderGraphResourceType::Buffer;
		node->BufferDesc = desc;
		
		RenderGraphResource handle(node->ID, RenderGraphResourceType::Buffer);
		m_Graph->m_Resources.push_back(std::move(node));
		
		return handle;
	}
	
	RenderGraphResource RenderPassBuilder::ReadTexture(RenderGraphResource handle) {
		m_Reads.push_back(handle);
		return handle;
	}
	
	RenderGraphResource RenderPassBuilder::WriteTexture(RenderGraphResource handle) {
		m_Writes.push_back(handle);
		return handle;
	}
	
	RenderGraphResource RenderPassBuilder::ReadBuffer(RenderGraphResource handle) {
		m_Reads.push_back(handle);
		return handle;
	}
	
	RenderGraphResource RenderPassBuilder::WriteBuffer(RenderGraphResource handle) {
		m_Writes.push_back(handle);
		return handle;
	}
	
	void RenderPassBuilder::SetRenderTarget(RenderGraphResource handle, uint32_t index) {
		if (index >= m_ColorTargets.size()) {
			m_ColorTargets.resize(index + 1);
		}
		m_ColorTargets[index] = handle;
	}
	
	void RenderPassBuilder::SetDepthTarget(RenderGraphResource handle) {
		m_DepthTarget = handle;
	}

	// ============================================================================
	// RENDER PASS RESOURCES IMPLEMENTATION
	// ============================================================================
	
	Ref<RHI::RHITexture2D> RenderPassResources::GetTexture(RenderGraphResource handle) const {
		auto it = m_Textures.find(handle.GetID());
		return it != m_Textures.end() ? it->second : nullptr;
	}
	
	Ref<RHI::RHIBuffer> RenderPassResources::GetBuffer(RenderGraphResource handle) const {
		auto it = m_Buffers.find(handle.GetID());
		return it != m_Buffers.end() ? it->second : nullptr;
	}

} // namespace Lunex
