#pragma once

#include <glm/glm.hpp>
#include <vector>

namespace Lunex {

	/**
	 * @brief Shared SSBO (binding 4) holding the bone palette of the mesh being drawn.
	 *
	 * Vertex shaders with skinning read `layout(std430, binding = 4) buffer BoneMatrices`.
	 */
	namespace SkinningBuffer {

		constexpr unsigned int Binding = 4;

		// Uploads the palette and binds the SSBO. Returns 1 if skinning should be enabled
		// for the next draw (bones non-null and non-empty), 0 otherwise.
		int Upload(const std::vector<glm::mat4>* bones);

		// Editor weight-paint view: bone whose weights the next draws visualize (-1 = off)
		void SetDebugBone(int bone);
		int GetDebugBone();

	} // namespace SkinningBuffer

} // namespace Lunex
