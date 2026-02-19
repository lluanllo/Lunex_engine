#pragma once

/**
 * @file ShadowTypes.h
 * @brief Shadow system data structures, GPU layouts, and configuration
 */

#include <glm/glm.hpp>
#include <cstdint>

namespace Lunex {

	// ============================================================================
	// SHADOW CONFIGURATION
	// ============================================================================

	struct ShadowConfig {
		// Atlas
		uint32_t DirectionalResolution = 2048;
		uint32_t SpotResolution = 1024;
		uint32_t PointResolution = 512;
		uint32_t MaxShadowCastingLights = 16;

		// CSM (Cascaded Shadow Maps) for directional lights
		uint32_t CSMCascadeCount = 4;
		float CSMSplitLambda = 0.75f; // 0 = linear, 1 = logarithmic
		float MaxShadowDistance = 200.0f;

		// Filtering
		bool EnablePCF = true;
		float PCFRadius = 1.5f;

		// Distance-based shadow softening
		float DistanceSofteningStart = 50.0f;   // Start softening shadows beyond this distance
		float DistanceSofteningMax = 4.0f;       // Max PCF radius multiplier at max distance

		// Sky color tinting (ambient light contamination on shadows)
		bool EnableSkyColorTint = true;
		float SkyTintStrength = 0.15f;           // How much sky color bleeds into shadows

		// Bias
		float DefaultDepthBias = 0.005f;
		float DefaultNormalBias = 0.03f;
		float DirectionalBias = 0.002f;
		float SpotBias = 0.004f;
		float PointBias = 0.015f;
	};

	// ============================================================================
	// SHADOW TYPE ENUM
	// ============================================================================

	enum class ShadowType : uint32_t {
		None = 0,
		Directional_CSM = 1,
		Spot = 2,
		Point_Cubemap = 3
	};

	// ============================================================================
	// GPU-READY STRUCTURES (must match GLSL layout std140)
	// ============================================================================

	/**
	 * Per-cascade data for directional CSM.
	 * 4 cascades max.
	 */
	struct alignas(16) ShadowCascadeGPU {
		glm::mat4 ViewProjection;      // 64 bytes
		float SplitDepth;              // 4 bytes
		float _pad[3];                 // 12 bytes  ? total 80 bytes
	};

	/**
	 * Per shadow-casting light data.
	 * For directional: ViewProjection[0..3] = cascade matrices
	 * For spot:        ViewProjection[0] = single matrix
	 * For point:       ViewProjection[0..5] = 6 cubemap face matrices
	 */
	struct alignas(16) ShadowLightGPU {
		glm::mat4 ViewProjection[6];   // 384 bytes (6 × 64)
		glm::vec4 ShadowParams;        // x=bias, y=normalBias, z=firstAtlasLayer, w=shadowType
		glm::vec4 ShadowParams2;       // x=lightRange, y=pcfRadius, z=numFaces, w=resolution
	};

	/**
	 * Main shadow UBO uploaded to binding=7.
	 * Must be ? 16KB for UBO on most GPUs; if large, use SSBO.
	 * With 16 lights: 16 + (80*4) + (400*16) = 6736 bytes ? fits UBO.
	 */
	static constexpr uint32_t MAX_SHADOW_CASCADES = 4;
	static constexpr uint32_t MAX_SHADOW_LIGHTS = 16;

	struct alignas(16) ShadowUniformData {
		int NumShadowLights;              // 4
		int CSMCascadeCount;              // 4
		float MaxShadowDistance;          // 4
		float DistanceSofteningStart;     // 4

		float DistanceSofteningMax;       // 4
		float SkyTintStrength;            // 4
		float _pad1;                      // 4
		float _pad2;                      // 4  ? 32 bytes

		ShadowCascadeGPU Cascades[MAX_SHADOW_CASCADES]; // 80 × 4 = 320 bytes
		ShadowLightGPU Shadows[MAX_SHADOW_LIGHTS];      // 400 × 16 = 6400 bytes
		// Total ? 6752 bytes
	};

	// ============================================================================
	// CPU-SIDE SHADOW CASTER INFO
	// ============================================================================

	struct ShadowCasterInfo {
		int LightIndex = -1;             // Index into LightSystem's light array
		ShadowType Type = ShadowType::None;
		float Priority = 0.0f;           // Higher = more important

		// Assigned atlas layers
		int AtlasFirstLayer = -1;
		int LayerCount = 0;

		// Light-space matrices (computed per frame)
		glm::mat4 ViewProjections[6];    // 1 for spot, 4 for CSM, 6 for point
		int NumMatrices = 0;

		// CSM cascade split depths (stored from actual computation)
		float CascadeSplitDepths[MAX_SHADOW_CASCADES] = { 0.0f };

		// Light properties needed for rendering
		glm::vec3 Position;
		glm::vec3 Direction;
		float Range = 0.0f;
		float InnerCone = 0.0f;
		float OuterCone = 0.0f;
		float Bias = 0.005f;
		float NormalBias = 0.02f;
	};

} // namespace Lunex
