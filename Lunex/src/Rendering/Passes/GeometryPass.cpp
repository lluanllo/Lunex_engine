#include "stpch.h"
#include "GeometryPass.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Log/Log.h"
#include "Renderer/Renderer3D.h"

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
			return;
		}
		
		// For now, delegate to existing Renderer3D which already works well
		// This will be replaced with pure RHI rendering in the future
		
		Scene* scene = sceneInfo.ScenePtr;
		const ViewInfo& view = sceneInfo.View;
		
		// Use existing Renderer3D for mesh rendering
		if (view.IsEditorCamera) {
			// We need to reconstruct EditorCamera - for now just use view matrices
			// TODO: Pass EditorCamera directly or use view matrices
		}
		
		// Collect and render meshes using existing system
		auto meshView = scene->GetAllEntitiesWith<TransformComponent, MeshComponent>();
		
		for (auto entityID : meshView) {
			Entity entity = { entityID, scene };
			
			auto& transform = entity.GetComponent<TransformComponent>();
			auto& mesh = entity.GetComponent<MeshComponent>();
			
			glm::mat4 worldTransform = scene->GetWorldTransform(entity);
			
			// Use existing Renderer3D
			if (entity.HasComponent<MaterialComponent>()) {
				auto& material = entity.GetComponent<MaterialComponent>();
				if (entity.HasComponent<TextureComponent>()) {
					auto& texture = entity.GetComponent<TextureComponent>();
					Renderer3D::DrawMesh(worldTransform, mesh, material, texture, static_cast<int>(entityID));
				} else {
					Renderer3D::DrawMesh(worldTransform, mesh, material, static_cast<int>(entityID));
				}
			} else {
				Renderer3D::DrawMesh(worldTransform, mesh, static_cast<int>(entityID));
			}
		}
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
			
			// TODO: Frustum culling here
			// TODO: Create draw commands for pure RHI rendering
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
		
		// TODO: Implement transparent rendering with proper sorting
		// For now, transparent objects are handled by the existing system
		
		Scene* scene = sceneInfo.ScenePtr;
		
		// Collect sprites and other transparent objects
		auto spriteView = scene->GetAllEntitiesWith<TransformComponent, SpriteRendererComponent>();
		
		for (auto entityID : spriteView) {
			Entity entity = { entityID, scene };
			auto& transform = entity.GetComponent<TransformComponent>();
			auto& sprite = entity.GetComponent<SpriteRendererComponent>();
			
			glm::mat4 worldTransform = scene->GetWorldTransform(entity);
			
			// Sprites are rendered via Renderer2D in the existing system
			// TODO: Add to transparent draw list for proper sorting
		}
	}

} // namespace Lunex
