#include "VersionManager.h"
#include "Log/Log.h"
#include <fstream>
#include <filesystem>

#ifdef _WIN32
#include <windows.h>
#include <vulkan/vulkan.h>
#pragma comment(lib, "vulkan-1.lib")
#endif

namespace Lunex {

	Version VersionManager::s_CurrentVersion = { 1, 0, 0, "Dev" };
	Version VersionManager::s_LatestVersion = { 1, 0, 0, "Dev" };

	void VersionManager::Init() {
		// Load version from file
		std::filesystem::path versionFile = "version.txt";
		if (std::filesystem::exists(versionFile)) {
			std::ifstream file(versionFile);
			if (file.is_open()) {
				std::string line;
				if (std::getline(file, line)) {
					// Parse version string (format: "1.0.0 Dev")
					size_t firstDot = line.find('.');
					size_t secondDot = line.find('.', firstDot + 1);
					size_t space = line.find(' ', secondDot + 1);
					
					if (firstDot != std::string::npos && secondDot != std::string::npos) {
						try {
							s_CurrentVersion.Major = std::stoi(line.substr(0, firstDot));
							s_CurrentVersion.Minor = std::stoi(line.substr(firstDot + 1, secondDot - firstDot - 1));
							
							if (space != std::string::npos) {
								s_CurrentVersion.Patch = std::stoi(line.substr(secondDot + 1, space - secondDot - 1));
								s_CurrentVersion.BuildType = line.substr(space + 1);
							} else {
								s_CurrentVersion.Patch = std::stoi(line.substr(secondDot + 1));
							}
						} catch (const std::exception& e) {
							LNX_LOG_ERROR("Failed to parse version: {0}", e.what());
						}
					}
				}
				file.close();
			}
		} else {
			// Create version file with default version
			std::ofstream file(versionFile);
			if (file.is_open()) {
				file << s_CurrentVersion.ToString();
				file.close();
			}
		}
		
		LNX_LOG_INFO("Lunex Engine Version: {0}", s_CurrentVersion.ToString());
	}

	Version VersionManager::GetLatestVersion() {
		// TODO: Implement check against remote server/GitHub releases
		// For now, return current version
		return s_CurrentVersion;
	}

	bool VersionManager::IsUpdateAvailable() {
		s_LatestVersion = GetLatestVersion();
		return s_LatestVersion > s_CurrentVersion;
	}

	bool VersionManager::CheckPrerequisites() {
		auto missing = GetMissingPrerequisites();
		return missing.empty();
	}

	std::vector<std::string> VersionManager::GetMissingPrerequisites() {
		std::vector<std::string> missing;
		
#ifdef _WIN32
		// Check Vulkan
		VkInstance instance;
		VkApplicationInfo appInfo = {};
		appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
		appInfo.apiVersion = VK_API_VERSION_1_0;
		
		VkInstanceCreateInfo createInfo = {};
		createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
		createInfo.pApplicationInfo = &appInfo;
		
		VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);
		if (result != VK_SUCCESS) {
			missing.push_back("Vulkan SDK (Driver or Runtime not found)");
		} else {
			vkDestroyInstance(instance, nullptr);
		}
		
		// Check Visual C++ Redistributable
		HKEY hKey;
		bool vcRedistFound = false;
		
		// Check for VC++ 2015-2022 Redistributable
		const char* vcRedistKeys[] = {
			"SOFTWARE\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\X64",
			"SOFTWARE\\WOW6432Node\\Microsoft\\VisualStudio\\14.0\\VC\\Runtimes\\X64"
		};
		
		for (const char* keyPath : vcRedistKeys) {
			if (RegOpenKeyExA(HKEY_LOCAL_MACHINE, keyPath, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
				vcRedistFound = true;
				RegCloseKey(hKey);
				break;
			}
		}
		
		if (!vcRedistFound) {
			missing.push_back("Visual C++ Redistributable 2015-2022");
		}
		
		// Check OpenGL
		// Note: This requires a valid OpenGL context, so we skip this check in launcher
		// The engine will handle OpenGL checks at runtime
		
#endif
		
		return missing;
	}

}
