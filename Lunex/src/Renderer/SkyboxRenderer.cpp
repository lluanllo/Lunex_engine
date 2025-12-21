#include "stpch.h"
#include "SkyboxRenderer.h"

#include "Renderer/Shader.h"
#include "Renderer/VertexArray.h"
#include "Renderer/Buffer.h"
#include "Renderer/UniformBuffer.h"
#include "RHI/RHI.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Lunex {
	
	struct SkyboxRendererData {
		Ref<Shader> SkyboxShader;
		Ref<VertexArray> CubeVAO;
		Ref<UniformBuffer> SkyboxUniformBuffer;
		
		struct SkyboxUniformData {
			glm::mat4 ViewRotation;
			glm::mat4 Projection;
			float Intensity;
			float Rotation;
			float Blur;
			float MaxMipLevel;
			glm::vec3 Tint;
			float _padding;
		};
		
		SkyboxUniformData UniformData;
		
		// Global skybox settings (controlled from SettingsPanel)
		bool Enabled = true;
		Ref<EnvironmentMap> GlobalEnvironment;
		glm::vec3 BackgroundColor = glm::vec3(0.2f, 0.2f, 0.2f);  // Default gray when no HDRI
	};
	
	static SkyboxRendererData s_Data;
	
	void SkyboxRenderer::Init() {
		LNX_PROFILE_FUNCTION();
		
		// Load skybox shader
		s_Data.SkyboxShader = Shader::Create("assets/shaders/Skybox.glsl");
		
		// Create cube vertex array
		float cubeVertices[] = {
			// Positions          
			-1.0f,  1.0f, -1.0f,
			-1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,

			-1.0f, -1.0f,  1.0f,
			-1.0f, -1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f, -1.0f,
			-1.0f,  1.0f,  1.0f,
			-1.0f, -1.0f,  1.0f,

			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,

			-1.0f, -1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f, -1.0f,  1.0f,
			-1.0f, -1.0f,  1.0f,

			-1.0f,  1.0f, -1.0f,
			 1.0f,  1.0f, -1.0f,
			 1.0f,  1.0f,  1.0f,
			 1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f,  1.0f,
			-1.0f,  1.0f, -1.0f,

			-1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f,
			 1.0f, -1.0f, -1.0f,
			 1.0f, -1.0f, -1.0f,
			-1.0f, -1.0f,  1.0f,
			 1.0f, -1.0f,  1.0f
		};
		
		s_Data.CubeVAO = VertexArray::Create();
		
		Ref<VertexBuffer> cubeVBO = VertexBuffer::Create(cubeVertices, sizeof(cubeVertices));
		cubeVBO->SetLayout({
			{ ShaderDataType::Float3, "a_Position" }
		});
		s_Data.CubeVAO->AddVertexBuffer(cubeVBO);
		
		// Create uniform buffer for skybox data (binding point 4)
		s_Data.SkyboxUniformBuffer = UniformBuffer::Create(sizeof(SkyboxRendererData::SkyboxUniformData), 4);
		
		// Create global environment map
		s_Data.GlobalEnvironment = CreateRef<EnvironmentMap>();
		
		LNX_LOG_INFO("SkyboxRenderer initialized");
	}
	
	void SkyboxRenderer::Shutdown() {
		LNX_PROFILE_FUNCTION();
		
		s_Data.SkyboxShader = nullptr;
		s_Data.CubeVAO = nullptr;
		s_Data.SkyboxUniformBuffer = nullptr;
		s_Data.GlobalEnvironment = nullptr;
		
		LNX_LOG_INFO("SkyboxRenderer shutdown");
	}
	
	void SkyboxRenderer::Render(const EnvironmentMap& environment, const EditorCamera& camera) {
		RenderInternal(environment, camera.GetViewMatrix(), camera.GetProjection());
	}
	
	void SkyboxRenderer::Render(const EnvironmentMap& environment, const Camera& camera, const glm::mat4& transform) {
		glm::mat4 view = glm::inverse(transform);
		RenderInternal(environment, view, camera.GetProjection());
	}
	
	void SkyboxRenderer::Render(const EnvironmentMap& environment, const glm::mat4& view, const glm::mat4& projection) {
		RenderInternal(environment, view, projection);
	}
	
	void SkyboxRenderer::RenderInternal(const EnvironmentMap& environment, 
										 const glm::mat4& view, 
										 const glm::mat4& projection) {
		LNX_PROFILE_FUNCTION();
		
		if (!environment.IsLoaded() || !environment.GetEnvironmentMap()) {
			return;
		}
		
		auto* cmdList = RHI::GetImmediateCommandList();
		if (!cmdList) return;
		
		// Save current depth function
		RHI::CompareFunc previousDepthFunc = cmdList->GetDepthFunc();
		
		// Change depth function to LEQUAL so skybox passes at z = 1.0
		cmdList->SetDepthFunc(RHI::CompareFunc::LessEqual);
		
		// Don't write to depth buffer (skybox should always be at maximum depth)
		cmdList->SetDepthMask(false);
		
		// Update uniform buffer
		s_Data.UniformData.ViewRotation = view;
		s_Data.UniformData.Projection = projection;
		s_Data.UniformData.Intensity = environment.GetIntensity();
		s_Data.UniformData.Rotation = environment.GetRotation();
		s_Data.UniformData.Blur = environment.GetBlur();
		s_Data.UniformData.MaxMipLevel = static_cast<float>(environment.GetEnvironmentMap()->GetMipLevelCount() - 1);
		s_Data.UniformData.Tint = environment.GetTint();
		
		s_Data.SkyboxUniformBuffer->SetData(&s_Data.UniformData, sizeof(SkyboxRendererData::SkyboxUniformData));
		
		// Bind shader and environment map
		s_Data.SkyboxShader->Bind();
		environment.GetEnvironmentMap()->Bind(7);
		
		// Draw cube
		s_Data.CubeVAO->Bind();
		cmdList->DrawArrays(36);
		
		// Restore state
		cmdList->SetDepthMask(true);
		cmdList->SetDepthFunc(previousDepthFunc);
	}
	
	void SkyboxRenderer::RenderSolidColor(const glm::vec3& color, const EditorCamera& camera) {
		// For solid color background, we just clear with that color
		// This is handled by RenderCommand::SetClearColor and Clear
		// This function is a placeholder for future gradient/procedural sky
	}
	
	// ========================================
	// GLOBAL SKYBOX SETTINGS
	// ========================================
	
	void SkyboxRenderer::SetEnabled(bool enabled) {
		s_Data.Enabled = enabled;
	}
	
	bool SkyboxRenderer::IsEnabled() {
		return s_Data.Enabled;
	}
	
	bool SkyboxRenderer::LoadHDRI(const std::string& path) {
		if (path.empty()) {
			s_Data.GlobalEnvironment = CreateRef<EnvironmentMap>();
			return false;
		}
		return s_Data.GlobalEnvironment->LoadFromHDRI(path);
	}
	
	void SkyboxRenderer::SetIntensity(float intensity) {
		if (s_Data.GlobalEnvironment) {
			s_Data.GlobalEnvironment->SetIntensity(intensity);
		}
	}
	
	float SkyboxRenderer::GetIntensity() {
		return s_Data.GlobalEnvironment ? s_Data.GlobalEnvironment->GetIntensity() : 1.0f;
	}
	
	void SkyboxRenderer::SetRotation(float rotationDegrees) {
		if (s_Data.GlobalEnvironment) {
			s_Data.GlobalEnvironment->SetRotation(glm::radians(rotationDegrees));
		}
	}
	
	float SkyboxRenderer::GetRotation() {
		return s_Data.GlobalEnvironment ? glm::degrees(s_Data.GlobalEnvironment->GetRotation()) : 0.0f;
	}
	
	void SkyboxRenderer::SetTint(const glm::vec3& tint) {
		if (s_Data.GlobalEnvironment) {
			s_Data.GlobalEnvironment->SetTint(tint);
		}
	}
	
	glm::vec3 SkyboxRenderer::GetTint() {
		return s_Data.GlobalEnvironment ? s_Data.GlobalEnvironment->GetTint() : glm::vec3(1.0f);
	}
	
	void SkyboxRenderer::SetBlur(float blur) {
		if (s_Data.GlobalEnvironment) {
			s_Data.GlobalEnvironment->SetBlur(blur);
		}
	}
	
	float SkyboxRenderer::GetBlur() {
		return s_Data.GlobalEnvironment ? s_Data.GlobalEnvironment->GetBlur() : 0.0f;
	}
	
	void SkyboxRenderer::SetBackgroundColor(const glm::vec3& color) {
		s_Data.BackgroundColor = color;
	}
	
	glm::vec3 SkyboxRenderer::GetBackgroundColor() {
		return s_Data.BackgroundColor;
	}
	
	bool SkyboxRenderer::HasEnvironmentLoaded() {
		return s_Data.GlobalEnvironment && s_Data.GlobalEnvironment->IsLoaded();
	}
	
	Ref<EnvironmentMap> SkyboxRenderer::GetGlobalEnvironment() {
		return s_Data.GlobalEnvironment;
	}
	
	void SkyboxRenderer::RenderGlobalSkybox(const EditorCamera& camera) {
		if (!s_Data.Enabled) {
			return;
		}
		
		if (HasEnvironmentLoaded()) {
			RenderInternal(*s_Data.GlobalEnvironment, camera.GetViewMatrix(), camera.GetProjection());
		} else {
			// Apply background color as clear color when no environment is loaded
			ApplyBackgroundClearColor();
		}
	}
	
	void SkyboxRenderer::RenderGlobalSkybox(const Camera& camera, const glm::mat4& transform) {
		if (!s_Data.Enabled) {
			return;
		}
		
		if (HasEnvironmentLoaded()) {
			glm::mat4 view = glm::inverse(transform);
			RenderInternal(*s_Data.GlobalEnvironment, view, camera.GetProjection());
		} else {
			// Apply background color as clear color when no environment is loaded
			ApplyBackgroundClearColor();
		}
	}

	void SkyboxRenderer::ApplyBackgroundClearColor() {
		// Apply the stored background color to the renderer clear color
		auto* cmdList = RHI::GetImmediateCommandList();
		if (cmdList) {
			cmdList->SetClearColor(glm::vec4(s_Data.BackgroundColor, 1.0f));
		}
	}
	
}
