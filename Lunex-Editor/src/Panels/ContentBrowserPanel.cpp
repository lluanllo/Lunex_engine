#include "stpch.h"
#include "ContentBrowserPanel.h"
#include "Events/Event.h"
#include "Events/FileDropEvent.h"
#include "Assets/Materials/MaterialRegistry.h"
#include "Assets/Mesh/MeshAsset.h"
#include "Asset/Prefab.h"
#include "Assets/Core/AssetRegistry.h"
#include <imgui.h>
#include <imgui_internal.h>
#include <sstream>

namespace Lunex {
	const std::filesystem::path g_AssetPath = "assets";
	
	ContentBrowserPanel::ContentBrowserPanel()
		: m_BaseDirectory(g_AssetPath), m_CurrentDirectory(g_AssetPath)
	{
		// Iconos específicos para Content Browser
		m_DirectoryIcon = Texture2D::Create("Resources/Icons/ContentBrowser/FolderIcon.png");
		m_FileIcon = Texture2D::Create("Resources/Icons/ContentBrowser/FileIcon.png");
		m_BackIcon = Texture2D::Create("Resources/Icons/ContentBrowser/BackIcon.png");
		m_ForwardIcon = Texture2D::Create("Resources/Icons/ContentBrowser/ForwardIcon.png");
		
		// Iconos específicos por tipo de archivo
		m_SceneIcon = Texture2D::Create("Resources/Icons/ContentBrowser/SceneIcon.png");
		m_TextureIcon = Texture2D::Create("Resources/Icons/ContentBrowser/ImageIcon.png");
		m_ShaderIcon = Texture2D::Create("Resources/Icons/ContentBrowser/ShaderIcon.png");
		m_AudioIcon = Texture2D::Create("Resources/Icons/ContentBrowser/AudioIcon.png");
		m_ScriptIcon = Texture2D::Create("Resources/Icons/ContentBrowser/ScriptIcon.png");
		m_MaterialIcon = Texture2D::Create("Resources/Icons/ContentBrowser/MaterialIcon.png");
		m_MeshIcon = Texture2D::Create("Resources/Icons/ContentBrowser/MeshIcon.png");
		m_PrefabIcon = Texture2D::Create("Resources/Icons/ContentBrowser/PrefabIcon.png");
		
		// Inicializar historial
		m_DirectoryHistory.push_back(m_CurrentDirectory);
		m_HistoryIndex = 0;
		
		// ✅ Initialize material preview renderer for thumbnails
		// Materials: 160x160 resolution with blue background #6EC1FF
		m_MaterialPreviewRenderer = CreateScope<MaterialPreviewRenderer>();
		m_MaterialPreviewRenderer->SetResolution(160, 160);
		m_MaterialPreviewRenderer->SetAutoRotate(false);
		m_MaterialPreviewRenderer->SetBackgroundColor(glm::vec4(0.432f, 0.757f, 1.0f, 1.0f)); // #6EC1FF
		
		// ✅ FIXED: Lower camera slightly to center material sphere better
		m_MaterialPreviewRenderer->SetCameraPosition(glm::vec3(0.0f, -0.3f, 2.5f));
	}

	void ContentBrowserPanel::SetRootDirectory(const std::filesystem::path& directory) {
		m_BaseDirectory = directory;
		m_CurrentDirectory = directory;
		
		// Reset history
		m_DirectoryHistory.clear();
		m_DirectoryHistory.push_back(m_CurrentDirectory);
		m_HistoryIndex = 0;
		
		// Clear all thumbnail caches
		m_TextureCache.clear();
		m_MaterialThumbnailCache.clear();
		m_MeshThumbnailCache.clear();
		
		// Clear selection
		ClearSelection();
	}
	
	void ContentBrowserPanel::OnImGuiRender() {
		// ===== ESTILO PROFESIONAL OSCURO (como Unreal Engine) =====
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.12f, 0.12f, 0.13f, 1.0f));        // Fondo principal
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.12f, 0.12f, 0.13f, 1.0f));         // Child backgrounds
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.08f, 0.08f, 0.09f, 1.0f));          // Bordes más sutiles
		ImGui::PushStyleColor(ImGuiCol_BorderShadow, ImVec4(0.0f, 0.0f, 0.0f, 0.6f));       // Sombras
		
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("Content Browser", NULL, ImGuiWindowFlags_MenuBar);
		ImGui::PopStyleVar();
		
		RenderTopBar();
		
		// Layout horizontal con splitter ajustable
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		
		// Splitter ajustable
		static float sidebarWidth = 200.0f;
		const float minSidebarWidth = 150.0f;
		const float maxSidebarWidth = 400.0f;
		
		ImVec2 availSize = ImGui::GetContentRegionAvail();
		
		// Sidebar con fondo más oscuro
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.11f, 1.0f));
		ImGui::BeginChild("Sidebar", ImVec2(sidebarWidth, availSize.y), false);
		RenderSidebar();
		ImGui::EndChild();
		ImGui::PopStyleColor();
		
		// Dibujar sombra del sidebar (gradiente sutil)
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 sidebarMax = ImGui::GetItemRectMax();
		ImVec2 shadowStart = ImVec2(sidebarMax.x, sidebarMax.y - availSize.y);
		ImVec2 shadowEnd = sidebarMax;
		
		// Sombra gradiente de 6px (más sutil)
		for (int i = 0; i < 6; i++) {
			float alpha = (1.0f - (i / 6.0f)) * 0.25f;
			ImU32 shadowColor = IM_COL32(0, 0, 0, (int)(alpha * 255));
			drawList->AddRectFilled(
				ImVec2(shadowStart.x + i, shadowStart.y),
				ImVec2(shadowStart.x + i + 1, shadowEnd.y),
				shadowColor
			);
		}
		
		ImGui::SameLine();
		
		// Splitter invisible
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.3f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.59f, 0.98f, 0.5f));
		ImGui::Button("##Splitter", ImVec2(4.0f, availSize.y));
		ImGui::PopStyleColor(3);
		
		if (ImGui::IsItemActive()) {
			sidebarWidth += ImGui::GetIO().MouseDelta.x;
			if (sidebarWidth < minSidebarWidth) sidebarWidth = minSidebarWidth;
			if (sidebarWidth > maxSidebarWidth) sidebarWidth = maxSidebarWidth;
		}
		
		if (ImGui::IsItemHovered()) {
			ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
		}
		
		ImGui::SameLine();
		
		// Grid de archivos con fondo ligeramente más claro
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.14f, 0.14f, 0.15f, 1.0f));
		ImGui::BeginChild("FileGrid", ImVec2(0, availSize.y), false);
		RenderFileGrid();
		ImGui::EndChild();
		ImGui::PopStyleColor();
		
		ImGui::PopStyleVar();
		
		ImGui::End();
		
		ImGui::PopStyleColor(4);
	}
	
	void ContentBrowserPanel::RenderTopBar() {
		// Barra superior con colores profesionales
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.10f, 0.10f, 0.11f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.16f, 0.16f, 0.17f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.26f, 0.59f, 0.98f, 0.4f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.26f, 0.59f, 0.98f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.16f, 0.16f, 0.17f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.20f, 0.20f, 0.21f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.08f, 0.08f, 0.09f, 1.0f));
		
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(6, 6));
		ImGui::BeginChild("TopBar", ImVec2(0, 40), true, ImGuiWindowFlags_NoScrollbar);
		
		// Botones de navegación con iconos
		bool canGoBack = m_HistoryIndex > 0;
		if (!canGoBack) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.3f);
		
		if (m_BackIcon) {
			if (ImGui::ImageButton("##BackButton", (ImTextureID)(uintptr_t)m_BackIcon->GetRendererID(),
				ImVec2(22, 22), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0)) && canGoBack) {
				m_HistoryIndex--;
				m_CurrentDirectory = m_DirectoryHistory[m_HistoryIndex];
			}
		} else {
			if (ImGui::Button("<", ImVec2(30, 30)) && canGoBack) {
				m_HistoryIndex--;
				m_CurrentDirectory = m_DirectoryHistory[m_HistoryIndex];
			}
		}
		
		if (!canGoBack) ImGui::PopStyleVar();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Back");
		
		ImGui::SameLine();
		
		bool canGoForward = m_HistoryIndex < (int)m_DirectoryHistory.size() - 1;
		if (!canGoForward) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.3f);
		
		if (m_ForwardIcon) {
			if (ImGui::ImageButton("##ForwardButton", (ImTextureID)(uintptr_t)m_ForwardIcon->GetRendererID(),
				ImVec2(22, 22), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0)) && canGoForward) {
				m_HistoryIndex++;
				m_CurrentDirectory = m_DirectoryHistory[m_HistoryIndex];
			}
		} else {
			if (ImGui::Button(">", ImVec2(30, 30)) && canGoForward) {
				m_HistoryIndex++;
				m_CurrentDirectory = m_DirectoryHistory[m_HistoryIndex];
			}
		}
		
		if (!canGoForward) ImGui::PopStyleVar();
		if (ImGui::IsItemHovered()) ImGui::SetTooltip("Forward");
		
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(16, 0));
		ImGui::SameLine();
		
		// Path display con estilo mejorado
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.80f, 0.80f, 0.82f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.15f, 1.0f));
		ImGui::AlignTextToFramePadding();
		
		static char pathBuffer[512];
		std::string currentPath = m_CurrentDirectory.string();
		strncpy_s(pathBuffer, sizeof(pathBuffer), currentPath.c_str(), _TRUNCATE);
		
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 220);
		ImGui::InputText("##PathDisplay", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_ReadOnly);
		
		ImGui::PopStyleColor(2);
		
		// Search bar con icono
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.18f, 0.18f, 0.19f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.22f, 0.22f, 0.23f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgActive, ImVec4(0.26f, 0.59f, 0.98f, 0.3f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.87f, 1.0f));
		ImGui::SetNextItemWidth(200);
		ImGui::InputTextWithHint("##Search", "?? Search...", m_SearchBuffer, 256);
		ImGui::PopStyleColor(4);
		
		ImGui::EndChild();
		
		// Dibujar sombra debajo de la topbar
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 topbarMax = ImGui::GetItemRectMax();
		ImVec2 shadowStart = ImVec2(topbarMax.x - ImGui::GetContentRegionAvail().x, topbarMax.y);
		
		for (int i = 0; i < 3; i++) {
			float alpha = (1.0f - (i / 3.0f)) * 0.35f;
			ImU32 shadowColor = IM_COL32(0, 0, 0, (int)(alpha * 255));
			drawList->AddRectFilled(
				ImVec2(shadowStart.x, shadowStart.y + i),
				ImVec2(topbarMax.x, shadowStart.y + i + 1),
				shadowColor
			);
		}
		
		ImGui::PopStyleVar();
		ImGui::PopStyleColor(7);
	}
	
	void ContentBrowserPanel::RenderSidebar() {
		ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, 12.0f);
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 2));
		
		ImGui::Dummy(ImVec2(0, 8));
		
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.60f, 0.60f, 0.60f, 1.0f));
		ImGui::Indent(8);
		ImGui::Text("FOLDERS");
		ImGui::Unindent(8);
		ImGui::PopStyleColor();
		
		ImGui::Dummy(ImVec2(0, 4));
		
		// Colores del árbol (como Unity)
		ImGui::PushStyleColor(ImGuiCol_Header, ImVec4(0.20f, 0.20f, 0.20f, 0.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderHovered, ImVec4(0.25f, 0.25f, 0.25f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_HeaderActive, ImVec4(0.18f, 0.40f, 0.65f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.90f, 0.90f, 0.90f, 1.0f));
		
		// Función recursiva para mostrar árbol de directorios
		std::function<void(const std::filesystem::path&)> displayDirectoryTree = 
			[&](const std::filesystem::path& path) {
			for (auto& entry : std::filesystem::directory_iterator(path)) {
				if (entry.is_directory()) {
					const auto& dirPath = entry.path();
					std::string dirName = dirPath.filename().string();
					
					ImGuiTreeNodeFlags flags = ImGuiTreeNodeFlags_OpenOnArrow | 
											   ImGuiTreeNodeFlags_SpanAvailWidth |
											   ImGuiTreeNodeFlags_FramePadding;
					
					if (dirPath == m_CurrentDirectory) {
					(flags |= ImGuiTreeNodeFlags_Selected);
					}
					
					bool hasSubdirs = false;
					for (auto& sub : std::filesystem::directory_iterator(dirPath)) {
						if (sub.is_directory()) {
							hasSubdirs = true;
							break;
						}
					}
					
					if (!hasSubdirs) {
					flags |= ImGuiTreeNodeFlags_Leaf | ImGuiTreeNodeFlags_NoTreePushOnOpen;
					}
					
					// Dibujar icono de carpeta antes del TreeNode
					ImGui::PushID(dirName.c_str());
					
					// Añadir espacio invisible al nombre para que el texto no se superponga con el icono
					std::string displayName = "   " + dirName; // 3 espacios para el icono
					
					// Guardar posición del cursor
					ImVec2 cursorPos = ImGui::GetCursorScreenPos();
					
					bool opened = ImGui::TreeNodeEx(displayName.c_str(), flags);
					
					// Dibujar el icono después del TreeNode para que aparezca encima
					if (m_DirectoryIcon) {
						ImDrawList* drawList = ImGui::GetWindowDrawList();
						float iconSize = 16.0f;
						float arrowWidth = 20.0f; // Espacio que ocupa la flecha del TreeNode
						ImVec2 iconPos = ImVec2(cursorPos.x + arrowWidth, cursorPos.y + 2.0f);
						
						drawList->AddImage(
							(ImTextureID)(uintptr_t)m_DirectoryIcon->GetRendererID(),
							iconPos,
							ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
							ImVec2(0, 0),
							ImVec2(1, 1)
						);
					}
					
					// Drag & drop target for sidebar folders
					if (ImGui::BeginDragDropTarget()) {
						// Visual highlight when hovering
						ImDrawList* dropDrawList = ImGui::GetWindowDrawList();
						ImVec2 itemMin = ImGui::GetItemRectMin();
						ImVec2 itemMax = ImGui::GetItemRectMax();
						
						// Draw highlight background
						ImU32 highlightColor = IM_COL32(90, 150, 255, 80);
						dropDrawList->AddRectFilled(itemMin, itemMax, highlightColor);
						
						// Draw border
						ImU32 borderColor = IM_COL32(90, 150, 255, 200);
						dropDrawList->AddRect(itemMin, itemMax, borderColor, 0.0f, 0, 2.0f);
						
						// Accept single item
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
							"CONTENT_BROWSER_ITEM", 
							ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
							
							if (payload->IsDelivery()) {
								ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
								std::filesystem::path sourcePath(data->FilePath);
								std::filesystem::path destPath = dirPath / sourcePath.filename();
								
								try {
									if (sourcePath != destPath && !std::filesystem::exists(destPath)) {
										std::filesystem::rename(sourcePath, destPath);
										LNX_LOG_INFO("Moved {0} to {1}", 
											sourcePath.filename().string(), dirPath.filename().string());
									}
								}
								catch (const std::exception& e) {
									LNX_LOG_ERROR("Failed to move {0}: {1}", 
										sourcePath.filename().string(), e.what());
								}
							}
						}
						
						// Accept multiple items
						if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
							"CONTENT_BROWSER_ITEMS", 
							ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
							
							if (payload->IsDelivery()) {
								std::string payloadData = (const char*)payload->Data;
								std::stringstream ss(payloadData);
								std::string line;
								
								while (std::getline(ss, line)) {
									if (line.empty()) continue;
									
									std::filesystem::path sourcePath = std::filesystem::path(g_AssetPath) / line;
									std::filesystem::path destPath = dirPath / sourcePath.filename();
									
									try {
										if (sourcePath != destPath && !std::filesystem::exists(destPath)) {
											std::filesystem::rename(sourcePath, destPath);
											LNX_LOG_INFO("Moved {0} to {1}", 
												sourcePath.filename().string(), dirPath.filename().string());
										}
									}
									catch (const std::exception& e) {
										LNX_LOG_ERROR("Failed to move {0}: {1}", 
											sourcePath.filename().string(), e.what());
									}
								}
								
								ClearSelection();
							}
						}
						
						ImGui::EndDragDropTarget();
					}
					
					if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
						if (m_CurrentDirectory != dirPath) {
							m_HistoryIndex++;
							if (m_HistoryIndex < (int)m_DirectoryHistory.size()) {
								m_DirectoryHistory.erase(
									m_DirectoryHistory.begin() + m_HistoryIndex,
									m_DirectoryHistory.end()
								);
							}
							m_DirectoryHistory.push_back(dirPath);
							m_CurrentDirectory = dirPath;
						}
					}
					
					if (opened && hasSubdirs) {
						displayDirectoryTree(dirPath);
						ImGui::TreePop();
					}
					
					ImGui::PopID();
				}
			}
		};
		
		// Raíz "Assets"
		ImGuiTreeNodeFlags rootFlags = ImGuiTreeNodeFlags_OpenOnArrow | 
									   ImGuiTreeNodeFlags_SpanAvailWidth | 
									   ImGuiTreeNodeFlags_DefaultOpen |
									   ImGuiTreeNodeFlags_FramePadding;
		
		if (m_CurrentDirectory == m_BaseDirectory) {
			rootFlags |= ImGuiTreeNodeFlags_Selected;
		}
		
		// Guardar posición del cursor para la raíz
		ImVec2 rootCursorPos = ImGui::GetCursorScreenPos();
		
		// Añadir espacio invisible al nombre de Assets
		bool rootOpened = ImGui::TreeNodeEx("   Assets", rootFlags);
		
		// Dibujar el icono de carpeta para Assets
		if (m_DirectoryIcon) {
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			float iconSize = 16.0f;
			float arrowWidth = 20.0f;
			ImVec2 iconPos = ImVec2(rootCursorPos.x + arrowWidth, rootCursorPos.y + 2.0f);
			
			drawList->AddImage(
				(ImTextureID)(uintptr_t)m_DirectoryIcon->GetRendererID(),
				iconPos,
				ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
				ImVec2(0, 0),
				ImVec2(1, 1)
			);
		}
		
		// Drag & drop target for Assets root
		if (ImGui::BeginDragDropTarget()) {
			// Visual highlight when hovering
			ImDrawList* dropDrawList = ImGui::GetWindowDrawList();
			ImVec2 itemMin = ImGui::GetItemRectMin();
			ImVec2 itemMax = ImGui::GetItemRectMax();
			
			// Draw highlight background
			ImU32 highlightColor = IM_COL32(90, 150, 255, 80);
			dropDrawList->AddRectFilled(itemMin, itemMax, highlightColor);
			
			// Draw border
			ImU32 borderColor = IM_COL32(90, 150, 255, 200);
			dropDrawList->AddRect(itemMin, itemMax, borderColor, 0.0f, 0, 2.0f);
			
			// Accept single item
			if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
				"CONTENT_BROWSER_ITEM", 
				ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
				
				if (payload->IsDelivery()) {
					ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
					std::filesystem::path sourcePath(data->FilePath);
					std::filesystem::path destPath = m_BaseDirectory / sourcePath.filename();
					
				try {
					if (sourcePath != destPath && !std::filesystem::exists(destPath)) {
						std::filesystem::rename(sourcePath, destPath);
						LNX_LOG_INFO("Moved {0} to Assets", sourcePath.filename().string());
					}
				}
				catch (const std::exception& e) {
					LNX_LOG_ERROR("Failed to move {0}: {1}", 
						sourcePath.filename().string(), e.what());
				}
			}
		}
		
		// Accept multiple items
		if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload(
			"CONTENT_BROWSER_ITEMS", 
			ImGuiDragDropFlags_AcceptBeforeDelivery | ImGuiDragDropFlags_AcceptNoDrawDefaultRect)) {
			
			if (payload->IsDelivery()) {
				std::string payloadData = (const char*)payload->Data;
				std::stringstream ss(payloadData);
				std::string line;
				
				while (std::getline(ss, line)) {
					if (line.empty()) continue;
					
					std::filesystem::path sourcePath = std::filesystem::path(g_AssetPath) / line;
					std::filesystem::path destPath = m_BaseDirectory / sourcePath.filename();
					
					try {
						if (sourcePath != destPath && !std::filesystem::exists(destPath)) {
							std::filesystem::rename(sourcePath, destPath);
							LNX_LOG_INFO("Moved {0} to Assets", sourcePath.filename().string());
						}
					}
					catch (const std::exception& e) {
						LNX_LOG_ERROR("Failed to move {0}: {1}", 
							sourcePath.filename().string(), e.what());
					}
				}
				
				ClearSelection();
			}
		}
		
		ImGui::EndDragDropTarget();
	}
	
	if (ImGui::IsItemClicked() && !ImGui::IsItemToggledOpen()) {
		if (m_CurrentDirectory != m_BaseDirectory) {
			m_HistoryIndex++;
			if (m_HistoryIndex < (int)m_DirectoryHistory.size()) {
				m_DirectoryHistory.erase(
					m_DirectoryHistory.begin() + m_HistoryIndex,
					m_DirectoryHistory.end()
				);
			}
			m_DirectoryHistory.push_back(m_BaseDirectory);
			m_CurrentDirectory = m_BaseDirectory;
		}
	}
	
	if (rootOpened) {
		displayDirectoryTree(m_BaseDirectory);
		ImGui::TreePop();
	}
	
	ImGui::PopStyleColor(4);
	ImGui::PopStyleVar(2);
}
	
	Ref<Texture2D> ContentBrowserPanel::GetIconForFile(const std::filesystem::path& path) {
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
		
		if (extension == ".lunex") return m_SceneIcon ? m_SceneIcon : m_FileIcon;
		if (extension == ".lumat") return m_MaterialIcon ? m_MaterialIcon : m_FileIcon;
		if (extension == ".lumesh") return m_MeshIcon ? m_MeshIcon : m_FileIcon;
		if (extension == ".luprefab") return m_PrefabIcon ? m_PrefabIcon : m_FileIcon;
		if (extension == ".png" || extension == ".jpg" || extension == ".jpeg") 
			return m_TextureIcon ? m_TextureIcon : m_FileIcon;
		if (extension == ".glsl" || extension == ".shader") 
			return m_ShaderIcon ? m_ShaderIcon : m_FileIcon;
		if (extension == ".wav" || extension == ".mp3" || extension == ".ogg") 
			return m_AudioIcon ? m_AudioIcon : m_FileIcon;
		if (extension == ".cpp" || extension == ".h" || extension == ".cs") 
			return m_ScriptIcon ? m_ScriptIcon : m_FileIcon;
		
		return m_FileIcon;
	}
	
	Ref<Texture2D> ContentBrowserPanel::GetThumbnailForFile(const std::filesystem::path& path) {
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
		
		// ✅ HDR/HDRI files - render ultra-wide thumbnail (2:1 aspect ratio)
		if (extension == ".hdr" || extension == ".hdri") {
			std::string pathStr = path.string();
			
			// Check cache first
			auto it = m_TextureCache.find(pathStr);
			if (it != m_TextureCache.end() && it->second) {
				return it->second;
			}
			
			// Load HDR texture directly
			try {
				// HDR files are equirectangular, so we load them as regular 2D textures
				// They will appear ultra-wide naturally (typically 2:1 aspect ratio)
				Ref<Texture2D> hdrTexture = Texture2D::Create(pathStr);
				if (hdrTexture && hdrTexture->IsLoaded()) {
					m_TextureCache[pathStr] = hdrTexture;
					return hdrTexture;
				}
			}
			catch (...) {
				LNX_LOG_WARN("Failed to load HDR thumbnail: {0}", path.filename().string());
			}
			
			// Fallback to texture icon
			return m_TextureIcon ? m_TextureIcon : m_FileIcon;
		}
		
		// ✅ Material files - render sphere preview (with disk caching)
		if (extension == ".lumat") {
			std::string pathStr = path.string();
			
			// Check cache first - now stores Ref<Texture2D>
			auto it = m_MaterialThumbnailCache.find(pathStr);
			if (it != m_MaterialThumbnailCache.end() && it->second) {
				return it->second;
			}
			
			// ✅ Use disk-cached thumbnails for faster loading
			try {
				auto material = MaterialRegistry::Get().LoadMaterial(path);
				if (material && m_MaterialPreviewRenderer) {
					// Use cached thumbnail system (loads from disk if available)
					Ref<Texture2D> thumbnail = m_MaterialPreviewRenderer->GetOrGenerateCachedThumbnail(path, material);
					if (thumbnail) {
						m_MaterialThumbnailCache[pathStr] = thumbnail;
						return thumbnail;
					}
				}
			}
			catch (...) {
				// If rendering fails, fall back to icon
			}
			
			return m_MaterialIcon ? m_MaterialIcon : m_FileIcon;
		}
		
		// ✅ Mesh Asset files (.lumesh) - render 3D preview with angled camera and new background
		if (extension == ".lumesh") {
			std::string pathStr = path.string();
			
			// Check cache first
			auto it = m_MeshThumbnailCache.find(pathStr);
			if (it != m_MeshThumbnailCache.end() && it->second) {
				return it->second;
			}
			
			// Load mesh and render preview
			try {
				auto meshAsset = MeshAsset::LoadFromFile(path);
				if (meshAsset && meshAsset->GetModel() && m_MaterialPreviewRenderer) {
					// Temporarily set the preview model to the mesh's model
					Ref<Model> originalModel = m_MaterialPreviewRenderer->GetPreviewModel();
					m_MaterialPreviewRenderer->SetPreviewModel(meshAsset->GetModel());
					
					// ✅ Save original state
					auto originalCameraPos = m_MaterialPreviewRenderer->GetCameraPosition();
					auto originalBgColor = m_MaterialPreviewRenderer->GetBackgroundColor();
					
					// ✅ FIXED: Set better camera angle and background color #7297C2
					m_MaterialPreviewRenderer->SetCameraPosition(glm::vec3(2.0f, 1.2f, 2.5f));
					m_MaterialPreviewRenderer->SetBackgroundColor(glm::vec4(0.447f, 0.592f, 0.761f, 1.0f)); // #7297C2
					
					// Create a default gray material for mesh preview
					auto defaultMaterial = CreateRef<MaterialAsset>("MeshPreview");
					defaultMaterial->SetAlbedo(glm::vec4(0.7f, 0.7f, 0.7f, 1.0f));
					defaultMaterial->SetMetallic(0.0f);
					defaultMaterial->SetRoughness(0.5f);
					
					// Render the mesh with default material
					Ref<Texture2D> thumbnail = m_MaterialPreviewRenderer->RenderToTexture(defaultMaterial);
					
					// ✅ Restore original state
					m_MaterialPreviewRenderer->SetCameraPosition(originalCameraPos);
					m_MaterialPreviewRenderer->SetBackgroundColor(originalBgColor);
					m_MaterialPreviewRenderer->SetPreviewModel(originalModel);
					
					if (thumbnail) {
						m_MeshThumbnailCache[pathStr] = thumbnail;
						return thumbnail;
					}
				}
			}
			catch (...) {
				// If rendering fails, fall back to icon
			}
			
			return m_MeshIcon ? m_MeshIcon : m_FileIcon;
		}
		
		// ✅ Prefab files (.luprefab) - render 3D preview with angled camera and new background
		if (extension == ".luprefab") {
			std::string pathStr = path.string();
			
			// Check cache first
			auto it = m_MeshThumbnailCache.find(pathStr);
			if (it != m_MeshThumbnailCache.end() && it->second) {
				return it->second;
			}
			
			// Load prefab and try to render preview from its mesh
			try {
				auto prefab = Prefab::LoadFromFile(path);
				if (prefab && m_MaterialPreviewRenderer) {
					// Try to find mesh asset path in prefab data
					for (const auto& entityData : prefab->GetEntityData()) {
						for (const auto& compData : entityData.Components) {
							if (compData.ComponentType == "MeshComponent") {
								// Parse mesh asset path from component data
								// Format: "type;color;meshAssetID;meshAssetPath;filePath"
								std::istringstream iss(compData.SerializedData);
								std::string token;
								
								std::getline(iss, token, ';'); // type
								std::getline(iss, token, ';'); // color
								std::getline(iss, token, ';'); // meshAssetID
								std::getline(iss, token, ';'); // meshAssetPath
								std::string meshAssetPath = token;
								
								if (!meshAssetPath.empty()) {
									// Resolve path relative to assets
									std::filesystem::path fullMeshPath = m_BaseDirectory / meshAssetPath;
									if (std::filesystem::exists(fullMeshPath)) {
										auto meshAsset = MeshAsset::LoadFromFile(fullMeshPath);
										if (meshAsset && meshAsset->GetModel()) {
											// Render mesh preview with angled camera
											Ref<Model> originalModel = m_MaterialPreviewRenderer->GetPreviewModel();
											m_MaterialPreviewRenderer->SetPreviewModel(meshAsset->GetModel());
											
											// ✅ Save original state
											auto originalCameraPos = m_MaterialPreviewRenderer->GetCameraPosition();
											auto originalBgColor = m_MaterialPreviewRenderer->GetBackgroundColor();
											
											// ✅ FIXED: Better camera angle and background color #7297C2
											m_MaterialPreviewRenderer->SetCameraPosition(glm::vec3(2.2f, 1.5f, 2.8f));
											m_MaterialPreviewRenderer->SetBackgroundColor(glm::vec4(0.447f, 0.592f, 0.761f, 1.0f)); // #7297C2
											
											auto defaultMaterial = CreateRef<MaterialAsset>("PrefabPreview");
											defaultMaterial->SetAlbedo(glm::vec4(0.6f, 0.65f, 0.7f, 1.0f));
											defaultMaterial->SetMetallic(0.0f);
											defaultMaterial->SetRoughness(0.4f);
											
											Ref<Texture2D> thumbnail = m_MaterialPreviewRenderer->RenderToTexture(defaultMaterial);
											
											// ✅ Restore original state
											m_MaterialPreviewRenderer->SetCameraPosition(originalCameraPos);
											m_MaterialPreviewRenderer->SetBackgroundColor(originalBgColor);
											m_MaterialPreviewRenderer->SetPreviewModel(originalModel);
											
											if (thumbnail) {
												m_MeshThumbnailCache[pathStr] = thumbnail;
												return thumbnail;
											}
										}
									}
								}
								break;
							}
						}
					}
				}
			}
			catch (...) {
				// If rendering fails, fall back to icon
			}
			
			return m_PrefabIcon ? m_PrefabIcon : m_FileIcon;
		}
		
		// Si es una imagen, intentar cargar el preview
		if (extension == ".png" || extension == ".jpg" || extension == ".jpeg" || extension == ".bmp" || extension == ".tga") {
			std::string pathStr = path.string();
			
			// Verificar si ya está en caché
			auto it = m_TextureCache.find(pathStr);
			if (it != m_TextureCache.end()) {
				return it->second;
			}
			
			// Intentar cargar la textura
			try {
				Ref<Texture2D> texture = Texture2D::Create(pathStr);
				if (texture) {
					m_TextureCache[pathStr] = texture;
					return texture;
				}
			}
			catch (...) {
				// Si falla, usar el icono por defecto
			}
		}
		
		// Para otros archivos, usar el icono correspondiente
		return GetIconForFile(path);
	}
	
	// ============================================================================
	// HELPER: Get asset type label for display (like "MATERIAL", "MESH", etc.)
	// ============================================================================
	std::string ContentBrowserPanel::GetAssetTypeLabel(const std::filesystem::path& path) {
		if (std::filesystem::is_directory(path)) {
			return "FOLDER";
		}
		
		std::string extension = path.extension().string();
		std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
		
		if (extension == ".lumat") return "MATERIAL";
		if (extension == ".lumesh") return "MESH";
		if (extension == ".luprefab") return "PREFAB";
		if (extension == ".lunex") return "SCENE";
		if (extension == ".hdr" || extension == ".hdri") return "HDRI";
		if (extension == ".png" || extension == ".jpg" || extension == ".jpeg") return "TEXTURE";
		if (extension == ".glsl" || extension == ".shader") return "SHADER";
		if (extension == ".wav" || extension == ".mp3" || extension == ".ogg") return "AUDIO";
		if (extension == ".cpp" || extension == ".h" || extension == ".cs") return "SCRIPT";
		
		return "FILE";
	}
	
	void ContentBrowserPanel::RenderFileGrid() {
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8, 8));
		ImGui::BeginChild("FileGridContent", ImVec2(0, -28), false);
		
		// Track if mouse is hovering over the file grid
		m_IsHovered = ImGui::IsWindowHovered(ImGuiHoveredFlags_RootAndChildWindows);
		m_HoveredFolder.clear();
		m_ItemBounds.clear();
		
		ImGui::Indent(16.0f);
		
		// Handle selection rectangle
		bool isCtrlDown = ImGui::GetIO().KeyCtrl;
		bool isShiftDown = ImGui::GetIO().KeyShift;
		
		// Start selection rectangle
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !isCtrlDown && !isShiftDown) {
			m_IsSelecting = true;
			m_SelectionStart = ImGui::GetMousePos();
			m_SelectionEnd = m_SelectionStart;
			ClearSelection();
		}
		
		// Update selection rectangle
		if (m_IsSelecting && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
			m_SelectionEnd = ImGui::GetMousePos();
		}
		
		// End selection rectangle
		if (m_IsSelecting && ImGui::IsMouseReleased(ImGuiMouseButton_Left)) {
			m_IsSelecting = false;
		}
		
		// Context menu on right click in empty space
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Right)) {
			ImGui::OpenPopup("FileGridContextMenu");
		}
		
		RenderContextMenu();
		
		float cellSize = m_ThumbnailSize + m_Padding * 2;
		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columnCount = (int)(panelWidth / cellSize);
		if (columnCount < 1) columnCount = 1;
		
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(m_Padding, m_Padding + 8));
		
		// ✅ NO usar ImGui::BeginTable - renderizar manualmente para control de layout
		// Esto permite que archivos HDR ocupen 2 espacios sin problemas
		
		// Filtrar por búsqueda
		std::string searchQuery = m_SearchBuffer;
		std::transform(searchQuery.begin(), searchQuery.end(), searchQuery.begin(), ::tolower);
		
		// Colores para efectos visuales del grid
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.22f, 0.22f, 0.6f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.40f, 0.65f, 0.8f));
		
		// ✅ Layout manual con control de posición para items de ancho variable
		float currentX = 0.0f;
		float currentY = 0.0f;
		float rowHeight = 0.0f;
		float startX = ImGui::GetCursorScreenPos().x;
		float startY = ImGui::GetCursorScreenPos().y;
		
		for (auto& directoryEntry : std::filesystem::directory_iterator(m_CurrentDirectory)) {
			const auto& path = directoryEntry.path();
			std::string filenameString = path.filename().string();
			
			// Aplicar filtro de búsqueda
			if (!searchQuery.empty()) {
				std::string filenameLower = filenameString;
				std::transform(filenameLower.begin(), filenameLower.end(), filenameLower.begin(), ::tolower);
				if (filenameLower.find(searchQuery) == std::string::npos) {
					continue;
				}
			}
			
			// ✅ Calcular ancho del item (HDR ocupa doble)
			std::string fileExt = path.extension().string();
			std::transform(fileExt.begin(), fileExt.end(), fileExt.begin(), ::tolower);
			bool isHDRFile = (fileExt == ".hdr" || fileExt == ".hdri");
			
			float itemWidth = isHDRFile ? (m_ThumbnailSize * 2.0f + m_Padding * 2) : cellSize;
			
			// ✅ Check si cabe en la fila actual, si no, nueva fila
			if (currentX > 0 && currentX + itemWidth > panelWidth) {
				currentX = 0.0f;
				currentY += rowHeight + m_Padding + 8;
				rowHeight = 0.0f;
			}
			
			// ✅ Posicionar cursor en la posición calculada
			ImGui::SetCursorScreenPos(ImVec2(startX + currentX, startY + currentY));
			
			ImGui::PushID(filenameString.c_str());
			ImGui::BeginGroup();
			
			Ref<Texture2D> icon = directoryEntry.is_directory() ? m_DirectoryIcon : GetThumbnailForFile(path);
			
			// ============================================================================
			// RENDERING SYSTEM
			// ============================================================================
			float cardWidth = m_ThumbnailSize;
			float cardHeight; // Will be set based on type (folder or file)
			ImVec2 cardMin, cardMax; // Bounds for interaction
			
			ImVec2 cursorPos = ImGui::GetCursorScreenPos();
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			
			float cardRounding = 6.0f;
			float cardPadding = 8.0f;
			
			if (directoryEntry.is_directory()) {
				// ========================================
				// FOLDERS: NO CARD - ONLY SHADOW
				// ========================================
				cardHeight = m_ThumbnailSize + 30.0f;
				cardMin = ImVec2(cursorPos.x, cursorPos.y);
				cardMax = ImVec2(cursorPos.x + cardWidth, cursorPos.y + cardHeight);
				
				float iconSize = m_ThumbnailSize;
				ImVec2 iconPos = ImVec2(cursorPos.x, cursorPos.y);
				
				if (icon) {
					drawList->AddImage(
						(ImTextureID)(uintptr_t)icon->GetRendererID(),
						iconPos,
						ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
						ImVec2(0, 1), ImVec2(1, 0)
					);
				}
				
				float textAreaY = iconPos.y + iconSize + 4.0f;
				std::string displayName = filenameString;
				
				const int maxChars = 15;
				if (displayName.length() > maxChars) {
					displayName = displayName.substr(0, maxChars - 2) + "..";
				}
				
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
				float nameWidth = ImGui::CalcTextSize(displayName.c_str()).x;
				float nameOffsetX = (cardWidth - nameWidth) * 0.5f;
				drawList->AddText(
					ImVec2(cursorPos.x + nameOffsetX, textAreaY),
					IM_COL32(245, 245, 245, 255),
					displayName.c_str()
				);
				ImGui::PopStyleColor();
			}
			else {
				// ========================================
				// FILES: SOFT GAUSSIAN SHADOW ON CARD
				// ========================================
				
				// ✅ Ultra-wide card for HDR files (2:1 aspect ratio)
				if (isHDRFile) {
					cardWidth = m_ThumbnailSize * 2.0f;
					cardHeight = m_ThumbnailSize + 50.0f;
				} else {
					cardHeight = m_ThumbnailSize + 50.0f;
				}
				
				cardMin = ImVec2(cursorPos.x, cursorPos.y);
				cardMax = ImVec2(cursorPos.x + cardWidth, cursorPos.y + cardHeight);
				
				// ===== CARD BACKGROUND =====
				ImU32 cardBgColor = IM_COL32(45, 45, 48, 255);
				
				// ✅ SOMBRA DEBAJO DE LA CARD
				ImVec2 shadowOffset = ImVec2(3.0f, 3.0f);
				ImVec2 shadowMin = ImVec2(cardMin.x + shadowOffset.x, cardMin.y + shadowOffset.y);
				ImVec2 shadowMax = ImVec2(cardMax.x + shadowOffset.x, cardMax.y + shadowOffset.y);
				drawList->AddRectFilled(shadowMin, shadowMax, IM_COL32(0, 0, 0, 80), cardRounding);
				
				drawList->AddRectFilled(cardMin, cardMax, cardBgColor, cardRounding);
				
				// ===== ICON/THUMBNAIL AREA =====
				float iconWidth = cardWidth - (cardPadding * 2);
				float iconHeight;
				
				if (isHDRFile) {
					// Ultra-wide: 2:1 aspect ratio
					iconHeight = iconWidth / 2.0f;
				} else {
					// Square thumbnails
					iconHeight = iconWidth;
				}
				
				ImVec2 iconMin = ImVec2(cursorPos.x + cardPadding, cursorPos.y + cardPadding);
				ImVec2 iconMax = ImVec2(iconMin.x + iconWidth, iconMin.y + iconHeight);
				
				ImU32 iconBgColor = IM_COL32(55, 55, 58, 255);
				drawList->AddRectFilled(iconMin, iconMax, iconBgColor, 4.0f);
				
				if (icon) {
					drawList->AddImageRounded(
						(ImTextureID)(uintptr_t)icon->GetRendererID(),
						iconMin,
						iconMax,
						ImVec2(0, 1), ImVec2(1, 0),
						IM_COL32(255, 255, 255, 255),
						4.0f
					);
				}
				
				// ===== TEXT AREA =====
				float textAreaY = iconMax.y + 4.0f;
				
				std::string displayName = filenameString;
				std::filesystem::path filePath(filenameString);
				std::string extension = filePath.extension().string();
				std::transform(extension.begin(), extension.end(), extension.begin(), ::tolower);
				
				// Hide extensions for engine assets
				if (extension == ".lumat" || extension == ".lumesh" || extension == ".luprefab") {
					displayName = filePath.stem().string();
				}
				
				// Truncate if too long
				const int maxChars = isHDRFile ? 30 : 15; // ✅ More chars for HDR files
				if (displayName.length() > maxChars) {
					displayName = displayName.substr(0, maxChars - 2) + "..";
				}
				
				// Draw name (centered, white)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.95f, 0.95f, 0.95f, 1.0f));
				float nameWidth = ImGui::CalcTextSize(displayName.c_str()).x;
				float nameOffsetX = (cardWidth - nameWidth) * 0.5f;
				drawList->AddText(
					ImVec2(cursorPos.x + nameOffsetX, textAreaY),
					IM_COL32(245, 245, 245, 255),
					displayName.c_str()
				);
				ImGui::PopStyleColor();
				
				// ✅ Asset type label SOLO para archivos (no carpetas)
				std::string typeLabel = GetAssetTypeLabel(path);
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.52f, 1.0f));
				float typeWidth = ImGui::CalcTextSize(typeLabel.c_str()).x;
				float typeOffsetX = (cardWidth - typeWidth) * 0.5f;
				drawList->AddText(
					ImVec2(cursorPos.x + typeOffsetX, textAreaY + 16.0f),
					IM_COL32(128, 128, 132, 255),
					typeLabel.c_str()
				);
				ImGui::PopStyleColor();
			}

			// ===== INVISIBLE BUTTON FOR INTERACTION =====
			ImGui::SetCursorScreenPos(cardMin);
			ImGui::InvisibleButton(filenameString.c_str(), ImVec2(cardWidth, cardHeight));
			
			// Store item bounds for selection rectangle
			m_ItemBounds[path.string()] = ImRect(cardMin, cardMax);
			
			// Selection rectangle check
			if (m_IsSelecting) {
				ImRect selectionRect(
					ImVec2((std::min)(m_SelectionStart.x, m_SelectionEnd.x), 
						   (std::min)(m_SelectionStart.y, m_SelectionEnd.y)),
					ImVec2((std::max)(m_SelectionStart.x, m_SelectionEnd.x), 
						   (std::max)(m_SelectionStart.y, m_SelectionEnd.y))
				);
				
				if (selectionRect.Overlaps(m_ItemBounds[path.string()])) {
					AddToSelection(path);
				}
			}
			
			// Click handling
			if (ImGui::IsItemClicked(ImGuiMouseButton_Left)) {
				bool ctrlDown = ImGui::GetIO().KeyCtrl;
				bool shiftDown = ImGui::GetIO().KeyShift;
				
				if (ctrlDown) {
					ToggleSelection(path);
					m_LastSelectedItem = path;
				}
				else if (shiftDown && !m_LastSelectedItem.empty()) {
					SelectRange(m_LastSelectedItem, path);
					m_LastSelectedItem = path;
				}
				else {
					if (!IsSelected(path)) {
						ClearSelection();
						AddToSelection(path);
					}
					m_LastSelectedItem = path;
				}
				m_IsSelecting = false;
			}
			
			// Visual effects
			if (ImGui::IsItemHovered()) {
				ImU32 hoverColor = IM_COL32(60, 60, 65, 255);
				drawList->AddRect(cardMin, cardMax, hoverColor, cardRounding, 0, 2.0f);
			}
			
			if (IsSelected(path)) {
				ImU32 selectedColor = IM_COL32(66, 150, 250, 255);
				drawList->AddRect(cardMin, cardMax, selectedColor, cardRounding, 0, 2.5f);
				ImU32 selectedFill = IM_COL32(66, 150, 250, 40);
				drawList->AddRectFilled(cardMin, cardMax, selectedFill, cardRounding);
			}
			
			if (directoryEntry.is_directory() && ImGui::IsDragDropActive()) {
				if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenBlockedByActiveItem)) {
					ImU32 dropTargetColor = IM_COL32(90, 150, 255, 255);
					drawList->AddRect(cardMin, cardMax, dropTargetColor, cardRounding, 0, 3.0f);
				}
			}
			
			if (directoryEntry.is_directory() && ImGui::IsItemHovered()) {
				m_HoveredFolder = path;
			}
			
			// ============ DRAG & DROP SOURCE ============
			if (ImGui::BeginDragDropSource(ImGuiDragDropFlags_SourceAllowNullID)) {
				if (IsSelected(path) && m_SelectedItems.size() > 1) {
					std::string payloadData;
					for (const auto& selectedPath : m_SelectedItems) {
						std::filesystem::path sp(selectedPath);
						auto relPath = std::filesystem::relative(sp, g_AssetPath);
						payloadData += relPath.string() + "\n";
					}
					ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEMS", 
						payloadData.c_str(), payloadData.size() + 1);
					ImGui::Text("%d items", (int)m_SelectedItems.size());
				}
				else {
					ContentBrowserPayload payload = {};
					
					std::filesystem::path fullPath = path;
					auto relativePath = std::filesystem::relative(fullPath, g_AssetPath);
					
					strncpy_s(payload.FilePath, fullPath.string().c_str(), _TRUNCATE);
					strncpy_s(payload.RelativePath, relativePath.string().c_str(), _TRUNCATE);
					
					std::string dragExt = path.extension().string();
					std::transform(dragExt.begin(), dragExt.end(), dragExt.begin(), ::tolower);
					strncpy_s(payload.Extension, dragExt.c_str(), _TRUNCATE);
					
					payload.IsDirectory = directoryEntry.is_directory();
					payload.ItemCount = 1;
					
					ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", &payload, sizeof(ContentBrowserPayload));
					
					ImGui::Text("%s", filenameString.c_str());
				}
				
				ImGui::EndDragDropSource();
			}
			
			// ============ DRAG & DROP TARGET (folders only) ============
			if (directoryEntry.is_directory()) {
				if (ImGui::BeginDragDropTarget()) {
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEM")) {
						ContentBrowserPayload* data = (ContentBrowserPayload*)payload->Data;
						std::filesystem::path sourcePath(data->FilePath);
						std::filesystem::path destPath = path / sourcePath.filename();
						
						try {
							if (sourcePath != destPath && !std::filesystem::exists(destPath)) {
								std::filesystem::rename(sourcePath, destPath);
								LNX_LOG_INFO("Moved {0} to {1}", 
									sourcePath.filename().string(), path.filename().string());
							}
						}
						catch (const std::exception& e) {
							LNX_LOG_ERROR("Failed to move {0}: {1}", 
								sourcePath.filename().string(), e.what());
						}
					}
					
					if (const ImGuiPayload* payload = ImGui::AcceptDragDropPayload("CONTENT_BROWSER_ITEMS")) {
						std::string payloadData = (const char*)payload->Data;
						std::stringstream ss(payloadData);
						std::string line;
						
						while (std::getline(ss, line)) {
							if (line.empty()) continue;

							std::filesystem::path sourcePath = std::filesystem::path(g_AssetPath) / line;
							std::filesystem::path destPath = path / sourcePath.filename();
							
							try {
								if (sourcePath != destPath && !std::filesystem::exists(destPath)) {
									std::filesystem::rename(sourcePath, destPath);
									LNX_LOG_INFO("Moved {0} to {1}", 
										sourcePath.filename().string(), path.filename().string());
								}
							}
							catch (const std::exception& e) {
								LNX_LOG_ERROR("Failed to move {0}: {1}", 
									sourcePath.filename().string(), e.what());
							}
						}
						
						ClearSelection();
					}
					
					ImGui::EndDragDropTarget();
				}
			}
			
			// Right-click context menu
			if (ImGui::IsItemClicked(ImGuiMouseButton_Right)) {
				if (!IsSelected(path)) {
					ClearSelection();
					AddToSelection(path);
				}
				ImGui::OpenPopup(("ItemContextMenu##" + filenameString).c_str());
			}
			
			if (ImGui::BeginPopup(("ItemContextMenu##" + filenameString).c_str())) {
				if (m_SelectedItems.size() > 1) {
					ImGui::Text("%d items selected", (int)m_SelectedItems.size());
				}
				else {
					ImGui::Text("%s", filenameString.c_str());
				}
				ImGui::Separator();
				
				std::string menuExt = path.extension().string();
				std::transform(menuExt.begin(), menuExt.end(), menuExt.begin(), ::tolower);
				
				if (menuExt == ".lumesh" && m_SelectedItems.size() == 1) {
					if (ImGui::MenuItem("Create Prefab")) {
						CreatePrefabFromMesh(path);
					}
					ImGui::Separator();
				}
				
				if (m_SelectedItems.size() == 1) {
					if (ImGui::MenuItem("Rename")) {
						m_ItemToRename = path;
						strncpy_s(m_NewItemName, sizeof(m_NewItemName), 
							filenameString.c_str(), _TRUNCATE);
						m_ShowRenameDialog = true;
					}
				}
				
				if (ImGui::MenuItem("Delete")) {
					for (const auto& selectedPath : m_SelectedItems) {
						DeleteItem(std::filesystem::path(selectedPath));
					}
					ClearSelection();
				}
				
				if (m_SelectedItems.size() == 1) {
					ImGui::Separator();
					
					if (ImGui::MenuItem("Show in Explorer")) {
						std::string command = "explorer /select," + path.string();
						system(command.c_str());
					}
					
					if (directoryEntry.is_directory()) {
						if (ImGui::MenuItem("Open in File Explorer")) {
							std::string command = "explorer " + path.string();
							system(command.c_str());
						}
					}
				}
				
				ImGui::EndPopup();
			}
			
			// Double click to open folder or file
			if (ImGui::IsItemHovered() && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left)) {
				if (directoryEntry.is_directory()) {
					m_HistoryIndex++;
					if (m_HistoryIndex < (int)m_DirectoryHistory.size()) {
						m_DirectoryHistory.erase(
							m_DirectoryHistory.begin() + m_HistoryIndex,
							m_DirectoryHistory.end()
						);
					}
					m_DirectoryHistory.push_back(path);
					m_CurrentDirectory = path;
				}
				else {
					std::string dblExt = path.extension().string();
					std::transform(dblExt.begin(), dblExt.end(), dblExt.begin(), ::tolower);
					
					if (dblExt == ".lumat") {
						if (m_OnMaterialOpenCallback) {
							m_OnMaterialOpenCallback(path);
						} else {
							LNX_LOG_WARN("Material editor not connected - cannot open {0}", path.filename().string());
						}
					}
					else if (dblExt == ".cpp" || dblExt == ".h" || dblExt == ".hpp" || dblExt == ".c") {
						std::string command = "cmd /c start \"\" \"" + path.string() + "\"";
						system(command.c_str());
						LNX_LOG_INFO("Opening script in editor: {0}", path.filename().string());
					}
					else if (dblExt == ".lunex") {
						LNX_LOG_INFO("Double-clicked scene: {0}", path.filename().string());
					}
					else {
						std::string command = "cmd /c start \"\" \"" + path.string() + "\"";
						system(command.c_str());
					}
				}
			}
		}
		
		// Deselect when clicking empty space
		if (ImGui::IsWindowHovered() && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && 
			!ImGui::IsAnyItemHovered() && !ImGui::GetIO().KeyCtrl && !ImGui::GetIO().KeyShift && !m_IsSelecting) {
			ClearSelection();
		}
		
		ImGui::PopStyleColor(3);
		
		ImGui::PopStyleVar();
		ImGui::Unindent(16.0f);
		
		// Draw selection rectangle
		if (m_IsSelecting) {
			ImDrawList* drawList = ImGui::GetWindowDrawList();
			ImVec2 rectMin((std::min)(m_SelectionStart.x, m_SelectionEnd.x), (std::min)(m_SelectionStart.y, m_SelectionEnd.y));
			ImVec2 rectMax((std::max)(m_SelectionStart.x, m_SelectionEnd.x), (std::max)(m_SelectionStart.y, m_SelectionEnd.y));
			ImU32 fillColor = IM_COL32(90, 150, 255, 50);
			drawList->AddRectFilled(rectMin, rectMax, fillColor);
			
			ImU32 borderColor = IM_COL32(90, 150, 255, 200);
			drawList->AddRect(rectMin, rectMax, borderColor, 0.0f, 0, 2.0f);
		}
		
		ImGui::EndChild();
		ImGui::PopStyleVar();
		
		// Bottom bar con slider de tamaño (oscuro con sombra superior)
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 bottomBarStart = ImGui::GetCursorScreenPos();
		
		// Sombra superior del bottom bar
		for (int i = 0; i < 3; i++) {
			float alpha = (1.0f - (i / 3.0f)) * 0.3f;
			ImU32 shadowColor = IM_COL32(0, 0, 0, (int)(alpha * 255));
			drawList->AddRectFilled(
				ImVec2(bottomBarStart.x, bottomBarStart.y - i),
				ImVec2(bottomBarStart.x + ImGui::GetContentRegionAvail().x, bottomBarStart.y - i + 1),
				shadowColor
			);
		}
		
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.09f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_SliderGrab, ImVec4(0.50f, 0.50f, 0.50f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_SliderGrabActive, ImVec4(0.70f, 0.70f, 0.70f, 1.0f));
		
		ImGui::BeginChild("BottomBar", ImVec2(0, 0), false);
		ImGui::SetCursorPosX(ImGui::GetWindowWidth() - 170);
		ImGui::SetCursorPosY(6);
		ImGui::SetNextItemWidth(160);
		ImGui::SliderFloat("##Size", &m_ThumbnailSize, 64, 160);
		ImGui::EndChild();
		
		ImGui::PopStyleColor(4);
	}