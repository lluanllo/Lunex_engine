#include "stpch.h"
#include "ProjectSerializer.h"

#include "Log/Log.h"
#include <fstream>
#include <yaml-cpp/yaml.h>

namespace Lunex {
	ProjectSerializer::ProjectSerializer(Ref<Project> project)
		: m_Project(project)
	{
	}
	
	bool ProjectSerializer::Serialize(const std::filesystem::path& filepath) {
		const auto& config = m_Project->m_Config;
		
		YAML::Emitter out;
		out << YAML::BeginMap;
		out << YAML::Key << "Project" << YAML::Value;
		out << YAML::BeginMap;
		
		out << YAML::Key << "Name" << YAML::Value << config.Name;
		out << YAML::Key << "Version" << YAML::Value << config.Version;
		out << YAML::Key << "StartScene" << YAML::Value << config.StartScene.string();
		out << YAML::Key << "AssetDirectory" << YAML::Value << config.AssetDirectory.string();
		out << YAML::Key << "ScriptModulePath" << YAML::Value << config.ScriptModulePath.string();
		
		out << YAML::Key << "Settings" << YAML::Value;
		out << YAML::BeginMap;
		out << YAML::Key << "Width" << YAML::Value << config.Width;
		out << YAML::Key << "Height" << YAML::Value << config.Height;
		out << YAML::Key << "VSync" << YAML::Value << config.VSync;
		out << YAML::EndMap;
		
		out << YAML::EndMap;
		out << YAML::EndMap;
		
		std::ofstream fout(filepath);
		fout << out.c_str();
		
		LNX_LOG_INFO("Project saved: {0}", filepath.string());
		return true;
	}
	
	bool ProjectSerializer::Deserialize(const std::filesystem::path& filepath) {
		YAML::Node data;
		try {
			data = YAML::LoadFile(filepath.string());
		}
		catch (YAML::ParserException e) {
			LNX_LOG_ERROR("Failed to load project file '{0}'\n     {1}", filepath.string(), e.what());
			return false;
		}
		
		auto projectNode = data["Project"];
		if (!projectNode)
			return false;
		
		m_Project->m_Config.Name = projectNode["Name"].as<std::string>();
		m_Project->m_Config.Version = projectNode["Version"] ? projectNode["Version"].as<uint32_t>() : 1;
		m_Project->m_Config.StartScene = projectNode["StartScene"].as<std::string>();
		m_Project->m_Config.AssetDirectory = projectNode["AssetDirectory"].as<std::string>();
		
		if (projectNode["ScriptModulePath"])
			m_Project->m_Config.ScriptModulePath = projectNode["ScriptModulePath"].as<std::string>();
		
		auto settings = projectNode["Settings"];
		if (settings) {
			m_Project->m_Config.Width = settings["Width"].as<uint32_t>();
			m_Project->m_Config.Height = settings["Height"].as<uint32_t>();
			m_Project->m_Config.VSync = settings["VSync"].as<bool>();
		}
		
		LNX_LOG_INFO("Project loaded: {0}", m_Project->m_Config.Name);
		return true;
	}
}
