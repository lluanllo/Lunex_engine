#include "stpch.h"
#include "ContentBrowserPanel.h"
#include <imgui.h>
#include <imgui_internal.h>

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
		
		// Inicializar historial
		m_DirectoryHistory.push_back(m_CurrentDirectory);
		m_HistoryIndex = 0;
	}
	
	void ContentBrowserPanel::OnImGuiRender() {
		// Estilo específico para Content Browser (oscuro como Unity con sombras)
		ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.11f, 0.11f, 0.11f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.11f, 0.11f, 0.11f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.5f));
		
		ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0, 0));
		ImGui::Begin("Content Browser");
		ImGui::PopStyleVar();
		
		RenderTopBar();
		
		// Layout horizontal con splitter ajustable
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(0, 0));
		
		// Splitter ajustable
		static float sidebarWidth = 200.0f;
		const float minSidebarWidth = 150.0f;
		const float maxSidebarWidth = 400.0f;
		
		ImVec2 availSize = ImGui::GetContentRegionAvail();
		
		// Sidebar con fondo más oscuro y sombra
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.09f, 1.0f));
		ImGui::BeginChild("Sidebar", ImVec2(sidebarWidth, availSize.y), false);
		RenderSidebar();
		ImGui::EndChild();
		ImGui::PopStyleColor();
		
		// Dibujar sombra del sidebar
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 sidebarMax = ImGui::GetItemRectMax();
		ImVec2 shadowStart = ImVec2(sidebarMax.x, sidebarMax.y - availSize.y);
		ImVec2 shadowEnd = sidebarMax;
		
		// Sombra gradiente de 8px
		for (int i = 0; i < 8; i++) {
			float alpha = (1.0f - (i / 8.0f)) * 0.3f;
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
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.3f, 0.3f, 0.3f, 0.3f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.3f, 0.3f, 0.3f, 0.5f));
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
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.13f, 0.13f, 0.13f, 1.0f));
		ImGui::BeginChild("FileGrid", ImVec2(0, availSize.y), false);
		RenderFileGrid();
		ImGui::EndChild();
		ImGui::PopStyleColor();
		
		ImGui::PopStyleVar();
		
		ImGui::End();
		
		ImGui::PopStyleColor(3);
	}
	
	void ContentBrowserPanel::RenderTopBar() {
		// Barra superior oscura con sombra inferior
		ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.09f, 0.09f, 0.09f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.15f, 0.15f, 0.15f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.20f, 0.20f, 0.20f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.14f, 0.14f, 0.14f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Border, ImVec4(0.0f, 0.0f, 0.0f, 0.5f));
		
		ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(4, 4));
		ImGui::BeginChild("TopBar", ImVec2(0, 36), true, ImGuiWindowFlags_NoScrollbar);
		
		// Botones de navegación con iconos
		bool canGoBack = m_HistoryIndex > 0;
		if (!canGoBack) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.3f);
		
		if (m_BackIcon) {
			if (ImGui::ImageButton("##BackButton", (ImTextureID)(intptr_t)m_BackIcon->GetRendererID(),
				ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0)) && canGoBack) {
				m_HistoryIndex--;
				m_CurrentDirectory = m_DirectoryHistory[m_HistoryIndex];
			}
		} else {
			if (ImGui::Button("<", ImVec2(28, 28)) && canGoBack) {
				m_HistoryIndex--;
				m_CurrentDirectory = m_DirectoryHistory[m_HistoryIndex];
			}
		}
		
		if (!canGoBack) ImGui::PopStyleVar();
		
		ImGui::SameLine();
		
		bool canGoForward = m_HistoryIndex < (int)m_DirectoryHistory.size() - 1;
		if (!canGoForward) ImGui::PushStyleVar(ImGuiStyleVar_Alpha, 0.3f);
		
		if (m_ForwardIcon) {
			if (ImGui::ImageButton("##ForwardButton", (ImTextureID)(intptr_t)m_ForwardIcon->GetRendererID(),
				ImVec2(20, 20), ImVec2(0, 0), ImVec2(1, 1), ImVec4(0, 0, 0, 0)) && canGoForward) {
				m_HistoryIndex++;
				m_CurrentDirectory = m_DirectoryHistory[m_HistoryIndex];
			}
		} else {
			if (ImGui::Button(">", ImVec2(28, 28)) && canGoForward) {
				m_HistoryIndex++;
				m_CurrentDirectory = m_DirectoryHistory[m_HistoryIndex];
			}
		}
		
		if (!canGoForward) ImGui::PopStyleVar();
		
		ImGui::SameLine();
		ImGui::Dummy(ImVec2(12, 0));
		ImGui::SameLine();
		
		// Path display con fondo sutil
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.75f, 0.75f, 0.75f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.12f, 0.12f, 0.12f, 1.0f));
		ImGui::AlignTextToFramePadding();
		
		// Crear buffer estático con tamaño fijo para el path
		static char pathBuffer[512];
		std::string currentPath = m_CurrentDirectory.string();
		
		// Copiar el path al buffer de forma segura
		strncpy_s(pathBuffer, sizeof(pathBuffer), currentPath.c_str(), _TRUNCATE);
		
		ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - 220);
		ImGui::InputText("##PathDisplay", pathBuffer, sizeof(pathBuffer), ImGuiInputTextFlags_ReadOnly);
		
		ImGui::PopStyleColor(2);
		
		// Search bar a la derecha
		ImGui::SameLine();
		ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(0.18f, 0.18f, 0.18f, 1.0f));
		ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.8f, 0.8f, 0.8f, 1.0f));
		ImGui::SetNextItemWidth(200);
		ImGui::InputTextWithHint("##Search", "?? Search...", m_SearchBuffer, 256);
		ImGui::PopStyleColor(2);
		
		ImGui::EndChild();
		
		// Dibujar sombra debajo de la topbar
		ImDrawList* drawList = ImGui::GetWindowDrawList();
		ImVec2 topbarMax = ImGui::GetItemRectMax();
		ImVec2 shadowStart = ImVec2(topbarMax.x - ImGui::GetContentRegionAvail().x, topbarMax.y);
		
		for (int i = 0; i < 4; i++) {
			float alpha = (1.0f - (i / 4.0f)) * 0.4f;
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
						flags |= ImGuiTreeNodeFlags_Selected;
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
							(ImTextureID)(intptr_t)m_DirectoryIcon->GetRendererID(),
							iconPos,
							ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
							ImVec2(0, 0),
							ImVec2(1, 1)
						);
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
				(ImTextureID)(intptr_t)m_DirectoryIcon->GetRendererID(),
				iconPos,
				ImVec2(iconPos.x + iconSize, iconPos.y + iconSize),
				ImVec2(0, 0),
				ImVec2(1, 1)
			);
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
	
	void ContentBrowserPanel::RenderFileGrid() {
		ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(8, 8));
		ImGui::BeginChild("FileGridContent", ImVec2(0, -28), false);
		
		// Agregar padding lateral al contenido
		ImGui::Indent(16.0f);
		
		float cellSize = m_ThumbnailSize + m_Padding * 2;
		float panelWidth = ImGui::GetContentRegionAvail().x;
		int columnCount = (int)(panelWidth / cellSize);
		if (columnCount < 1) columnCount = 1;
		
		// Grid sin separadores visibles
		ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(m_Padding, m_Padding + 8));
		
		if (ImGui::BeginTable("FileGridTable", columnCount, ImGuiTableFlags_None)) {
			
			// Filtrar por búsqueda
			std::string searchQuery = m_SearchBuffer;
			std::transform(searchQuery.begin(), searchQuery.end(), searchQuery.begin(), ::tolower);
			
			// Colores de los botones (transparentes con hover sutil)
			ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0, 0, 0, 0));
			ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(0.22f, 0.22f, 0.22f, 0.6f));
			ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(0.18f, 0.40f, 0.65f, 0.8f));
			
			int itemIndex = 0;
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
				
				if (itemIndex % columnCount == 0) {
					ImGui::TableNextRow();
				}
				ImGui::TableSetColumnIndex(itemIndex % columnCount);
				
				ImGui::PushID(filenameString.c_str());
				
				// Usar columna vertical para icono + texto
				ImGui::BeginGroup();
				
				// Usar GetThumbnailForFile en lugar de GetIconForFile para mostrar previews
				Ref<Texture2D> icon = directoryEntry.is_directory() ? m_DirectoryIcon : GetThumbnailForFile(path);
				
				if (icon) {
					float iconSize = m_ThumbnailSize;
					
					// Botón con padding para el hover effect
					ImVec2 cursorPos = ImGui::GetCursorScreenPos();
					
					// Dibujar marco redondeado detrás de la imagen
					ImDrawList* drawList = ImGui::GetWindowDrawList();
					float rounding = 8.0f;
					float padding = 2.0f;
					
					// Marco de fondo con bordes redondeados
					ImU32 bgColor = IM_COL32(30, 30, 30, 255);
					drawList->AddRectFilled(
						ImVec2(cursorPos.x - padding, cursorPos.y - padding),
						ImVec2(cursorPos.x + iconSize + padding, cursorPos.y + iconSize + padding),
						bgColor,
						rounding
					);
					
					// Dibujar la imagen con bordes redondeados usando AddImageRounded
					ImVec2 imageMin = ImVec2(cursorPos.x, cursorPos.y);
					ImVec2 imageMax = ImVec2(cursorPos.x + iconSize, cursorPos.y + iconSize);
					
					drawList->AddImageRounded(
						(ImTextureID)(intptr_t)icon->GetRendererID(),
						imageMin,
						imageMax,
						ImVec2(0, 1),
						ImVec2(1, 0),
						IM_COL32(255, 255, 255, 255),
						rounding
					);
					
					// Botón invisible para capturar clicks
					ImGui::SetCursorScreenPos(cursorPos);
					ImGui::InvisibleButton(filenameString.c_str(), ImVec2(iconSize, iconSize));
					
					// Borde redondeado al hacer hover
					if (ImGui::IsItemHovered()) {
						ImU32 borderColor = IM_COL32(100, 100, 100, 200);
						drawList->AddRect(
							ImVec2(cursorPos.x - padding, cursorPos.y - padding),
							ImVec2(cursorPos.x + iconSize + padding, cursorPos.y + iconSize + padding),
							borderColor,
							rounding,
							0,
							2.0f
						);
						
						// Sombra sutil bajo el icono cuando se hace hover
						for (int i = 0; i < 3; i++) {
							float alpha = (1.0f - (i / 3.0f)) * 0.2f;
							ImU32 shadowColor = IM_COL32(0, 0, 0, (int)(alpha * 255));
							drawList->AddRectFilled(
								ImVec2(cursorPos.x - padding - i, cursorPos.y + iconSize + padding + i),
								ImVec2(cursorPos.x + iconSize + padding + i, cursorPos.y + iconSize + padding + i + 1),
								shadowColor
							);
						}
					}
					
					if (ImGui::BeginDragDropSource()) {
						auto relativePath = std::filesystem::relative(path, g_AssetPath);
						const wchar_t* itemPath = relativePath.c_str();
						ImGui::SetDragDropPayload("CONTENT_BROWSER_ITEM", itemPath, (wcslen(itemPath) + 1) * sizeof(wchar_t));
						ImGui::Text("%s", filenameString.c_str());
						ImGui::EndDragDropSource();
					}
					
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
					}
				}
				
				// Nombre del archivo debajo del icono (centrado y truncado)
				ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.85f, 0.85f, 0.85f, 1.0f));
				
				// Truncar texto si es muy largo
				std::string displayName = filenameString;
				const int maxChars = 18;
				if (displayName.length() > maxChars) {
					displayName = displayName.substr(0, maxChars - 3) + "...";
				}
				
				// Centrar texto
				float textWidth = ImGui::CalcTextSize(displayName.c_str()).x;
				float offsetX = (m_ThumbnailSize - textWidth) * 0.5f;
				if (offsetX > 0) {
					ImGui::SetCursorPosX(ImGui::GetCursorPosX() + offsetX);
				}
				ImGui::TextWrapped("%s", displayName.c_str());
				
				ImGui::PopStyleColor();
				
				ImGui::EndGroup();
				
				ImGui::PopID();
				
				itemIndex++;
			}
			
			ImGui::PopStyleColor(3);
			ImGui::EndTable();
		}
		
		ImGui::PopStyleVar();
		
		// Remover el indent al final
		ImGui::Unindent(16.0f);
		
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
		ImGui::SliderFloat("##Size", &m_ThumbnailSize, 60, 128);
		ImGui::EndChild();
		
		ImGui::PopStyleColor(4);
	}
}