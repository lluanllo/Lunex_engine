#pragma once

/**
 * @file AnimationPreviewRenderer.h
 * @brief Isolated preview renderer for skeletal animation
 * 
 * Uses Renderer3D and EditorCamera similar to MaterialPreviewRenderer.
 * Renders to its own framebuffer with auto-fit camera for model bounds.
 */

#include "Core/Core.h"
#include "Renderer/FrameBuffer.h"
#include "Renderer/StorageBuffer.h"
#include "Renderer/Shader.h"
#include "Renderer/BoneVisualization.h"
#include "Scene/Camera/EditorCamera.h"
#include "Scene/Components/AnimationComponents.h"
#include "Resources/Mesh/SkinnedModel.h"
#include "Assets/Animation/SkeletonAsset.h"
#include "Assets/Animation/AnimationClipAsset.h"

#include <glm/glm.hpp>
#include <vector>

namespace Lunex {

	// Forward declarations
	class Scene;

	class AnimationPreviewRenderer {
	public:
		AnimationPreviewRenderer();
		~AnimationPreviewRenderer();

		void Init(uint32_t width, uint32_t height);
		void Shutdown();
		void Resize(uint32_t width, uint32_t height);

		// Set the model and animation to preview
		void SetSkinnedModel(Ref<SkinnedModel> model);
		void SetSkeleton(Ref<SkeletonAsset> skeleton);
		void SetAnimationClip(Ref<AnimationClipAsset> clip);
		
		// Animation control
		void Play();
		void Pause();
		void Stop();
		void SetTime(float time);
		void SetPlaybackSpeed(float speed);
		void SetLoop(bool loop);
		
		// Render the preview scene
		void Render(float deltaTime);
		
		// Get the rendered texture for ImGui display
		uint32_t GetRendererID() const;
		Ref<Framebuffer> GetFramebuffer() const { return m_Framebuffer; }
		
		// Camera control
		void RotateCamera(float deltaX, float deltaY);
		void ZoomCamera(float delta);
		void ResetCamera();
		
		// State
		bool IsPlaying() const { return m_IsPlaying; }
		float GetCurrentTime() const { return m_CurrentTime; }
		float GetDuration() const;
		float GetNormalizedTime() const;
		
		// Bone visualization
		void SetShowSkeleton(bool show) { m_ShowSkeleton = show; }
		void SetShowBoneNames(bool show) { m_ShowBoneNames = show; }
		bool GetShowSkeleton() const { return m_ShowSkeleton; }
		bool GetShowBoneNames() const { return m_ShowBoneNames; }
		
		// Bone interaction
		BoneVisualization& GetBoneVisualization() { return m_BoneViz; }
		const BoneVisualization& GetBoneVisualization() const { return m_BoneViz; }
		
		int32_t GetSelectedBone() const { return m_BoneViz.GetSelectedBone(); }
		int32_t GetHoveredBone() const { return m_BoneViz.GetHoveredBone(); }
		void SetSelectedBone(int32_t boneIndex) { m_BoneViz.SetSelectedBone(boneIndex); }
		void SetHoveredBone(int32_t boneIndex) { m_BoneViz.SetHoveredBone(boneIndex); }
		
		// Bone picking
		int32_t PickBone(const glm::vec2& screenPos);
		
		// Get matrices for UI
		const std::vector<glm::mat4>& GetModelSpaceMatrices() const { return m_TempModelSpaceMatrices; }
		glm::mat4 GetViewProjectionMatrix() const;
		
		// Model bounds
		glm::vec3 GetModelBoundsMin() const { return m_ModelBoundsMin; }
		glm::vec3 GetModelBoundsMax() const { return m_ModelBoundsMax; }

	private:
		void InitializePreviewScene();
		void UpdateAnimation(float deltaTime);
		void SampleAnimation(float time);
		void SampleBindPose();
		void CalculateModelBounds();
		void UploadBoneMatrices();
		void RenderSkeleton(const glm::mat4& viewProjection);
		
	private:
		Ref<Framebuffer> m_Framebuffer;
		uint32_t m_Width = 400;
		uint32_t m_Height = 400;
		
		// Camera (EditorCamera for proper orbit behavior)
		EditorCamera m_Camera;
		float m_CameraDistance = 3.0f;
		float m_CameraYaw = 0.0f;
		float m_CameraPitch = 0.3f;
		glm::vec3 m_CameraTarget = glm::vec3(0.0f, 1.0f, 0.0f);
		glm::mat4 m_LastViewProjection = glm::mat4(1.0f);
		
		// Model bounds for auto-fit camera
		glm::vec3 m_ModelBoundsMin = glm::vec3(-0.5f, 0.0f, -0.5f);
		glm::vec3 m_ModelBoundsMax = glm::vec3(0.5f, 2.0f, 0.5f);
		
		// Assets
		Ref<SkinnedModel> m_Model;
		Ref<SkeletonAsset> m_Skeleton;
		Ref<AnimationClipAsset> m_Clip;
		
		// Animation state
		float m_CurrentTime = 0.0f;
		float m_PlaybackSpeed = 1.0f;
		bool m_IsPlaying = true;
		bool m_Loop = true;
		
		// Bone matrices
		std::vector<glm::mat4> m_BoneMatrices;
		Ref<StorageBuffer> m_BoneMatrixBuffer;
		bool m_BoneMatricesDirty = true;
		
		// Shader
		Ref<Shader> m_SkinnedShader;
		
		// Temp pose buffers
		std::vector<BoneTransform> m_TempPose;
		std::vector<glm::mat4> m_TempModelSpaceMatrices;
		
		// Preview scene with lights
		Ref<Scene> m_PreviewScene;
		
		// Bone visualization
		BoneVisualization m_BoneViz;
		bool m_ShowSkeleton = false;
		bool m_ShowBoneNames = false;
		
		bool m_Initialized = false;
	};

}
