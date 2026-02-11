#pragma once

/**
 * @file CascadedShadowMap.h
 * @brief Cascade split calculation for directional light CSM
 */

#include "ShadowTypes.h"
#include <glm/glm.hpp>
#include <vector>
#include <array>

namespace Lunex {

	class CascadedShadowMap {
	public:
		struct CascadeInfo {
			glm::mat4 ViewProjection;
			float SplitDepth;   // View-space depth of cascade far plane
		};

		/**
		 * Calculate cascade view-projection matrices for a directional light.
		 *
		 * @param cameraView       Camera view matrix
		 * @param cameraProj       Camera projection matrix
		 * @param lightDirection    Normalized direction FROM light (e.g. sun direction)
		 * @param cameraNear       Camera near plane
		 * @param cameraFar        Camera far plane (or maxShadowDistance)
		 * @param cascadeCount     Number of cascades (1-4)
		 * @param splitLambda      PSSM blend: 0 = uniform, 1 = logarithmic
		 * @param shadowResolution Shadow map resolution (for stabilization)
		 */
		static std::vector<CascadeInfo> CalculateCascades(
			const glm::mat4& cameraView,
			const glm::mat4& cameraProj,
			const glm::vec3& lightDirection,
			float cameraNear,
			float cameraFar,
			uint32_t cascadeCount,
			float splitLambda,
			uint32_t shadowResolution
		);

	private:
		/**
		 * Compute frustum corners in world space for a given near/far range
		 */
		static std::array<glm::vec3, 8> GetFrustumCornersWorldSpace(
			const glm::mat4& viewProj
		);

		/**
		 * Snap the orthographic projection to texel grid to prevent edge shimmer
		 */
		static glm::mat4 StabilizeProjection(
			const glm::mat4& lightViewProj,
			uint32_t shadowResolution
		);
	};

} // namespace Lunex
