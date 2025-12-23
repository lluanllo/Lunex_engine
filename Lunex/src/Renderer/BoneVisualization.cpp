#include "stpch.h"
#include "BoneVisualization.h"

#include "Renderer/Renderer2D.h"
#include "Log/Log.h"

#include <glm/gtc/matrix_transform.hpp>

namespace Lunex {

	BoneVisualization::BoneVisualization() {
	}

	BoneVisualization::~BoneVisualization() {
		Shutdown();
	}

	void BoneVisualization::Init() {
		if (m_Initialized) return;
		
		// No need for custom shaders - we use Renderer2D for lines
		m_Initialized = true;
		LNX_LOG_INFO("BoneVisualization initialized");
	}

	void BoneVisualization::Shutdown() {
		m_Bones.clear();
		m_Initialized = false;
	}

	void BoneVisualization::CreateLineShader() {
		// We use Renderer2D's line drawing
	}

	void BoneVisualization::CreateSphereGeometry() {
		// Not needed for simple bone viz - we'll draw circles instead
	}

	// ============================================================================
	// BONE DATA UPDATE
	// ============================================================================

	void BoneVisualization::UpdateBones(
		Ref<SkeletonAsset> skeleton,
		const std::vector<glm::mat4>& modelSpaceMatrices
	) {
		if (!skeleton) {
			m_Bones.clear();
			return;
		}
		
		uint32_t boneCount = skeleton->GetJointCount();
		m_Bones.resize(boneCount);
		
		for (uint32_t i = 0; i < boneCount; i++) {
			const auto& joint = skeleton->GetJoint(i);
			
			m_Bones[i].Index = static_cast<int32_t>(i);
			m_Bones[i].Name = joint.Name;
			m_Bones[i].ParentIndex = joint.ParentIndex;
			
			// Get world position from model-space matrix
			if (i < modelSpaceMatrices.size()) {
				m_Bones[i].WorldMatrix = modelSpaceMatrices[i];
				m_Bones[i].WorldPosition = glm::vec3(modelSpaceMatrices[i][3]);
			}
			
			// Get parent world position
			if (joint.ParentIndex >= 0 && joint.ParentIndex < static_cast<int32_t>(modelSpaceMatrices.size())) {
				m_Bones[i].ParentWorldPosition = glm::vec3(modelSpaceMatrices[joint.ParentIndex][3]);
			} else {
				m_Bones[i].ParentWorldPosition = glm::vec3(0.0f);
			}
			
			// Preserve selection state
			if (static_cast<int32_t>(i) == m_SelectedBone) {
				m_Bones[i].State = BoneVisualState::Selected;
			} else if (static_cast<int32_t>(i) == m_HoveredBone) {
				m_Bones[i].State = BoneVisualState::Hovered;
			} else {
				m_Bones[i].State = BoneVisualState::Normal;
			}
		}
	}

	void BoneVisualization::SetBoneState(int32_t boneIndex, BoneVisualState state) {
		if (boneIndex >= 0 && boneIndex < static_cast<int32_t>(m_Bones.size())) {
			m_Bones[boneIndex].State = state;
		}
	}

	void BoneVisualization::SetSelectedBone(int32_t boneIndex) {
		// Clear previous selection
		if (m_SelectedBone >= 0 && m_SelectedBone < static_cast<int32_t>(m_Bones.size())) {
			m_Bones[m_SelectedBone].State = BoneVisualState::Normal;
		}
		
		m_SelectedBone = boneIndex;
		
		// Set new selection
		if (boneIndex >= 0 && boneIndex < static_cast<int32_t>(m_Bones.size())) {
			m_Bones[boneIndex].State = BoneVisualState::Selected;
		}
	}

	void BoneVisualization::SetHoveredBone(int32_t boneIndex) {
		// Clear previous hover (if not selected)
		if (m_HoveredBone >= 0 && m_HoveredBone < static_cast<int32_t>(m_Bones.size())) {
			if (m_HoveredBone != m_SelectedBone) {
				m_Bones[m_HoveredBone].State = BoneVisualState::Normal;
			}
		}
		
		m_HoveredBone = boneIndex;
		
		// Set new hover (if not selected)
		if (boneIndex >= 0 && boneIndex < static_cast<int32_t>(m_Bones.size())) {
			if (boneIndex != m_SelectedBone) {
				m_Bones[boneIndex].State = BoneVisualState::Hovered;
			}
		}
	}

	void BoneVisualization::ClearSelection() {
		if (m_SelectedBone >= 0 && m_SelectedBone < static_cast<int32_t>(m_Bones.size())) {
			m_Bones[m_SelectedBone].State = BoneVisualState::Normal;
		}
		m_SelectedBone = -1;
	}

	void BoneVisualization::ClearHover() {
		if (m_HoveredBone >= 0 && m_HoveredBone < static_cast<int32_t>(m_Bones.size())) {
			if (m_HoveredBone != m_SelectedBone) {
				m_Bones[m_HoveredBone].State = BoneVisualState::Normal;
			}
		}
		m_HoveredBone = -1;
	}

	// ============================================================================
	// RENDERING
	// ============================================================================

	void BoneVisualization::Render(const glm::mat4& viewProjection) {
		if (m_Bones.empty()) return;
		
		RenderBoneLines(viewProjection);
		RenderJointSpheres(viewProjection, m_JointRadius);
		
		// Render axes for selected bone
		if (m_SelectedBone >= 0) {
			RenderBoneAxes(viewProjection, m_SelectedBone, m_AxisLength);
		}
	}

	void BoneVisualization::RenderBoneLines(const glm::mat4& viewProjection) {
		if (m_Bones.empty()) return;
		
		// Use Renderer2D line drawing
		Renderer2D::SetLineWidth(m_LineWidth);
		
		for (const auto& bone : m_Bones) {
			if (bone.ParentIndex < 0) continue;
			
			glm::vec4 color = GetColorForState(bone.State);
			glm::vec3 start = bone.ParentWorldPosition;
			glm::vec3 end = bone.WorldPosition;
			Renderer2D::DrawLine(start, end, color);
		}
	}

	void BoneVisualization::RenderJointSpheres(const glm::mat4& viewProjection, float jointRadius) {
		if (m_Bones.empty()) return;
		
		// Draw joints as small circles/points
		for (const auto& bone : m_Bones) {
			glm::vec4 color = GetJointColorForState(bone.State);
			
			// Draw a small circle at joint position (facing camera)
			glm::mat4 transform = glm::translate(glm::mat4(1.0f), bone.WorldPosition);
			transform = glm::scale(transform, glm::vec3(jointRadius * 2.0f));
			
			// Draw a circle in the XY plane at this position
			Renderer2D::DrawCircle(transform, color, 0.1f);
		}
	}

	void BoneVisualization::RenderBoneAxes(const glm::mat4& viewProjection, int32_t boneIndex, float axisLength) {
		if (boneIndex < 0 || boneIndex >= static_cast<int32_t>(m_Bones.size())) return;
		
		const auto& bone = m_Bones[boneIndex];
		
		// Extract axes from bone's world matrix
		glm::vec3 xAxis = glm::normalize(glm::vec3(bone.WorldMatrix[0])) * axisLength;
		glm::vec3 yAxis = glm::normalize(glm::vec3(bone.WorldMatrix[1])) * axisLength;
		glm::vec3 zAxis = glm::normalize(glm::vec3(bone.WorldMatrix[2])) * axisLength;
		
		Renderer2D::SetLineWidth(m_LineWidth * 1.5f);
		
		glm::vec3 pos = bone.WorldPosition;
		
		// X axis (red)
		glm::vec3 xEnd = pos + xAxis;
		Renderer2D::DrawLine(pos, xEnd, m_Colors.AxisX);
		
		// Y axis (green)
		glm::vec3 yEnd = pos + yAxis;
		Renderer2D::DrawLine(pos, yEnd, m_Colors.AxisY);
		
		// Z axis (blue)
		glm::vec3 zEnd = pos + zAxis;
		Renderer2D::DrawLine(pos, zEnd, m_Colors.AxisZ);
		
		// Reset line width
		Renderer2D::SetLineWidth(m_LineWidth);
	}

	// ============================================================================
	// PICKING
	// ============================================================================

	int32_t BoneVisualization::PickBone(
		const glm::vec2& screenPos,
		const glm::mat4& viewProjection,
		float pickRadius
	) {
		if (m_Bones.empty()) return -1;
		
		float closestDistance = std::numeric_limits<float>::max();
		int32_t closestBone = -1;
		
		for (const auto& bone : m_Bones) {
			// Project bone position to screen space
			glm::vec4 clipPos = viewProjection * glm::vec4(bone.WorldPosition, 1.0f);
			if (clipPos.w <= 0.0f) continue;
			
			glm::vec2 ndcPos = glm::vec2(clipPos.x / clipPos.w, clipPos.y / clipPos.w);
			
			// Calculate distance to mouse position
			float distance = glm::length(ndcPos - screenPos);
			
			// Check if within pick radius (in NDC space, roughly)
			float screenPickRadius = pickRadius * 2.0f; // Approximate
			if (distance < screenPickRadius && distance < closestDistance) {
				closestDistance = distance;
				closestBone = bone.Index;
			}
		}
		
		return closestBone;
	}

	const BoneVisualData* BoneVisualization::GetBone(int32_t index) const {
		if (index >= 0 && index < static_cast<int32_t>(m_Bones.size())) {
			return &m_Bones[index];
		}
		return nullptr;
	}

	// ============================================================================
	// HELPERS
	// ============================================================================

	glm::vec4 BoneVisualization::GetColorForState(BoneVisualState state) const {
		switch (state) {
			case BoneVisualState::Hovered:  return m_Colors.Hovered;
			case BoneVisualState::Selected: return m_Colors.Selected;
			default:                        return m_Colors.Normal;
		}
	}

	glm::vec4 BoneVisualization::GetJointColorForState(BoneVisualState state) const {
		switch (state) {
			case BoneVisualState::Hovered:  return m_Colors.Hovered;
			case BoneVisualState::Selected: return m_Colors.JointSelected;
			default:                        return m_Colors.JointNormal;
		}
	}

}
