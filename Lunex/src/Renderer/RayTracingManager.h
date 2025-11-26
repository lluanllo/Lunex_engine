#pragma once

#include "Core/Core.h"
#include "RayTracingGeometry.h"
#include "BVHBuilder.h"
#include "ComputeShader.h"
#include "StorageBuffer.h"
#include "Framebuffer.h"
#include "Scene/Scene.h"

namespace Lunex {
	
	// Ray Tracing Manager - Orchestrates the compute shader ray tracing pipeline
	class RayTracingManager {
	public:
		RayTracingManager();
		~RayTracingManager();
		
		// Initialize resources
		void Init(uint32_t width, uint32_t height);
		void Shutdown();
		
		// Resize G-Buffer
		void Resize(uint32_t width, uint32_t height);
		
		// Update scene geometry (call when scene changes)
		void UpdateSceneGeometry(Scene* scene);
		
		// Render G-Buffer pass (position, normal, depth)
		void RenderGBuffer(Scene* scene, const class Camera& camera, const glm::mat4& viewMatrix);
		
		// Compute shadows
		void ComputeShadows(Scene* scene);
		
		// Compute shadows using external G-Buffer textures
		void ComputeShadowsWithGBuffer(Scene* scene, uint32_t positionTexture, uint32_t normalTexture);
		
		// Get shadow output texture
		uint32_t GetShadowTexture() const;
		
		// Get G-Buffer textures
		uint32_t GetPositionTexture() const;
		uint32_t GetNormalTexture() const;
		uint32_t GetDepthTexture() const;
		
		// Bind shadow texture to a specific slot for rendering
		void BindShadowTexture(uint32_t slot = 10) const;
		
		// Settings
		struct Settings {
			bool enabled = true;
			float shadowBias = 0.001f;
			float shadowSoftness = 1.0f;
			int samplesPerLight = 4;
			bool enableDenoiser = true;
			float denoiseRadius = 2.0f;
		};
		
		void SetSettings(const Settings& settings) { m_Settings = settings; }
		const Settings& GetSettings() const { return m_Settings; }
		
		// Statistics
		struct Stats {
			uint32_t triangleCount = 0;
			uint32_t bvhNodeCount = 0;
			float bvhBuildTime = 0.0f;
			float shadowComputeTime = 0.0f;
			bool geometryDirty = true;
		};
		
		const Stats& GetStats() const { return m_Stats; }
		
	private:
		void RebuildBVH();
		void UploadGeometryToGPU();
		
	private:
		// Resources
		Ref<Framebuffer> m_GBuffer;
		Ref<Framebuffer> m_ShadowBuffer;
		
		// Compute shaders
		Ref<ComputeShader> m_ShadowRayTracingShader;
		Ref<ComputeShader> m_ShadowDenoiseShader;
		
		// Storage buffers (SSBOs)
		Ref<StorageBuffer> m_TriangleBuffer;
		Ref<StorageBuffer> m_BVHBuffer;
		Ref<StorageBuffer> m_LightBuffer;
		
		// Scene data
		SceneGeometry m_Geometry;
		std::vector<RTBVHNode> m_BVHNodes;
		BVHBuilder m_BVHBuilder;
		
		// State
		Settings m_Settings;
		Stats m_Stats;
		uint32_t m_Width = 0;
		uint32_t m_Height = 0;
		bool m_Initialized = false;
	};
}
