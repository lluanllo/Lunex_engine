#include "stpch.h"
#include "AnimationPreviewRenderer.h"

#include "Renderer/Renderer3D.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/StorageBuffer.h"
#include "Renderer/UniformBuffer.h"
#include "Renderer/Shader.h"
#include "RHI/RHI.h"
#include "Log/Log.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>

namespace Lunex {

	AnimationPreviewRenderer::AnimationPreviewRenderer() {
	}

	AnimationPreviewRenderer::~AnimationPreviewRenderer() {
		Shutdown();
	}

	void AnimationPreviewRenderer::Init(uint32_t width, uint32_t height) {
		if (m_Initialized) return;
		
		m_Width = width;
		m_Height = height;
		
		// Create framebuffer
		FramebufferSpecification spec;
		spec.Width = width;
		spec.Height = height;
		spec.Attachments = { 
			FramebufferTextureFormat::RGBA8,
			FramebufferTextureFormat::RED_INTEGER,
			FramebufferTextureFormat::Depth 
		};
		m_Framebuffer = Framebuffer::Create(spec);
		
		// Create preview scene with lights
		InitializePreviewScene();
		
		// Load skinned mesh shader
		m_SkinnedShader = Shader::Create("assets/shaders/SkinnedMesh3D.glsl");
		
		// Create bone matrix buffer
		m_BoneMatrixBuffer = StorageBuffer::Create(256 * sizeof(glm::mat4), 10);
		
		// Initialize bone visualization
		m_BoneViz.Init();
		
		// Initialize camera
		m_Camera = EditorCamera(45.0f, 1.0f, 0.01f, 1000.0f);
		m_Camera.SetViewportSize(width, height);
		
		m_Initialized = true;
		LNX_LOG_INFO("AnimationPreviewRenderer initialized ({0}x{1})", width, height);
	}
	
	void AnimationPreviewRenderer::InitializePreviewScene() {
		m_PreviewScene = CreateRef<Scene>();
		
		// Main directional light
		Entity mainLight = m_PreviewScene->CreateEntity("Main Light");
		auto& lightComp = mainLight.AddComponent<LightComponent>(LightType::Directional);
		auto& lightTransform = mainLight.GetComponent<TransformComponent>();
		lightTransform.Rotation = glm::vec3(glm::radians(-45.0f), glm::radians(45.0f), 0.0f);
		lightComp.SetColor(glm::vec3(1.0f, 0.98f, 0.95f));
		lightComp.SetIntensity(2.0f);
		
		// Fill light
		Entity fillLight = m_PreviewScene->CreateEntity("Fill Light");
		auto& fillLightComp = fillLight.AddComponent<LightComponent>(LightType::Directional);
		auto& fillLightTransform = fillLight.GetComponent<TransformComponent>();
		fillLightTransform.Rotation = glm::vec3(glm::radians(-30.0f), glm::radians(-135.0f), 0.0f);
		fillLightComp.SetColor(glm::vec3(0.5f, 0.6f, 0.7f));
		fillLightComp.SetIntensity(0.5f);
	}

	void AnimationPreviewRenderer::Shutdown() {
		m_BoneViz.Shutdown();
		m_Framebuffer = nullptr;
		m_Model = nullptr;
		m_Skeleton = nullptr;
		m_Clip = nullptr;
		m_BoneMatrixBuffer = nullptr;
		m_SkinnedShader = nullptr;
		m_PreviewScene = nullptr;
		m_Initialized = false;
	}

	void AnimationPreviewRenderer::Resize(uint32_t width, uint32_t height) {
		if (width == 0 || height == 0) return;
		if (m_Width == width && m_Height == height) return;
		
		m_Width = width;
		m_Height = height;
		
		if (m_Framebuffer) {
			m_Framebuffer->Resize(width, height);
		}
		
		m_Camera.SetViewportSize(width, height);
	}

	// ============================================================================
	// ASSET SETTERS
	// ============================================================================

	void AnimationPreviewRenderer::SetSkinnedModel(Ref<SkinnedModel> model) {
		m_Model = model;
		
		if (model && model->HasBones()) {
			int boneCount = model->GetBoneCount();
			m_BoneMatrices.resize(boneCount, glm::mat4(1.0f));
			m_TempModelSpaceMatrices.resize(boneCount, glm::mat4(1.0f));
			m_BoneMatricesDirty = true;
			
			CalculateModelBounds();
			LNX_LOG_INFO("AnimationPreviewRenderer: Model set with {0} bones", boneCount);
		}
	}

	void AnimationPreviewRenderer::SetSkeleton(Ref<SkeletonAsset> skeleton) {
		m_Skeleton = skeleton;
		
		if (skeleton) {
			uint32_t boneCount = skeleton->GetJointCount();
			m_BoneMatrices.resize(boneCount, glm::mat4(1.0f));
			m_TempPose.resize(boneCount);
			m_TempModelSpaceMatrices.resize(boneCount);
			m_BoneMatricesDirty = true;
			
			SampleBindPose();
			CalculateModelBounds();
			LNX_LOG_INFO("AnimationPreviewRenderer: Skeleton set with {0} joints", boneCount);
		}
	}

	void AnimationPreviewRenderer::SetAnimationClip(Ref<AnimationClipAsset> clip) {
		m_Clip = clip;
		m_CurrentTime = 0.0f;
		
		if (clip && m_Skeleton) {
			SampleAnimation(0.0f);
		} else if (m_Skeleton) {
			SampleBindPose();
		}
	}
	
	void AnimationPreviewRenderer::CalculateModelBounds() {
		m_ModelBoundsMin = glm::vec3(-0.5f, 0.0f, -0.5f);
		m_ModelBoundsMax = glm::vec3(0.5f, 2.0f, 0.5f);
		
		if (m_Model) {
			const auto& meshes = m_Model->GetMeshes();
			if (!meshes.empty()) {
				m_ModelBoundsMin = glm::vec3(std::numeric_limits<float>::max());
				m_ModelBoundsMax = glm::vec3(std::numeric_limits<float>::lowest());
				
				for (const auto& mesh : meshes) {
					const auto& vertices = mesh->GetVertices();
					for (const auto& v : vertices) {
						m_ModelBoundsMin = glm::min(m_ModelBoundsMin, v.Position);
						m_ModelBoundsMax = glm::max(m_ModelBoundsMax, v.Position);
					}
				}
			}
		}
		
		glm::vec3 center = (m_ModelBoundsMin + m_ModelBoundsMax) * 0.5f;
		glm::vec3 size = m_ModelBoundsMax - m_ModelBoundsMin;
		float maxExtent = glm::max(size.x, glm::max(size.y, size.z));
		
		m_CameraTarget = center;
		m_CameraDistance = maxExtent * 1.5f;
		m_CameraDistance = glm::clamp(m_CameraDistance, 1.0f, 50.0f);
		
		LNX_LOG_INFO("AnimationPreviewRenderer: Model bounds calculated, camera distance: {0}", m_CameraDistance);
	}

	// ============================================================================
	// PLAYBACK CONTROL
	// ============================================================================

	void AnimationPreviewRenderer::Play() { m_IsPlaying = true; }
	void AnimationPreviewRenderer::Pause() { m_IsPlaying = false; }
	
	void AnimationPreviewRenderer::Stop() {
		m_IsPlaying = false;
		m_CurrentTime = 0.0f;
		if (m_Skeleton) SampleAnimation(0.0f);
	}

	void AnimationPreviewRenderer::SetTime(float time) {
		m_CurrentTime = time;
		if (m_Skeleton) SampleAnimation(time);
	}

	void AnimationPreviewRenderer::SetPlaybackSpeed(float speed) { m_PlaybackSpeed = speed; }
	void AnimationPreviewRenderer::SetLoop(bool loop) { m_Loop = loop; }
	float AnimationPreviewRenderer::GetDuration() const { return m_Clip ? m_Clip->GetDuration() : 0.0f; }
	
	float AnimationPreviewRenderer::GetNormalizedTime() const {
		float duration = GetDuration();
		return (duration > 0.0f) ? (m_CurrentTime / duration) : 0.0f;
	}

	// ============================================================================
	// CAMERA CONTROL
	// ============================================================================

	void AnimationPreviewRenderer::RotateCamera(float deltaX, float deltaY) {
		m_CameraYaw += deltaX * 0.01f;
		m_CameraPitch += deltaY * 0.01f;
		m_CameraPitch = glm::clamp(m_CameraPitch, -1.5f, 1.5f);
	}

	void AnimationPreviewRenderer::ZoomCamera(float delta) {
		m_CameraDistance -= delta * 0.5f;
		m_CameraDistance = glm::clamp(m_CameraDistance, 0.5f, 50.0f);
	}

	void AnimationPreviewRenderer::ResetCamera() {
		CalculateModelBounds();
		m_CameraYaw = 0.0f;
		m_CameraPitch = 0.3f;
	}

	// ============================================================================
	// RENDER - Uses Renderer3D like MaterialPreviewRenderer
	// ============================================================================

	void AnimationPreviewRenderer::Render(float deltaTime) {
		if (!m_Initialized || !m_Framebuffer || !m_Model || !m_Skeleton) return;
		
		// Update animation
		if (m_IsPlaying && m_Clip) {
			UpdateAnimation(deltaTime);
		}
		
		// Calculate camera position (orbit camera)
		glm::vec3 cameraPos;
		cameraPos.x = m_CameraTarget.x + m_CameraDistance * cos(m_CameraPitch) * sin(m_CameraYaw);
		cameraPos.y = m_CameraTarget.y + m_CameraDistance * sin(m_CameraPitch);
		cameraPos.z = m_CameraTarget.z + m_CameraDistance * cos(m_CameraPitch) * cos(m_CameraYaw);
		
		// Calculate view and projection matrices manually for preview
		glm::mat4 viewMatrix = glm::lookAt(cameraPos, m_CameraTarget, glm::vec3(0, 1, 0));
		float aspect = (m_Height > 0) ? (static_cast<float>(m_Width) / static_cast<float>(m_Height)) : 1.0f;
		glm::mat4 projMatrix = glm::perspective(glm::radians(45.0f), aspect, 0.01f, 1000.0f);
		m_LastViewProjection = projMatrix * viewMatrix;
		
		// Update EditorCamera for Renderer3D (it needs the camera object)
		m_Camera.SetViewportSize(static_cast<float>(m_Width), static_cast<float>(m_Height));
		
		auto* cmdList = RHI::GetImmediateCommandList();
		if (!cmdList) return;
		
		// ========== BIND FRAMEBUFFER ==========
		m_Framebuffer->Bind();
		
		cmdList->SetViewport(0, 0, m_Width, m_Height);
		cmdList->SetClearColor({ 0.15f, 0.15f, 0.18f, 1.0f });
		cmdList->Clear();
		
		m_Framebuffer->ClearAttachment(1, -1);
		
		// ========== RENDER USING DIRECT OPENGL ==========
		// We avoid Renderer3D to prevent UBO conflicts with main viewport
		
		// Upload bone matrices
		UploadBoneMatrices();
		
		// Draw skinned mesh with direct shader setup
		if (m_SkinnedShader && m_BoneMatrixBuffer) {
			glEnable(GL_DEPTH_TEST);
			
			m_SkinnedShader->Bind();
			m_BoneMatrixBuffer->Bind();
			
			// Set camera/transform uniforms directly (shader has UBOs but we set them)
			// The shader uses UBOs at binding 0 (Camera) and 1 (Transform)
			// We need to create temporary UBOs or set uniform data directly
			
			// For now, set the uniforms that the shader might accept as fallback
			m_SkinnedShader->SetMat4("u_ViewProjection", m_LastViewProjection);
			m_SkinnedShader->SetMat4("u_Transform", glm::mat4(1.0f));
			m_SkinnedShader->SetFloat4("u_Color", glm::vec4(0.75f, 0.75f, 0.8f, 1.0f));
			m_SkinnedShader->SetFloat("u_Metallic", 0.0f);
			m_SkinnedShader->SetFloat("u_Roughness", 0.5f);
			m_SkinnedShader->SetFloat("u_Specular", 0.5f);
			m_SkinnedShader->SetFloat3("u_ViewPos", cameraPos);
			m_SkinnedShader->SetInt("u_UseSkinning", 1);
			m_SkinnedShader->SetInt("u_BoneCount", static_cast<int>(m_BoneMatrices.size()));
			m_SkinnedShader->SetInt("u_UseAlbedoMap", 0);
			m_SkinnedShader->SetInt("u_UseNormalMap", 0);
			m_SkinnedShader->SetInt("u_UseMetallicMap", 0);
			m_SkinnedShader->SetInt("u_UseRoughnessMap", 0);
			m_SkinnedShader->SetInt("u_NumLights", 0); // No lights for simple preview
			
			m_Model->SetEntityID(-1);
			m_Model->Draw(m_SkinnedShader);
			
			m_SkinnedShader->Unbind();
		}
		
		// ========== UNBIND FRAMEBUFFER ==========
		m_Framebuffer->Unbind();
	}

	void AnimationPreviewRenderer::UpdateAnimation(float deltaTime) {
		if (!m_Clip) return;
		
		m_CurrentTime += deltaTime * m_PlaybackSpeed;
		float duration = m_Clip->GetDuration();
		
		if (duration > 0.0f) {
			if (m_Loop) {
				while (m_CurrentTime >= duration) m_CurrentTime -= duration;
				while (m_CurrentTime < 0.0f) m_CurrentTime += duration;
			} else if (m_CurrentTime >= duration) {
				m_CurrentTime = duration;
				m_IsPlaying = false;
			}
		}
		
		SampleAnimation(m_CurrentTime);
	}

	void AnimationPreviewRenderer::SampleBindPose() {
		if (!m_Skeleton) return;
		
		uint32_t boneCount = m_Skeleton->GetJointCount();
		if (boneCount == 0) return;
		
		m_TempPose.resize(boneCount);
		m_TempModelSpaceMatrices.resize(boneCount);
		m_BoneMatrices.resize(boneCount);
		
		for (uint32_t i = 0; i < boneCount; i++) {
			const auto& joint = m_Skeleton->GetJoint(i);
			m_TempPose[i].Translation = joint.LocalPosition;
			m_TempPose[i].Rotation = joint.LocalRotation;
			m_TempPose[i].Scale = joint.LocalScale;
		}
		
		for (uint32_t i = 0; i < boneCount; i++) {
			const auto& joint = m_Skeleton->GetJoint(i);
			glm::mat4 localTransform = m_TempPose[i].ToMatrix();
			
			if (joint.ParentIndex >= 0 && joint.ParentIndex < static_cast<int32_t>(i)) {
				m_TempModelSpaceMatrices[i] = m_TempModelSpaceMatrices[joint.ParentIndex] * localTransform;
			} else {
				m_TempModelSpaceMatrices[i] = localTransform;
			}
		}
		
		auto inverseBindPoses = m_Skeleton->GetInverseBindPoseMatrices();
		for (uint32_t i = 0; i < boneCount; i++) {
			m_BoneMatrices[i] = m_TempModelSpaceMatrices[i] * inverseBindPoses[i];
		}
		
		m_BoneMatricesDirty = true;
	}

	void AnimationPreviewRenderer::SampleAnimation(float time) {
		if (!m_Skeleton) return;
		
		uint32_t boneCount = m_Skeleton->GetJointCount();
		if (boneCount == 0) return;
		
		if (!m_Clip) {
			SampleBindPose();
			return;
		}
		
		m_TempPose.resize(boneCount);
		m_TempModelSpaceMatrices.resize(boneCount);
		
		for (uint32_t i = 0; i < boneCount; i++) {
			const auto& joint = m_Skeleton->GetJoint(i);
			m_TempPose[i].Translation = joint.LocalPosition;
			m_TempPose[i].Rotation = joint.LocalRotation;
			m_TempPose[i].Scale = joint.LocalScale;
		}
		
		for (const auto& channel : m_Clip->GetChannels()) {
			if (channel.JointIndex < 0 || channel.JointIndex >= static_cast<int32_t>(boneCount)) continue;
			
			AnimationKeyframe kf = channel.Sample(time);
			m_TempPose[channel.JointIndex].Translation = kf.Translation;
			m_TempPose[channel.JointIndex].Rotation = kf.Rotation;
			m_TempPose[channel.JointIndex].Scale = kf.Scale;
		}
		
		for (uint32_t i = 0; i < boneCount; i++) {
			const auto& joint = m_Skeleton->GetJoint(i);
			glm::mat4 localTransform = m_TempPose[i].ToMatrix();
			
			if (joint.ParentIndex >= 0 && joint.ParentIndex < static_cast<int32_t>(i)) {
				m_TempModelSpaceMatrices[i] = m_TempModelSpaceMatrices[joint.ParentIndex] * localTransform;
			} else {
				m_TempModelSpaceMatrices[i] = localTransform;
			}
		}
		
		m_BoneMatrices.resize(boneCount);
		auto inverseBindPoses = m_Skeleton->GetInverseBindPoseMatrices();
		
		for (uint32_t i = 0; i < boneCount; i++) {
			m_BoneMatrices[i] = m_TempModelSpaceMatrices[i] * inverseBindPoses[i];
		}
		
		m_BoneMatricesDirty = true;
	}

	void AnimationPreviewRenderer::UploadBoneMatrices() {
		if (!m_BoneMatricesDirty || m_BoneMatrices.empty() || !m_BoneMatrixBuffer) return;
		
		m_BoneMatrixBuffer->SetData(
			m_BoneMatrices.data(),
			static_cast<uint32_t>(m_BoneMatrices.size() * sizeof(glm::mat4))
		);
		
		m_BoneMatricesDirty = false;
	}

	void AnimationPreviewRenderer::RenderSkeleton(const glm::mat4& viewProjection) {
		// Skeleton visualization disabled to avoid state pollution
		// Will be re-enabled with proper isolated line renderer
	}
	
	int32_t AnimationPreviewRenderer::PickBone(const glm::vec2& screenPos) {
		if (!m_Skeleton || m_TempModelSpaceMatrices.empty()) return -1;
		m_BoneViz.UpdateBones(m_Skeleton, m_TempModelSpaceMatrices);
		return m_BoneViz.PickBone(screenPos, m_LastViewProjection);
	}
	
	glm::mat4 AnimationPreviewRenderer::GetViewProjectionMatrix() const {
		return m_LastViewProjection;
	}

	uint32_t AnimationPreviewRenderer::GetRendererID() const {
		return m_Framebuffer ? m_Framebuffer->GetColorAttachmentRendererID() : 0;
	}

}
