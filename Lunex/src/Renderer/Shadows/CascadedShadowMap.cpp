#include "stpch.h"
#include "CascadedShadowMap.h"

#include <glm/gtc/matrix_transform.hpp>
#include <algorithm>
#include <cmath>

namespace Lunex {

	std::array<glm::vec3, 8> CascadedShadowMap::GetFrustumCornersWorldSpace(
		const glm::mat4& viewProj)
	{
		const glm::mat4 inv = glm::inverse(viewProj);

		// NDC corners ? world space
		std::array<glm::vec3, 8> corners;
		int idx = 0;
		for (int x = 0; x < 2; ++x) {
			for (int y = 0; y < 2; ++y) {
				for (int z = 0; z < 2; ++z) {
					const glm::vec4 pt = inv * glm::vec4(
						2.0f * x - 1.0f,
						2.0f * y - 1.0f,
						2.0f * z - 1.0f,
						1.0f
					);
					corners[idx++] = glm::vec3(pt) / pt.w;
				}
			}
		}
		return corners;
	}

	glm::mat4 CascadedShadowMap::StabilizeProjection(
		const glm::mat4& lightViewProj,
		uint32_t shadowResolution)
	{
		// Sample the origin through the matrix to find the texel offset
		glm::vec4 shadowOrigin = lightViewProj * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
		shadowOrigin *= static_cast<float>(shadowResolution) / 2.0f;

		glm::vec4 roundedOrigin = glm::round(shadowOrigin);
		glm::vec4 roundOffset = roundedOrigin - shadowOrigin;
		roundOffset *= 2.0f / static_cast<float>(shadowResolution);
		roundOffset.z = 0.0f;
		roundOffset.w = 0.0f;

		glm::mat4 result = lightViewProj;
		result[3] += roundOffset;
		return result;
	}

	std::vector<CascadedShadowMap::CascadeInfo> CascadedShadowMap::CalculateCascades(
		const glm::mat4& cameraView,
		const glm::mat4& cameraProj,
		const glm::vec3& lightDirection,
		float cameraNear,
		float cameraFar,
		uint32_t cascadeCount,
		float splitLambda,
		uint32_t shadowResolution)
	{
		cascadeCount = std::clamp(cascadeCount, 1u, MAX_SHADOW_CASCADES);
		float effectiveFar = std::min(cameraFar, 500.0f); // Clamp to reasonable distance

		// ------------------------------------------------------------------
		// 1. Compute cascade split depths using PSSM (Practical Split Scheme)
		// ------------------------------------------------------------------
		std::vector<float> splits(cascadeCount + 1);
		splits[0] = cameraNear;

		for (uint32_t i = 1; i <= cascadeCount; ++i) {
			float p = static_cast<float>(i) / static_cast<float>(cascadeCount);
			float logSplit = cameraNear * std::pow(effectiveFar / cameraNear, p);
			float uniformSplit = cameraNear + (effectiveFar - cameraNear) * p;
			splits[i] = splitLambda * logSplit + (1.0f - splitLambda) * uniformSplit;
		}

		// ------------------------------------------------------------------
		// 2. For each cascade, compute a light-space ortho projection
		// ------------------------------------------------------------------
		std::vector<CascadeInfo> cascades(cascadeCount);

		for (uint32_t i = 0; i < cascadeCount; ++i) {
			float nearSplit = splits[i];
			float farSplit = splits[i + 1];

			// Build a sub-frustum projection for this cascade range
			// We modify the projection to use the split near/far
			glm::mat4 cascadeProj = cameraProj;
			// Overwrite near/far in projection (perspective assumed)
			// P[2][2] = -(far+near)/(far-near), P[3][2] = -(2*far*near)/(far-near)
			cascadeProj[2][2] = -(farSplit + nearSplit) / (farSplit - nearSplit);
			cascadeProj[3][2] = -(2.0f * farSplit * nearSplit) / (farSplit - nearSplit);

			glm::mat4 viewProj = cascadeProj * cameraView;
			auto corners = GetFrustumCornersWorldSpace(viewProj);

			// Find frustum center
			glm::vec3 center(0.0f);
			for (const auto& c : corners) {
				center += c;
			}
			center /= 8.0f;

			// Build light view matrix looking along lightDirection
			glm::vec3 lightDir = glm::normalize(lightDirection);
			glm::mat4 lightView = glm::lookAt(
				center - lightDir * 100.0f, // Offset so near plane catches geometry
				center,
				glm::vec3(0.0f, 1.0f, 0.0f)
			);

			// Handle degenerate up vector (light pointing straight up/down)
			if (std::abs(glm::dot(lightDir, glm::vec3(0, 1, 0))) > 0.999f) {
				lightView = glm::lookAt(
					center - lightDir * 100.0f,
					center,
					glm::vec3(0.0f, 0.0f, 1.0f)
				);
			}

			// Find AABB of frustum in light space
			float minX = std::numeric_limits<float>::max();
			float maxX = std::numeric_limits<float>::lowest();
			float minY = std::numeric_limits<float>::max();
			float maxY = std::numeric_limits<float>::lowest();
			float minZ = std::numeric_limits<float>::max();
			float maxZ = std::numeric_limits<float>::lowest();

			for (const auto& c : corners) {
				glm::vec4 lc = lightView * glm::vec4(c, 1.0f);
				minX = std::min(minX, lc.x);
				maxX = std::max(maxX, lc.x);
				minY = std::min(minY, lc.y);
				maxY = std::max(maxY, lc.y);
				minZ = std::min(minZ, lc.z);
				maxZ = std::max(maxZ, lc.z);
			}

			// Extend Z range to catch shadow casters behind the camera frustum
			float zExtent = maxZ - minZ;
			minZ -= zExtent * 2.0f;
			maxZ += zExtent * 0.5f;

			glm::mat4 lightOrtho = glm::ortho(minX, maxX, minY, maxY, minZ, maxZ);
			glm::mat4 lightVP = lightOrtho * lightView;

			// Stabilize to prevent edge shimmer when camera moves
			lightVP = StabilizeProjection(lightVP, shadowResolution);

			cascades[i].ViewProjection = lightVP;
			cascades[i].SplitDepth = farSplit;
		}

		return cascades;
	}

} // namespace Lunex
