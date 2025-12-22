#include "stpch.h"
#include "AnimationComponents.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Lunex {

	// ============================================================================
	// BONE TRANSFORM
	// ============================================================================
	
	glm::mat4 BoneTransform::ToMatrix() const {
		glm::mat4 translation = glm::translate(glm::mat4(1.0f), Translation);
		glm::mat4 rotation = glm::mat4_cast(Rotation);
		glm::mat4 scale = glm::scale(glm::mat4(1.0f), Scale);
		return translation * rotation * scale;
	}
	
	BoneTransform BoneTransform::Lerp(const BoneTransform& a, const BoneTransform& b, float t) {
		BoneTransform result;
		result.Translation = glm::mix(a.Translation, b.Translation, t);
		result.Rotation = glm::slerp(a.Rotation, b.Rotation, t);
		result.Scale = glm::mix(a.Scale, b.Scale, t);
		return result;
	}

}
