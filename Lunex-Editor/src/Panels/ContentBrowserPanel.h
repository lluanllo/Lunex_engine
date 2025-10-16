#pragma once

#include <filesystem>

namespace Lunex {
	class ContentBrowserPanel {
		public:
			ContentBrowserPanel();
		
			void OnImGuiRender();
		private:
			std::filesystem::path m_CurrentDirectory;
	};
}