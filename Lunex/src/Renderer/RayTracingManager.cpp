#include "stpch.h"
#include "RayTracingManager.h"
#include "Renderer/Renderer3D.h"
#include "Renderer/RenderCommand.h"
#include "Scene/Components.h"
#include "Scene/Entity.h"
#include <glad/glad.h>
#include <chrono>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/quaternion.hpp>

namespace Lunex {
	
	RayTracingManager::RayTracingManager() {
	}
	
	RayTracingManager::~RayTracingManager() {
		Shutdown();
	}
	
	// ============================================================================
	// INITIALIZATION
	// ============================================================================
	
	void RayTracingManager::Init(uint32_t width, uint32_t height) {
		LNX_PROFILE_FUNCTION();
		
		if (m_Initialized) {
			LNX_LOG_WARN("RayTracingManager already initialized");
			return;
		}
		
		m_Width = width;
		m_Height = height;
		
		LNX_LOG_INFO("RayTracingManager::Init - Resolution: {0}x{1}", width, height);
		
		// Create G-Buffer (Position + Normal + Depth)
		FramebufferSpecification gbufferSpec;
		gbufferSpec.Width = width;
		gbufferSpec.Height = height;
		gbufferSpec.Attachments = {
			FramebufferTextureFormat::RGBA16F,  // Position (world space)
			FramebufferTextureFormat::RGBA16F,  // Normal (world space)
			FramebufferTextureFormat::DEPTH32F  // Depth
		};
		m_GBuffer = Framebuffer::Create(gbufferSpec);
		
		// Create Shadow Buffer (for compute shader output)
		FramebufferSpecification shadowSpec;
		shadowSpec.Width = width;
		shadowSpec.Height = height;
		shadowSpec.Attachments = {
			FramebufferTextureFormat::RGBA16F  // Shadow factor
		};
		m_ShadowBuffer = Framebuffer::Create(shadowSpec);
		
		// Load compute shaders
		m_ShadowRayTracingShader = ComputeShader::Create("assets/shaders/compute/ShadowRayTracing.glsl");
		m_ShadowDenoiseShader = ComputeShader::Create("assets/shaders/compute/ShadowDenoise.glsl");
		
		// Create storage buffers (initially empty)
		m_TriangleBuffer = StorageBuffer::Create(sizeof(RTTriangle) * 1000); // Start with space for 1000 triangles
		m_BVHBuffer = StorageBuffer::Create(sizeof(RTBVHNode) * 2000);      // Space for 2000 nodes
		m_LightBuffer = StorageBuffer::Create(1024);                         // Light data buffer
		
		m_Initialized = true;
		LNX_LOG_INFO("RayTracingManager initialized successfully");
	}
	
	void RayTracingManager::Shutdown() {
		if (!m_Initialized) return;
		
		m_GBuffer.reset();
		m_ShadowBuffer.reset();
		m_ShadowRayTracingShader.reset();
		m_ShadowDenoiseShader.reset();
		m_TriangleBuffer.reset();
		m_BVHBuffer.reset();
		m_LightBuffer.reset();
		
		m_Initialized = false;
		LNX_LOG_INFO("RayTracingManager shutdown");
	}
	
	void RayTracingManager::Resize(uint32_t width, uint32_t height) {
		if (!m_Initialized || (width == m_Width && height == m_Height)) return;
		
		m_Width = width;
		m_Height = height;
		
		m_GBuffer->Resize(width, height);
		m_ShadowBuffer->Resize(width, height);
		
		LNX_LOG_INFO("RayTracingManager resized to {0}x{1}", width, height);
	}
	
	// ============================================================================
	// SCENE GEOMETRY UPDATE
	// ============================================================================
	
	void RayTracingManager::UpdateSceneGeometry(Scene* scene) {
		LNX_PROFILE_FUNCTION();
		
		if (!m_Initialized || !scene) return;
		
		auto startTime = std::chrono::high_resolution_clock::now();
		
		// Extract geometry from scene
		m_Geometry = GeometryExtractor::ExtractFromScene(scene);
		m_Stats.triangleCount = static_cast<uint32_t>(m_Geometry.triangles.size());
		
		if (m_Geometry.triangles.empty()) {
			LNX_LOG_WARN("RayTracingManager: No geometry to process");
			m_Stats.geometryDirty = false;
			return;
		}
		
		// Build BVH
		RebuildBVH();
		
		// Upload to GPU
		UploadGeometryToGPU();
		
		auto endTime = std::chrono::high_resolution_clock::now();
		float totalTime = std::chrono::duration<float, std::milli>(endTime - startTime).count();
		
		LNX_LOG_INFO("Scene geometry updated in {0:.2f}ms ({1} triangles, {2} BVH nodes)",
					 totalTime, m_Stats.triangleCount, m_Stats.bvhNodeCount);
		
		m_Stats.geometryDirty = false;
	}
	
	void RayTracingManager::RebuildBVH() {
		LNX_PROFILE_FUNCTION();
		
		m_BVHNodes = m_BVHBuilder.Build(m_Geometry);
		m_Stats.bvhNodeCount = m_BVHBuilder.GetStats().nodeCount;
		m_Stats.bvhBuildTime = m_BVHBuilder.GetStats().buildTimeMs;
	}
	
	void RayTracingManager::UploadGeometryToGPU() {
		LNX_PROFILE_FUNCTION();
		
		// Upload triangles
		uint32_t triangleSize = static_cast<uint32_t>(m_Geometry.triangles.size() * sizeof(RTTriangle));
		if (triangleSize > m_TriangleBuffer->GetSize()) {
			m_TriangleBuffer = StorageBuffer::Create(triangleSize * 2); // Double for future growth
		}
		m_TriangleBuffer->SetData(m_Geometry.triangles.data(), triangleSize);
		
		// Upload BVH
		uint32_t bvhSize = static_cast<uint32_t>(m_BVHNodes.size() * sizeof(RTBVHNode));
		if (bvhSize > m_BVHBuffer->GetSize()) {
			m_BVHBuffer = StorageBuffer::Create(bvhSize * 2);
		}
		m_BVHBuffer->SetData(m_BVHNodes.data(), bvhSize);
		
		LNX_LOG_INFO("Uploaded {0} triangles ({1} bytes) and {2} BVH nodes ({3} bytes) to GPU",
					  m_Geometry.triangles.size(), triangleSize, m_BVHNodes.size(), bvhSize);
		
		// Debug: Log first few triangles and nodes
		if (!m_Geometry.triangles.empty()) {
			auto& tri = m_Geometry.triangles[0];
			LNX_LOG_TRACE("First triangle: v0=({0}, {1}, {2}), v1=({3}, {4}, {5}), v2=({6}, {7}, {8})",
						 tri.v0.x, tri.v0.y, tri.v0.z,
						 tri.v1.x, tri.v1.y, tri.v1.z,
						 tri.v2.x, tri.v2.y, tri.v2.z);
		}
		
		if (!m_BVHNodes.empty()) {
			auto& root = m_BVHNodes[0];
			LNX_LOG_TRACE("BVH Root: min=({0}, {1}, {2}), max=({3}, {4}, {5}), leftChild={6}, triangleCount={7}",
						 root.aabbMin.x, root.aabbMin.y, root.aabbMin.z,
						 root.aabbMax.x, root.aabbMax.y, root.aabbMax.z,
						 (int)root.aabbMin.w, (int)root.aabbMax.w);
		}
	}
	
	// ============================================================================
	// G-BUFFER RENDERING
	// ============================================================================
	
	void RayTracingManager::RenderGBuffer(Scene* scene, const Camera& camera, const glm::mat4& viewMatrix) {
		LNX_PROFILE_FUNCTION();
		
		if (!m_Initialized || !scene) return;
		
		// Bind G-Buffer
		m_GBuffer->Bind();
		
		// Clear buffers
		RenderCommand::SetClearColor({ 0.0f, 0.0f, 0.0f, 0.0f });
		RenderCommand::Clear();
		
		// Render scene to G-Buffer
		// TODO: This needs a specialized shader that outputs position/normal
		// For now, we'll use the existing renderer and read back data
		
		m_GBuffer->Unbind();
	}
	
	// ============================================================================
	// SHADOW COMPUTATION
	// ============================================================================
	
	void RayTracingManager::ComputeShadows(Scene* scene) {
		if (!m_Initialized || !m_Settings.enabled) return;
		
		// Call the new method with our internal G-Buffer
		ComputeShadowsWithGBuffer(
			scene,
			m_GBuffer->GetColorAttachmentRendererID(0),
			m_GBuffer->GetColorAttachmentRendererID(1)
		);
	}
	
	void RayTracingManager::ComputeShadowsWithGBuffer(Scene* scene, uint32_t positionTexture, uint32_t normalTexture) {
		if (!m_Initialized || !m_Settings.enabled) return;
		
		auto start = std::chrono::high_resolution_clock::now();
		
		// Bind external G-Buffer textures (from main framebuffer)
		glBindTextureUnit(0, positionTexture);  // Position from main render
		glBindTextureUnit(1, normalTexture);    // Normal from main render
		glBindTextureUnit(2, m_GBuffer->GetColorAttachmentRendererID(2)); // Depth (if needed)
		
		// Extract lights from scene
		auto lightView = scene->GetAllEntitiesWith<LightComponent, TransformComponent>();
		
		// Upload lights to GPU
		std::vector<Light::LightData> lightData;
		for (auto entity : lightView) {
			Entity e = { entity, scene };
			auto& light = e.GetComponent<LightComponent>();
			auto& transform = e.GetComponent<TransformComponent>();
			
			// Get light position and direction
			glm::vec3 position = transform.Translation;
			glm::vec3 direction = glm::normalize(
				glm::rotate(glm::quat(transform.Rotation), glm::vec3(0.0f, 0.0f, -1.0f))
			);
			
			// Get light data using Light class method
			Light::LightData data = light.LightInstance->GetLightData(position, direction);
			lightData.push_back(data);
		}
		
		// Resize light buffer if necessary
		if (lightData.size() * sizeof(Light::LightData) > m_LightBuffer->GetSize()) {
			m_LightBuffer = StorageBuffer::Create(lightData.size() * sizeof(Light::LightData) * 2);
		}
		
		// Upload to GPU
		if (!lightData.empty()) {
			m_LightBuffer->SetData(lightData.data(), lightData.size() * sizeof(Light::LightData));
			LNX_LOG_TRACE("Uploaded {0} lights to GPU for ray tracing", lightData.size());
		} else {
			LNX_LOG_WARN("No lights found in scene for ray tracing!");
		}
		
		// Bind storage buffers
		m_TriangleBuffer->Bind(0);
		m_BVHBuffer->Bind(1);
		m_LightBuffer->Bind(2);
		
		// Bind output image
		glBindImageTexture(3, m_ShadowBuffer->GetColorAttachmentRendererID(0), 
						   0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
		
		// Set uniforms using location-based setting
		glUseProgram(m_ShadowRayTracingShader->GetRendererID());
		glUniform1i(0, static_cast<int>(m_Geometry.triangles.size()));  // location 0: u_TriangleCount
		glUniform1i(1, static_cast<int>(m_BVHNodes.size()));            // location 1: u_NodeCount  
		glUniform2i(2, m_Width, m_Height);                               // location 2: u_Resolution
		glUniform1f(3, m_Settings.shadowBias);                           // location 3: u_ShadowRayBias
		glUniform1f(4, m_Settings.shadowSoftness);                       // location 4: u_ShadowSoftness
		glUniform1i(5, m_Settings.samplesPerLight);                      // location 5: u_ShadowSamplesPerLight

		// Dispatch compute shader
		uint32_t groupsX = (m_Width + 7) / 8;
		uint32_t groupsY = (m_Height + 7) / 8;
		m_ShadowRayTracingShader->Dispatch(groupsX, groupsY, 1);
		
		// Memory barrier
		m_ShadowRayTracingShader->MemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		
		// Optional: Denoise pass
		if (m_Settings.enableDenoiser) {
			m_ShadowDenoiseShader->Bind();
			
			// Bind inputs/outputs
			glBindTextureUnit(0, m_ShadowBuffer->GetColorAttachmentRendererID(0));
			glBindTextureUnit(1, m_GBuffer->GetColorAttachmentRendererID(1)); // Normal
			glBindTextureUnit(2, m_GBuffer->GetColorAttachmentRendererID(2)); // Depth
			
			// Create temp buffer for ping-pong
			// For now, write back to same buffer (not ideal but works)
			glBindImageTexture(3, m_ShadowBuffer->GetColorAttachmentRendererID(0),
							   0, GL_FALSE, 0, GL_WRITE_ONLY, GL_RGBA16F);
			
			// Set uniforms using location-based setting
			glUseProgram(m_ShadowDenoiseShader->GetRendererID());
			glUniform2i(0, m_Width, m_Height);               // location 0: u_Resolution
			glUniform1f(1, m_Settings.denoiseRadius);        // location 1: u_FilterRadius
			glUniform1f(2, 0.1f);                            // location 2: u_DepthThreshold
			glUniform1f(3, 0.5f);                            // location 3: u_NormalThreshold
			
			m_ShadowDenoiseShader->Dispatch(groupsX, groupsY, 1);
			m_ShadowDenoiseShader->MemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);
		}
		
		auto endTime = std::chrono::high_resolution_clock::now();
		m_Stats.shadowComputeTime = std::chrono::duration<float, std::milli>(endTime - start).count();
	}
	
	// ============================================================================
	// GETTERS
	// ============================================================================
	
	uint32_t RayTracingManager::GetShadowTexture() const {
		return m_ShadowBuffer ? m_ShadowBuffer->GetColorAttachmentRendererID(0) : 0;
	}
	
	uint32_t RayTracingManager::GetPositionTexture() const {
		return m_GBuffer ? m_GBuffer->GetColorAttachmentRendererID(0) : 0;
	}
	
	uint32_t RayTracingManager::GetNormalTexture() const {
		return m_GBuffer ? m_GBuffer->GetColorAttachmentRendererID(1) : 0;
	}
	
	uint32_t RayTracingManager::GetDepthTexture() const {
		return m_GBuffer ? m_GBuffer->GetColorAttachmentRendererID(2) : 0;
	}
	
	void RayTracingManager::BindShadowTexture(uint32_t slot) const {
		if (m_ShadowBuffer) {
			glBindTextureUnit(slot, m_ShadowBuffer->GetColorAttachmentRendererID(0));
		} else {
			// Bind white texture (no shadow)
			static uint32_t whiteTex = 0;
			if (whiteTex == 0) {
				glGenTextures(1, &whiteTex);
				glBindTexture(GL_TEXTURE_2D, whiteTex);
				float white[] = { 1.0f, 1.0f, 1.0f, 1.0f };
				glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA16F, 1, 1, 0, GL_RGBA, GL_FLOAT, white);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
				glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
			}
			glBindTextureUnit(slot, whiteTex);
		}
	}
}
