#pragma once

/**
 * @file Camera.h
 * @brief Base camera class
 * 
 * AAA Architecture: Base camera lives in Scene/Camera/
 * Contains only projection data.
 */

#include <glm/glm.hpp>

namespace Lunex {
	
	/**
	 * @class Camera
	 * @brief Base camera class with projection matrix
	 */
	class Camera { 
	public:
		Camera() = default;
		Camera(const glm::mat4& projection)
			: m_Projection(projection) {
		}
		
		virtual ~Camera() = default;
		
		const glm::mat4& GetProjection() const { return m_Projection; }
		
	protected:
		glm::mat4 m_Projection = glm::mat4(1.0f);
	};
	
} // namespace Lunex
