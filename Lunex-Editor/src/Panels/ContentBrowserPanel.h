#pragma once

#include "Renderer/Texture.h"
#include <filesystem>
#include <vector>

namespace Lunex {
    // Declaración externa para que otros archivos puedan usar g_AssetPath
    extern const std::filesystem::path g_AssetPath;
       
    class ContentBrowserPanel {
        public:
          ContentBrowserPanel();
         
void OnImGuiRender();
  
        private:
            void RenderTopBar();
   void RenderSidebar();
         void RenderFileGrid();
         Ref<Texture2D> GetIconForFile(const std::filesystem::path& path);

            std::filesystem::path m_BaseDirectory;
       std::filesystem::path m_CurrentDirectory;
   
       // History navigation
     std::vector<std::filesystem::path> m_DirectoryHistory;
        int m_HistoryIndex = 0;
       
            // Search
          char m_SearchBuffer[256] = "";
    
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
    };
}