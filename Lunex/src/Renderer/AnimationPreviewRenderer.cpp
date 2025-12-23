#include "stpch.h"
#include "AnimationPreviewRenderer.h"

#include "Renderer/Renderer3D.h"
#include "Renderer/Renderer2D.h"
#include "Renderer/StorageBuffer.h"
#include "Renderer/UniformBuffer.h"
#include "Renderer/Shader.h"
#include "RHI/RHI.h"
#include "Log/Log.h"

#include <glm/gtc/matrix_transform.hpp>
#include <glad/glad.h>

namespace Lunex {

	// ============================================================================
	// INTERNAL DATA - Uniforms for preview rendering (isolated from main scene)
	// ============================================================================
	struct PreviewCameraData {
		glm::mat4 ViewProjection;
	};
	
	struct PreviewTransformData {
		glm::mat4 Transform;
	};
	
	struct PreviewMaterialData {
		glm::vec4 Color = glm::vec4(0.8f, 0.8f, 0.8f, 1.0f);
		float Metallic = 0.0f;
		float Roughness = 0.5f;
		float Specular = 0.5f;
		float EmissionIntensity = 0.0f;
		glm::vec3 EmissionColor = glm::vec3(0.0f);
		float _padding1 = 0.0f;
		glm::vec3 ViewPos = glm::vec3(0.0f);
		
		int UseAlbedoMap = 0;
		int UseNormalMap = 0;
		int UseMetallicMap = 0;
		int UseRoughnessMap = 0;
		int UseSpecularMap = 0;
		int UseEmissionMap = 0;
		int UseAOMap = 0;
		float _padding2 = 0.0f;
		
		float MetallicMultiplier = 1.0f;
		float RoughnessMultiplier = 1.0f;
		float SpecularMultiplier = 1.0f;
		float AOMultiplier = 1.0f;
	};
	
	struct PreviewSkinningData {
		int UseSkinning = 1;
		int BoneCount = 0;
		float _padding1 = 0.0f;
		float _padding2 = 0.0f;
	};
	
	struct PreviewLightsHeader {
		int NumLights = 0;
		float _padding1 = 0.0f;
		float _padding2 = 0.0f;
		float _padding3 = 0.0f;
	};
	
	struct PreviewLightData {
		glm::vec4 Position;    // xyz = position, w = type
		glm::vec4 Direction;   // xyz = direction, w = range
		glm::vec4 Color;       // rgb = color, a = intensity
		glm::vec4 Params;      // x = innerCone, y = outerCone, z = castShadows, w = shadowMapIndex
		glm::vec4 Attenuation; // x = constant, y = linear, z = quadratic, w = unused
	};

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
		
		// Create uniform buffers for ISOLATED preview rendering
		// These are separate from the main scene's uniform buffers
		m_CameraUBO = UniformBuffer::Create(sizeof(PreviewCameraData), 0);
		m_TransformUBO = UniformBuffer::Create(sizeof(PreviewTransformData), 1);
		m_MaterialUBO = UniformBuffer::Create(sizeof(PreviewMaterialData), 2);
		m_SkinningUBO = UniformBuffer::Create(sizeof(PreviewSkinningData), 11);
		
		// Create lights storage buffer for preview (simple 3-light setup)
		uint32_t lightsBufferSize = sizeof(PreviewLightsHeader) + sizeof(PreviewLightData) * 3;
		m_LightsSSBO = StorageBuffer::Create(lightsBufferSize, 3);
		
		// Load skinned mesh shader
		m_SkinnedShader = Shader::Create("assets/shaders/SkinnedMesh3D.glsl");
		
		// Initialize bone visualization
		m_BoneViz.Init();
		
		m_Initialized = true;
		LNX_LOG_INFO("AnimationPreviewRenderer initialized ({0}x{1})", width, height);
	}

	void AnimationPreviewRenderer::Shutdown() {
		m_BoneViz.Shutdown();
		m_Framebuffer = nullptr;
		m_Model = nullptr;
		m_Skeleton = nullptr;
		m_Clip = nullptr;
		m_BoneMatrixBuffer = nullptr;
		m_CameraUBO = nullptr;
		m_TransformUBO = nullptr;
		m_MaterialUBO = nullptr;
		m_SkinningUBO = nullptr;
		m_LightsSSBO = nullptr;
		m_SkinnedShader = nullptr;
		m_Initialized = false;
	}

	void AnimationPreviewRenderer::Resize(uint32_t width, uint32_t height) {
		if (width == 0 || height == 0) return;
		
		m_Width = width;
		m_Height = height;
		
		if (m_Framebuffer) {
			m_Framebuffer->Resize(width, height);
		}
	}

	// ============================================================================
	// ASSET SETTERS
	// ============================================================================

	void AnimationPreviewRenderer::SetSkinnedModel(Ref<SkinnedModel> model) {
		m_Model = model;
		
		// Initialize bone matrices if model has bones
		if (model && model->HasBones()) {
			int boneCount = model->GetBoneCount();
			m_BoneMatrices.resize(boneCount, glm::mat4(1.0f));
			m_TempModelSpaceMatrices.resize(boneCount, glm::mat4(1.0f));
			m_BoneMatricesDirty = true;
			
			LNX_LOG_INFO("AnimationPreviewRenderer: Model set with {0} bones", boneCount);
		}
	}

	void AnimationPreviewRenderer::SetSkeleton(Ref<SkeletonAsset> skeleton) {
		m_Skeleton = skeleton;
		
		// Initialize bone matrices
		if (skeleton) {
			uint32_t boneCount = skeleton->GetJointCount();
			m_BoneMatrices.resize(boneCount, glm::mat4(1.0f));
			m_TempPose.resize(boneCount);
			m_TempModelSpaceMatrices.resize(boneCount);
			m_BoneMatricesDirty = true;
			
			// Set camera target to model center (estimate hip height)
			m_CameraTarget = glm::vec3(0.0f, 1.0f, 0.0f);
			
			// Sample bind pose immediately
			SampleBindPose();
			
			LNX_LOG_INFO("AnimationPreviewRenderer: Skeleton set with {0} joints", boneCount);
		}
	}

	void AnimationPreviewRenderer::SetAnimationClip(Ref<AnimationClipAsset> clip) {
		m_Clip = clip;
		m_CurrentTime = 0.0f;
		
		// Sample initial pose
		if (clip && m_Skeleton) {
			SampleAnimation(0.0f);
		} else if (m_Skeleton) {
			// No clip, show bind pose
			SampleBindPose();
		}
	}

	// ============================================================================
	// PLAYBACK CONTROL
	// ============================================================================

	void AnimationPreviewRenderer::Play() {
		m_IsPlaying = true;
	}

	void AnimationPreviewRenderer::Pause() {
		m_IsPlaying = false;
	}

	void AnimationPreviewRenderer::Stop() {
		m_IsPlaying = false;
		m_CurrentTime = 0.0f;
		SampleAnimation(0.0f);
	}

	void AnimationPreviewRenderer::SetTime(float time) {
		m_CurrentTime = time;
		SampleAnimation(time);
	}

	void AnimationPreviewRenderer::SetPlaybackSpeed(float speed) {
		m_PlaybackSpeed = speed;
	}

	void AnimationPreviewRenderer::SetLoop(bool loop) {
		m_Loop = loop;
	}

	float AnimationPreviewRenderer::GetDuration() const {
		return m_Clip ? m_Clip->GetDuration() : 0.0f;
	}

	float AnimationPreviewRenderer::GetNormalizedTime() const {
		float duration = GetDuration();
		if (duration <= 0.0f) return 0.0f;
		return m_CurrentTime / duration;
	}

	// ============================================================================
	// CAMERA CONTROL
	// ============================================================================

	void AnimationPreviewRenderer::RotateCamera(float deltaX, float deltaY) {
		m_CameraYaw += deltaX * 0.01f;
		m_CameraPitch += deltaY * 0.01f;
		
		// Clamp pitch
		m_CameraPitch = glm::clamp(m_CameraPitch, -1.5f, 1.5f);
	}

	void AnimationPreviewRenderer::ZoomCamera(float delta) {
		m_CameraDistance -= delta * 0.5f;
		m_CameraDistance = glm::clamp(m_CameraDistance, 0.5f, 20.0f);
	}

	void AnimationPreviewRenderer::ResetCamera() {
		m_CameraDistance = 3.0f;
		m_CameraYaw = 0.0f;
		m_CameraPitch = 0.3f;
		m_CameraTarget = glm::vec3(0.0f, 1.0f, 0.0f);
	}

	// ============================================================================
	// RENDER - ISOLATED FROM MAIN SCENE
	// ============================================================================

	void AnimationPreviewRenderer::Render(float deltaTime) {
		if (!m_Initialized || !m_Framebuffer) return;
		
		// Update animation
		if (m_IsPlaying && m_Clip) {
			UpdateAnimation(deltaTime);
		}
		
		// ========== SAVE OPENGL STATE ==========
		GLint previousViewport[4];
		glGetIntegerv(GL_VIEWPORT, previousViewport);
		
		GLint previousFramebuffer;
		glGetIntegerv(GL_FRAMEBUFFER_BINDING, &previousFramebuffer);
		
		// Begin render to our isolated framebuffer
		m_Framebuffer->Bind();
		
		auto* cmdList = RHI::GetImmediateCommandList();
		if (cmdList) {
			cmdList->SetViewport(0, 0, m_Width, m_Height);
			cmdList->SetClearColor({ 0.15f, 0.15f, 0.18f, 1.0f });
			cmdList->Clear();
		}
		
		// Clear entity ID attachment
		m_Framebuffer->ClearAttachment(1, -1);
		
		// Calculate camera position
		glm::vec3 cameraPos;
		cameraPos.x = m_CameraTarget.x + m_CameraDistance * cos(m_CameraPitch) * sin(m_CameraYaw);
		cameraPos.y = m_CameraTarget.y + m_CameraDistance * sin(m_CameraPitch);
		cameraPos.z = m_CameraTarget.z + m_CameraDistance * cos(m_CameraPitch) * cos(m_CameraYaw);
		
		glm::mat4 viewMatrix = glm::lookAt(cameraPos, m_CameraTarget, glm::vec3(0, 1, 0));
		float aspect = static_cast<float>(m_Width) / static_cast<float>(m_Height);
		glm::mat4 projMatrix = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
		glm::mat4 viewProjection = projMatrix * viewMatrix;
		
		// Store for bone picking
		m_LastViewProjection = viewProjection;
		
		// Upload camera data to our isolated UBO
		PreviewCameraData cameraData;
		cameraData.ViewProjection = viewProjection;
		m_CameraUBO->SetData(&cameraData, sizeof(PreviewCameraData));
		
		// Setup preview lights
		SetupPreviewLights(cameraPos);
		
		// Render the mesh
		if (m_Model) {
			if (m_Skeleton && m_BoneMatrices.empty()) {
				SampleBindPose();
			}
			RenderMesh(cameraPos);
		}
		
		// Render skeleton overlay if enabled
		if (m_ShowSkeleton && m_Skeleton && !m_TempModelSpaceMatrices.empty()) {
			RenderSkeleton(viewProjection);
		}
		
		// Unbind our framebuffer
		m_Framebuffer->Unbind();
		
		// ========== RESTORE OPENGL STATE ==========
		glBindFramebuffer(GL_FRAMEBUFFER, previousFramebuffer);
		glViewport(previousViewport[0], previousViewport[1], previousViewport[2], previousViewport[3]);
	}
	
	void AnimationPreviewRenderer::SetupPreviewLights(const glm::vec3& cameraPos) {
		if (!m_LightsSSBO) return;
		
		// Create simple 3-light setup for preview
		std::vector<uint8_t> lightsData(sizeof(PreviewLightsHeader) + sizeof(PreviewLightData) * 3);
		
		PreviewLightsHeader* header = reinterpret_cast<PreviewLightsHeader*>(lightsData.data());
		header->NumLights = 2;
		
		PreviewLightData* lights = reinterpret_cast<PreviewLightData*>(lightsData.data() + sizeof(PreviewLightsHeader));
		
		// Main directional light
		lights[0].Position = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // type 0 = directional
		lights[0].Direction = glm::vec4(glm::normalize(glm::vec3(-1.0f, -1.0f, -1.0f)), 0.0f);
		lights[0].Color = glm::vec4(1.0f, 0.98f, 0.95f, 2.0f); // warm white, intensity 2
		lights[0].Params = glm::vec4(0.0f);
		lights[0].Attenuation = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
		
		// Fill light from opposite side
		lights[1].Position = glm::vec4(0.0f, 0.0f, 0.0f, 0.0f); // type 0 = directional
		lights[1].Direction = glm::vec4(glm::normalize(glm::vec3(1.0f, -0.5f, 1.0f)), 0.0f);
		lights[1].Color = glm::vec4(0.6f, 0.7f, 0.8f, 0.5f); // cool fill, intensity 0.5
		lights[1].Params = glm::vec4(0.0f);
		lights[1].Attenuation = glm::vec4(1.0f, 0.0f, 0.0f, 0.0f);
		
		m_LightsSSBO->SetData(lightsData.data(), static_cast<uint32_t>(lightsData.size()));
	}

	void AnimationPreviewRenderer::UpdateAnimation(float deltaTime) {
		if (!m_Clip) return;
		
		m_CurrentTime += deltaTime * m_PlaybackSpeed;
		
		float duration = m_Clip->GetDuration();
		if (duration > 0.0f) {
			if (m_Loop) {
				while (m_CurrentTime >= duration) {
					m_CurrentTime -= duration;
				}
				while (m_CurrentTime < 0.0f) {
					m_CurrentTime += duration;
				}
			} else {
				if (m_CurrentTime >= duration) {
					m_CurrentTime = duration;
					m_IsPlaying = false;
				}
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
		
		// Initialize with bind pose from skeleton
		for (uint32_t i = 0; i < boneCount; i++) {
			const auto& joint = m_Skeleton->GetJoint(i);
			m_TempPose[i].Translation = joint.LocalPosition;
			m_TempPose[i].Rotation = joint.LocalRotation;
			m_TempPose[i].Scale = joint.LocalScale;
		}
		
		// Build model-space matrices hierarchically
		for (uint32_t i = 0; i < boneCount; i++) {
			const auto& joint = m_Skeleton->GetJoint(i);
			glm::mat4 localTransform = m_TempPose[i].ToMatrix();
			
			if (joint.ParentIndex >= 0 && joint.ParentIndex < static_cast<int32_t>(i)) {
				m_TempModelSpaceMatrices[i] = m_TempModelSpaceMatrices[joint.ParentIndex] * localTransform;
			} else {
				m_TempModelSpaceMatrices[i] = localTransform;
			}
		}
		
		// Apply inverse bind pose
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
		
		// If no clip, use bind pose
		if (!m_Clip) {
			SampleBindPose();
			return;
		}
		
		m_TempPose.resize(boneCount);
		m_TempModelSpaceMatrices.resize(boneCount);
		
		// Initialize with bind pose from skeleton
		for (uint32_t i = 0; i < boneCount; i++) {
			const auto& joint = m_Skeleton->GetJoint(i);
			m_TempPose[i].Translation = joint.LocalPosition;
			m_TempPose[i].Rotation = joint.LocalRotation;
			m_TempPose[i].Scale = joint.LocalScale;
		}
		
		// Sample animation channels (overwrite with animation data)
		for (const auto& channel : m_Clip->GetChannels()) {
			if (channel.JointIndex < 0 || channel.JointIndex >= static_cast<int32_t>(boneCount)) {
				continue;
			}
			
			AnimationKeyframe kf = channel.Sample(time);
			m_TempPose[channel.JointIndex].Translation = kf.Translation;
			m_TempPose[channel.JointIndex].Rotation = kf.Rotation;
			m_TempPose[channel.JointIndex].Scale = kf.Scale;
		}
		
		// Build bone matrices hierarchically
		for (uint32_t i = 0; i < boneCount; i++) {
			const auto& joint = m_Skeleton->GetJoint(i);
			glm::mat4 localTransform = m_TempPose[i].ToMatrix();
			
			if (joint.ParentIndex >= 0 && joint.ParentIndex < static_cast<int32_t>(i)) {
				m_TempModelSpaceMatrices[i] = m_TempModelSpaceMatrices[joint.ParentIndex] * localTransform;
			} else {
				m_TempModelSpaceMatrices[i] = localTransform;
			}
		}
		
		// Apply inverse bind pose to get final skinning matrices
		m_BoneMatrices.resize(boneCount);
		auto inverseBindPoses = m_Skeleton->GetInverseBindPoseMatrices();
		
		for (uint32_t i = 0; i < boneCount; i++) {
			m_BoneMatrices[i] = m_TempModelSpaceMatrices[i] * inverseBindPoses[i];
		}
		
		m_BoneMatricesDirty = true;
	}

	void AnimationPreviewRenderer::UploadBoneMatrices() {
		if (!m_BoneMatricesDirty || m_BoneMatrices.empty()) return;
		
		// Create buffer if needed
		if (!m_BoneMatrixBuffer) {
			uint32_t bufferSize = static_cast<uint32_t>(m_BoneMatrices.size() * sizeof(glm::mat4));
			m_BoneMatrixBuffer = StorageBuffer::Create(bufferSize, 10);
		}
		
		// Upload
		if (m_BoneMatrixBuffer) {
			m_BoneMatrixBuffer->SetData(
				m_BoneMatrices.data(),
				static_cast<uint32_t>(m_BoneMatrices.size() * sizeof(glm::mat4))
			);
		}
		
		m_BoneMatricesDirty = false;
	}

	void AnimationPreviewRenderer::RenderMesh(const glm::vec3& cameraPos) {
		if (!m_Model || !m_SkinnedShader) return;
		
		// Upload bone matrices
		UploadBoneMatrices();
		
		// Upload transform (identity for preview, mesh is at origin)
		PreviewTransformData transformData;
		transformData.Transform = glm::mat4(1.0f);
		m_TransformUBO->SetData(&transformData, sizeof(PreviewTransformData));
		
		// Upload material (simple gray material for preview)
		PreviewMaterialData materialData;
		materialData.Color = glm::vec4(0.75f, 0.75f, 0.8f, 1.0f);
		materialData.Metallic = 0.0f;
		materialData.Roughness = 0.5f;
		materialData.Specular = 0.5f;
		materialData.ViewPos = cameraPos;
		m_MaterialUBO->SetData(&materialData, sizeof(PreviewMaterialData));
		
		// Upload skinning config
		PreviewSkinningData skinningData;
		skinningData.UseSkinning = 1;
		skinningData.BoneCount = static_cast<int>(m_BoneMatrices.size());
		m_SkinningUBO->SetData(&skinningData, sizeof(PreviewSkinningData));
		
		// Bind shader
		m_SkinnedShader->Bind();
		
		// Bind bone matrix buffer
		if (m_BoneMatrixBuffer) {
			m_BoneMatrixBuffer->Bind();
		}
		
		// Bind lights buffer
		if (m_LightsSSBO) {
			m_LightsSSBO->Bind();
		}
		
		// Draw the model
		m_Model->SetEntityID(-1);
		m_Model->Draw(m_SkinnedShader);
	}

	void AnimationPreviewRenderer::RenderSkeleton(const glm::mat4& viewProjection) {
		if (!m_Skeleton || m_TempModelSpaceMatrices.empty()) return;
		
		// Update bone visualization data
		m_BoneViz.UpdateBones(m_Skeleton, m_TempModelSpaceMatrices);
		
		// Begin 2D scene for line drawing
		// Note: We're already in our isolated framebuffer
		Renderer2D::BeginScene(viewProjection);
		
		// Render bones
		m_BoneViz.Render(viewProjection);
		
		Renderer2D::EndScene();
	}
	
	int32_t AnimationPreviewRenderer::PickBone(const glm::vec2& screenPos) {
		if (!m_Skeleton || m_TempModelSpaceMatrices.empty()) return -1;
		
		// Update bone positions first
		m_BoneViz.UpdateBones(m_Skeleton, m_TempModelSpaceMatrices);
		
		// Pick using last view-projection matrix
		return m_BoneViz.PickBone(screenPos, m_LastViewProjection);
	}
	
	glm::mat4 AnimationPreviewRenderer::GetViewProjectionMatrix() const {
		return m_LastViewProjection;
	}

	uint32_t AnimationPreviewRenderer::GetRendererID() const {
		if (m_Framebuffer) {
			return m_Framebuffer->GetColorAttachmentRendererID();
		}
		return 0;
	}

}
