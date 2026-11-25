#include "stpch.h"
#include "AnimationEditorPanel.h"

#include "../UI/LunexUI.h"
#include "Resources/Animation/IKSolver.h"
#include "Resources/Mesh/Model.h"
#include "Project/Project.h"
#include "Utils/PlatformUtils.h"

#include <imgui.h>
#include <imgui_internal.h>
#include <ImGuizmo.h>

#include <glm/gtc/type_ptr.hpp>
#include <glm/gtx/quaternion.hpp>

#include <algorithm>
#include <cmath>
#include <functional>

namespace Lunex {

	AnimationEditorPanel* AnimationEditorPanel::s_Instance = nullptr;
	AnimationEditorPanel* AnimationEditorPanel::s_Active = nullptr;

	namespace {
		ImU32 KeyColor(AnimationKeyType type) {
			switch (type) {
				case AnimationKeyType::Rotation: return IM_COL32(217, 130, 43, 255);   // accent orange
				case AnimationKeyType::Position: return IM_COL32(70, 150, 255, 255);   // blue
				default:                         return IM_COL32(235, 235, 235, 255);  // both
			}
		}

		const char* KeyTypeLabel(AnimationKeyType type) {
			switch (type) {
				case AnimationKeyType::Rotation: return "Rotation";
				case AnimationKeyType::Position: return "Position";
				default: return "Both";
			}
		}

		std::filesystem::path AssetRelative(const std::filesystem::path& p) {
			if (auto project = Project::GetActive()) {
				std::error_code ec;
				auto rel = std::filesystem::relative(p, project->GetAssetDirectory(), ec);
				if (!ec && !rel.empty() && rel.string().rfind("..", 0) != 0) return rel;
			}
			return p;
		}

		std::filesystem::path ResolveAssetPath(const std::string& stored) {
			std::filesystem::path p(stored);
			if (p.is_relative() && Project::GetActive()) p = Project::GetActiveAssetDirectory() / p;
			return p;
		}
	}

	// ============================================================================
	// LIFETIME / TARGET
	// ============================================================================

	AnimationEditorPanel::AnimationEditorPanel() { s_Instance = this; }
	AnimationEditorPanel::~AnimationEditorPanel() {
		if (s_Instance == this) s_Instance = nullptr;
		if (s_Active == this) s_Active = nullptr;
	}

	AnimationEditorPanel* AnimationEditorPanel::GetActive() { return s_Active; }

	void AnimationEditorPanel::OpenFor(Entity entity) {
		if (!s_Instance) return;
		s_Instance->SetSelectedEntity(entity);
		s_Instance->Open();
	}

	void AnimationEditorPanel::SetContext(const Ref<Scene>& scene) {
		if (scene == m_Scene) return;
		ReleaseTarget();
		m_Scene = scene;
		m_Entity = {};
		m_SelectedNode = -1;
		m_Pose.clear();
		m_DirtyNodes.clear();
		m_PoseEdited = false;
		m_Playing = false;
	}

	void AnimationEditorPanel::SetSelectedEntity(Entity entity) {
		if (entity == m_Entity) return;
		ReleaseTarget();
		m_Entity = entity;
		m_Asset.reset();          // the new target's active clip is picked up in OnUpdate
		m_AssetPath.clear();
		m_Time = 0.0f;
		m_Playing = false;
		m_SelectedNode = -1;
		m_Pose.clear();
		m_DirtyNodes.clear();
		m_PoseEdited = false;
	}

	bool AnimationEditorPanel::HasValidTarget() const {
		if (!m_Scene || !m_Scene->IsEntityValid(m_Entity)) return false;
		Entity e = m_Entity;
		if (!e.HasComponent<MeshComponent>() || !e.HasComponent<AnimatorComponent>()) return false;
		const auto& mesh = e.GetComponent<MeshComponent>();
		return mesh.MeshModel && mesh.MeshModel->IsSkinned();
	}

	const Skeleton* AnimationEditorPanel::GetSkeleton() const {
		Entity e = m_Entity;
		return HasValidTarget() ? e.GetComponent<MeshComponent>().MeshModel->GetSkeleton().get() : nullptr;
	}

	AnimatorComponent* AnimationEditorPanel::GetAnimator() const {
		Entity e = m_Entity;
		return HasValidTarget() ? &e.GetComponent<AnimatorComponent>() : nullptr;
	}

	void AnimationEditorPanel::ReleaseTarget() {
		if (auto* animator = GetAnimator()) {
			animator->UseEditPose = false;
			animator->EditPose.clear();
			animator->EditorSelectedNode = -1;
		}
		m_HoveredJoint = -1;
		if (s_Active == this) s_Active = nullptr;
	}

	std::string AnimationEditorPanel::NodeName(int node) const {
		const Skeleton* s = GetSkeleton();
		return (s && node >= 0 && node < static_cast<int>(s->Nodes.size())) ? s->Nodes[node].Name : std::string();
	}

	// assimp inserts "$AssimpFbx$" pivot helper nodes; they are hidden in the tree
	bool AnimationEditorPanel::IsHelperNode(int node) const {
		return NodeName(node).find("$AssimpFbx$") != std::string::npos;
	}

	float AnimationEditorPanel::SnapTime(float t) const {
		float fps = m_Asset ? std::max(m_Asset->FrameRate, 1.0f) : 30.0f;
		return std::round(t * fps) / fps;
	}

	void AnimationEditorPanel::SamplePoseAtTime() {
		const Skeleton* skel = GetSkeleton();
		if (!skel) return;
		const AnimationClip* clip = m_Asset ? m_Asset->GetClip(*skel) : nullptr;
		AnimationSampler::SampleLocalPose(*skel, clip, m_Time, m_Asset ? m_Asset->Loop : true, m_Pose);
	}

	// ============================================================================
	// UPDATE
	// ============================================================================

	void AnimationEditorPanel::OnUpdate(float dt) {
		if (!m_IsOpen || !HasValidTarget()) {
			ReleaseTarget();
			return;
		}

		const Skeleton* skel = GetSkeleton();

		// Pick up the clip the Animator is playing when it is an authored .luanim
		if (!m_Asset) {
			auto* animator = GetAnimator();
			const int modelClips = static_cast<int>(m_Entity.GetComponent<MeshComponent>().MeshModel->GetAnimations().size());
			const int assetIndex = animator->ClipIndex - modelClips;
			if (assetIndex >= 0 && assetIndex < static_cast<int>(animator->AnimationAssets.size()))
				OpenAsset(ResolveAssetPath(animator->AnimationAssets[assetIndex]));
		}

		if (m_Playing && m_Asset) {
			m_Time += dt;
			float duration = std::max(m_Asset->Duration, 1e-3f);
			if (m_Time > duration) m_Time = m_Asset->Loop ? std::fmod(m_Time, duration) : duration;
			m_PoseEdited = false;
		}
		if (!m_PoseEdited || m_Pose.size() != skel->Nodes.size()) {
			SamplePoseAtTime();
			m_DirtyNodes.clear();
		}

		auto* animator = GetAnimator();
		animator->UseEditPose = m_EditMode;
		animator->EditPose = m_Pose;
		animator->ShowSkeleton = true;
		animator->EditorSelectedNode = m_SelectedNode;
		s_Active = this;
	}

	// ============================================================================
	// ACTIONS
	// ============================================================================

	void AnimationEditorPanel::OpenAsset(const std::filesystem::path& path) {
		auto asset = AnimationAsset::GetOrLoad(path);
		if (!asset) {
			LNX_LOG_ERROR("Animation Editor: could not open {0}", path.string());
			return;
		}
		m_Asset = asset;
		m_AssetPath = path;
		m_Time = 0.0f;
		m_PoseEdited = false;
		m_IsOpen = true;
		AttachAssetToAnimator();
	}

	void AnimationEditorPanel::NewAsset() {
		std::string file = FileDialogs::SaveFile("Lunex Animation (*.luanim)\0*.luanim\0");
		if (file.empty()) return;
		std::filesystem::path path(file);
		if (path.extension() != ".luanim") path += ".luanim";

		auto asset = CreateRef<AnimationAsset>(path.stem().string());
		asset->Duration = 2.0f;
		asset->FrameRate = 30.0f;
		if (!asset->SaveToFile(path)) return;
		OpenAsset(path);
	}

	void AnimationEditorPanel::OpenAssetDialog() {
		std::string file = FileDialogs::OpenFile("Lunex Animation (*.luanim)\0*.luanim\0");
		if (!file.empty()) OpenAsset(file);
	}

	void AnimationEditorPanel::SaveAsset() {
		if (m_Asset && !m_AssetPath.empty() && m_Asset->SaveToFile(m_AssetPath)) {
			LNX_LOG_INFO("Animation saved: {0} ({1} keys)", m_AssetPath.filename().string(), m_Asset->GetKeyCount());
		}
	}

	// Add the clip to the Animator's list (if missing) and select it
	void AnimationEditorPanel::AttachAssetToAnimator() {
		auto* animator = GetAnimator();
		if (!animator || m_AssetPath.empty()) return;
		std::string stored = AssetRelative(m_AssetPath).generic_string();

		int index = -1;
		for (size_t i = 0; i < animator->AnimationAssets.size(); i++) {
			std::error_code ec;
			if (animator->AnimationAssets[i] == stored ||
				std::filesystem::equivalent(ResolveAssetPath(animator->AnimationAssets[i]), m_AssetPath, ec)) {
				index = static_cast<int>(i);
				break;
			}
		}
		if (index < 0) {
			animator->AnimationAssets.push_back(stored);
			index = static_cast<int>(animator->AnimationAssets.size()) - 1;
		}
		const auto& model = m_Entity.GetComponent<MeshComponent>().MeshModel;
		animator->ClipIndex = static_cast<int>(model->GetAnimations().size()) + index;
	}

	void AnimationEditorPanel::KeySelected(AnimationKeyType type) {
		const Skeleton* skel = GetSkeleton();
		if (!skel || !m_Asset) return;

		std::set<int> nodes = m_DirtyNodes;
		if (m_SelectedNode >= 0) nodes.insert(m_SelectedNode);
		const float t = SnapTime(m_Time);

		for (int node : nodes) {
			if (node < 0 || node >= static_cast<int>(m_Pose.size())) continue;
			// Chain bones moved by IK only need their rotation keyed
			AnimationKeyType nodeType = (node == m_SelectedNode) ? type : AnimationKeyType::Rotation;
			m_Asset->SetKey(skel->Nodes[node].Name, t, nodeType, m_Pose[node].Translation, m_Pose[node].Rotation);
		}
		m_DirtyNodes.clear();
		m_PoseEdited = false;   // the clip now reproduces this pose
		if (m_SelectedNode >= 0) { m_SelectedKeyBone = NodeName(m_SelectedNode); m_SelectedKeyTime = t; }
	}

	void AnimationEditorPanel::DeleteSelectedKey() {
		if (!m_Asset || m_SelectedKeyBone.empty()) return;
		if (m_Asset->RemoveKey(m_SelectedKeyBone, m_SelectedKeyTime)) {
			m_SelectedKeyBone.clear();
			m_SelectedKeyTime = -1.0f;
			m_PoseEdited = false;
		}
	}

	// ============================================================================
	// VIEWPORT BONE GIZMO
	// ============================================================================

	bool AnimationEditorPanel::WantsBoneGizmo() const {
		return m_IsOpen && m_EditMode && !m_Playing && HasValidTarget() && m_SelectedNode >= 0;
	}

	void AnimationEditorPanel::DrawJointOverlay(const glm::mat4& view, const glm::mat4& projection,
		const glm::vec2& viewportMin, const glm::vec2& viewportMax)
	{
		m_HoveredJoint = -1;
		const Skeleton* skel = GetSkeleton();
		if (!skel || !m_EditMode || m_Pose.size() != skel->Nodes.size()) return;

		std::vector<glm::mat4> model;
		IK::ComputeModelSpace(*skel, m_Pose, model);
		const glm::mat4 viewProj = projection * view * m_Scene->GetWorldTransform(m_Entity);
		const glm::vec2 size = viewportMax - viewportMin;
		const ImVec2 mouse = ImGui::GetIO().MousePos;
		ImDrawList* dl = ImGui::GetWindowDrawList();

		auto toScreen = [&](int node, ImVec2& out) {
			glm::vec4 clip = viewProj * glm::vec4(glm::vec3(model[node][3]), 1.0f);
			if (clip.w <= 0.0f) return false;
			glm::vec2 ndc = glm::vec2(clip) / clip.w;
			out = ImVec2(viewportMin.x + (ndc.x * 0.5f + 0.5f) * size.x, viewportMin.y + (0.5f - ndc.y * 0.5f) * size.y);
			return out.x >= viewportMin.x && out.y >= viewportMin.y && out.x <= viewportMax.x && out.y <= viewportMax.y;
		};

		float bestDist = 8.0f;
		for (int i = 0; i < static_cast<int>(skel->Nodes.size()); i++) {
			if (skel->Nodes[i].BoneIndex < 0 || IsHelperNode(i)) continue;
			ImVec2 p;
			if (!toScreen(i, p)) continue;
			const bool selected = (i == m_SelectedNode);
			dl->AddCircleFilled(p, selected ? 5.5f : 3.5f, IM_COL32(15, 15, 15, 200));
			dl->AddCircleFilled(p, selected ? 4.0f : 2.5f, selected ? IM_COL32(50, 215, 255, 255) : IM_COL32(255, 215, 60, 255));
			float d = std::hypot(mouse.x - p.x, mouse.y - p.y);
			if (d < bestDist) { bestDist = d; m_HoveredJoint = i; }
		}

		if (ImGuizmo::IsOver() || ImGuizmo::IsUsing()) { m_HoveredJoint = -1; return; }
		ImVec2 p;
		if (m_HoveredJoint >= 0 && toScreen(m_HoveredJoint, p)) {
			dl->AddCircle(p, 7.0f, IM_COL32(255, 255, 255, 230), 0, 1.5f);
			std::string name = NodeName(m_HoveredJoint);
			if (name.rfind("mixamorig:", 0) == 0) name = name.substr(10);
			dl->AddText(ImVec2(p.x + 9.0f, p.y - 7.0f), IM_COL32(255, 255, 255, 230), name.c_str());
			if (ImGui::IsMouseClicked(ImGuiMouseButton_Left)) m_SelectedNode = m_HoveredJoint;
		}
	}

	void AnimationEditorPanel::DrawBoneGizmo(const glm::mat4& view, const glm::mat4& projection, int gizmoType,
		const glm::vec2& viewportMin, const glm::vec2& viewportMax)
	{
		const Skeleton* skel = GetSkeleton();
		if (!skel || m_Pose.size() != skel->Nodes.size()) return;

		std::vector<glm::mat4> model;
		IK::ComputeModelSpace(*skel, m_Pose, model);
		glm::mat4 entityWorld = m_Scene->GetWorldTransform(m_Entity);
		glm::mat4 boneWorld = entityWorld * model[m_SelectedNode];

		// IK mode always drags the effector; FK uses the editor's W/E selection
		ImGuizmo::OPERATION op = m_IKMode ? ImGuizmo::TRANSLATE
			: (gizmoType == ImGuizmo::ROTATE ? ImGuizmo::ROTATE : ImGuizmo::TRANSLATE);

		ImGuizmo::SetOrthographic(false);
		ImGuizmo::SetDrawlist();
		ImGuizmo::SetRect(viewportMin.x, viewportMin.y, viewportMax.x - viewportMin.x, viewportMax.y - viewportMin.y);

		// Normalize away the (often 0.01) skeleton scale so the gizmo has a usable size
		glm::vec3 scale(glm::length(glm::vec3(boneWorld[0])), glm::length(glm::vec3(boneWorld[1])), glm::length(glm::vec3(boneWorld[2])));
		glm::mat4 gizmo = boneWorld;
		for (int c = 0; c < 3; c++) if (scale[c] > 1e-8f) gizmo[c] /= scale[c];

		ImGuizmo::Manipulate(glm::value_ptr(view), glm::value_ptr(projection), op,
			op == ImGuizmo::ROTATE ? ImGuizmo::LOCAL : ImGuizmo::WORLD, glm::value_ptr(gizmo));

		const bool using_ = ImGuizmo::IsUsing();
		if (using_) {
			for (int c = 0; c < 3; c++) gizmo[c] *= scale[c];
			glm::mat4 desiredModel = glm::inverse(entityWorld) * gizmo;

			if (m_IKMode) {
				IK::SolveCCD(*skel, m_Pose, m_SelectedNode, m_IKChainLength, glm::vec3(desiredModel[3]));
				for (int j = skel->Nodes[m_SelectedNode].Parent, c = 0; j >= 0 && c < m_IKChainLength; j = skel->Nodes[j].Parent, c++)
					m_DirtyNodes.insert(j);
			}
			else {
				IK::SetNodeModelTransform(*skel, m_Pose, model, m_SelectedNode, desiredModel,
					op == ImGuizmo::ROTATE, op == ImGuizmo::TRANSLATE);
				m_DirtyNodes.insert(m_SelectedNode);
			}
			m_PoseEdited = true;
		}

		// Auto-key when a drag finishes
		if (m_GizmoWasUsing && !using_ && m_AutoKey) {
			KeySelected(m_IKMode ? AnimationKeyType::Rotation
				: (op == ImGuizmo::ROTATE ? AnimationKeyType::Rotation : AnimationKeyType::Position));
		}
		m_GizmoWasUsing = using_;
	}

	// ============================================================================
	// UI
	// ============================================================================

	void AnimationEditorPanel::OnImGuiRender() {
		if (!m_IsOpen) return;

		ImGui::SetNextWindowSize(ImVec2(980, 380), ImGuiCond_FirstUseEver);
		if (!ImGui::Begin("Animation Editor", &m_IsOpen)) {
			ImGui::End();
			if (!m_IsOpen) ReleaseTarget();
			return;
		}

		if (!HasValidTarget()) {
			ImGui::TextDisabled("Select an entity with a rigged Mesh Renderer and an Animator component.");
			ImGui::TextDisabled("(Import an FBX/glTF with bones, add Animator from 'Add Component'.)");
			ImGui::End();
			return;
		}

		DrawToolbar();
		ImGui::Separator();

		ImGui::BeginChild("##AnimBones", ImVec2(240, 0), true);
		DrawBoneTree();
		ImGui::EndChild();
		ImGui::SameLine();
		ImGui::BeginChild("##AnimTimeline", ImVec2(0, 0), true, ImGuiWindowFlags_NoScrollWithMouse);
		DrawTimeline();
		ImGui::EndChild();

		// Shortcuts while the panel is focused
		if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && !ImGui::GetIO().WantTextInput) {
			if (ImGui::IsKeyPressed(ImGuiKey_I, false)) KeySelected(AnimationKeyType::Both);
			if (ImGui::IsKeyPressed(ImGuiKey_Delete, false)) DeleteSelectedKey();
			if (ImGui::IsKeyPressed(ImGuiKey_Space, false)) m_Playing = !m_Playing;
		}

		ImGui::End();
		if (!m_IsOpen) ReleaseTarget();
	}

	void AnimationEditorPanel::DrawToolbar() {

		if (ImGui::Button("New")) NewAsset();
		ImGui::SameLine();
		if (ImGui::Button("Open")) OpenAssetDialog();
		ImGui::SameLine();
		ImGui::BeginDisabled(!m_Asset);
		if (ImGui::Button(m_Asset && m_Asset->IsDirty() ? "Save*" : "Save")) SaveAsset();
		ImGui::EndDisabled();

		// Clip picker (authored clips on this Animator)
		ImGui::SameLine();
		ImGui::SetNextItemWidth(170);
		auto* animator = GetAnimator();
		std::string current = m_AssetPath.empty() ? "(no clip)" : m_AssetPath.stem().string();
		if (ImGui::BeginCombo("##Clip", current.c_str())) {
			for (const auto& stored : animator->AnimationAssets) {
				auto path = ResolveAssetPath(stored);
				if (ImGui::Selectable(path.stem().string().c_str(), path == m_AssetPath)) OpenAsset(path);
			}
			ImGui::EndCombo();
		}

		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();
		if (ImGui::Button("|<")) { m_Time = 0.0f; m_PoseEdited = false; }
		ImGui::SameLine();
		if (ImGui::Button(m_Playing ? "Pause" : "Play")) m_Playing = !m_Playing;
		ImGui::SameLine();
		if (ImGui::Button(">|") && m_Asset) { m_Time = m_Asset->Duration; m_PoseEdited = false; }

		ImGui::SameLine();
		ImGui::SetNextItemWidth(70);
		if (ImGui::DragFloat("##Time", &m_Time, 0.01f, 0.0f, m_Asset ? m_Asset->Duration : 10.0f, "%.2f s")) m_PoseEdited = false;

		if (m_Asset) {
			ImGui::SameLine();
			ImGui::SetNextItemWidth(70);
			if (ImGui::DragFloat("##Duration", &m_Asset->Duration, 0.05f, 0.1f, 600.0f, "len %.1fs")) m_Asset->MarkDirty();
			ImGui::SameLine();
			ImGui::SetNextItemWidth(60);
			if (ImGui::DragFloat("##FPS", &m_Asset->FrameRate, 1.0f, 1.0f, 120.0f, "%.0f fps")) m_Asset->MarkDirty();
			ImGui::SameLine();
			if (ImGui::Checkbox("Loop", &m_Asset->Loop)) m_Asset->MarkDirty();
		}

		// Second row: editing tools
		ImGui::Checkbox("Edit Pose", &m_EditMode);
		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();
		bool fk = !m_IKMode;
		if (ImGui::RadioButton("FK", fk)) m_IKMode = false;
		ImGui::SameLine();
		if (ImGui::RadioButton("IK", m_IKMode)) m_IKMode = true;
		// Always visible (disabled in FK) so the toolbar layout doesn't jump
		ImGui::SameLine();
		ImGui::BeginDisabled(!m_IKMode);
		ImGui::SetNextItemWidth(70);
		ImGui::SliderInt("##Chain", &m_IKChainLength, 1, 8, "chain %d");
		if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
			ImGui::SetTooltip("IK chain length: joints above the dragged bone that bend (2 = arm / leg)");
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::TextDisabled("|");
		ImGui::SameLine();

		ImGui::BeginDisabled(!m_Asset || m_SelectedNode < 0);
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(KeyColor(AnimationKeyType::Rotation)));
		if (ImGui::Button("Key Rot")) KeySelected(AnimationKeyType::Rotation);
		ImGui::PopStyleColor();
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_Button, ImGui::ColorConvertU32ToFloat4(KeyColor(AnimationKeyType::Position)));
		if (ImGui::Button("Key Pos")) KeySelected(AnimationKeyType::Position);
		ImGui::PopStyleColor();
		ImGui::SameLine();
		if (ImGui::Button("Key Both (I)")) KeySelected(AnimationKeyType::Both);
		ImGui::EndDisabled();
		ImGui::SameLine();
		ImGui::Checkbox("Auto Key", &m_AutoKey);
		ImGui::SameLine();
		ImGui::BeginDisabled(!m_PoseEdited);
		if (ImGui::Button("Reset Pose")) { m_PoseEdited = false; m_DirtyNodes.clear(); }
		ImGui::EndDisabled();

		if (!m_Asset) {
			ImGui::SameLine();
			ImGui::TextColored(ImVec4(0.85f, 0.51f, 0.17f, 1.0f), "  Create or open a .luanim clip to start keying");
		}
	}

	void AnimationEditorPanel::DrawBoneTree() {
		const Skeleton* skel = GetSkeleton();
		ImGui::TextDisabled("Bones");
		ImGui::Separator();

		std::vector<std::vector<int>> children(skel->Nodes.size());
		std::vector<int> roots;
		for (int i = 0; i < static_cast<int>(skel->Nodes.size()); i++) {
			int p = skel->Nodes[i].Parent;
			if (p >= 0) children[p].push_back(i); else roots.push_back(i);
		}
		for (int r : roots) DrawBoneTreeNode(r, children);
	}

	void AnimationEditorPanel::DrawBoneTreeNode(int node, const std::vector<std::vector<int>>& children) {
		const Skeleton* skel = GetSkeleton();

		// Hide helper nodes but keep their children (flattened)
		if (IsHelperNode(node)) {
			for (int c : children[node]) DrawBoneTreeNode(c, children);
			return;
		}

		const auto& n = skel->Nodes[node];
		std::string label = n.Name;
		const std::string prefix = "mixamorig:";
		if (label.rfind(prefix, 0) == 0) label = label.substr(prefix.size());
		bool hasTrack = m_Asset && m_Asset->FindTrack(n.Name);
		if (hasTrack) label = "* " + label;

		ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | ImGuiTreeNodeFlags_SpanAvailWidth | ImGuiTreeNodeFlags_DefaultOpen;
		if (children[node].empty()) flags |= ImGuiTreeNodeFlags_Leaf;
		if (node == m_SelectedNode) flags |= ImGuiTreeNodeFlags_Selected;
		if (n.BoneIndex < 0) ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));

		bool open = ImGui::TreeNodeEx((void*)(intptr_t)node, flags, "%s", label.c_str());
		if (n.BoneIndex < 0) ImGui::PopStyleColor();
		if (ImGui::IsItemClicked()) {
			m_SelectedNode = node;
			if (auto* animator = GetAnimator()) animator->DebugWeightsBone = -1;
		}
		if (open) {
			for (int c : children[node]) DrawBoneTreeNode(c, children);
			ImGui::TreePop();
		}
	}

	void AnimationEditorPanel::DrawTimeline() {
		ImDrawList* dl = ImGui::GetWindowDrawList();
		const ImVec2 origin = ImGui::GetCursorScreenPos();
		const ImVec2 avail = ImGui::GetContentRegionAvail();
		const float labelWidth = 150.0f;
		const float rulerHeight = 24.0f;
		const float rowHeight = 20.0f;
		const float x0 = origin.x + labelWidth;
		const float width = std::max(avail.x - labelWidth, 50.0f);
		const float duration = m_Asset ? std::max(m_Asset->Duration, 0.1f) : 2.0f;
		const float fps = m_Asset ? std::max(m_Asset->FrameRate, 1.0f) : 30.0f;

		// Fit the clip in the view unless zoomed (Ctrl+wheel zooms, wheel pans)
		if (ImGui::IsWindowHovered()) {
			const ImGuiIO& io = ImGui::GetIO();
			if (io.MouseWheel != 0.0f) {
				if (io.KeyCtrl) m_PixelsPerSecond = std::clamp(m_PixelsPerSecond * (io.MouseWheel > 0 ? 1.15f : 1.0f / 1.15f), 20.0f, 2000.0f);
				else m_ViewStart = std::max(0.0f, m_ViewStart - io.MouseWheel * 40.0f / m_PixelsPerSecond);
			}
		}
		if (m_PixelsPerSecond * duration < width * 0.5f && !ImGui::GetIO().KeyCtrl) m_PixelsPerSecond = width / duration;

		auto timeToX = [&](float t) { return x0 + (t - m_ViewStart) * m_PixelsPerSecond; };
		auto xToTime = [&](float x) { return std::max(0.0f, m_ViewStart + (x - x0) / m_PixelsPerSecond); };

		// ---- Ruler ----
		const ImU32 rulerBg = ImGui::GetColorU32(ImGuiCol_FrameBg);
		const ImU32 tickCol = ImGui::GetColorU32(ImGuiCol_TextDisabled);
		const ImU32 textCol = ImGui::GetColorU32(ImGuiCol_Text);
		dl->AddRectFilled(ImVec2(x0, origin.y), ImVec2(x0 + width, origin.y + rulerHeight), rulerBg);
		const float pxPerFrame = m_PixelsPerSecond / fps;
		int frameStep = pxPerFrame >= 8.0f ? 1 : pxPerFrame >= 3.0f ? 5 : 10;
		int firstFrame = static_cast<int>(m_ViewStart * fps);
		for (int f = firstFrame; ; f += 1) {
			float t = f / fps;
			float x = timeToX(t);
			if (x > x0 + width) break;
			if (x < x0 || f % frameStep != 0) continue;
			bool major = (f % static_cast<int>(fps)) == 0;
			dl->AddLine(ImVec2(x, origin.y + (major ? 4.0f : 14.0f)), ImVec2(x, origin.y + rulerHeight), tickCol);
			if (major) {
				char buf[16];
				snprintf(buf, sizeof(buf), "%.0fs", t);
				dl->AddText(ImVec2(x + 3.0f, origin.y + 2.0f), textCol, buf);
			}
		}
		// end of clip marker
		float endX = timeToX(duration);
		if (endX >= x0 && endX <= x0 + width)
			dl->AddLine(ImVec2(endX, origin.y), ImVec2(endX, origin.y + avail.y), IM_COL32(200, 60, 60, 160), 2.0f);

		// Scrub on the ruler
		ImGui::SetCursorScreenPos(ImVec2(x0, origin.y));
		ImGui::InvisibleButton("##Ruler", ImVec2(width, rulerHeight));
		if (ImGui::IsItemActive()) {
			m_Time = SnapTime(std::min(xToTime(ImGui::GetIO().MousePos.x), duration));
			m_Playing = false;
			m_PoseEdited = false;
		}

		// ---- Rows: bones with tracks (+ the selected bone) ----
		std::vector<std::string> rows;
		if (m_Asset) for (const auto& t : m_Asset->GetTracks()) rows.push_back(t.Bone);
		std::string selectedName = NodeName(m_SelectedNode);
		if (!selectedName.empty() && std::find(rows.begin(), rows.end(), selectedName) == rows.end())
			rows.push_back(selectedName);

		float y = origin.y + rulerHeight + 2.0f;
		bool clickedKey = false;
		for (size_t r = 0; r < rows.size(); r++) {
			const std::string& bone = rows[r];
			bool isSelectedRow = (bone == selectedName);
			ImU32 rowBg = isSelectedRow ? IM_COL32(217, 130, 43, 40) : ((r % 2) ? IM_COL32(255, 255, 255, 8) : IM_COL32(0, 0, 0, 0));
			dl->AddRectFilled(ImVec2(origin.x, y), ImVec2(x0 + width, y + rowHeight), rowBg);

			std::string label = bone;
			if (label.rfind("mixamorig:", 0) == 0) label = label.substr(10);
			dl->PushClipRect(ImVec2(origin.x, y), ImVec2(x0 - 4.0f, y + rowHeight), true);
			dl->AddText(ImVec2(origin.x + 6.0f, y + 3.0f), isSelectedRow ? textCol : tickCol, label.c_str());
			dl->PopClipRect();

			// Click the label to select the bone
			ImGui::SetCursorScreenPos(ImVec2(origin.x, y));
			ImGui::PushID(static_cast<int>(r));
			if (ImGui::InvisibleButton("##RowLabel", ImVec2(labelWidth - 4.0f, rowHeight))) {
				if (const Skeleton* skel = GetSkeleton()) {
					auto it = skel->NodeIndexByName.find(bone);
					if (it != skel->NodeIndexByName.end()) m_SelectedNode = it->second;
				}
			}

			// Keyframe diamonds
			if (m_Asset) {
				if (auto* track = m_Asset->FindTrack(bone)) {
					for (size_t k = 0; k < track->Keys.size(); k++) {
						auto& key = track->Keys[k];
						float kx = timeToX(key.Time);
						if (kx < x0 - 6.0f || kx > x0 + width + 6.0f) continue;
						float cy = y + rowHeight * 0.5f;
						const float s = 6.0f;
						bool selected = (bone == m_SelectedKeyBone && std::abs(key.Time - m_SelectedKeyTime) < 0.5f / fps);
						ImVec2 pts[4] = { {kx, cy - s}, {kx + s, cy}, {kx, cy + s}, {kx - s, cy} };
						dl->AddConvexPolyFilled(pts, 4, KeyColor(key.Type));
						if (selected) dl->AddPolyline(pts, 4, IM_COL32(255, 255, 255, 255), ImDrawFlags_Closed, 2.0f);

						ImGui::SetCursorScreenPos(ImVec2(kx - s, cy - s));
						ImGui::PushID(static_cast<int>(k));
						ImGui::InvisibleButton("##Key", ImVec2(s * 2.0f, s * 2.0f));
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip("%s  %.2fs  (%s)", label.c_str(), key.Time, KeyTypeLabel(key.Type));
						if (ImGui::IsItemActivated()) {
							m_SelectedKeyBone = bone;
							m_SelectedKeyTime = key.Time;
							m_Time = key.Time;
							m_PoseEdited = false;
							clickedKey = true;
							if (const Skeleton* skel = GetSkeleton()) {
								auto it = skel->NodeIndexByName.find(bone);
								if (it != skel->NodeIndexByName.end()) m_SelectedNode = it->second;
							}
						}
						// Drag to retime
						if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left, 2.0f)) {
							float newTime = SnapTime(std::clamp(xToTime(ImGui::GetIO().MousePos.x), 0.0f, duration));
							if (std::abs(newTime - key.Time) > 1e-4f) {
								key.Time = newTime;
								m_SelectedKeyTime = newTime;
								m_Time = newTime;
								std::sort(track->Keys.begin(), track->Keys.end(),
									[](const AnimationKeyframe& a, const AnimationKeyframe& b) { return a.Time < b.Time; });
								m_Asset->MarkEdited();
								ImGui::PopID();
								break;   // vector reordered
							}
						}
						if (ImGui::BeginPopupContextItem("##KeyCtx")) {
							for (auto t : { AnimationKeyType::Rotation, AnimationKeyType::Position, AnimationKeyType::Both }) {
								if (ImGui::MenuItem(KeyTypeLabel(t), nullptr, key.Type == t)) { key.Type = t; m_Asset->MarkEdited(); }
							}
							ImGui::Separator();
							if (ImGui::MenuItem("Delete", "Del")) {
								m_SelectedKeyBone = bone; m_SelectedKeyTime = key.Time;
								ImGui::EndPopup(); ImGui::PopID();
								DeleteSelectedKey();
								break;
							}
							ImGui::EndPopup();
						}
						ImGui::PopID();
					}
				}
			}
			ImGui::PopID();
			y += rowHeight;
		}

		if (rows.empty()) {
			dl->AddText(ImVec2(x0 + 8.0f, y + 6.0f), tickCol,
				"Select a bone, pose it with the viewport gizmo (E rotate, W move) and press a Key button.");
		}

		// ---- Playhead ----
		float px = timeToX(m_Time);
		if (px >= x0 && px <= x0 + width) {
			dl->AddLine(ImVec2(px, origin.y), ImVec2(px, origin.y + avail.y), IM_COL32(217, 130, 43, 255), 2.0f);
			dl->AddTriangleFilled(ImVec2(px - 6, origin.y), ImVec2(px + 6, origin.y), ImVec2(px, origin.y + 8), IM_COL32(217, 130, 43, 255));
		}

		// Submit an item so the child's content size is valid (ImGui asserts on bare SetCursorPos)
		ImGui::SetCursorScreenPos(origin);
		ImGui::Dummy(ImVec2(1.0f, std::max(y - origin.y, avail.y - 1.0f)));
		(void)clickedKey;
	}

} // namespace Lunex
