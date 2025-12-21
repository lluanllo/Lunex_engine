#pragma once

/**
 * @file LightTypes.h
 * @brief Light data structures for the lighting system
 * 
 * AAA Architecture: Light types live in Scene/Lighting/
 * Defines data structures for all light types.
 */

#include <glm/glm.hpp>
#include <vector>
#include <cstdint>

namespace Lunex {

	/**
	 * @enum LightType
	 * @brief Types of lights supported
	 */
	enum class LightType : uint8_t {
		Directional = 0,
		Point = 1,
		Spot = 2,
		Area = 3  // Future: area lights
	};

	/**
	 * @struct LightData
	 * @brief GPU-ready light data structure
	 * 
	 * This is what gets uploaded to the GPU for rendering.
	 * Aligned for efficient GPU access.
	 */
	struct LightData {
		glm::vec4 Position;      // xyz = position, w = type
		glm::vec4 Direction;     // xyz = direction, w = range
		glm::vec4 Color;         // rgb = color, a = intensity
		glm::vec4 Params;        // x = innerCone, y = outerCone, z = castShadows, w = shadowMapIndex
		glm::vec4 Attenuation;   // x = constant, y = linear, z = quadratic, w = unused
	};

	/**
	 * @struct LightProperties
	 * @brief Full light properties for scene storage
	 */
	struct LightProperties {
		LightType Type = LightType::Point;
		
		glm::vec3 Color = { 1.0f, 1.0f, 1.0f };
		float Intensity = 1.0f;
		
		// Point & Spot
		float Range = 10.0f;
		glm::vec3 Attenuation = { 1.0f, 0.09f, 0.032f };
		
		// Spot only
		float InnerConeAngle = 12.5f;  // degrees
		float OuterConeAngle = 17.5f;  // degrees
		
		// Shadows
		bool CastShadows = true;
		int ShadowMapIndex = -1;
		float ShadowBias = 0.005f;
		float ShadowNormalBias = 0.02f;
		
		// Area light (future)
		glm::vec2 AreaSize = { 1.0f, 1.0f };
		
		/**
		 * @brief Convert to GPU-ready LightData
		 */
		LightData ToGPUData(const glm::vec3& worldPosition, const glm::vec3& worldDirection) const {
			return {
				glm::vec4(worldPosition, static_cast<float>(Type)),
				glm::vec4(worldDirection, Range),
				glm::vec4(Color, Intensity),
				glm::vec4(
					glm::cos(glm::radians(InnerConeAngle)),
					glm::cos(glm::radians(OuterConeAngle)),
					CastShadows ? 1.0f : 0.0f,
					static_cast<float>(ShadowMapIndex)
				),
				glm::vec4(Attenuation, 0.0f)
			};
		}
	};

	/**
	 * @struct LightingData
	 * @brief Container for all scene lighting data
	 * 
	 * This is what the renderer receives - no direct light access.
	 */
	struct LightingData {
		std::vector<LightData> Lights;
		
		// Ambient
		glm::vec3 AmbientColor = { 0.03f, 0.03f, 0.03f };
		float AmbientIntensity = 1.0f;
		
		// Environment
		bool HasEnvironmentMap = false;
		float EnvironmentIntensity = 1.0f;
		
		// Statistics
		uint32_t DirectionalLightCount = 0;
		uint32_t PointLightCount = 0;
		uint32_t SpotLightCount = 0;
		
		uint32_t GetTotalLightCount() const {
			return DirectionalLightCount + PointLightCount + SpotLightCount;
		}
	};

} // namespace Lunex
