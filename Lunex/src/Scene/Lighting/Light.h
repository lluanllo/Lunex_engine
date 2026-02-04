#pragma once

/**
 * @file Light.h
 * @brief Light class for scene entities
 * 
 * AAA Architecture: Light lives in Scene/Lighting/
 * This is the high-level light used by LightComponent.
 */

#include "LightTypes.h"
#include "Core/Core.h"

namespace Lunex {

	/**
	 * @class Light
	 * @brief Light object for scene entities
	 * 
	 * Supports all light types including Directional lights that can
	 * be designated as the "Sun" to control skybox/environment.
	 */
	class Light {
	public:
		Light();
		Light(LightType type);
		~Light() = default;

		// Type
		void SetType(LightType type) { m_Properties.Type = type; }
		LightType GetType() const { return m_Properties.Type; }

		// Color and intensity
		void SetColor(const glm::vec3& color) { m_Properties.Color = color; }
		void SetIntensity(float intensity) { m_Properties.Intensity = glm::max(0.0f, intensity); }
		
		const glm::vec3& GetColor() const { return m_Properties.Color; }
		float GetIntensity() const { return m_Properties.Intensity; }

		// Range (Point & Spot)
		void SetRange(float range) { m_Properties.Range = glm::max(0.0f, range); }
		void SetAttenuation(const glm::vec3& attenuation) { m_Properties.Attenuation = attenuation; }
		
		float GetRange() const { return m_Properties.Range; }
		const glm::vec3& GetAttenuation() const { return m_Properties.Attenuation; }

		// Cone angles (Spot only)
		void SetInnerConeAngle(float angle) { m_Properties.InnerConeAngle = glm::clamp(angle, 0.0f, 90.0f); }
		void SetOuterConeAngle(float angle) { m_Properties.OuterConeAngle = glm::clamp(angle, 0.0f, 90.0f); }
		
		float GetInnerConeAngle() const { return m_Properties.InnerConeAngle; }
		float GetOuterConeAngle() const { return m_Properties.OuterConeAngle; }

		// Shadows
		void SetCastShadows(bool cast) { m_Properties.CastShadows = cast; }
		bool GetCastShadows() const { return m_Properties.CastShadows; }

		// ========================================
		// SUN/SKY SETTINGS (Directional Light only)
		// ========================================
		
		// Mark this light as the "Sun" that controls the skybox
		void SetIsSunLight(bool isSun) { m_Properties.SunSky.IsSunLight = isSun; }
		bool IsSunLight() const { return m_Properties.SunSky.IsSunLight; }
		
		// Link to skybox rotation
		void SetLinkToSkyboxRotation(bool link) { m_Properties.SunSky.LinkToSkyboxRotation = link; }
		bool GetLinkToSkyboxRotation() const { return m_Properties.SunSky.LinkToSkyboxRotation; }
		
		// Skybox intensity multiplier
		void SetSkyboxIntensityMultiplier(float mult) { m_Properties.SunSky.SkyboxIntensityMultiplier = glm::max(0.0f, mult); }
		float GetSkyboxIntensityMultiplier() const { return m_Properties.SunSky.SkyboxIntensityMultiplier; }
		
		// Atmosphere settings
		void SetAffectAtmosphere(bool affect) { m_Properties.SunSky.AffectAtmosphere = affect; }
		bool GetAffectAtmosphere() const { return m_Properties.SunSky.AffectAtmosphere; }
		
		void SetAtmosphericDensity(float density) { m_Properties.SunSky.AtmosphericDensity = glm::max(0.0f, density); }
		float GetAtmosphericDensity() const { return m_Properties.SunSky.AtmosphericDensity; }
		
		// Sun disk appearance
		void SetRenderSunDisk(bool render) { m_Properties.SunSky.RenderSunDisk = render; }
		bool GetRenderSunDisk() const { return m_Properties.SunSky.RenderSunDisk; }
		
		void SetSunDiskSize(float size) { m_Properties.SunSky.SunDiskSize = glm::max(0.0f, size); }
		float GetSunDiskSize() const { return m_Properties.SunSky.SunDiskSize; }
		
		void SetSunDiskIntensity(float intensity) { m_Properties.SunSky.SunDiskIntensity = glm::max(0.0f, intensity); }
		float GetSunDiskIntensity() const { return m_Properties.SunSky.SunDiskIntensity; }
		
		// Ambient contribution
		void SetContributeToAmbient(bool contribute) { m_Properties.SunSky.ContributeToAmbient = contribute; }
		bool GetContributeToAmbient() const { return m_Properties.SunSky.ContributeToAmbient; }
		
		void SetAmbientContribution(float contribution) { m_Properties.SunSky.AmbientContribution = glm::clamp(contribution, 0.0f, 1.0f); }
		float GetAmbientContribution() const { return m_Properties.SunSky.AmbientContribution; }
		
		void SetGroundColor(const glm::vec3& color) { m_Properties.SunSky.GroundColor = color; }
		const glm::vec3& GetGroundColor() const { return m_Properties.SunSky.GroundColor; }
		
		// Time of day
		void SetUseTimeOfDay(bool use) { m_Properties.SunSky.UseTimeOfDay = use; }
		bool GetUseTimeOfDay() const { return m_Properties.SunSky.UseTimeOfDay; }
		
		void SetTimeOfDay(float time) { m_Properties.SunSky.TimeOfDay = glm::mod(time, 24.0f); }
		float GetTimeOfDay() const { return m_Properties.SunSky.TimeOfDay; }
		
		void SetTimeOfDaySpeed(float speed) { m_Properties.SunSky.TimeOfDaySpeed = speed; }
		float GetTimeOfDaySpeed() const { return m_Properties.SunSky.TimeOfDaySpeed; }
		
		// Get full Sun/Sky settings
		const SunSkySettings& GetSunSkySettings() const { return m_Properties.SunSky; }
		SunSkySettings& GetSunSkySettings() { return m_Properties.SunSky; }

		// Get full properties
		const LightProperties& GetProperties() const { return m_Properties; }
		LightProperties& GetProperties() { return m_Properties; }

		// Get GPU-ready data
		LightData GetLightData(const glm::vec3& position, const glm::vec3& direction) const {
			return m_Properties.ToGPUData(position, direction);
		}
		
		/**
		 * @brief Calculate skybox rotation from light direction
		 * 
		 * Converts the light's forward direction to a rotation angle for the skybox.
		 * The skybox rotates so that the "sun" position in the HDRI aligns with
		 * the light's direction.
		 * 
		 * @param direction The world-space direction of the light
		 * @return Rotation angle in degrees for the skybox
		 */
		static float CalculateSkyboxRotationFromDirection(const glm::vec3& direction);
		
		/**
		 * @brief Calculate sun elevation angle from direction
		 * @param direction The world-space direction of the light
		 * @return Elevation angle in degrees (-90 to 90)
		 */
		static float CalculateSunElevation(const glm::vec3& direction);
		
		/**
		 * @brief Calculate sun azimuth angle from direction
		 * @param direction The world-space direction of the light (pointing towards the light source)
		 * @return Azimuth angle in degrees (0-360)
		 */
		static float CalculateSunAzimuth(const glm::vec3& direction);

	private:
		LightProperties m_Properties;
	};

} // namespace Lunex
