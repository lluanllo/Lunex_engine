#pragma once

/**
 * @file AnimationEditorPanel.h
 * @brief Full-featured animation editor panel with 3D preview
 * 
 * Similar to Unreal's animation editor, this panel provides:
 *   - 3D preview viewport with orbiting camera
 *   - Timeline with playback controls
 *   - Animation clip management
 *   - Blend settings
 *   - Skeleton visualization with bone selection
 */

#include "Core/Core.h"
#include "Core/Timestep.h"
#include "Renderer/AnimationPreviewRenderer.h"
#include "Scene/Entity.h"
#include "Assets/Animation/AnimationClipAsset.h"
#include "Assets/Animation/SkeletonAsset.h"
#include "Resources/Mesh/SkinnedModel.h"

#include <imgui.h>
#include <string>
#include <vector>
#include <functional>

namespace Lunex {

	class AnimationEditorPanel {
	public:
		AnimationEditorPanel();
		~AnimationEditorPanel() = default;

		// Open the editor for a specific entity
		void Open(Entity entity);
		void Close();
		bool IsOpen() const { return m_IsOpen; }

		// Update (called every frame when open)
		void OnUpdate(Timestep ts);
		void OnImGuiRender();

		// Callbacks
		using OnAnimationChangedCallback = std::function<void(Entity, Ref<AnimationClipAsset>)>;
		void SetOnAnimationChangedCallback(OnAnimationChangedCallback callback) { m_OnAnimationChanged = callback; }
		
		// ========== FOCUS SYSTEM ==========
		// Returns true if the animation panel viewport is focused/hovered
		// Used to block main editor camera input ONLY when actively dragging
		bool IsViewportFocused() const { return m_ViewportFocused; }
		bool IsViewportHovered() const { return m_ViewportHovered; }
		
		// Only wants camera input when ACTIVELY interacting (dragging in viewport)
		// This prevents blocking main editor camera when just hovering
		bool WantsCameraInput() const { return m_ViewportDragging && m_IsOpen; }

	private:
		void RenderMenuBar();
		void RenderPreviewViewport();
		void RenderBoneLabels();
		void RenderTimeline();
		void RenderAnimationList();
		void RenderBlendSettings();
		void RenderSkeletonInfo();
		void RenderBoneHierarchy();
		void RenderPlaybackControls();
		
		// Import animation from file
		void ImportAnimation();
		
		// Timeline helpers
		std::string FormatTime(float seconds) const;
		
		// Preview control
		void HandleViewportInput();
		void HandleBonePicking();
		
		// Bone selection
		void SelectBone(int32_t boneIndex);
		void ScrollToBoneInList(int32_t boneIndex);
		
	private:
		bool m_IsOpen = false;
		Entity m_Entity;
		
		// Preview renderer
		AnimationPreviewRenderer m_PreviewRenderer;
		bool m_PreviewInitialized = false;
		
		// Animation clips for blending
		struct AnimationSlot {
			Ref<AnimationClipAsset> Clip;
			float Weight = 1.0f;
			float Offset = 0.0f;
			bool Loop = true;
			bool Enabled = true;
		};
		std::vector<AnimationSlot> m_AnimationSlots;
		int m_SelectedSlotIndex = -1;
		
		// Playback state
		bool m_IsPlaying = true;
		float m_CurrentTime = 0.0f;
		float m_PlaybackSpeed = 1.0f;
		bool m_Loop = true;
		
		// Timeline settings
		float m_TimelineZoom = 1.0f;
		float m_TimelineScroll = 0.0f;
		
		// Visualization
		bool m_ShowSkeleton = false;
		bool m_ShowBoneNames = false;
		bool m_ShowFloor = true;
		
		// Bone selection
		int32_t m_SelectedBoneIndex = -1;
		int32_t m_HoveredBoneIndex = -1;
		std::string m_BoneSearchFilter;
		bool m_ScrollToBone = false;
		
		// UI state - viewport focus
		bool m_ViewportFocused = false;
		bool m_ViewportHovered = false;
		bool m_ViewportDragging = false;
		glm::vec2 m_LastMousePos = glm::vec2(0.0f);
		glm::vec2 m_ViewportPos = glm::vec2(0.0f);
		glm::vec2 m_ViewportSize = glm::vec2(0.0f);
		
		// Callbacks
		OnAnimationChangedCallback m_OnAnimationChanged;
	};

}
