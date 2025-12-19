#include "stpch.h"
#include "GeometryPass.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Log/Log.h"

namespace Lunex {

	// ============================================================================
	// GEOMETRY PASS IMPLEMENTATION
	// ============================================================================
	
	void GeometryPass::Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) {
		builder.SetName("Geometry Pass");
		
		// Create or use existing color target
		if (!m_ColorTarget.IsValid()) {
			m_ColorTarget = builder.CreateTexture(
				RenderGraphTextureDesc::ScaledRenderTarget("GeometryColor", 1.0f, RHI::TextureFormat::RGBA8)
			);
		}
		
		// Create or use existing depth target
		if (!m_DepthTarget.IsValid()) {
			m_DepthTarget = builder.CreateTexture(
				RenderGraphTextureDesc::ScaledRenderTarget("GeometryDepth", 1.0f, RHI::TextureFormat::Depth24Stencil8)
			);
		}
		
		// Declare as render targets
		builder.SetRenderTarget(m_ColorTarget, 0);
		builder.SetDepthTarget(m_DepthTarget);
		
		// Write to both
		builder.WriteTexture(m_ColorTarget);
		builder.WriteTexture(m_DepthTarget);
	}
	
	void GeometryPass::Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) {
		if (!sceneInfo.ScenePtr) {
			LNX_LOG_WARN("GeometryPass: No scene provided");
			return;
		}
		
		auto* cmdList = resources.GetCommandList();
		if (!cmdList) return;
		
		// Collect draw commands
		DrawList drawList;
		CollectDrawCommands(sceneInfo, drawList);
		
		if (drawList.IsEmpty()) {
			return;  // Nothing to render
		}
		
		// Begin render pass
		RHI::RenderPassBeginInfo passInfo;
		passInfo.Framebuffer = resources.GetRenderTarget().get();
		passInfo.ClearColor = true;
		passInfo.ClearDepth = true;
		passInfo.ClearValues.push_back(RHI::ClearValue::ColorValue(0.1f, 0.1f, 0.1f, 1.0f));
		passInfo.RenderViewport.Width = static_cast<float>(sceneInfo.View.ViewportWidth);
		passInfo.RenderViewport.Height = static_cast<float>(sceneInfo.View.ViewportHeight);
		
		cmdList->BeginRenderPass(passInfo);
		
		// Setup camera uniforms
		SetupUniforms(cmdList, sceneInfo.View);
		
		// Execute draw list (will sort and batch automatically)
		drawList.Execute(cmdList);
		
		cmdList->EndRenderPass();
	}
	
	void GeometryPass::CollectDrawCommands(const SceneRenderInfo& sceneInfo, DrawList& outDrawList) {
		Scene* scene = sceneInfo.ScenePtr;
		if (!scene) return;
		
		// Get all entities with mesh components
		auto view = scene->GetAllEntitiesWith<TransformComponent, MeshComponent>();
		
		for (auto entityID : view) {
			Entity entity = { entityID, scene };
			
			auto& transform = entity.GetComponent<TransformComponent>();
			auto& mesh = entity.GetComponent<MeshComponent>();
			
			// Skip if no mesh
			if (!mesh.MeshAsset) continue;
			
			// TODO: Frustum culling here
			
			// For now, create a simple draw command
			// In a real implementation, this would:
			// 1. Get vertex/index buffers from mesh
			// 2. Get material/pipeline from mesh or material component
			// 3. Calculate sort key based on distance to camera
			// 4. Add to draw list
			
			// Placeholder: We need to integrate with the existing Model/Mesh system
			LNX_LOG_TRACE("GeometryPass: Would render mesh for entity {0}", entity.GetUUID());
		}
	}
	
	void GeometryPass::SetupUniforms(RHI::RHICommandList* cmdList, const ViewInfo& view) {
		// Create camera uniform buffer if needed
		if (!m_CameraUniformBuffer) {
			RHI::BufferDesc desc;
			desc.Type = RHI::BufferType::Uniform;
			desc.Usage = RHI::BufferUsage::Dynamic;
			desc.Size = sizeof(glm::mat4) * 3 + sizeof(glm::vec4);  // VP, View, Proj, CamPos
			
			// TODO: Create through RHIDevice when available
			// m_CameraUniformBuffer = RHI::RHIDevice::Get()->CreateUniformBuffer(desc.Size, desc.Usage);
		}
		
		// Update camera data
		if (m_CameraUniformBuffer) {
			struct CameraData {
				glm::mat4 ViewProjection;
				glm::mat4 View;
				glm::mat4 Projection;
				glm::vec4 CameraPosition;
			} cameraData;
			
			cameraData.ViewProjection = view.ViewProjectionMatrix;
			cameraData.View = view.ViewMatrix;
			cameraData.Projection = view.ProjectionMatrix;
			cameraData.CameraPosition = glm::vec4(view.CameraPosition, 1.0f);
			
			m_CameraUniformBuffer->SetData(&cameraData, sizeof(CameraData));
			
			// Bind to shader binding point 0
			cmdList->SetUniformBuffer(m_CameraUniformBuffer.get(), 0, RHI::ShaderStage::AllGraphics);
		}
	}

	// ============================================================================
	// TRANSPARENT PASS IMPLEMENTATION
	// ============================================================================
	
	void TransparentPass::Setup(RenderGraph& graph, RenderPassBuilder& builder, const SceneRenderInfo& sceneInfo) {
		builder.SetName("Transparent Pass");
		
		// Read from depth (no write)
		builder.ReadTexture(m_DepthTarget);
		
		// Write to color (blend)
		builder.SetRenderTarget(m_ColorTarget, 0);
		builder.WriteTexture(m_ColorTarget);
	}
	
	void TransparentPass::Execute(const RenderPassResources& resources, const SceneRenderInfo& sceneInfo) {
		if (!sceneInfo.ScenePtr) return;
		
		auto* cmdList = resources.GetCommandList();
		if (!cmdList) return;
		
		// TODO: Implement transparent rendering
		// - Collect transparent meshes
		// - Sort back-to-front
		// - Render with alpha blending enabled
		
		RHI::RenderPassBeginInfo passInfo;
		passInfo.Framebuffer = resources.GetRenderTarget().get();
		passInfo.ClearColor = false;  // Don't clear, blend on top
		passInfo.ClearDepth = false;
		
		cmdList->BeginRenderPass(passInfo);
		
		// Render transparent objects here
		
		cmdList->EndRenderPass();
	}

} // namespace Lunex
