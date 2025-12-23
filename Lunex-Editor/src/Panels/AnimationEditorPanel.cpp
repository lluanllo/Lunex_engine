#include "stpch.h"
#include "AnimationEditorPanel.h"

#include "Scene/Components.h"
#include "Scene/Components/AnimationComponents.h"
#include "Utils/PlatformUtils.h"
#include "Assets/Animation/AnimationImporter.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <glm/gtc/type_ptr.hpp>

namespace Lunex {

	AnimationEditorPanel::AnimationEditorPanel() {
	}

	void AnimationEditorPanel::Open(Entity entity) {
		m_Entity = entity;
		m_IsOpen = true;
		m_CurrentTime = 0.0f;
		m_IsPlaying = true;
		m_AnimationSlots.clear();
		m_SelectedSlotIndex = -1;
		
		// Initialize preview renderer
		if (!m_PreviewInitialized) {
			m_PreviewRenderer.Init(400, 400);
			m_PreviewInitialized = true;
		}
		
		// Load entity's animation data into preview
		if (entity && entity.HasComponent<SkeletalMeshComponent>()) {
			auto& skeletal = entity.GetComponent<SkeletalMeshComponent>();
			
			// Try to get SkinnedMeshModel
			Ref<SkinnedModel> model = skeletal.SkinnedMeshModel;
			
			// Fallback: Try to load from SourceMeshPath if model is null
			if (!model && !skeletal.SourceMeshPath.empty()) {
				LNX_LOG_INFO("AnimationEditorPanel: Loading SkinnedModel from SourceMeshPath: {0}", skeletal.SourceMeshPath);
				model = CreateRef<SkinnedModel>(skeletal.SourceMeshPath);
				if (model && model->IsValid()) {
					skeletal.SkinnedMeshModel = model;
					LNX_LOG_INFO("AnimationEditorPanel: SkinnedModel loaded successfully with {0} bones", model->GetBoneCount());
				} else {
					LNX_LOG_WARN("AnimationEditorPanel: Failed to load SkinnedModel from SourceMeshPath");
					model = nullptr;
				}
			}
			
			// Fallback: Try to load from MeshAsset source path
			if (!model && skeletal.Mesh && skeletal.Mesh->HasValidSource()) {
				std::string sourcePath = skeletal.Mesh->GetSourcePath().string();
				LNX_LOG_INFO("AnimationEditorPanel: Loading SkinnedModel from MeshAsset source: {0}", sourcePath);
				model = CreateRef<SkinnedModel>(sourcePath);
				if (model && model->IsValid()) {
					skeletal.SkinnedMeshModel = model;
					skeletal.SourceMeshPath = sourcePath;
					LNX_LOG_INFO("AnimationEditorPanel: SkinnedModel loaded successfully with {0} bones", model->GetBoneCount());
				} else {
					LNX_LOG_WARN("AnimationEditorPanel: Failed to load SkinnedModel from MeshAsset source");
					model = nullptr;
				}
			}
			
			if (model) {
				m_PreviewRenderer.SetSkinnedModel(model);
				LNX_LOG_INFO("AnimationEditorPanel: Preview model set ({0} bones)", model->GetBoneCount());
			} else {
				LNX_LOG_WARN("AnimationEditorPanel: No SkinnedModel available for preview");
			}
			
			// Set skeleton
			if (skeletal.Skeleton) {
				m_PreviewRenderer.SetSkeleton(skeletal.Skeleton);
				LNX_LOG_INFO("AnimationEditorPanel: Skeleton set ({0} joints)", skeletal.Skeleton->GetJointCount());
			} else {
				LNX_LOG_WARN("AnimationEditorPanel: No skeleton assigned to entity");
			}
		} else {
			LNX_LOG_WARN("AnimationEditorPanel: Entity has no SkeletalMeshComponent");
		}
		
		if (entity && entity.HasComponent<AnimatorComponent>()) {
			auto& animator = entity.GetComponent<AnimatorComponent>();
			
			if (animator.CurrentClip) {
				m_PreviewRenderer.SetAnimationClip(animator.CurrentClip);
				LNX_LOG_INFO("AnimationEditorPanel: Animation clip set: {0}", animator.CurrentClip->GetName());
				
				// Add to slots
				AnimationSlot slot;
				slot.Clip = animator.CurrentClip;
				slot.Weight = 1.0f;
				slot.Loop = animator.Loop;
				slot.Enabled = true;
				m_AnimationSlots.push_back(slot);
				m_SelectedSlotIndex = 0;
			} else {
				LNX_LOG_INFO("AnimationEditorPanel: No animation clip assigned");
			}
			
			m_PlaybackSpeed = animator.PlaybackSpeed;
			m_Loop = animator.Loop;
		}
		
		m_PreviewRenderer.ResetCamera();
		m_PreviewRenderer.Play();
		
		LNX_LOG_INFO("AnimationEditorPanel: Opened for entity '{0}'", entity ? entity.GetName() : "null");
	}

	void AnimationEditorPanel::Close() {
		m_IsOpen = false;
		m_Entity = {};
	}

	void AnimationEditorPanel::OnUpdate(Timestep ts) {
		if (!m_IsOpen) return;
		
		// Only render if we have valid model and skeleton
		bool canRender = false;
		if (m_Entity && m_Entity.HasComponent<SkeletalMeshComponent>()) {
			auto& skeletal = m_Entity.GetComponent<SkeletalMeshComponent>();
			canRender = skeletal.SkinnedMeshModel && skeletal.Skeleton;
		}
		
		if (!canRender) {
			// Just render empty background
			if (m_IsPlaying) {
				m_PreviewRenderer.Render(0.0f);
			}
			return;
		}
		
		// Update preview renderer
		float deltaTime = m_IsPlaying ? ts.GetSeconds() : 0.0f;
		m_PreviewRenderer.Render(deltaTime);
		m_CurrentTime = m_PreviewRenderer.GetCurrentTime();
	}

	void AnimationEditorPanel::OnImGuiRender() {
		if (!m_IsOpen) return;
		
		ImGui::SetNextWindowSize(ImVec2(1000, 700), ImGuiCond_FirstUseEver);
		
		std::string windowTitle = "Animation Editor";
		if (m_Entity) {
			windowTitle += " - " + m_Entity.GetName();
		}
		windowTitle += "###AnimationEditor";
		
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.13f, 1.0f));
		
		if (ImGui::Begin(windowTitle.c_str(), &m_IsOpen, ImGuiWindowFlags_MenuBar)) {
			RenderMenuBar();
			
			// Main layout: Preview on left, properties on right
			float panelWidth = ImGui::GetContentRegionAvail().x;
			float previewWidth = panelWidth * 0.6f;
			float propertiesWidth = panelWidth * 0.4f - 10.0f;
			
			// Left side - Preview and Timeline
			ImGui::BeginChild("PreviewArea", ImVec2(previewWidth, 0), false);
			{
				// Preview viewport
				float viewportHeight = ImGui::GetContentRegionAvail().y * 0.7f;
				ImGui::BeginChild("PreviewViewport", ImVec2(0, viewportHeight), true);
				RenderPreviewViewport();
				ImGui::EndChild();
				
				// Playback controls
				RenderPlaybackControls();
				
				// Timeline
				ImGui::BeginChild("TimelineArea", ImVec2(0, 0), true);
				RenderTimeline();
				ImGui::EndChild();
			}
			ImGui::EndChild();
			
			ImGui::SameLine();
			
			// Right side - Animation list and settings
			ImGui::BeginChild("PropertiesArea", ImVec2(propertiesWidth, 0), false);
			{
				// Animation clips list
				if (ImGui::CollapsingHeader("Animation Clips", ImGuiTreeNodeFlags_DefaultOpen)) {
					RenderAnimationList();
				}
				
				// Blend settings
				if (ImGui::CollapsingHeader("Blend Settings", ImGuiTreeNodeFlags_DefaultOpen)) {
					RenderBlendSettings();
				}
				
				// Skeleton info
				if (ImGui::CollapsingHeader("Skeleton", ImGuiTreeNodeFlags_DefaultOpen)) {
					RenderSkeletonInfo();
				}
			}
			ImGui::EndChild();
		}
		ImGui::End();
		
		ImGui::PopStyleColor();
		
		if (!m_IsOpen) {
			Close();
		}
	}

	void AnimationEditorPanel::RenderMenuBar() {
		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("Import Animation...")) {
					ImportAnimation();
				}
				ImGui::Separator();
				if (ImGui::MenuItem("Close")) {
					Close();
				}
				ImGui::EndMenu();
			}
			
			if (ImGui::BeginMenu("View")) {
				ImGui::MenuItem("Show Skeleton", nullptr, &m_ShowSkeleton);
				ImGui::MenuItem("Show Bone Names", nullptr, &m_ShowBoneNames);
				ImGui::MenuItem("Show Floor Grid", nullptr, &m_ShowFloor);
				ImGui::Separator();
				if (ImGui::MenuItem("Reset Camera")) {
					m_PreviewRenderer.ResetCamera();
				}
				ImGui::EndMenu();
			}
			
			if (ImGui::BeginMenu("Playback")) {
				if (ImGui::MenuItem(m_IsPlaying ? "Pause" : "Play")) {
					m_IsPlaying = !m_IsPlaying;
					if (m_IsPlaying) {
						m_PreviewRenderer.Play();
					} else {
						m_PreviewRenderer.Pause();
					}
				}
				if (ImGui::MenuItem("Stop")) {
					m_IsPlaying = false;
					m_CurrentTime = 0.0f;
					m_PreviewRenderer.Stop();
				}
				ImGui::Separator();
				ImGui::MenuItem("Loop", nullptr, &m_Loop);
				ImGui::EndMenu();
			}
			
			ImGui::EndMenuBar();
		}
	}

	void AnimationEditorPanel::RenderPreviewViewport() {
		ImVec2 viewportSize = ImGui::GetContentRegionAvail();
		
		// Store viewport position and size for bone picking
		m_ViewportPos = glm::vec2(ImGui::GetCursorScreenPos().x, ImGui::GetCursorScreenPos().y);
		m_ViewportSize = glm::vec2(viewportSize.x, viewportSize.y);
		
		// Resize if needed
		if (viewportSize.x > 0 && viewportSize.y > 0) {
			m_PreviewRenderer.Resize(
				static_cast<uint32_t>(viewportSize.x),
				static_cast<uint32_t>(viewportSize.y)
			);
		}
		
		// Display preview texture
		uint32_t textureID = m_PreviewRenderer.GetRendererID();
		if (textureID != 0) {
			ImGui::Image(
				(ImTextureID)(uint64_t)textureID,
				viewportSize,
				ImVec2(0, 1), ImVec2(1, 0)
			);
		}
		
		// Handle viewport input
		m_ViewportHovered = ImGui::IsItemHovered();
		HandleViewportInput();
		
		// Render bone name labels overlay
		if (m_ShowBoneNames && m_PreviewRenderer.GetShowSkeleton()) {
			RenderBoneLabels();
		}
	}

	void AnimationEditorPanel::HandleViewportInput() {
		if (!m_ViewportHovered) {
			m_ViewportDragging = false;
			return;
		}
		
		ImVec2 mousePos = ImGui::GetMousePos();
		glm::vec2 currentMouse(mousePos.x, mousePos.y);
		
		// Orbit camera with left mouse (if not clicking on bone)
		if (ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			if (m_ViewportDragging) {
				glm::vec2 delta = currentMouse - m_LastMousePos;
				m_PreviewRenderer.RotateCamera(delta.x, delta.y);
			}
			m_ViewportDragging = true;
		} else {
			m_ViewportDragging = false;
		}
		
		m_LastMousePos = currentMouse;
		
		// Zoom with scroll
		float scroll = ImGui::GetIO().MouseWheel;
		if (scroll != 0.0f) {
			m_PreviewRenderer.ZoomCamera(scroll);
		}
		
		// Bone picking on click (only when skeleton is visible)
		if (m_ShowSkeleton) {
			HandleBonePicking();
		}
	}
	
	void AnimationEditorPanel::HandleBonePicking() {
		if (!m_ViewportHovered) return;
		
		// Calculate normalized screen position (-1 to 1)
		ImVec2 mousePos = ImGui::GetMousePos();
		float relX = (mousePos.x - m_ViewportPos.x) / m_ViewportSize.x;
		float relY = (mousePos.y - m_ViewportPos.y) / m_ViewportSize.y;
		
		// Convert to NDC (-1 to 1, Y inverted)
		glm::vec2 ndcPos(relX * 2.0f - 1.0f, -(relY * 2.0f - 1.0f));
		
		// Pick bone under cursor
		int32_t hoveredBone = m_PreviewRenderer.PickBone(ndcPos);
		
		// Update hovered bone
		if (hoveredBone != m_HoveredBoneIndex) {
			m_HoveredBoneIndex = hoveredBone;
			m_PreviewRenderer.SetHoveredBone(hoveredBone);
		}
		
		// Select bone on click
		if (ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !m_ViewportDragging) {
			if (hoveredBone >= 0) {
				SelectBone(hoveredBone);
			}
		}
	}
	
	void AnimationEditorPanel::SelectBone(int32_t boneIndex) {
		m_SelectedBoneIndex = boneIndex;
		m_PreviewRenderer.SetSelectedBone(boneIndex);
		m_ScrollToBone = true;
	}
	
	void AnimationEditorPanel::ScrollToBoneInList(int32_t boneIndex) {
		// This will be handled in RenderSkeletonInfo when m_ScrollToBone is true
		m_ScrollToBone = false;
	}
	
	void AnimationEditorPanel::RenderBoneLabels() {
		if (!m_Entity || !m_Entity.HasComponent<SkeletalMeshComponent>()) return;
		
		auto& skeletal = m_Entity.GetComponent<SkeletalMeshComponent>();
		if (!skeletal.Skeleton) return;
		
		const auto& bones = m_PreviewRenderer.GetBoneVisualization().GetBones();
		if (bones.empty()) return;
		
		glm::mat4 viewProj = m_PreviewRenderer.GetViewProjectionMatrix();
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		for (const auto& bone : bones) {
			// Project bone position to screen space
			glm::vec4 clipPos = viewProj * glm::vec4(bone.WorldPosition, 1.0f);
			if (clipPos.w <= 0.0f) continue;
			
			glm::vec2 ndcPos(clipPos.x / clipPos.w, clipPos.y / clipPos.w);
			
			// Convert NDC to screen position
			float screenX = m_ViewportPos.x + (ndcPos.x * 0.5f + 0.5f) * m_ViewportSize.x;
			float screenY = m_ViewportPos.y + (1.0f - (ndcPos.y * 0.5f + 0.5f)) * m_ViewportSize.y;
			
			// Only draw if on screen
			if (screenX < m_ViewportPos.x || screenX > m_ViewportPos.x + m_ViewportSize.x) continue;
			if (screenY < m_ViewportPos.y || screenY > m_ViewportPos.y + m_ViewportSize.y) continue;
			
			// Choose color based on selection state
			ImU32 textColor = IM_COL32(200, 200, 200, 200);
			if (bone.Index == m_SelectedBoneIndex) {
				textColor = IM_COL32(255, 180, 100, 255);
			} else if (bone.Index == m_HoveredBoneIndex) {
				textColor = IM_COL32(255, 255, 150, 255);
			}
			
			// Draw bone name with small offset
			drawList->AddText(ImVec2(screenX + 5, screenY - 5), textColor, bone.Name.c_str());
		}
	}

	void AnimationEditorPanel::RenderPlaybackControls() {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 6));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
		
		float buttonSize = 28.0f;
		ImU32 iconColor = IM_COL32(200, 200, 200, 255);
		
		// Stop button
		if (ImGui::Button("##Stop", ImVec2(buttonSize, buttonSize))) {
			m_IsPlaying = false;
			m_CurrentTime = 0.0f;
			m_PreviewRenderer.Stop();
		}
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 btnMin = ImGui::GetItemRectMin();
		ImVec2 btnMax = ImGui::GetItemRectMax();
		float cx = (btnMin.x + btnMax.x) * 0.5f;
		float cy = (btnMin.y + btnMax.y) * 0.5f;
		drawList->AddRectFilled(ImVec2(cx - 5, cy - 5), ImVec2(cx + 5, cy + 5), iconColor);
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop");
		
		ImGui::SameLine();
		
		// Play/Pause button
		if (ImGui::Button("##PlayPause", ImVec2(buttonSize, buttonSize))) {
			m_IsPlaying = !m_IsPlaying;
			if (m_IsPlaying) {
				m_PreviewRenderer.Play();
			} else {
				m_PreviewRenderer.Pause();
			}
		}
		btnMin = ImGui::GetItemRectMin();
		btnMax = ImGui::GetItemRectMax();
		cx = (btnMin.x + btnMax.x) * 0.5f;
		cy = (btnMin.y + btnMax.y) * 0.5f;
		if (m_IsPlaying) {
			drawList->AddRectFilled(ImVec2(cx - 4, cy - 5), ImVec2(cx - 1, cy + 5), iconColor);
			drawList->AddRectFilled(ImVec2(cx + 1, cy - 5), ImVec2(cx + 4, cy + 5), iconColor);
		} else {
			ImVec2 p1(cx - 3, cy - 5);
			ImVec2 p2(cx - 3, cy + 5);
			ImVec2 p3(cx + 5, cy);
			drawList->AddTriangleFilled(p1, p2, p3, iconColor);
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(m_IsPlaying ? "Pause" : "Play");
		
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(10, 0));
		ImGui::SameLine();
		
		// Time display
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 0.7f, 1.0f));
		float duration = m_PreviewRenderer.GetDuration();
		ImGui::Text("%s / %s", FormatTime(m_CurrentTime).c_str(), FormatTime(duration).c_str());
		ImGui::PopStyleColor();
		
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(20, 0));
		ImGui::SameLine();
		
		// Speed slider
		ImGui::Text("Speed:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(80);
		if (ImGui::SliderFloat("##Speed", &m_PlaybackSpeed, 0.0f, 2.0f, "%.2fx")) {
			m_PreviewRenderer.SetPlaybackSpeed(m_PlaybackSpeed);
		}
		
		ImGui::SameLine();
		
		// Loop toggle
		if (ImGui::Checkbox("Loop", &m_Loop)) {
			m_PreviewRenderer.SetLoop(m_Loop);
		}
		
		ImGui::PopStyleVar(2);
	}

	void AnimationEditorPanel::RenderTimeline() {
		ImVec2 timelinePos = ImGui::GetCursorScreenPos();
		ImVec2 timelineSize = ImGui::GetContentRegionAvail();
		timelineSize.y = std::max(timelineSize.y, 100.0f);
		
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImU32 bgColor = IM_COL32(30, 30, 35, 255);
		ImU32 lineColor = IM_COL32(60, 60, 65, 255);
		ImU32 textColor = IM_COL32(150, 150, 150, 255);
		ImU32 clipColor = IM_COL32(80, 140, 200, 255);
		ImU32 selectedClipColor = IM_COL32(100, 180, 255, 255);
		ImU32 playheadColor = IM_COL32(255, 80, 80, 255);
		
		// Background
		drawList->AddRectFilled(timelinePos,
			ImVec2(timelinePos.x + timelineSize.x, timelinePos.y + timelineSize.y),
			bgColor);
		
		float duration = m_PreviewRenderer.GetDuration();
		if (duration <= 0.0f) duration = 1.0f;
		
		float pixelsPerSecond = (timelineSize.x - 40.0f) / duration * m_TimelineZoom;
		float timelineStartX = timelinePos.x + 20.0f;
		
		// Time markers
		float markerInterval = 0.5f;
		if (pixelsPerSecond < 50.0f) markerInterval = 1.0f;
		if (pixelsPerSecond < 25.0f) markerInterval = 2.0f;
		if (pixelsPerSecond > 100.0f) markerInterval = 0.25f;
		
		for (float t = 0.0f; t <= duration; t += markerInterval) {
			float x = timelineStartX + t * pixelsPerSecond;
			if (x > timelinePos.x + timelineSize.x - 20.0f) break;
			
			drawList->AddLine(
				ImVec2(x, timelinePos.y + 20.0f),
				ImVec2(x, timelinePos.y + timelineSize.y),
				lineColor
			);
			
			char label[32];
			snprintf(label, sizeof(label), "%.1fs", t);
			drawList->AddText(ImVec2(x - 10, timelinePos.y + 3), textColor, label);
		}
		
		// Draw animation clips
		float clipY = timelinePos.y + 35.0f;
		float clipHeight = 25.0f;
		
		for (size_t i = 0; i < m_AnimationSlots.size(); i++) {
			auto& slot = m_AnimationSlots[i];
			if (!slot.Clip || !slot.Enabled) continue;
			
			float clipDuration = slot.Clip->GetDuration();
			float clipStartX = timelineStartX + slot.Offset * pixelsPerSecond;
			float clipEndX = clipStartX + clipDuration * pixelsPerSecond;
			
			ImU32 color = (static_cast<int>(i) == m_SelectedSlotIndex) ? selectedClipColor : clipColor;
			
			drawList->AddRectFilled(
				ImVec2(clipStartX, clipY),
				ImVec2(clipEndX, clipY + clipHeight),
				color, 4.0f
			);
			
			drawList->AddText(
				ImVec2(clipStartX + 5, clipY + 5),
				IM_COL32(255, 255, 255, 255),
				slot.Clip->GetName().c_str()
			);
			
			clipY += clipHeight + 5.0f;
		}
		
		// Playhead
		float playheadX = timelineStartX + m_CurrentTime * pixelsPerSecond;
		drawList->AddLine(
			ImVec2(playheadX, timelinePos.y + 15.0f),
			ImVec2(playheadX, timelinePos.y + timelineSize.y),
			playheadColor, 2.0f
		);
		
		// Playhead triangle
		ImVec2 triPoints[3] = {
			ImVec2(playheadX - 6, timelinePos.y + 15.0f),
			ImVec2(playheadX + 6, timelinePos.y + 15.0f),
			ImVec2(playheadX, timelinePos.y + 25.0f)
		};
		drawList->AddTriangleFilled(triPoints[0], triPoints[1], triPoints[2], playheadColor);
		
		// Handle click to scrub
		ImGui::InvisibleButton("Timeline", timelineSize);
		if (ImGui::IsItemActive()) {
			float mouseX = ImGui::GetMousePos().x;
			float newTime = (mouseX - timelineStartX) / pixelsPerSecond;
			newTime = glm::clamp(newTime, 0.0f, duration);
			m_CurrentTime = newTime;
			m_PreviewRenderer.SetTime(newTime);
		}
	}

	void AnimationEditorPanel::RenderAnimationList() {
		// Add animation button
		if (ImGui::Button("+ Add Animation", ImVec2(-1, 0))) {
			ImportAnimation();
		}
		
		ImGui::Spacing();
		
		// List animations
		for (size_t i = 0; i < m_AnimationSlots.size(); i++) {
			auto& slot = m_AnimationSlots[i];
			
			ImGui::PushID(static_cast<int>(i));
			
			bool isSelected = (i == m_SelectedSlotIndex);
			std::string label = slot.Clip ? slot.Clip->GetName() : "Empty Slot";
			
			if (ImGui::Selectable(label.c_str(), isSelected)) {
				m_SelectedSlotIndex = static_cast<int>(i);
				
				if (slot.Clip) {
					m_PreviewRenderer.SetAnimationClip(slot.Clip);
				}
			}
			
			// Context menu
			if (ImGui::BeginPopupContextItem()) {
				if (ImGui::MenuItem("Remove")) {
					m_AnimationSlots.erase(m_AnimationSlots.begin() + i);
					if (m_SelectedSlotIndex >= static_cast<int>(m_AnimationSlots.size())) {
						m_SelectedSlotIndex = static_cast<int>(m_AnimationSlots.size()) - 1;
					}
				}
				ImGui::EndPopup();
			}
			
			ImGui::PopID();
		}
		
		if (m_AnimationSlots.empty()) {
			ImGui::TextDisabled("No animations added");
		}
	}

	void AnimationEditorPanel::RenderBlendSettings() {
		if (m_SelectedSlotIndex < 0 || m_SelectedSlotIndex >= static_cast<int>(m_AnimationSlots.size())) {
			ImGui::TextDisabled("Select an animation");
			return;
		}
		
		auto& slot = m_AnimationSlots[m_SelectedSlotIndex];
		
		ImGui::Checkbox("Enabled", &slot.Enabled);
		
		ImGui::Text("Weight");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1);
		ImGui::SliderFloat("##Weight", &slot.Weight, 0.0f, 1.0f);
		
		ImGui::Text("Offset (s)");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(-1);
		ImGui::DragFloat("##Offset", &slot.Offset, 0.01f, 0.0f, 10.0f);
		
		ImGui::Checkbox("Loop", &slot.Loop);
		
		if (slot.Clip) {
			ImGui::Separator();
			ImGui::Text("Duration: %.2fs", slot.Clip->GetDuration());
			ImGui::Text("Channels: %d", slot.Clip->GetChannelCount());
		}
	}

	void AnimationEditorPanel::RenderSkeletonInfo() {
		ImGui::Checkbox("Show Skeleton", &m_ShowSkeleton);
		m_PreviewRenderer.SetShowSkeleton(m_ShowSkeleton);
		
		ImGui::Checkbox("Show Bone Names", &m_ShowBoneNames);
		m_PreviewRenderer.SetShowBoneNames(m_ShowBoneNames);
		
		if (m_Entity && m_Entity.HasComponent<SkeletalMeshComponent>()) {
			auto& skeletal = m_Entity.GetComponent<SkeletalMeshComponent>();
			
			if (skeletal.Skeleton) {
				ImGui::Separator();
				ImGui::Text("Bones: %d", skeletal.Skeleton->GetJointCount());
				
				// Bone search filter
				ImGui::Spacing();
				ImGui::Text("Search:");
				ImGui::SameLine();
				ImGui::SetNextItemWidth(-1);
				char searchBuffer[256] = {};
				strncpy(searchBuffer, m_BoneSearchFilter.c_str(), sizeof(searchBuffer) - 1);
				if (ImGui::InputText("##BoneSearch", searchBuffer, sizeof(searchBuffer))) {
					m_BoneSearchFilter = searchBuffer;
				}
				
				// Selected bone info
				if (m_SelectedBoneIndex >= 0 && m_SelectedBoneIndex < static_cast<int32_t>(skeletal.Skeleton->GetJointCount())) {
					ImGui::Separator();
					const auto& joint = skeletal.Skeleton->GetJoint(m_SelectedBoneIndex);
					ImGui::TextColored(ImVec4(1.0f, 0.7f, 0.3f, 1.0f), "Selected: %s", joint.Name.c_str());
					ImGui::Text("Index: %d", m_SelectedBoneIndex);
					ImGui::Text("Parent: %d", joint.ParentIndex);
					
					// Show local transform
					if (ImGui::TreeNode("Local Transform")) {
						ImGui::Text("Position: %.3f, %.3f, %.3f", 
							joint.LocalPosition.x, joint.LocalPosition.y, joint.LocalPosition.z);
						
						glm::vec3 euler = glm::degrees(glm::eulerAngles(joint.LocalRotation));
						ImGui::Text("Rotation: %.1f, %.1f, %.1f", euler.x, euler.y, euler.z);
						
						ImGui::Text("Scale: %.3f, %.3f, %.3f",
							joint.LocalScale.x, joint.LocalScale.y, joint.LocalScale.z);
						ImGui::TreePop();
					}
				}
				
				ImGui::Separator();
				
				// Bone hierarchy tree
				RenderBoneHierarchy();
			} else {
				ImGui::TextDisabled("No skeleton assigned");
			}
		}
	}
	
	void AnimationEditorPanel::RenderBoneHierarchy() {
		if (!m_Entity || !m_Entity.HasComponent<SkeletalMeshComponent>()) return;
		
		auto& skeletal = m_Entity.GetComponent<SkeletalMeshComponent>();
		if (!skeletal.Skeleton) return;
		
		uint32_t boneCount = skeletal.Skeleton->GetJointCount();
		
		// Build hierarchy - find root bones (parent == -1)
		std::vector<int32_t> rootBones;
		for (uint32_t i = 0; i < boneCount; i++) {
			const auto& joint = skeletal.Skeleton->GetJoint(i);
			if (joint.ParentIndex < 0) {
				rootBones.push_back(static_cast<int32_t>(i));
			}
		}
		
		// Recursive lambda to draw bone tree
		std::function<void(int32_t)> drawBoneTree = [&](int32_t boneIndex) {
			const auto& joint = skeletal.Skeleton->GetJoint(boneIndex);
			
			// Filter check
			bool matchesFilter = m_BoneSearchFilter.empty() || 
				(joint.Name.find(m_BoneSearchFilter) != std::string::npos);
			
			if (!matchesFilter) {
				// Check if any child matches
				bool childMatches = false;
				for (uint32_t i = 0; i < boneCount; i++) {
					const auto& childJoint = skeletal.Skeleton->GetJoint(i);
					if (childJoint.ParentIndex == boneIndex) {
						if (childJoint.Name.find(m_BoneSearchFilter) != std::string::npos) {
							childMatches = true;
							break;
						}
					}
				}
				if (!childMatches) return;
			}
			
			// Find children
			std::vector<int32_t> children;
			for (uint32_t i = 0; i < boneCount; i++) {
				const auto& childJoint = skeletal.Skeleton->GetJoint(i);
				if (childJoint.ParentIndex == boneIndex) {
					children.push_back(static_cast<int32_t>(i));
				}
			}
			
			ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth;
			if (children.empty()) {
				flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
			}
			if (boneIndex == m_SelectedBoneIndex) {
				flags |= ImGuiTreeNodeFlags_Selected;
			}
			
			// Scroll to selected bone if requested
			if (m_ScrollToBone && boneIndex == m_SelectedBoneIndex) {
				ImGui::SetScrollHereY(0.5f);
				m_ScrollToBone = false;
			}
			
			// Color selected/hovered bones
			if (boneIndex == m_SelectedBoneIndex) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.7f, 0.3f, 1.0f));
			} else if (boneIndex == m_HoveredBoneIndex) {
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 1.0f, 0.5f, 1.0f));
			}
			
			std::string label = joint.Name + "##" + std::to_string(boneIndex);
			bool nodeOpen = ImGui::TreeNodeEx(label.c_str(), flags);
			
			if (boneIndex == m_SelectedBoneIndex || boneIndex == m_HoveredBoneIndex) {
				ImGui::PopStyleColor();
			}
			
			// Handle selection
			if (ImGui::IsItemClicked()) {
				SelectBone(boneIndex);
			}
			
			// Tooltip with bone info
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::Text("Index: %d", boneIndex);
				ImGui::Text("Parent: %d", joint.ParentIndex);
				ImGui::Text("Children: %zu", children.size());
				ImGui::EndTooltip();
				
				// Update hovered bone in preview
				if (m_HoveredBoneIndex != boneIndex) {
					m_HoveredBoneIndex = boneIndex;
					m_PreviewRenderer.SetHoveredBone(boneIndex);
				}
			}
			
			// Draw children
			if (nodeOpen && !children.empty()) {
				for (int32_t childIndex : children) {
					drawBoneTree(childIndex);
				}
				ImGui::TreePop();
			}
		};
		
		// Draw hierarchy starting from root bones
		if (ImGui::TreeNodeEx("Bone Hierarchy", ImGuiTreeNodeFlags_DefaultOpen)) {
			for (int32_t rootBone : rootBones) {
				drawBoneTree(rootBone);
			}
			ImGui::TreePop();
		}
	}

	void AnimationEditorPanel::ImportAnimation() {
		std::string path = FileDialogs::OpenFile("Animation (*.fbx;*.gltf;*.glb;*.luanim)\0*.fbx;*.gltf;*.glb;*.luanim\0");
		
		if (path.empty()) return;
		
		std::filesystem::path animPath(path);
		
		// Check if it's already a Lunex animation file
		if (animPath.extension() == ".luanim") {
			auto clip = AnimationClipAsset::LoadFromFile(animPath);
			if (clip) {
				AnimationSlot slot;
				slot.Clip = clip;
				slot.Weight = 1.0f;
				slot.Loop = true;
				slot.Enabled = true;
				m_AnimationSlots.push_back(slot);
				m_SelectedSlotIndex = static_cast<int>(m_AnimationSlots.size()) - 1;
				m_PreviewRenderer.SetAnimationClip(clip);
			}
		} else {
			// Import from FBX/GLTF
			Ref<SkeletonAsset> skeleton = nullptr;
			if (m_Entity && m_Entity.HasComponent<SkeletalMeshComponent>()) {
				skeleton = m_Entity.GetComponent<SkeletalMeshComponent>().Skeleton;
			}
			
			// ? FIX: Use correct scale for Mixamo models (0.01 = cm to meters)
			AnimationImportSettings settings;
			settings.Scale = 0.01f;  // Mixamo uses centimeters, convert to meters
			
			auto clips = AnimationImporter::ImportAnimations(animPath, skeleton, settings);
			
			for (auto& clip : clips) {
				AnimationSlot slot;
				slot.Clip = clip;
				slot.Weight = 1.0f;
				slot.Loop = true;
				slot.Enabled = true;
				m_AnimationSlots.push_back(slot);
			}
			
			if (!clips.empty()) {
				m_SelectedSlotIndex = static_cast<int>(m_AnimationSlots.size()) - 1;
				m_PreviewRenderer.SetAnimationClip(clips.back());
			}
		}
	}

	std::string AnimationEditorPanel::FormatTime(float seconds) const {
		int mins = static_cast<int>(seconds) / 60;
		int secs = static_cast<int>(seconds) % 60;
		int ms = static_cast<int>((seconds - static_cast<int>(seconds)) * 100);
		
		char buffer[32];
		if (mins > 0) {
			snprintf(buffer, sizeof(buffer), "%d:%02d.%02d", mins, secs, ms);
		} else {
			snprintf(buffer, sizeof(buffer), "%d.%02d", secs, ms);
		}
		return buffer;
	}

}