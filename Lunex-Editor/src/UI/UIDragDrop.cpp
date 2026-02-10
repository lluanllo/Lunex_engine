/**
 * @file UIDragDrop.cpp
 * @brief Implementation of Drag & Drop System
 */

#include "stpch.h"
#include "UIDragDrop.h"

namespace Lunex::UI {

	// ============================================================================
	// DRAG SOURCE
	// ============================================================================
	
	bool BeginDragSource(ImGuiDragDropFlags flags) {
		return ImGui::BeginDragDropSource(flags);
	}
	
	void EndDragSource() {
		ImGui::EndDragDropSource();
	}
	
	void SetDragPayload(const char* type, const void* data, size_t size) {
		ImGui::SetDragDropPayload(type, data, size);
	}
	
	void SetContentPayload(const ContentPayload& payload) {
		ImGui::SetDragDropPayload(PAYLOAD_CONTENT_BROWSER_ITEM, &payload, sizeof(ContentPayload));
	}
	
	void SetEntityPayload(uint64_t entityID) {
		EntityPayload payload;
		payload.EntityID = entityID;
		ImGui::SetDragDropPayload(PAYLOAD_ENTITY_NODE, &payload, sizeof(EntityPayload));
	}
	
	// ============================================================================
	// DROP TARGET
	// ============================================================================
	
	bool BeginDropTarget() {
		return ImGui::BeginDragDropTarget();
	}
	
	void EndDropTarget() {
		ImGui::EndDragDropTarget();
	}
	
	const void* AcceptDragPayload(const char* type, ImGuiDragDropFlags flags) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(type, flags)) {
			return payload->Data;
		}
		return nullptr;
	}
	
	std::optional<ContentPayload> AcceptContentPayload(ImGuiDragDropFlags flags) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(PAYLOAD_CONTENT_BROWSER_ITEM, flags)) {
			return *static_cast<const ContentPayload*>(payload->Data);
		}
		return std::nullopt;
	}
	
	std::vector<std::string> AcceptMultipleContentPayload(ImGuiDragDropFlags flags) {
		std::vector<std::string> paths;
		
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(PAYLOAD_CONTENT_BROWSER_ITEMS, flags)) {
			std::string payloadData = static_cast<const char*>(payload->Data);
			std::stringstream ss(payloadData);
			std::string line;
			
			while (std::getline(ss, line)) {
				if (!line.empty()) {
					paths.push_back(line);
				}
			}
		}
		
		return paths;
	}
	
	std::optional<uint64_t> AcceptEntityPayload(ImGuiDragDropFlags flags) {
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(PAYLOAD_ENTITY_NODE, flags)) {
			const EntityPayload* entityPayload = static_cast<const EntityPayload*>(payload->Data);
			return entityPayload->EntityID;
		}
		return std::nullopt;
	}
	
	// ============================================================================
	// DROP TARGET VISUAL FEEDBACK
	// ============================================================================
	
	void DrawDropTargetHighlight(const Color& color) {
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 min = ImGui::GetItemRectMin();
		ImVec2 max = ImGui::GetItemRectMax();
		
		// Fill with semi-transparent color
		Color fillColor = color;
		fillColor.a = 0.2f;
		drawList->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(ToImVec4(fillColor)));
		
		// Border
		drawList->AddRect(min, max, ImGui::ColorConvertFloat4ToU32(ToImVec4(color)), 0.0f, 0, 2.0f);
	}
	
	bool IsDragging() {
		return ImGui::IsDragDropActive();
	}
	
	bool IsDraggingPayloadType(const char* type) {
		if (!IsDragging()) return false;
		
		const ImGuiPayload* payload = ImGui::GetDragDropPayload();
		if (payload && payload->IsDataType(type)) {
			return true;
		}
		return false;
	}
	
	// ============================================================================
	// HIGH-LEVEL DROP ZONE
	// ============================================================================
	
	DropZoneResult DropZone(const std::string& id, const Size& size, const std::string& placeholderText, const std::vector<std::string>* acceptedExtensions) {
		DropZoneResult result;
		
		ScopedID scopedID(id);
		
		// Calculate actual size
		ImVec2 actualSize = ToImVec2(size);
		if (actualSize.x <= 0) actualSize.x = ImGui::GetContentRegionAvail().x;
		if (actualSize.y <= 0) actualSize.y = 60.0f;
		
		// Style
		bool isDropTarget = IsDraggingPayloadType(PAYLOAD_CONTENT_BROWSER_ITEM);
		
		Color bgColor = isDropTarget ? Color(Colors::Primary().r, Colors::Primary().g, Colors::Primary().b, 0.1f) : Colors::BgMedium();
		Color borderColor = isDropTarget ? Colors::Primary() : Colors::BorderLight();
		
		ScopedColor colors({
			{ImGuiCol_Button, bgColor},
			{ImGuiCol_ButtonHovered, Colors::BgHover()},
			{ImGuiCol_Border, borderColor}
		});
		ScopedStyle borderStyle(ImGuiStyleVar_FrameBorderSize, 1.5f);
		
		// Draw button as drop zone
		ImGui::Button(placeholderText.c_str(), actualSize);
		
		// Handle drop
		if (ImGui::BeginDragDropTarget()) {
			if (isDropTarget) {
				DrawDropTargetHighlight(Colors::Primary());
			}
			
			if (auto contentPayload = AcceptContentPayload()) {
				std::string ext = contentPayload->GetExtension();
				
				bool accepted = true;
				if (acceptedExtensions && !acceptedExtensions->empty()) {
					accepted = false;
					for (const auto& acceptedExt : *acceptedExtensions) {
						if (ext == acceptedExt) {
							accepted = true;
							break;
						}
					}
				}
				
				if (accepted) {
					result.wasDropped = true;
					result.droppedPath = contentPayload->GetFilePath();
					result.droppedExtension = ext;
					result.isDirectory = contentPayload->IsDirectory;
				}
			}
			
			ImGui::EndDragDropTarget();
		}
		
		return result;
	}
	
	DropZoneResult TextureDropZone(const std::string& id, const Size& size) {
		static std::vector<std::string> textureExtensions = {
			".png", ".jpg", ".jpeg", ".bmp", ".tga", ".hdr"
		};
		
		return DropZone(id, size, "?? Drop Texture Here\n(.png, .jpg, .bmp, .tga, .hdr)", &textureExtensions);
	}
	
	DropZoneResult MaterialDropZone(const std::string& id, const Size& size) {
		static std::vector<std::string> materialExtensions = { ".lumat" };
		
		return DropZone(id, size, "?? Drop Material Here\n(.lumat)", &materialExtensions);
	}
	
	DropZoneResult MeshDropZone(const std::string& id, const Size& size) {
		static std::vector<std::string> meshExtensions = {
			".lumesh", ".obj", ".fbx", ".gltf", ".glb", ".dae"
		};
		
		return DropZone(id, size, "?? Drop Mesh Here\n(.lumesh, .obj, .fbx, .gltf)", &meshExtensions);
	}
	
	DropZoneResult ScriptDropZone(const std::string& id, const Size& size) {
		static std::vector<std::string> scriptExtensions = { ".cpp", ".h" };
		
		return DropZone(id, size, "?? Drop Script Here\n(.cpp)", &scriptExtensions);
	}

} // namespace Lunex::UI
