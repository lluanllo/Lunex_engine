#pragma once

/**
 * @file AnimationEditorPanel.h
 * @brief Keyframe animation editor for skinned meshes (.luanim assets).
 *
 * Workflow:
 *   1. Select an entity with a rigged Mesh Renderer + Animator.
 *   2. New/Open a .luanim clip (it is added to the Animator's clip list).
 *   3. Pick a bone in the tree, pose it with the viewport gizmo:
 *        FK - rotate (E) / move (W) the bone itself
 *        IK - move the bone as an end effector; the chain above it bends to follow
 *   4. Key it: Rotation, Position or Both (I = both).
 *   5. Scrub / play the timeline, drag diamonds to retime, Del removes the selected key.
 */

#include "Core/Core.h"
#include "Scene/Scene.h"
#include "Scene/Entity.h"
#include "Scene/Components.h"
#include "Assets/Animation/AnimationAsset.h"
#include "Resources/Animation/Animation.h"

#include <glm/glm.hpp>

#include <filesystem>
#include <set>
#include <string>
#include <vector>

namespace Lunex {

	class AnimationEditorPanel {
	public:
		AnimationEditorPanel();
		~AnimationEditorPanel();

		void SetContext(const Ref<Scene>& scene);
		void SetSelectedEntity(Entity entity);

		void Open() { m_IsOpen = true; }
		void Toggle() { m_IsOpen = !m_IsOpen; }
		bool IsOpen() const { return m_IsOpen; }

		void OpenAsset(const std::filesystem::path& path);

		void OnUpdate(float dt);
		void OnImGuiRender();

		// ---- Viewport integration (bone gizmo) ----
		static AnimationEditorPanel* GetActive();          // non-null while a bone is being edited
		static void OpenFor(Entity entity);                 // used by the Animator inspector
		bool WantsBoneGizmo() const;
		bool IsHoveringJoint() const { return m_HoveredJoint >= 0; }
		// Joint dots in the viewport: click one to select that bone
		void DrawJointOverlay(const glm::mat4& view, const glm::mat4& projection,
			const glm::vec2& viewportMin, const glm::vec2& viewportMax);
		void DrawBoneGizmo(const glm::mat4& view, const glm::mat4& projection, int gizmoType,
			const glm::vec2& viewportMin, const glm::vec2& viewportMax);

	private:
		// State helpers
		bool HasValidTarget() const;
		const Skeleton* GetSkeleton() const;
		AnimatorComponent* GetAnimator() const;
		void ReleaseTarget();
		void SamplePoseAtTime();
		float SnapTime(float t) const;
		std::string NodeName(int node) const;
		bool IsHelperNode(int node) const;

		// Actions
		void NewAsset();
		void OpenAssetDialog();
		void SaveAsset();
		void AttachAssetToAnimator();
		void KeySelected(AnimationKeyType type);
		void DeleteSelectedKey();

		// UI
		void DrawToolbar();
		void DrawBoneTree();
		void DrawBoneTreeNode(int node, const std::vector<std::vector<int>>& children);
		void DrawTimeline();

	private:
		bool m_IsOpen = false;
		Ref<Scene> m_Scene;
		Entity m_Entity;

		Ref<AnimationAsset> m_Asset;
		std::filesystem::path m_AssetPath;

		float m_Time = 0.0f;
		bool m_Playing = false;
		bool m_EditMode = true;           // show the edited pose on the model
		bool m_PoseEdited = false;        // pose differs from the sampled clip
		bool m_IKMode = false;
		int m_IKChainLength = 2;
		bool m_AutoKey = false;
		AnimationKeyType m_KeyType = AnimationKeyType::Rotation;

		std::vector<NodeTRS> m_Pose;      // edited local pose
		std::set<int> m_DirtyNodes;       // bones changed since last key
		int m_SelectedNode = -1;

		// Selected key on the timeline
		std::string m_SelectedKeyBone;
		float m_SelectedKeyTime = -1.0f;
		bool m_DraggingKey = false;
		bool m_GizmoWasUsing = false;
		int m_HoveredJoint = -1;

		// Timeline view
		float m_PixelsPerSecond = 160.0f;
		float m_ViewStart = 0.0f;

		static AnimationEditorPanel* s_Instance;
		static AnimationEditorPanel* s_Active;
	};

} // namespace Lunex
