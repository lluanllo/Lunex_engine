#include "stpch.h"
#include "AnimatorPanel.h"
#include "Scene/Scene.h"

#include <imgui.h>
#include <imgui_internal.h>

namespace Lunex {

	AnimatorPanel::AnimatorPanel() {
	}

	void AnimatorPanel::SetContext(Entity entity) {
		if (m_SelectedEntity != entity) {
			m_SelectedEntity = entity;
			m_PreviewTime = 0.0f;
			m_IsPlaying = false;
			m_SelectedClipIndex = -1;
		}
	}

	void AnimatorPanel::OnImGuiRender() {
		// Professional dark style
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.13f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.11f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.08f, 0.08f, 0.09f, 1.0f));
		
		ImGui::Begin("Animator", nullptr, ImGuiWindowFlags_MenuBar);
		
		// Menu bar
		if (ImGui::BeginMenuBar()) {
			if (ImGui::BeginMenu("File")) {
				if (ImGui::MenuItem("Import Animation...")) {
					// TODO: Open file dialog
				}
				ImGui::EndMenu();
			}
			if (ImGui::BeginMenu("View")) {
				ImGui::MenuItem("Show Skeleton", nullptr, false);
				ImGui::MenuItem("Show Bone Names", nullptr, false);
				ImGui::EndMenu();
			}
			ImGui::EndMenuBar();
		}
		
		// Check if entity is valid and has animation components
		bool hasAnimator = m_SelectedEntity && m_SelectedEntity.HasComponent<AnimatorComponent>();
		bool hasSkeletal = m_SelectedEntity && m_SelectedEntity.HasComponent<SkeletalMeshComponent>();
		
		if (!m_SelectedEntity) {
			// No entity selected
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
			
			float windowWidth = ImGui::GetWindowWidth();
			float windowHeight = ImGui::GetWindowHeight();
			
			const char* message = "Select an entity with Animator component";
			float textWidth = ImGui::CalcTextSize(message).x;
			
			ImGui::SetCursorPos(ImVec2((windowWidth - textWidth) * 0.5f, windowHeight * 0.45f));
			ImGui::Text("%s", message);
			
			ImGui::PopStyleColor();
		}
		else if (!hasAnimator && !hasSkeletal) {
			// Entity doesn't have animation components
			float windowWidth = ImGui::GetWindowWidth();
			float windowHeight = ImGui::GetWindowHeight();
			
			ImGui::SetCursorPos(ImVec2(20, windowHeight * 0.4f));
			
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.7f, 0.7f, 1.0f));
			ImGui::Text("Entity: %s", m_SelectedEntity.GetName().c_str());
			ImGui::PopStyleColor();
			
			ImGui::Spacing();
			ImGui::Spacing();
			
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
			ImGui::TextWrapped("This entity doesn't have animation components.");
			ImGui::Spacing();
			ImGui::TextWrapped("Add SkeletalMeshComponent and AnimatorComponent to enable animation.");
			ImGui::PopStyleColor();
			
			ImGui::Spacing();
			ImGui::Spacing();
			
			// Add component buttons
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.4f, 0.6f, 1.0f));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.5f, 0.7f, 1.0f));
			
			if (!hasSkeletal) {
				if (ImGui::Button("+ Add Skeletal Mesh Component", ImVec2(250, 30))) {
					m_SelectedEntity.AddComponent<SkeletalMeshComponent>();
				}
			}
			
			ImGui::Spacing();
			
			if (!hasAnimator) {
				if (ImGui::Button("+ Add Animator Component", ImVec2(250, 30))) {
					m_SelectedEntity.AddComponent<AnimatorComponent>();
				}
			}
			
			ImGui::PopStyleColor(2);
		}
		else {
			// Entity has animation components - show full UI
			RenderToolbar();
			
			ImGui::Separator();
			
			// Main layout: Clips on left, Timeline on right
			float availWidth = ImGui::GetContentRegionAvail().x;
			float clipListWidth = 250.0f;
			
			// Clip list panel
			ImGui::BeginChild("ClipListPanel", ImVec2(clipListWidth, 0), true);
			RenderClipList();
			RenderBlendingInfo();
			RenderSkeletonInfo();
			ImGui::EndChild();
			
			ImGui::SameLine();
			
			// Timeline panel
			ImGui::BeginChild("TimelinePanel", ImVec2(0, 0), true);
			RenderTimeline();
			ImGui::EndChild();
		}
		
		ImGui::End();
		
		ImGui::PopStyleColor(3);
	}

	void AnimatorPanel::RenderToolbar() {
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(8, 8));
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(4, 4));
		
		float buttonSize = 32.0f;
		
		// Entity name
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.9f, 0.9f, 0.9f, 1.0f));
		ImGui::Text("Entity: %s", m_SelectedEntity.GetName().c_str());
		ImGui::PopStyleColor();
		
		ImGui::SameLine(200);
		
		// Playback controls
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.2f, 0.2f, 0.22f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.32f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.15f, 0.15f, 0.17f, 1.0f));
		
		// Stop button
		if (ImGui::Button("##Stop", ImVec2(buttonSize, buttonSize))) {
			StopAnimation();
		}
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 btnMin = ImGui::GetItemRectMin();
		ImVec2 btnMax = ImGui::GetItemRectMax();
		float cx = (btnMin.x + btnMax.x) * 0.5f;
		float cy = (btnMin.y + btnMax.y) * 0.5f;
		float s = 6.0f;
		drawList->AddRectFilled(ImVec2(cx - s, cy - s), ImVec2(cx + s, cy + s), IM_COL32(200, 200, 200, 255));
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Stop");
		
		ImGui::SameLine();
		
		// Play/Pause button
		if (ImGui::Button("##PlayPause", ImVec2(buttonSize, buttonSize))) {
			if (m_IsPlaying) {
				PauseAnimation();
			} else {
				PlayAnimation();
			}
		}
		btnMin = ImGui::GetItemRectMin();
		btnMax = ImGui::GetItemRectMax();
		cx = (btnMin.x + btnMax.x) * 0.5f;
		cy = (btnMin.y + btnMax.y) * 0.5f;
		if (m_IsPlaying) {
			// Pause icon (two bars)
			drawList->AddRectFilled(ImVec2(cx - 5, cy - 6), ImVec2(cx - 2, cy + 6), IM_COL32(200, 200, 200, 255));
			drawList->AddRectFilled(ImVec2(cx + 2, cy - 6), ImVec2(cx + 5, cy + 6), IM_COL32(200, 200, 200, 255));
		} else {
			// Play icon (triangle)
			ImVec2 p1(cx - 4, cy - 6);
			ImVec2 p2(cx - 4, cy + 6);
			ImVec2 p3(cx + 6, cy);
			drawList->AddTriangleFilled(p1, p2, p3, IM_COL32(200, 200, 200, 255));
		}
		if (ImGui::IsItemHovered()) ImGui::SetTooltip(m_IsPlaying ? "Pause" : "Play");
		
		ImGui::PopStyleColor(3);
		
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(20, 0));
		ImGui::SameLine();
		
		// Time display
		if (m_SelectedEntity.HasComponent<AnimatorComponent>()) {
			auto& animator = m_SelectedEntity.GetComponent<AnimatorComponent>();
			
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.7f, 0.9f, 0.7f, 1.0f));
			ImGui::Text("%s / %s", 
				FormatTime(animator.CurrentTime).c_str(),
				FormatTime(animator.GetDuration()).c_str());
			ImGui::PopStyleColor();
			
			ImGui::SameLine();
			ImGui::Dummy(ImVec2(20, 0));
			ImGui::SameLine();
			
			// Speed control
			ImGui::Text("Speed:");
			ImGui::SameLine();
			ImGui::SetNextItemWidth(80);
			ImGui::SliderFloat("##Speed", &animator.PlaybackSpeed, 0.0f, 2.0f, "%.2fx");
			
			ImGui::SameLine();
			ImGui::Dummy(ImVec2(20, 0));
			ImGui::SameLine();
			
			// Loop toggle
			bool loop = animator.Loop;
			if (ImGui::Checkbox("Loop", &loop)) {
				animator.Loop = loop;
			}
		}
		
		ImGui::PopStyleVar(2);
	}

	void AnimatorPanel::RenderTimeline() {
		if (!m_SelectedEntity.HasComponent<AnimatorComponent>()) {
			ImGui::TextDisabled("No animator component");
			return;
		}
		
		auto& animator = m_SelectedEntity.GetComponent<AnimatorComponent>();
		
		ImVec2 timelinePos = ImGui::GetCursorScreenPos();
		ImVec2 timelineSize = ImGui::GetContentRegionAvail();
		timelineSize.y = (std::max)(timelineSize.y, 150.0f);
		
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		
		// Timeline background
		drawList->AddRectFilled(timelinePos, 
			ImVec2(timelinePos.x + timelineSize.x, timelinePos.y + timelineSize.y),
			m_TimelineBgColor);
		
		// Calculate time range
		float duration = animator.GetDuration();
		if (duration <= 0.0f) duration = 1.0f;
		
		float pixelsPerSecond = (timelineSize.x - 40.0f) / duration * m_TimelineZoom;
		float timelineStartX = timelinePos.x + 20.0f;
		
		// Draw time markers
		float markerInterval = 1.0f; // 1 second intervals
		if (pixelsPerSecond > 200.0f) markerInterval = 0.5f;
		if (pixelsPerSecond > 400.0f) markerInterval = 0.25f;
		if (pixelsPerSecond < 50.0f) markerInterval = 2.0f;
		if (pixelsPerSecond < 25.0f) markerInterval = 5.0f;
		
		for (float t = 0.0f; t <= duration; t += markerInterval) {
			float x = timelineStartX + t * pixelsPerSecond;
			if (x > timelinePos.x + timelineSize.x - 20.0f) break;
			
			// Vertical line
			drawList->AddLine(
				ImVec2(x, timelinePos.y + 25.0f),
				ImVec2(x, timelinePos.y + timelineSize.y),
				m_TimelineGridColor
			);
			
			// Time label
			char timeLabel[32];
			snprintf(timeLabel, sizeof(timeLabel), "%.1fs", t);
			drawList->AddText(ImVec2(x - 10.0f, timelinePos.y + 5.0f), 
				IM_COL32(150, 150, 150, 255), timeLabel);
		}
		
		// Draw animation clip bar
		if (animator.CurrentClip) {
			float clipStartX = timelineStartX;
			float clipEndX = timelineStartX + duration * pixelsPerSecond;
			float clipY = timelinePos.y + 40.0f;
			float clipHeight = 30.0f;
			
			// Clip background
			drawList->AddRectFilled(
				ImVec2(clipStartX, clipY),
				ImVec2(clipEndX, clipY + clipHeight),
				m_ClipColor,
				4.0f
			);
			
			// Clip name
			std::string clipName = animator.CurrentClip->GetName();
			drawList->AddText(
				ImVec2(clipStartX + 8.0f, clipY + 7.0f),
				IM_COL32(255, 255, 255, 255),
				clipName.c_str()
			);
		}
		
		// Draw blending clip if present
		if (animator.IsBlending && animator.NextClip) {
			float blendStartX = timelineStartX;
			float blendEndX = timelineStartX + animator.NextClip->GetDuration() * pixelsPerSecond;
			float blendY = timelinePos.y + 75.0f;
			float blendHeight = 25.0f;
			
			// Blend clip with transparency
			ImU32 blendColor = IM_COL32(180, 100, 220, 180);
			drawList->AddRectFilled(
				ImVec2(blendStartX, blendY),
				ImVec2(blendEndX, blendY + blendHeight),
				blendColor,
				4.0f
			);
			
			// Blend indicator
			std::string blendName = "Blending: " + animator.NextClip->GetName();
			drawList->AddText(
				ImVec2(blendStartX + 8.0f, blendY + 5.0f),
				IM_COL32(255, 255, 255, 200),
				blendName.c_str()
			);
			
			// Blend progress bar
			float blendProgress = animator.GetBlendFactor();
			float barY = blendY + blendHeight + 5.0f;
			float barWidth = 100.0f;
			drawList->AddRectFilled(
				ImVec2(blendStartX, barY),
				ImVec2(blendStartX + barWidth, barY + 5.0f),
				IM_COL32(50, 50, 55, 255),
				2.0f
			);
			drawList->AddRectFilled(
				ImVec2(blendStartX, barY),
				ImVec2(blendStartX + barWidth * blendProgress, barY + 5.0f),
				IM_COL32(180, 100, 220, 255),
				2.0f
			);
		}
		
		// Draw playhead
		float playheadX = timelineStartX + animator.CurrentTime * pixelsPerSecond;
		drawList->AddLine(
			ImVec2(playheadX, timelinePos.y + 20.0f),
			ImVec2(playheadX, timelinePos.y + timelineSize.y),
			m_PlayheadColor,
			2.0f
		);
		
		// Playhead triangle
		ImVec2 t1(playheadX - 6.0f, timelinePos.y + 20.0f);
		ImVec2 t2(playheadX + 6.0f, timelinePos.y + 20.0f);
		ImVec2 t3(playheadX, timelinePos.y + 28.0f);
		drawList->AddTriangleFilled(t1, t2, t3, m_PlayheadColor);
		
		// Invisible button for interaction
		ImGui::SetCursorScreenPos(timelinePos);
		ImGui::InvisibleButton("##Timeline", timelineSize);
		
		// Handle playhead dragging
		if (ImGui::IsItemActive() || ImGui::IsItemHovered()) {
			if (ImGui::IsMouseDown(0)) {
				float mouseX = ImGui::GetMousePos().x;
				float newTime = (mouseX - timelineStartX) / pixelsPerSecond;
				newTime = glm::clamp(newTime, 0.0f, duration);
				animator.CurrentTime = newTime;
			}
		}
		
		// Zoom with scroll wheel
		if (ImGui::IsItemHovered()) {
			float wheel = ImGui::GetIO().MouseWheel;
			if (wheel != 0.0f) {
				m_TimelineZoom *= (wheel > 0.0f) ? 1.1f : 0.9f;
				m_TimelineZoom = glm::clamp(m_TimelineZoom, 0.25f, 4.0f);
			}
		}
	}

	void AnimatorPanel::RenderClipList() {
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
		ImGui::Text("ANIMATION CLIPS");
		ImGui::PopStyleColor();
		
		ImGui::Spacing();
		
		if (!m_SelectedEntity.HasComponent<AnimatorComponent>()) {
			ImGui::TextDisabled("No animator");
			return;
		}
		
		auto& animator = m_SelectedEntity.GetComponent<AnimatorComponent>();
		
		// Current clip
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.2f, 0.35f, 0.5f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.4f, 0.55f, 1.0f));
		
		if (animator.CurrentClip) {
			bool selected = m_SelectedClipIndex == 0;
			if (ImGui::Selectable(animator.CurrentClip->GetName().c_str(), selected)) {
				m_SelectedClipIndex = 0;
			}
			
			// Clip info on hover
			if (ImGui::IsItemHovered()) {
				ImGui::BeginTooltip();
				ImGui::Text("Duration: %.2f s", animator.CurrentClip->GetDuration());
				ImGui::Text("Channels: %d", (int)animator.CurrentClip->GetChannelCount());
				ImGui::Text("Loop: %s", animator.Loop ? "Yes" : "No");
				ImGui::EndTooltip();
			}
		}
		else {
			ImGui::TextDisabled("No clip assigned");
		}
		
		ImGui::PopStyleColor(2);
		
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		
		// Drag-drop target for clips
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.17f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.2f, 0.2f, 0.22f, 1.0f));
		
		if (ImGui::Button("+ Add Animation Clip", ImVec2(-1, 30))) {
			// TODO: Open clip selection dialog
		}
		
		// Accept drag-drop
		if (ImGui::BeginDragDropTarget()) {
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
				struct ContentBrowserPayload {
					char FilePath[512];
					char RelativePath[256];
					char Extension[32];
					bool IsDirectory;
					int ItemCount;
				};
				
				ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
				std::string ext = data->Extension;
				
				if (ext == ".luanim") {
					auto clip = AnimationClipAsset::LoadFromFile(data->FilePath);
					if (clip) {
						animator.Play(clip, animator.Loop);
						LNX_LOG_INFO("Assigned animation clip: {0}", clip->GetName());
					}
				}
			}
			ImGui::EndDragDropTarget();
		}
		
		ImGui::PopStyleColor(2);
	}

	void AnimatorPanel::RenderBlendingInfo() {
		if (!m_SelectedEntity.HasComponent<AnimatorComponent>()) return;
		
		auto& animator = m_SelectedEntity.GetComponent<AnimatorComponent>();
		
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
		ImGui::Text("BLENDING");
		ImGui::PopStyleColor();
		
		ImGui::Spacing();
		
		// Blend duration control
		ImGui::Text("Blend Duration:");
		ImGui::SameLine();
		ImGui::SetNextItemWidth(100);
		ImGui::SliderFloat("##BlendDuration", &animator.BlendDuration, 0.0f, 1.0f, "%.2f s");
		
		// Blending status
		if (animator.IsBlending) {
			ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.5f, 0.9f, 1.0f));
			ImGui::Text("Blending: %.0f%%", animator.GetBlendFactor() * 100.0f);
			ImGui::PopStyleColor();
			
			// Progress bar
			ImGui::ProgressBar(animator.GetBlendFactor(), ImVec2(-1, 0), "");
		}
		else {
			ImGui::TextDisabled("Not blending");
		}
	}

	void AnimatorPanel::RenderSkeletonInfo() {
		if (!m_SelectedEntity.HasComponent<SkeletalMeshComponent>()) return;
		
		auto& skeletal = m_SelectedEntity.GetComponent<SkeletalMeshComponent>();
		
		ImGui::Spacing();
		ImGui::Separator();
		ImGui::Spacing();
		
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.6f, 0.6f, 0.6f, 1.0f));
		ImGui::Text("SKELETON");
		ImGui::PopStyleColor();
		
		ImGui::Spacing();
		
		if (skeletal.Skeleton) {
			ImGui::Text("Name: %s", skeletal.Skeleton->GetName().c_str());
			ImGui::Text("Bones: %d", skeletal.Skeleton->GetJointCount());
			
			// Skeleton tree (collapsible)
			if (ImGui::TreeNode("Bone Hierarchy")) {
				const auto& joints = skeletal.Skeleton->GetJoints();
				
				std::function<void(int32_t, int)> displayJoint = [&](int32_t index, int depth) {
					if (index < 0 || index >= static_cast<int32_t>(joints.size())) return;
					
					const auto& joint = joints[index];
					
					ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | 
											   ImGuiTreeNodeFlags_SpanAvailWidth;
					
					auto children = skeletal.Skeleton->GetChildren(index);
					if (children.empty()) {
						flags |= ImGuiTreeNodeFlags_Leaf;
					}
					
					bool opened = ImGui::TreeNodeEx(joint.Name.c_str(), flags);
					
					if (opened) {
						for (int32_t childIndex : children) {
							displayJoint(childIndex, depth + 1);
						}
						ImGui::TreePop();
					}
				};
				
				// Display from root joints
				for (int32_t rootIndex : skeletal.Skeleton->GetRootJoints()) {
					displayJoint(rootIndex, 0);
				}
				
				ImGui::TreePop();
			}
		}
		else {
			ImGui::TextDisabled("No skeleton assigned");
			
			// Drag-drop target for skeleton
			if (ImGui::Button("+ Assign Skeleton", ImVec2(-1, 25))) {
				// TODO: Open skeleton selection
			}
			
			if (ImGui::BeginDragDropTarget()) {
				if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
					struct ContentBrowserPayload {
						char FilePath[512];
						char RelativePath[256];
						char Extension[32];
						bool IsDirectory;
						int ItemCount;
					};
					
					ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
					std::string ext = data->Extension;
					
					if (ext == ".luskel") {
						auto skeleton = SkeletonAsset::LoadFromFile(data->FilePath);
						if (skeleton) {
							skeletal.SetSkeleton(skeleton);
							LNX_LOG_INFO("Assigned skeleton: {0}", skeleton->GetName());
						}
					}
				}
				ImGui::EndDragDropTarget();
			}
		}
	}

	void AnimatorPanel::RenderProperties() {
		// Additional properties panel
	}

	void AnimatorPanel::PlayAnimation() {
		if (!m_SelectedEntity.HasComponent<AnimatorComponent>()) return;
		
		auto& animator = m_SelectedEntity.GetComponent<AnimatorComponent>();
		animator.Resume();
		m_IsPlaying = true;
	}

	void AnimatorPanel::PauseAnimation() {
		if (!m_SelectedEntity.HasComponent<AnimatorComponent>()) return;
		
		auto& animator = m_SelectedEntity.GetComponent<AnimatorComponent>();
		animator.Pause();
		m_IsPlaying = false;
	}

	void AnimatorPanel::StopAnimation() {
		if (!m_SelectedEntity.HasComponent<AnimatorComponent>()) return;
		
		auto& animator = m_SelectedEntity.GetComponent<AnimatorComponent>();
		animator.Stop();
		m_IsPlaying = false;
	}

	void AnimatorPanel::SetTime(float time) {
		if (!m_SelectedEntity.HasComponent<AnimatorComponent>()) return;
		
		auto& animator = m_SelectedEntity.GetComponent<AnimatorComponent>();
		animator.CurrentTime = glm::clamp(time, 0.0f, animator.GetDuration());
	}

	std::string AnimatorPanel::FormatTime(float seconds) const {
		int mins = (int)(seconds / 60.0f);
		float secs = fmod(seconds, 60.0f);
		
		char buffer[32];
		snprintf(buffer, sizeof(buffer), "%d:%05.2f", mins, secs);
		return std::string(buffer);
	}

}
