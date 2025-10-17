#pragma once

#include "Renderer/Texture.h"
#include <filesystem>

namespace Lunex {
    // Declaración externa para que otros archivos puedan usar g_AssetPath
    extern const std::filesystem::path g_AssetPath;
       
    class ContentBrowserPanel {
        public:
            ContentBrowserPanel();
                
            void OnImGuiRender();
                
        private:
            std::filesystem::path m_CurrentDirectory;
             
            Ref<Texture2D> m_DirectoryIcon;
            Ref<Texture2D> m_FileIcon;
    };
}