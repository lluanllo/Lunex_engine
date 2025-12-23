#pragma once

/**
 * @file BoneVisualization.h
 * @brief Bone visualization utilities for skeletal animation
 * 
 * Provides:
 *   - Bone line rendering
 *   - Joint sphere rendering
 *   - Bone picking/selection
 *   - Bone axis visualization
 */

#include "Core/Core.h"
#include "Renderer/Shader.h"
#include "Renderer/VertexArray.h"
#include "Renderer/Buffer.h"
#include "Assets/Animation/SkeletonAsset.h"

#include <glm/glm.hpp>
#include <vector>
#include <string>

namespace Lunex {

	// ============================================================================
	// BONE VISUAL STATE
	// ============================================================================
	
	enum class BoneVisualState {
		Normal,
		Hovered,
		Selected
	};
	
	struct BoneVisualData {
		int32_t Index = -1;
		std::string Name;
		glm::vec3 WorldPosition = glm::vec3(0.0f);
		glm::vec3 ParentWorldPosition = glm::vec3(0.0f);
		glm::mat4 WorldMatrix = glm::mat4(1.0f);
		int32_t ParentIndex = -1;
		BoneVisualState State = BoneVisualState::Normal;
	};
	
	// ============================================================================
	// BONE COLORS
	// ============================================================================
	
	struct BoneColors {
		glm::vec4 Normal = glm::vec4(0.2f, 0.8f, 0.3f, 1.0f);      // Green
		glm::vec4 Hovered = glm::vec4(1.0f, 0.9f, 0.2f, 1.0f);     // Yellow
		glm::vec4 Selected = glm::vec4(1.0f, 0.5f, 0.1f, 1.0f);    // Orange
		glm::vec4 JointNormal = glm::vec4(0.3f, 0.9f, 0.4f, 1.0f); // Bright green
		glm::vec4 JointSelected = glm::vec4(1.0f, 0.6f, 0.2f, 1.0f); // Bright orange
		glm::vec4 AxisX = glm::vec4(1.0f, 0.2f, 0.2f, 1.0f);       // Red
		glm::vec4 AxisY = glm::vec4(0.2f, 1.0f, 0.2f, 1.0f);       // Green
		glm::vec4 AxisZ = glm::vec4(0.2f, 0.2f, 1.0f, 1.0f);       // Blue
	};

	// ============================================================================
	// BONE VISUALIZATION
	// ============================================================================
	
	class BoneVisualization {
	public:
		BoneVisualization();
		~BoneVisualization();
		
		void Init();
		void Shutdown();
		
		// ========== BONE DATA UPDATE ==========
		
		/**
		 * Update bone visual data from skeleton and model-space matrices
		 */
		void UpdateBones(
			Ref<SkeletonAsset> skeleton,
			const std::vector<glm::mat4>& modelSpaceMatrices
		);
		
		/**
		 * Set selection state for a bone
		 */
		void SetBoneState(int32_t boneIndex, BoneVisualState state);
		void SetSelectedBone(int32_t boneIndex);
		void SetHoveredBone(int32_t boneIndex);
		void ClearSelection();
		void ClearHover();
		
		// ========== RENDERING ==========
		
		/**
		 * Render all bones as lines connecting joints
		 */
		void RenderBoneLines(const glm::mat4& viewProjection);
		
		/**
		 * Render joint spheres at each bone position
		 */
		void RenderJointSpheres(const glm::mat4& viewProjection, float jointRadius = 0.02f);
		
		/**
		 * Render local axes for selected bone
		 */
		void RenderBoneAxes(const glm::mat4& viewProjection, int32_t boneIndex, float axisLength = 0.1f);
		
		/**
		 * Render everything (lines + spheres + axes for selected)
		 */
		void Render(const glm::mat4& viewProjection);
		
		// ========== PICKING ==========
		
		/**
		 * Pick bone at screen position
		 * @param screenPos Normalized device coordinates (-1 to 1)
		 * @param viewProjection View-projection matrix
		 * @param pickRadius Radius in world units for picking
		 * @return Bone index or -1 if no hit
		 */
		int32_t PickBone(
			const glm::vec2& screenPos,
			const glm::mat4& viewProjection,
			float pickRadius = 0.05f
		);
		
		/**
		 * Get bone info at index
		 */
		const BoneVisualData* GetBone(int32_t index) const;
		
		/**
		 * Get all bones
		 */
		const std::vector<BoneVisualData>& GetBones() const { return m_Bones; }
		
		// ========== CONFIGURATION ==========
		
		void SetColors(const BoneColors& colors) { m_Colors = colors; }
		const BoneColors& GetColors() const { return m_Colors; }
		
		void SetLineWidth(float width) { m_LineWidth = width; }
		float GetLineWidth() const { return m_LineWidth; }
		
		void SetJointRadius(float radius) { m_JointRadius = radius; }
		float GetJointRadius() const { return m_JointRadius; }
		
		void SetAxisLength(float length) { m_AxisLength = length; }
		float GetAxisLength() const { return m_AxisLength; }
		
		// ========== STATE QUERIES ==========
		
		int32_t GetSelectedBone() const { return m_SelectedBone; }
		int32_t GetHoveredBone() const { return m_HoveredBone; }
		uint32_t GetBoneCount() const { return static_cast<uint32_t>(m_Bones.size()); }
		
	private:
		void CreateLineShader();
		void CreateSphereGeometry();
		
		glm::vec4 GetColorForState(BoneVisualState state) const;
		glm::vec4 GetJointColorForState(BoneVisualState state) const;
		
	private:
		std::vector<BoneVisualData> m_Bones;
		
		int32_t m_SelectedBone = -1;
		int32_t m_HoveredBone = -1;
		
		BoneColors m_Colors;
		float m_LineWidth = 2.0f;
		float m_JointRadius = 0.02f;
		float m_AxisLength = 0.1f;
		
		// Rendering resources
		Ref<Shader> m_LineShader;
		Ref<VertexArray> m_SphereVAO;
		Ref<VertexBuffer> m_SphereVBO;
		Ref<IndexBuffer> m_SphereIBO;
		uint32_t m_SphereIndexCount = 0;
		
		bool m_Initialized = false;
	};

}
