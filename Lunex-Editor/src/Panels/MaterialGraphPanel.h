#pragma once

/**
 * @file MaterialGraphPanel.h
 * @brief Standalone material node editor window (Unreal/Blender style).
 *
 * Opened from the Material Editor ("Node Editor"), from the Content Browser for graph
 * materials, or from the inspector. Layout:
 *   [toolbar: Save | Frame | compile status | node previews]
 *   [node details | node canvas]
 *   [shortcut hints]
 */

#include "Core/Core.h"
#include "Assets/Materials/MaterialAsset.h"
#include "Assets/Materials/MaterialGraphPreview.h"
#include "../UI/Graph/NodeGraphEditor.h"

#include <filesystem>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>

namespace Lunex {

	class MaterialGraphPanel {
	public:
		MaterialGraphPanel();

		// Opens the material's graph (creating it from the current properties if needed)
		void Open(Ref<MaterialAsset> material);
		void Close();
		bool IsOpen() const { return m_IsOpen && m_Material; }
		Ref<MaterialAsset> GetMaterial() const { return m_Material; }

		void OnUpdate(float deltaTime);
		void OnImGuiRender();

		using SavedCallback = std::function<void(const std::filesystem::path&)>;
		void SetOnSavedCallback(SavedCallback cb) { m_OnSaved = std::move(cb); }
		using OpenPropertiesCallback = std::function<void(Ref<MaterialAsset>)>;
		void SetOnOpenPropertiesCallback(OpenPropertiesCallback cb) { m_OnOpenProperties = std::move(cb); }

	private:
		void DrawToolbar();
		void DrawSidebar();
		void DrawDetails(Graph::NodeGraph& graph);
		void OnGraphChanged();
		void Save();

		// Node bodies
		bool DrawNodeBody(Graph::Node& node);
		bool DrawImageBody(Graph::Node& node);
		bool DrawRampBody(Graph::Node& node);
		bool DrawCurveBody(Graph::Node& node);
		bool DrawNormalBody(Graph::Node& node);

		// Node Wrangler hooks
		bool PreviewNode(Graph::NodeGraph& graph, Graph::ID node, int outputIndex);
		bool TextureSetup(Graph::NodeGraph& graph, Graph::ID node);
		bool PrincipledTextureSetup(Graph::NodeGraph& graph, Graph::ID node);
		Ref<Texture2D> GetThumbnail(const std::string& path);

		Ref<MaterialAsset> m_Material;
		std::unique_ptr<UI::NodeGraphEditor> m_Editor;
		std::unique_ptr<MaterialGraph::NodePreviewRenderer> m_NodePreviews;
		bool m_ShowNodePreviews = true;
		std::unordered_map<std::string, Ref<Texture2D>> m_Thumbnails;
		std::vector<std::string> m_Messages;
		SavedCallback m_OnSaved;
		OpenPropertiesCallback m_OnOpenProperties;

		bool m_IsOpen = false;
		bool m_Dirty = false;
		int m_Focus = 0;
		float m_SidebarRatio = 0.22f;
	};

} // namespace Lunex
