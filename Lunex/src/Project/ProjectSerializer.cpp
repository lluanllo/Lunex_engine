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
		
		// ? NEW: Serialize Input Bindings
		out << YAML::Key << "InputBindings" << YAML::Value;
		out << YAML::BeginSeq;
		for (const auto& binding : config.InputBindings) {
			out << YAML::BeginMap;
			out << YAML::Key << "Key" << YAML::Value << binding.KeyCode;
			out << YAML::Key << "Modifiers" << YAML::Value << binding.Modifiers;
			out << YAML::Key << "Action" << YAML::Value << binding.ActionName;
			out << YAML::EndMap;
		}
		out << YAML::EndSeq;
		
		// ? Serialize Outline Preferences
		{
			const auto& op = config.OutlinePreferences;
			out << YAML::Key << "OutlinePreferences" << YAML::Value;
			out << YAML::BeginMap;
			out << YAML::Key << "OutlineColor" << YAML::Value << YAML::Flow << YAML::BeginSeq << op.OutlineColor.r << op.OutlineColor.g << op.OutlineColor.b << op.OutlineColor.a << YAML::EndSeq;
			out << YAML::Key << "KernelSize" << YAML::Value << op.OutlineKernelSize;
			out << YAML::Key << "Hardness" << YAML::Value << op.OutlineHardness;
			out << YAML::Key << "InsideAlpha" << YAML::Value << op.OutlineInsideAlpha;
			out << YAML::Key << "ShowBehindObjects" << YAML::Value << op.ShowBehindObjects;
			out << YAML::Key << "Collider2DColor" << YAML::Value << YAML::Flow << YAML::BeginSeq << op.Collider2DColor.r << op.Collider2DColor.g << op.Collider2DColor.b << op.Collider2DColor.a << YAML::EndSeq;
			out << YAML::Key << "Collider3DColor" << YAML::Value << YAML::Flow << YAML::BeginSeq << op.Collider3DColor.r << op.Collider3DColor.g << op.Collider3DColor.b << op.Collider3DColor.a << YAML::EndSeq;
			out << YAML::Key << "ColliderLineWidth" << YAML::Value << op.ColliderLineWidth;
			out << YAML::Key << "GizmoLineWidth" << YAML::Value << op.GizmoLineWidth;
			out << YAML::EndMap;
		}
		
		// ? Serialize Post-Processing Preferences
		{
			const auto& pp = config.PostProcessPreferences;
			out << YAML::Key << "PostProcessPreferences" << YAML::Value;
			out << YAML::BeginMap;
			out << YAML::Key << "EnableBloom" << YAML::Value << pp.EnableBloom;
			out << YAML::Key << "BloomThreshold" << YAML::Value << pp.BloomThreshold;
			out << YAML::Key << "BloomIntensity" << YAML::Value << pp.BloomIntensity;
			out << YAML::Key << "BloomRadius" << YAML::Value << pp.BloomRadius;
			out << YAML::Key << "BloomMipLevels" << YAML::Value << pp.BloomMipLevels;
			out << YAML::Key << "EnableVignette" << YAML::Value << pp.EnableVignette;
			out << YAML::Key << "VignetteIntensity" << YAML::Value << pp.VignetteIntensity;
			out << YAML::Key << "VignetteRoundness" << YAML::Value << pp.VignetteRoundness;
			out << YAML::Key << "VignetteSmoothness" << YAML::Value << pp.VignetteSmoothness;
			out << YAML::Key << "EnableChromaticAberration" << YAML::Value << pp.EnableChromaticAberration;
			out << YAML::Key << "ChromaticAberrationIntensity" << YAML::Value << pp.ChromaticAberrationIntensity;
			out << YAML::Key << "ToneMapOperator" << YAML::Value << pp.ToneMapOperator;
			out << YAML::Key << "Exposure" << YAML::Value << pp.Exposure;
			out << YAML::Key << "Gamma" << YAML::Value << pp.Gamma;
			out << YAML::EndMap;
		}
		
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
		
		// ? NEW: Deserialize Input Bindings
		auto inputBindings = projectNode["InputBindings"];
		if (inputBindings) {
			m_Project->m_Config.InputBindings.clear();
			for (const auto& bindingNode : inputBindings) {
				InputBindingEntry entry;
				entry.KeyCode = bindingNode["Key"].as<int>();
				entry.Modifiers = bindingNode["Modifiers"].as<int>();
				entry.ActionName = bindingNode["Action"].as<std::string>();
				m_Project->m_Config.InputBindings.push_back(entry);
			}
			LNX_LOG_INFO("Loaded {0} input bindings from project", m_Project->m_Config.InputBindings.size());
		}
		
		// ? NEW: Deserialize Outline Preferences
		auto outlineNode = projectNode["OutlinePreferences"];
		if (outlineNode) {
			auto& op = m_Project->m_Config.OutlinePreferences;
			
			if (outlineNode["OutlineColor"]) {
				auto c = outlineNode["OutlineColor"];
				op.OutlineColor = glm::vec4(c[0].as<float>(), c[1].as<float>(), c[2].as<float>(), c[3].as<float>());
			}
			if (outlineNode["KernelSize"])
				op.OutlineKernelSize = outlineNode["KernelSize"].as<int>();
			if (outlineNode["Hardness"])
				op.OutlineHardness = outlineNode["Hardness"].as<float>();
			if (outlineNode["InsideAlpha"])
				op.OutlineInsideAlpha = outlineNode["InsideAlpha"].as<float>();
			if (outlineNode["ShowBehindObjects"])
				op.ShowBehindObjects = outlineNode["ShowBehindObjects"].as<bool>();
			if (outlineNode["Collider2DColor"]) {
				auto c = outlineNode["Collider2DColor"];
				op.Collider2DColor = glm::vec4(c[0].as<float>(), c[1].as<float>(), c[2].as<float>(), c[3].as<float>());
			}
			if (outlineNode["Collider3DColor"]) {
				auto c = outlineNode["Collider3DColor"];
				op.Collider3DColor = glm::vec4(c[0].as<float>(), c[1].as<float>(), c[2].as<float>(), c[3].as<float>());
			}
			if (outlineNode["ColliderLineWidth"])
				op.ColliderLineWidth = outlineNode["ColliderLineWidth"].as<float>();
			if (outlineNode["GizmoLineWidth"])
				op.GizmoLineWidth = outlineNode["GizmoLineWidth"].as<float>();
			
			LNX_LOG_INFO("Loaded outline preferences from project");
		}
		
		// ? NEW: Deserialize Post-Processing Preferences
		auto ppNode = projectNode["PostProcessPreferences"];
		if (ppNode) {
			auto& pp = m_Project->m_Config.PostProcessPreferences;
			
			if (ppNode["EnableBloom"])
				pp.EnableBloom = ppNode["EnableBloom"].as<bool>();
			if (ppNode["BloomThreshold"])
				pp.BloomThreshold = ppNode["BloomThreshold"].as<float>();
			if (ppNode["BloomIntensity"])
				pp.BloomIntensity = ppNode["BloomIntensity"].as<float>();
			if (ppNode["BloomRadius"])
				pp.BloomRadius = ppNode["BloomRadius"].as<float>();
			if (ppNode["BloomMipLevels"])
				pp.BloomMipLevels = ppNode["BloomMipLevels"].as<int>();
			if (ppNode["EnableVignette"])
				pp.EnableVignette = ppNode["EnableVignette"].as<bool>();
			if (ppNode["VignetteIntensity"])
				pp.VignetteIntensity = ppNode["VignetteIntensity"].as<float>();
			if (ppNode["VignetteRoundness"])
				pp.VignetteRoundness = ppNode["VignetteRoundness"].as<float>();
			if (ppNode["VignetteSmoothness"])
				pp.VignetteSmoothness = ppNode["VignetteSmoothness"].as<float>();
			if (ppNode["EnableChromaticAberration"])
				pp.EnableChromaticAberration = ppNode["EnableChromaticAberration"].as<bool>();
			if (ppNode["ChromaticAberrationIntensity"])
				pp.ChromaticAberrationIntensity = ppNode["ChromaticAberrationIntensity"].as<float>();
			if (ppNode["ToneMapOperator"])
				pp.ToneMapOperator = ppNode["ToneMapOperator"].as<int>();
			if (ppNode["Exposure"])
				pp.Exposure = ppNode["Exposure"].as<float>();
			if (ppNode["Gamma"])
				pp.Gamma = ppNode["Gamma"].as<float>();
			
			LNX_LOG_INFO("Loaded post-processing preferences from project");
		}
		
		LNX_LOG_INFO("Project loaded: {0}", m_Project->m_Config.Name);
		return true;
	}
}
