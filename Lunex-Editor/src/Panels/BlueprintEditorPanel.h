#pragma once

/**
 * @file BlueprintEditorPanel.h
 * @brief Visual scripting editor for .lublueprint assets (same canvas as the material nodes).
 *
 * Opened with "Edit Blueprint" on the Script component, or by double-clicking a
 * .lublueprint in the Content Browser.
 */

#include "Core/Core.h"
#include "Scripting/Blueprint/Blueprint.h"
#include "../UI/Graph/NodeGraphEditor.h"

#include <filesystem>
#include <memory>
#include <string>

namespace Lunex {

	class BlueprintEditorPanel {
	public:
		BlueprintEditorPanel();
		~BlueprintEditorPanel();

		void Open(const std::filesystem::path& path);
		void OnImGuiRender();

		// Global access for inspector buttons / content browser
		static BlueprintEditorPanel* Get() { return s_Instance; }
		static std::filesystem::path CreateForEntity(const std::string& entityName);

	private:
		void Save();
		void DrawSidebar(Graph::NodeGraph& graph);

		Ref<Blueprint::BlueprintAsset> m_Asset;
		std::unique_ptr<UI::NodeGraphEditor> m_Editor;
		bool m_Open = false;
		bool m_Dirty = false;
		int m_Focus = 0;
		float m_SidebarRatio = 0.2f;

		static inline BlueprintEditorPanel* s_Instance = nullptr;
	};

} // namespace Lunex
