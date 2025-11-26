#pragma once

#include "Core/Core.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexArray.h"
#include "Platform/OpenGL/OpenGLTextureCube.h"

#include <glm/glm.hpp>

namespace Lunex {

	// Forward declarations
	class UniformBuffer;

	// ============================================================================
	// SKYBOX RENDERER - Physically-Based Atmospheric Scattering
	// ============================================================================

	struct SkyboxSettings {
		bool Enabled = true;
		bool UseCubemap = false;
		
		// Time and Sun (for procedural)
		float TimeOfDay = 12.0f;         // 0-24 hours
		float SunIntensity = 1.0f;
		
		// Atmospheric parameters (for procedural)
		float Turbidity = 2.0f;          // 1-10 (atmospheric haziness)
		float RayleighCoefficient = 1.0f;
		float MieCoefficient = 1.0f;
		float MieDirectionalG = 0.76f;
		float AtmosphereThickness = 1.0f;
		
		// Ground (for procedural)
		glm::vec3 GroundColor = glm::vec3(0.3f, 0.25f, 0.2f);
		float GroundEmission = 0.1f;
		
		// HDR/Cubemap controls
		float HDRExposure = 1.0f;        // HDR exposure control
		float SkyboxRotation = 0.0f;     // Rotation in radians
		
		// Post-processing
		float Exposure = 1.0f;
		float AmbientIntensity = 1.0f;
		
		// Sky tint (for artistic control)
		float SkyTint = 1.0f;
		
		// Current HDR path
		std::string HDRPath = "";
	};

	class SkyboxRenderer {
	public:
		SkyboxRenderer();
		~SkyboxRenderer();

		void Init();
		void Shutdown();

		// Renderizar el skybox (debe llamarse DESPUES de la geometria)
		void RenderSkybox(const glm::mat4& viewProjection, const glm::vec3& cameraPos);

		// Configuracion
		void SetSettings(const SkyboxSettings& settings) { m_Settings = settings; UpdateSunPosition(); }
		const SkyboxSettings& GetSettings() const { return m_Settings; }
		
		SkyboxSettings& GetSettingsRef() { return m_Settings; }
		
		// Cubemap management
		void LoadCubemap(const std::array<std::string, 6>& faces);
		void LoadDefaultCubemap();
		void LoadHDR(const std::string& path);
		void UnloadCubemap();

		// Getters de iluminacion (para integrar con Renderer3D)
		glm::vec3 GetSunDirection() const { return m_SunDirection; }
		glm::vec3 GetSunColor() const { return m_SunColor; }
		glm::vec3 GetAmbientColor() const;
		
		// Utilidades
		bool IsNightTime() const;
		float GetDayNightBlend() const; // 0 = noche, 1 = dia

	private:
		void CreateSkyboxMesh();
		void UpdateSunPosition();
		glm::vec3 CalculateSunColor();
		glm::vec3 CalculateSkyColor(const glm::vec3& direction);
		
		// UBO data structure (must match shader layout)
		struct SkyboxParamsData {
			glm::vec3 SunDirection;
			float TimeOfDay;
			glm::vec3 SunColor;
			float SunIntensity;
			glm::vec3 GroundAlbedo;
			float Turbidity;
			float RayleighCoefficient;
			float MieCoefficient;
			float MieDirectionalG;
			float Exposure;
			float UseCubemap;
			float SkyTint;
			float AtmosphereThickness;
			float GroundEmission;
			float HDRExposure;
			float SkyboxRotation;
		};

	private:
		SkyboxSettings m_Settings;
		
		Ref<Shader> m_SkyboxShader;
		Ref<VertexArray> m_SkyboxVAO;
		Ref<VertexBuffer> m_SkyboxVBO;
		Ref<IndexBuffer> m_SkyboxIBO;
		Ref<UniformBuffer> m_SkyboxParamsUBO;
		Ref<OpenGLTextureCube> m_Cubemap;
		
		// Sun/Sky state
		glm::vec3 m_SunDirection;
		glm::vec3 m_SunColor;
		float m_SunElevation;  // Angulo de elevacion del sol (-90 a 90 grados)
		
		// Atmospheric scattering coefficients
		glm::vec3 m_BetaR;  // Rayleigh scattering
		glm::vec3 m_BetaM;  // Mie scattering
	};

}
