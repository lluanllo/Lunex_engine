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

	bool InputConfig::CreateDefault(const std::filesystem::path& filepath) {
		// Create directory if it doesn't exist
		std::filesystem::path directory = filepath.parent_path();
		if (!directory.empty() && !std::filesystem::exists(directory)) {
			try {
				std::filesystem::create_directories(directory);
			}
			catch (const std::exception& e) {
				LNX_LOG_ERROR("Failed to create directory {0}: {1}", directory.string(), e.what());
				return false;
			}
		}

		KeyMap defaultMap;

		// Editor Camera Controls
		defaultMap.Bind(Key::W, KeyModifiers::None, "Camera.MoveForward");
		defaultMap.Bind(Key::S, KeyModifiers::None, "Camera.MoveBackward");
		defaultMap.Bind(Key::A, KeyModifiers::None, "Camera.MoveLeft");
		defaultMap.Bind(Key::D, KeyModifiers::None, "Camera.MoveRight");
		defaultMap.Bind(Key::Q, KeyModifiers::None, "Camera.MoveDown");
		defaultMap.Bind(Key::E, KeyModifiers::None, "Camera.MoveUp");

		// Editor Operations
		defaultMap.Bind(Key::S, KeyModifiers::Ctrl, "Editor.SaveScene");
		defaultMap.Bind(Key::O, KeyModifiers::Ctrl, "Editor.OpenScene");
		defaultMap.Bind(Key::N, KeyModifiers::Ctrl, "Editor.NewScene");
		defaultMap.Bind(Key::P, KeyModifiers::Ctrl, "Editor.PlayScene");
		defaultMap.Bind(Key::D, KeyModifiers::Ctrl, "Editor.DuplicateEntity");

		// Gizmo Operations
		defaultMap.Bind(Key::Q, KeyModifiers::None, "Gizmo.None");
		defaultMap.Bind(Key::W, KeyModifiers::None, "Gizmo.Translate");
		defaultMap.Bind(Key::E, KeyModifiers::None, "Gizmo.Rotate");
		defaultMap.Bind(Key::R, KeyModifiers::None, "Gizmo.Scale");

		// Debug
		defaultMap.Bind(Key::F1, KeyModifiers::None, "Debug.ToggleStats");
		defaultMap.Bind(Key::F2, KeyModifiers::None, "Debug.ToggleColliders");
		defaultMap.Bind(Key::GraveAccent, KeyModifiers::None, "Debug.ToggleConsole");
		
		// Preferences
		defaultMap.Bind(Key::K, KeyModifiers::Ctrl, "Preferences.InputSettings");

		return Save(defaultMap, filepath);
	}

	std::filesystem::path InputConfig::GetDefaultConfigPath() {
		return "Config/InputBindings.yaml";
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
