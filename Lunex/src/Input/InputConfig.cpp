#include "stpch.h"
#include "InputConfig.h"
#include "Log/Log.h"
#include <yaml-cpp/yaml.h>
#include <fstream>

namespace Lunex {

	bool InputConfig::Save(const KeyMap& keyMap, const std::filesystem::path& filepath) {
		try {
			YAML::Emitter out;
			out << YAML::BeginMap;

			// Metadata
			out << YAML::Key << "version" << YAML::Value << 1;
			out << YAML::Key << "profile" << YAML::Value << "Default";

			// Bindings
			out << YAML::Key << "bindings" << YAML::Value << YAML::BeginSeq;

			for (const auto& [binding, actionName] : keyMap.GetAllBindings()) {
				out << YAML::BeginMap;
				out << YAML::Key << "key" << YAML::Value << static_cast<int>(binding.Key);
				out << YAML::Key << "modifiers" << YAML::Value << static_cast<int>(binding.Modifiers);
				out << YAML::Key << "action" << YAML::Value << actionName;
				out << YAML::EndMap;
			}

			out << YAML::EndSeq;
			out << YAML::EndMap;

			// Write to file
			std::ofstream file(filepath);
			if (!file.is_open()) {
				LNX_LOG_ERROR("Failed to open file for writing: {0}", filepath.string());
				return false;
			}

			file << out.c_str();
			file.close();

			LNX_LOG_INFO("Saved {0} key bindings to {1}", 
				keyMap.GetBindingCount(), filepath.string());
			return true;
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Failed to save input config: {0}", e.what());
			return false;
		}
	}

	bool InputConfig::Load(KeyMap& keyMap, const std::filesystem::path& filepath) {
		try {
			if (!std::filesystem::exists(filepath)) {
				LNX_LOG_WARN("Input config file not found: {0}", filepath.string());
				return false;
			}

			YAML::Node data = YAML::LoadFile(filepath.string());

			if (!data["bindings"]) {
				LNX_LOG_ERROR("Invalid input config: missing 'bindings' key");
				return false;
			}

			// Clear existing bindings
			keyMap.Clear();

			// Load bindings
			auto bindings = data["bindings"];
			int loadedCount = 0;

			for (const auto& bindingNode : bindings) {
				if (!bindingNode["key"] || !bindingNode["action"]) {
					LNX_LOG_WARN("Skipping invalid binding entry");
					continue;
				}

				int keyCode = bindingNode["key"].as<int>();
				int modifiers = bindingNode["modifiers"] ? bindingNode["modifiers"].as<int>() : 0;
				std::string actionName = bindingNode["action"].as<std::string>();

				if (keyMap.Bind(static_cast<KeyCode>(keyCode), static_cast<uint8_t>(modifiers), actionName)) {
					loadedCount++;
				}
			}

			LNX_LOG_INFO("Loaded {0} key bindings from {1}", loadedCount, filepath.string());
			return true;
		}
		catch (const std::exception& e) {
			LNX_LOG_ERROR("Failed to load input config: {0}", e.what());
			return false;
		}
	}

	std::filesystem::path InputConfig::GetDefaultConfigPath() {
		return "Config/InputBindings.yaml";
	}

	std::filesystem::path InputConfig::GetEditorConfigPath() {
		// ? Global config in editor's assets/InputConfigs directory
		// This persists across all projects and is version-controlled
		return "assets/InputConfigs/EditorInputBindings.yaml";
	}

	bool InputConfig::IsCompatible(const std::filesystem::path& filepath) {
		try {
			YAML::Node data = YAML::LoadFile(filepath.string());
			int version = data["version"].as<int>(0);
			return version == 1; // Current supported version
		}
		catch (...) {
			return false;
		}
	}

} // namespace Lunex
