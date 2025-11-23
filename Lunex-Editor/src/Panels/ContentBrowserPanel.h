#pragma once

#include "Renderer/Texture.h"
#include <filesystem>
#include <vector>
#include <unordered_map>
#include <unordered_set>
#include <imgui.h>
#include <imgui_internal.h>

namespace Lunex {
    class Event;
    
    // Declaración externa para que otros archivos puedan usar g_AssetPath
    extern const std::filesystem::path g_AssetPath;
 
    class ContentBrowserPanel {
      public:
          ContentBrowserPanel();
         
void OnImGuiRender();
void OnEvent(Event& e);

void SetRootDirectory(const std::filesystem::path& directory);
  
     private:
            void RenderTopBar();
   void RenderSidebar();
         void RenderFileGrid();
         void RenderContextMenu();
         
         Ref<Texture2D> GetIconForFile(const std::filesystem::path& path);
         Ref<Texture2D> GetThumbnailForFile(const std::filesystem::path& path);
         
         // File operations
         void CreateNewFolder();
         void CreateNewScene();
         void CreateNewScript();
         void DeleteItem(const std::filesystem::path& path);
         void RenameItem(const std::filesystem::path& oldPath);
         void ImportFiles(const std::vector<std::string>& files);
         void ImportFilesToFolder(const std::vector<std::string>& files, const std::filesystem::path& targetFolder);
         
         // Selection
         bool IsSelected(const std::filesystem::path& path) const;
         void ClearSelection();
         void AddToSelection(const std::filesystem::path& path);
         void RemoveFromSelection(const std::filesystem::path& path);
         void ToggleSelection(const std::filesystem::path& path);
         void SelectRange(const std::filesystem::path& from, const std::filesystem::path& to);

     std::filesystem::path m_BaseDirectory;
   std::filesystem::path m_CurrentDirectory;
   
     // History navigation
     std::vector<std::filesystem::path> m_DirectoryHistory;
        int m_HistoryIndex = 0;

            // Search
   char m_SearchBuffer[256] = "";
   
   // Context menu
   bool m_ShowCreateFolderDialog = false;
   bool m_ShowRenameDialog = false;
   char m_NewItemName[256] = "NewFolder";
   std::filesystem::path m_ItemToRename;
   
   // File drop
   bool m_IsHovered = false;
   std::filesystem::path m_HoveredFolder;
   
   // Selection
   std::unordered_set<std::string> m_SelectedItems; // Store paths as strings
   std::filesystem::path m_LastSelectedItem;
   bool m_IsSelecting = false;
   ImVec2 m_SelectionStart;
   ImVec2 m_SelectionEnd;
    
    // View settings
            float m_ThumbnailSize = 80.0f;
 float m_Padding = 12.0f;
     
            // Icons
            Ref<Texture2D> m_DirectoryIcon;
  Ref<Texture2D> m_FileIcon;
            Ref<Texture2D> m_BackIcon;
       Ref<Texture2D> m_ForwardIcon;
       
 // File type icons
   Ref<Texture2D> m_SceneIcon;
 Ref<Texture2D> m_TextureIcon;
       Ref<Texture2D> m_ShaderIcon;
     Ref<Texture2D> m_AudioIcon;
         Ref<Texture2D> m_ScriptIcon;
         
         // Texture preview cache
         std::unordered_map<std::string, Ref<Texture2D>> m_TextureCache;
         
         // Item positions for selection rectangle
         std::unordered_map<std::string, ImRect> m_ItemBounds;
    };
}