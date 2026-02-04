/**
 * @file UIDragDrop.h
 * @brief Lunex UI Framework - Drag & Drop System
 * 
 * Provides a clean API for drag and drop operations
 * commonly used in content browsers, hierarchy panels, etc.
 */

#pragma once

#include "UICore.h"
#include <variant>

namespace Lunex::UI {

	// ============================================================================
	// DRAG & DROP PAYLOAD STRUCTURES
	// ============================================================================
	
	/**
	 * @brief Payload for content browser items
	 */
	struct ContentPayload {
		char FilePath[512] = {};
		char RelativePath[256] = {};
		char Extension[32] = {};
		bool IsDirectory = false;
		uint32_t ItemCount = 1;
		
		std::string GetFilePath() const { return FilePath; }
		std::string GetRelativePath() const { return RelativePath; }
		std::string GetExtension() const { return Extension; }
	};
	
	/**
	 * @brief Payload for entity drag operations
	 */
	struct EntityPayload {
		uint64_t EntityID = 0;
		// Entity pointer is stored separately since we can't serialize it
	};
	
	// ============================================================================
	// DRAG SOURCE
	// ============================================================================
	
	/**
	 * @brief Begin drag source (call after the draggable item)
	 * @return true if dragging
	 */
	bool BeginDragSource(ImGuiDragDropFlags flags = 0);
	void EndDragSource();
	
	/**
	 * @brief Set drag payload data
	 */
	void SetDragPayload(const char* type, const void* data, size_t size);
	
	/**
	 * @brief Convenience: Set content browser payload
	 */
	void SetContentPayload(const ContentPayload& payload);
	
	/**
	 * @brief Convenience: Set entity payload
	 */
	void SetEntityPayload(uint64_t entityID);
	
	// ============================================================================
	// DROP TARGET
	// ============================================================================
	
	/**
	 * @brief Begin drop target (call after the target item)
	 * @return true if valid drop target
	 */
	bool BeginDropTarget();
	void EndDropTarget();
	
	/**
	 * @brief Accept and retrieve dropped payload
	 * @return Pointer to payload data or nullptr if not accepted
	 */
	const void* AcceptDragPayload(const char* type, ImGuiDragDropFlags flags = 0);
	
	/**
	 * @brief Convenience: Accept content browser payload
	 */
	std::optional<ContentPayload> AcceptContentPayload(ImGuiDragDropFlags flags = 0);
	
	/**
	 * @brief Convenience: Accept multiple content items
	 */
	std::vector<std::string> AcceptMultipleContentPayload(ImGuiDragDropFlags flags = 0);
	
	/**
	 * @brief Convenience: Accept entity payload
	 */
	std::optional<uint64_t> AcceptEntityPayload(ImGuiDragDropFlags flags = 0);
	
	// ============================================================================
	// DROP TARGET VISUAL FEEDBACK
	// ============================================================================
	
	/**
	 * @brief Draw drop target highlight (call within BeginDropTarget)
	 */
	void DrawDropTargetHighlight(const Color& color = Colors::Primary());
	
	/**
	 * @brief Check if currently dragging
	 */
	bool IsDragging();
	
	/**
	 * @brief Check if dragging specific payload type
	 */
	bool IsDraggingPayloadType(const char* type);
	
	// ============================================================================
	// HIGH-LEVEL DROP ZONE
	// ============================================================================
	
	/**
	 * @brief Result of a drop zone operation
	 */
	struct DropZoneResult {
		bool wasDropped = false;
		std::string droppedPath;
		std::string droppedExtension;
		bool isDirectory = false;
	};
	
	/**
	 * @brief Create a visual drop zone with placeholder text
	 * @param id Unique ID
	 * @param size Size of the drop zone
	 * @param placeholderText Text to show when nothing is dropped
	 * @param acceptedExtensions List of accepted file extensions (nullptr for all)
	 * @return Drop result
	 */
	DropZoneResult DropZone(const std::string& id,
							const Size& size,
							const std::string& placeholderText,
							const std::vector<std::string>* acceptedExtensions = nullptr);
	
	/**
	 * @brief Drop zone specifically for textures
	 */
	DropZoneResult TextureDropZone(const std::string& id,
								   const Size& size = Size(-1, 60));
	
	/**
	 * @brief Drop zone specifically for materials
	 */
	DropZoneResult MaterialDropZone(const std::string& id,
									const Size& size = Size(-1, 60));
	
	/**
	 * @brief Drop zone specifically for meshes
	 */
	DropZoneResult MeshDropZone(const std::string& id,
								const Size& size = Size(-1, 60));
	
	/**
	 * @brief Drop zone specifically for scripts
	 */
	DropZoneResult ScriptDropZone(const std::string& id,
								  const Size& size = Size(-1, 60));

} // namespace Lunex::UI
