#pragma once

#include "Core/Core.h"
#include <glm/glm.hpp>

namespace Lunex {

	enum class LightType {
		Directional = 0,
		Point = 1,
		Spot = 2
	};

	class Light {
	public:
		Light();
		Light(LightType type);
		~Light() = default;

		// Type
		void SetType(LightType type) { m_Type = type; }
		LightType GetType() const { return m_Type; }

		// Common properties
		void SetColor(const glm::vec3& color) { m_Color = color; }
		void SetIntensity(float intensity) { m_Intensity = glm::max(0.0f, intensity); }
		
		const glm::vec3& GetColor() const { return m_Color; }
		float GetIntensity() const { return m_Intensity; }

		// Point & Spot light properties
		void SetRange(float range) { m_Range = glm::max(0.0f, range); }
		void SetAttenuation(const glm::vec3& attenuation) { m_Attenuation = attenuation; }
		
		float GetRange() const { return m_Range; }
		const glm::vec3& GetAttenuation() const { return m_Attenuation; }

		// Spot light properties
		void SetInnerConeAngle(float angle) { m_InnerConeAngle = glm::clamp(angle, 0.0f, 90.0f); }
		void SetOuterConeAngle(float angle) { m_OuterConeAngle = glm::clamp(angle, 0.0f, 90.0f); }
		
		float GetInnerConeAngle() const { return m_InnerConeAngle; }
		float GetOuterConeAngle() const { return m_OuterConeAngle; }

		// Shadow properties
		void SetCastShadows(bool cast) { m_CastShadows = cast; }
		bool GetCastShadows() const { return m_CastShadows; }

		// Ray traced shadow properties
		void SetLightRadius(float radius) { m_LightRadius = glm::max(0.0f, radius); }
		float GetLightRadius() const { return m_LightRadius; }

		// For shader uniform upload
		struct LightData {
			glm::vec4 Position;      // xyz = position, w = type
			glm::vec4 Direction;     // xyz = direction, w = unused
			glm::vec4 Color;         // rgb = color, a = intensity
			glm::vec4 Params;        // x = range, y = innerCone, z = outerCone, w = radius (for soft shadows)
			glm::vec4 Attenuation;   // xyz = constant/linear/quadratic, w = unused
		};

		LightData GetLightData(const glm::vec3& position, const glm::vec3& direction) const {
			return {
				glm::vec4(position, static_cast<float>(m_Type)),
				glm::vec4(direction, 0.0f),
				glm::vec4(m_Color, m_Intensity),
				glm::vec4(m_Range, glm::cos(glm::radians(m_InnerConeAngle)), 
						  glm::cos(glm::radians(m_OuterConeAngle)), m_LightRadius),
				glm::vec4(m_Attenuation, 0.0f)
			};
		}

	private:
		LightType m_Type = LightType::Point;
		glm::vec3 m_Color = { 1.0f, 1.0f, 1.0f };
		float m_Intensity = 1.0f;

		// Point & Spot
		float m_Range = 10.0f;
		glm::vec3 m_Attenuation = { 1.0f, 0.09f, 0.032f }; // constant, linear, quadratic

		// Spot only
		float m_InnerConeAngle = 12.5f; // degrees
		float m_OuterConeAngle = 17.5f; // degrees

		// Shadows
		bool m_CastShadows = true;
		
		// Ray traced shadows
		float m_LightRadius = 0.1f; // Physical size of light for soft shadows
	};

}
