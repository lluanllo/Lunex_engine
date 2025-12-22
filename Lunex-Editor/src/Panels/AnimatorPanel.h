#pragma once

/**
 * @file AnimatorPanel.h
 * @brief Professional Animator Panel for skeletal animation
 * 
 * Features:
 *   - Timeline with playback controls
 *   - Animation clip list
 *   - Blending visualization
 *   - Preview controls
 *   - Animation import
 */

#include "Scene/Entity.h"
#include "Scene/Components/AnimationComponents.h"
#include "Assets/Animation/AnimationClipAsset.h"
#include "Assets/Animation/SkeletonAsset.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/glm.hpp>
#include <filesystem>
#include <functional>

namespace Lunex {

	class AnimatorPanel {
	public:
		AnimatorPanel();
		~AnimatorPanel() = default;

		void OnImGuiRender();
		void SetContext(Entity entity);
		
		// Callbacks
		using ImportCallback = std::function<void(const std::filesystem::path&)>;
		void SetImportCallback(ImportCallback callback) { m_ImportCallback = callback; }
		
		// State
		bool IsPlaying() const { return m_IsPlaying; }
		Entity GetSelectedEntity() const { return m_SelectedEntity; }

	private:
		void RenderToolbar();
		void RenderTimeline();
		void RenderClipList();
		void RenderBlendingInfo();
		void RenderSkeletonInfo();
		void RenderProperties();
		
		void PlayAnimation();
		void PauseAnimation();
		void StopAnimation();
		void SetTime(float time);
		
		std::string FormatTime(float seconds) const;
		
	private:
		Entity m_SelectedEntity;
		
		// Playback state (for editor preview)
		bool m_IsPlaying = false;
		float m_PreviewTime = 0.0f;
		float m_PlaybackSpeed = 1.0f;
		
		// Timeline state
		float m_TimelineZoom = 1.0f;
		float m_TimelineScroll = 0.0f;
		bool m_IsDraggingPlayhead = false;
		
		// Selected clip for editing
		int m_SelectedClipIndex = -1;
		
		// Import callback
		ImportCallback m_ImportCallback;
		
		// UI Colors (professional dark theme)
		ImU32 m_TimelineBgColor = IM_COL32(30, 30, 32, 255);
		ImU32 m_TimelineGridColor = IM_COL32(50, 50, 55, 255);
		ImU32 m_PlayheadColor = IM_COL32(255, 80, 80, 255);
		ImU32 m_ClipColor = IM_COL32(80, 140, 200, 255);
		ImU32 m_ClipHoverColor = IM_COL32(100, 160, 220, 255);
		ImU32 m_ClipSelectedColor = IM_COL32(120, 180, 240, 255);
	};

}
