#include "stpch.h"
#include "SkyboxRenderer.h"

#include "Renderer/RenderCommand.h"
#include "Renderer/UniformBuffer.h"
#include "Platform/OpenGL/OpenGLTextureCube.h"
#include "Log/Log.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/constants.hpp>

namespace Lunex {

	// ============================================================================
	// ATMOSPHERIC SCATTERING CONSTANTS
	// ============================================================================

	// Rayleigh scattering coefficients (wavelength dependent)
	static const glm::vec3 BETA_R = glm::vec3(5.8e-6f, 13.5e-6f, 33.1e-6f);
	
	// Mie scattering coefficient
	static const float BETA_M = 21e-6f;
	
	// Scale heights
	static const float H_R = 8000.0f;  // Rayleigh scale height
	static const float H_M = 1200.0f;  // Mie scale height
	
	// Earth radius (for atmospheric calculations)
	static const float EARTH_RADIUS = 6371000.0f;
	static const float ATMOSPHERE_RADIUS = 6471000.0f;

	// ============================================================================
	// CONSTRUCTOR / DESTRUCTOR
	// ============================================================================

	SkyboxRenderer::SkyboxRenderer()
		: m_SunDirection(0.0f, 1.0f, 0.0f)
		, m_SunColor(1.0f)
		, m_SunElevation(45.0f)
		, m_BetaR(BETA_R)
		, m_BetaM(BETA_M)
	{
	}

	SkyboxRenderer::~SkyboxRenderer() {
		Shutdown();
	}

	// ============================================================================
	// INITIALIZATION
	// ============================================================================

	void SkyboxRenderer::Init() {
		LNX_PROFILE_FUNCTION();

		// Load skybox shader
		m_SkyboxShader = Shader::Create("assets/shaders/Skybox.glsl");

		CreateSkyboxMesh();
		UpdateSunPosition();
		
		// Create uniform buffer for skybox parameters (binding = 4, after Camera=0, Transform=1, Material=2, Lights=3)
		m_SkyboxParamsUBO = UniformBuffer::Create(sizeof(SkyboxParamsData), 4);

		LNX_LOG_INFO("SkyboxRenderer initialized");
	}

	void SkyboxRenderer::Shutdown() {
		m_SkyboxShader.reset();
		m_SkyboxVAO.reset();
		m_SkyboxVBO.reset();
		m_SkyboxIBO.reset();
		m_SkyboxParamsUBO.reset();
	}

	// ============================================================================
	// SKYBOX MESH CREATION (Fullscreen Cube)
	// ============================================================================

	void SkyboxRenderer::CreateSkyboxMesh() {
		// Skybox cube vertices - Large cube to ensure it covers the entire scene
		// Using coordinates that will map to the far plane after projection
		float vertices[] = {
			// Positions (unit cube that will be placed at infinity via shader)
			-1.0f,  1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f,  1.0f, -1.0f,

			-1.0f, -1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f, -1.0f,  1.0f
		};

		// Skybox indices - winding order for OpenGL (CCW front-facing)
		uint32_t indices[] = {
			// Back face (facing +Z in view space)
			0, 2, 1, 0, 3, 2,
			// Front face (facing -Z in view space)
			4, 6, 5, 4, 7, 6,
			// Left face (facing +X in view space)
			4, 0, 1, 4, 1, 5,
			// Right face (facing -X in view space)
			3, 7, 2, 7, 6, 2,
			// Top face (facing -Y in view space)
			5, 3, 0, 5, 6, 3,
			// Bottom face (facing +Y in view space)
			1, 7, 4, 1, 2, 7
		};

		m_SkyboxVAO = VertexArray::Create();

		m_SkyboxVBO = VertexBuffer::Create(vertices, sizeof(vertices));
		m_SkyboxVBO->SetLayout({
			{ ShaderDataType::Float3, "a_Position" }
		});

		m_SkyboxVAO->AddVertexBuffer(m_SkyboxVBO);
		
		m_SkyboxIBO = IndexBuffer::Create(indices, 36);
		m_SkyboxVAO->SetIndexBuffer(m_SkyboxIBO);
	}

	// ============================================================================
	// SUN POSITION CALCULATION
	// ============================================================================

	void SkyboxRenderer::UpdateSunPosition() {
		// Improved sun position calculation
		// TimeOfDay: 0 = midnight, 6 = sunrise, 12 = noon, 18 = sunset, 24 = midnight
		
		// Convert to hours from noon
		float hoursFromNoon = m_Settings.TimeOfDay - 12.0f;
		
		// Sun angle (in radians)
		float sunAngle = (hoursFromNoon / 12.0f) * glm::pi<float>();
		
		// Sun elevation (-90 to +90 degrees)
		m_SunElevation = glm::sin(sunAngle) * 90.0f;
		
		// Calculate 3D sun direction
		// Assuming sun moves in a circular arc from east to west
		float azimuth = sunAngle; // East at sunrise, West at sunset
		float elevation = glm::radians(m_SunElevation);
		
		m_SunDirection = glm::normalize(glm::vec3(
			glm::cos(elevation) * glm::sin(azimuth),
			glm::sin(elevation),
			glm::cos(elevation) * glm::cos(azimuth)
		));
		
		// Calculate sun color based on elevation
		m_SunColor = CalculateSunColor();
	}

	// ============================================================================
	// SUN COLOR CALCULATION (Based on elevation)
	// ============================================================================

	glm::vec3 SkyboxRenderer::CalculateSunColor() {
		// More accurate sun color calculation
		float t = glm::smoothstep(-10.0f, 30.0f, m_SunElevation);
		
		// Night (below horizon)
		glm::vec3 nightColor = glm::vec3(0.05f, 0.08f, 0.15f) * 0.01f;
		
		// Sunrise/Sunset
		glm::vec3 sunriseColor = glm::vec3(1.0f, 0.5f, 0.2f);
		
		// Noon
		glm::vec3 noonColor = glm::vec3(1.0f, 0.98f, 0.95f);
		
		glm::vec3 color;
		
		if (t < 0.15f) {
			// Night to sunrise
			float blend = t / 0.15f;
			color = glm::mix(nightColor, sunriseColor, blend);
		}
		else if (t < 0.85f) {
			// Sunrise to noon
			float blend = (t - 0.15f) / 0.7f;
			color = glm::mix(sunriseColor, noonColor, blend);
		}
		else {
			// Noon to sunset
			float blend = (t - 0.85f) / 0.15f;
			color = glm::mix(noonColor, sunriseColor, blend);
		}
		
		return color * m_Settings.SunIntensity;
	}

	// ============================================================================
	// AMBIENT COLOR CALCULATION
	// ============================================================================

	glm::vec3 SkyboxRenderer::GetAmbientColor() const {
		if (m_Settings.UseCubemap && m_Cubemap) {
			// For cubemap mode, use a simple average ambient
			// In a real engine, this would sample the cubemap or use precomputed SH coefficients
			// For now, return a neutral ambient that will be modulated by AmbientIntensity
			return glm::vec3(0.5f, 0.5f, 0.5f) * m_Settings.AmbientIntensity;
		}
		
		// Procedural mode: Calculate ambient based on sun elevation
		float dayFactor = GetDayNightBlend();
		
		glm::vec3 dayAmbient = glm::vec3(0.4f, 0.5f, 0.6f);
		glm::vec3 nightAmbient = glm::vec3(0.05f, 0.08f, 0.15f);
		
		glm::vec3 ambient = glm::mix(nightAmbient, dayAmbient, dayFactor);
		return ambient * m_Settings.AmbientIntensity;
	}

	// ============================================================================
	// UTILITY FUNCTIONS
	// ============================================================================

	bool SkyboxRenderer::IsNightTime() const {
		return m_SunElevation < -5.0f;
	}

	float SkyboxRenderer::GetDayNightBlend() const {
		// Return 0 for night, 1 for day, smooth transition
		return glm::smoothstep(-10.0f, 20.0f, m_SunElevation);
	}
	
	// ============================================================================
	// CUBEMAP MANAGEMENT
	// ============================================================================
	
	void SkyboxRenderer::LoadCubemap(const std::array<std::string, 6>& faces) {
		m_Cubemap = CreateRef<OpenGLTextureCube>(faces);
		
		if (m_Cubemap->IsLoaded()) {
			m_Settings.UseCubemap = true;
			LNX_LOG_INFO("Cubemap loaded successfully for skybox");
		}
		else {
			m_Cubemap.reset();
			m_Settings.UseCubemap = false;
			LNX_LOG_ERROR("Failed to load cubemap for skybox");
		}
	}
	
	void SkyboxRenderer::UnloadCubemap() {
		m_Cubemap.reset();
		m_Settings.UseCubemap = false;
	}
	
	void SkyboxRenderer::LoadDefaultCubemap() {
		// Try to load a default sky cubemap if it exists
		std::array<std::string, 6> faces = {
			"assets/textures/skybox/right.jpg",
			"assets/textures/skybox/left.jpg",
			"assets/textures/skybox/top.jpg",
			"assets/textures/skybox/bottom.jpg",
			"assets/textures/skybox/front.jpg",
			"assets/textures/skybox/back.jpg"
		};
		
		LoadCubemap(faces);
	}
	
	void SkyboxRenderer::LoadHDR(const std::string& path) {
		m_Cubemap = OpenGLTextureCube::CreateFromHDR(path);
		
		if (m_Cubemap && m_Cubemap->IsLoaded()) {
			m_Settings.UseCubemap = true;
			m_Settings.HDRPath = path;
			LNX_LOG_INFO("HDR skybox loaded successfully: {0}", path);
		}
		else {
			m_Cubemap.reset();
			m_Settings.UseCubemap = false;
			m_Settings.HDRPath = "";
			LNX_LOG_ERROR("Failed to load HDR skybox: {0}", path);
		}
	}

	// ============================================================================
	// RENDERING
	// ============================================================================

	void SkyboxRenderer::RenderSkybox(const glm::mat4& viewProjection, const glm::vec3& cameraPos) {
		if (!m_Settings.Enabled)
			return;

		LNX_PROFILE_FUNCTION();
		
		// Safety check for shader
		if (!m_SkyboxShader) {
			LNX_LOG_ERROR("Skybox shader not initialized!");
			return;
		}
		
		// Safety check for VAO
		if (!m_SkyboxVAO) {
			LNX_LOG_ERROR("Skybox VAO not initialized!");
			return;
		}

		// Update sun if time changed (only for procedural)
		if (!m_Settings.UseCubemap) {
			static float lastTimeOfDay = -1.0f;
			if (m_Settings.TimeOfDay != lastTimeOfDay) {
				UpdateSunPosition();
				lastTimeOfDay = m_Settings.TimeOfDay;
			}
		}

		// Save current render state
		// We need to disable face culling because we're rendering from inside the cube
		RenderCommand::SetCullMode(RendererAPI::CullMode::None);
		
		// Change depth function to LEQUAL so skybox passes depth test at far plane
		RenderCommand::SetDepthFunc(RendererAPI::DepthFunc::LessEqual);
		// Disable depth writing (skybox doesn't write to depth buffer)
		RenderCommand::SetDepthMask(false);

		m_SkyboxShader->Bind();

		// Pack all parameters into UBO structure
		SkyboxParamsData params;
		params.SunDirection = m_SunDirection;
		params.TimeOfDay = m_Settings.TimeOfDay;
		params.SunColor = m_SunColor;
		params.SunIntensity = m_Settings.SunIntensity;
		params.GroundAlbedo = m_Settings.GroundColor;
		params.Turbidity = m_Settings.Turbidity;
		params.RayleighCoefficient = m_Settings.RayleighCoefficient;
		params.MieCoefficient = m_Settings.MieCoefficient;
		params.MieDirectionalG = m_Settings.MieDirectionalG;
		params.Exposure = m_Settings.Exposure;
		params.UseCubemap = m_Settings.UseCubemap ? 1.0f : 0.0f;
		params.SkyTint = m_Settings.SkyTint;
		params.AtmosphereThickness = m_Settings.AtmosphereThickness;
		params.GroundEmission = m_Settings.GroundEmission;
		params.HDRExposure = m_Settings.HDRExposure;
		params.SkyboxRotation = m_Settings.SkyboxRotation;
		
		// Upload to GPU
		if (m_SkyboxParamsUBO) {
			m_SkyboxParamsUBO->SetData(&params, sizeof(SkyboxParamsData));
		}
		
		// Bind cubemap if using cubemap mode
		if (m_Settings.UseCubemap && m_Cubemap && m_Cubemap->IsLoaded()) {
			m_Cubemap->Bind(0);
		}

		// Render cube - skybox uses Camera UBO at binding 0 which already has the ViewProjection
		// The shader will handle making it appear at infinity
		m_SkyboxVAO->Bind();
		RenderCommand::DrawIndexed(m_SkyboxVAO, 36);

		// Restore default render state
		RenderCommand::SetCullMode(RendererAPI::CullMode::Back);
		RenderCommand::SetDepthFunc(RendererAPI::DepthFunc::Less);
		RenderCommand::SetDepthMask(true);
	}

	// ============================================================================
	// SKY COLOR CALCULATION (For ambient/IBL - not used in shader)
	// ============================================================================

	glm::vec3 SkyboxRenderer::CalculateSkyColor(const glm::vec3& direction) {
		// Simple Preetham approximation for ambient calculations
		float cosTheta = glm::dot(direction, m_SunDirection);
		
		// Rayleigh phase function
		float phaseR = 0.75f * (1.0f + cosTheta * cosTheta);
		
		// Mie phase function (simplified)
		float g = 0.76f;
		float phaseM = (1.0f - g * g) / (4.0f * glm::pi<float>() * glm::pow(1.0f + g * g - 2.0f * g * cosTheta, 1.5f));
		
		// Combine
		glm::vec3 skyColor = m_BetaR * phaseR + m_BetaM * phaseM;
		
		return skyColor * m_SunColor;
	}

} // namespace Lunex
