#pragma once

/**
 * @file NodeGraphSerializer.h
 * @brief Serialization/deserialization for node graphs using YAML
 * 
 * Follows the same YAML serialization patterns as the rest of Lunex
 * (scenes, materials, etc.)
 */

#include "NodeGraph.h"
#include "NodeFactory.h"

#include <yaml-cpp/yaml.h>
#include <filesystem>
#include <fstream>

namespace Lunex::NodeGraph {

	class NodeGraphSerializer {
	public:
		// ========== SERIALIZE ==========

		static bool SaveToFile(const NodeGraph& graph, const std::filesystem::path& path) {
			YAML::Emitter out;
			Serialize(graph, out);

			std::ofstream fout(path);
			if (!fout.is_open()) return false;
			fout << out.c_str();
			return true;
		}

		static std::string SaveToString(const NodeGraph& graph) {
			YAML::Emitter out;
			Serialize(graph, out);
			return out.c_str();
		}

		// ========== DESERIALIZE ==========

		static bool LoadFromFile(NodeGraph& graph, const std::filesystem::path& path) {
			if (!std::filesystem::exists(path)) return false;

			YAML::Node root;
			try {
				root = YAML::LoadFile(path.string());
			}
			catch (const YAML::Exception&) {
				return false;
			}
			return Deserialize(graph, root);
		}

		static bool LoadFromString(NodeGraph& graph, const std::string& data) {
			YAML::Node root;
			try {
				root = YAML::Load(data);
			}
			catch (const YAML::Exception&) {
				return false;
			}
			return Deserialize(graph, root);
		}

	private:
		static void Serialize(const NodeGraph& graph, YAML::Emitter& out) {
			out << YAML::BeginMap;
			out << YAML::Key << "NodeGraph" << YAML::Value;
			out << YAML::BeginMap;
			
			out << YAML::Key << "Name" << YAML::Value << graph.GetName();
			out << YAML::Key << "Domain" << YAML::Value << GraphDomainToString(graph.GetDomain());
			out << YAML::Key << "ID" << YAML::Value << (uint64_t)graph.GetID();

			// Nodes
			out << YAML::Key << "Nodes" << YAML::Value << YAML::BeginSeq;
			for (const auto& [id, node] : graph.GetNodes()) {
				SerializeNode(*node, out);
			}
			out << YAML::EndSeq;

			// Links
			out << YAML::Key << "Links" << YAML::Value << YAML::BeginSeq;
			for (const auto& [id, link] : graph.GetLinks()) {
				out << YAML::BeginMap;
				out << YAML::Key << "ID" << YAML::Value << link.ID;
				out << YAML::Key << "StartPin" << YAML::Value << link.StartPinID;
				out << YAML::Key << "EndPin" << YAML::Value << link.EndPinID;
				out << YAML::EndMap;
			}
			out << YAML::EndSeq;

			out << YAML::EndMap;
			out << YAML::EndMap;
		}

		static void SerializeNode(const Node& node, YAML::Emitter& out) {
			out << YAML::BeginMap;
			out << YAML::Key << "ID" << YAML::Value << node.ID;
			out << YAML::Key << "Type" << YAML::Value << node.TypeName;
			out << YAML::Key << "Display" << YAML::Value << node.DisplayName;
			out << YAML::Key << "Position" << YAML::Value << YAML::Flow
				<< YAML::BeginSeq << node.Position.x << node.Position.y << YAML::EndSeq;

			// Serialize input pin default values
			if (!node.Inputs.empty()) {
				out << YAML::Key << "Inputs" << YAML::Value << YAML::BeginSeq;
				for (const auto& pin : node.Inputs) {
					out << YAML::BeginMap;
					out << YAML::Key << "ID" << YAML::Value << pin.ID;
					out << YAML::Key << "Name" << YAML::Value << pin.Name;
					out << YAML::Key << "Type" << YAML::Value << PinDataTypeToString(pin.DataType);
					SerializePinValue(pin.DefaultValue, out);
					out << YAML::EndMap;
				}
				out << YAML::EndSeq;
			}

			// Serialize output pins (mainly for ID reconstruction)
			if (!node.Outputs.empty()) {
				out << YAML::Key << "Outputs" << YAML::Value << YAML::BeginSeq;
				for (const auto& pin : node.Outputs) {
					out << YAML::BeginMap;
					out << YAML::Key << "ID" << YAML::Value << pin.ID;
					out << YAML::Key << "Name" << YAML::Value << pin.Name;
					out << YAML::Key << "Type" << YAML::Value << PinDataTypeToString(pin.DataType);
					out << YAML::EndMap;
				}
				out << YAML::EndSeq;
			}

			out << YAML::EndMap;
		}

		static void SerializePinValue(const PinValue& value, YAML::Emitter& out) {
			out << YAML::Key << "Value";
			std::visit([&](auto&& val) {
				using T = std::decay_t<decltype(val)>;
				if constexpr (std::is_same_v<T, std::monostate>) {
					out << YAML::Value << "null";
				}
				else if constexpr (std::is_same_v<T, bool>) {
					out << YAML::Value << val;
				}
				else if constexpr (std::is_same_v<T, int32_t>) {
					out << YAML::Value << val;
				}
				else if constexpr (std::is_same_v<T, float>) {
					out << YAML::Value << val;
				}
				else if constexpr (std::is_same_v<T, glm::vec2>) {
					out << YAML::Value << YAML::Flow
						<< YAML::BeginSeq << val.x << val.y << YAML::EndSeq;
				}
				else if constexpr (std::is_same_v<T, glm::vec3>) {
					out << YAML::Value << YAML::Flow
						<< YAML::BeginSeq << val.x << val.y << val.z << YAML::EndSeq;
				}
				else if constexpr (std::is_same_v<T, glm::vec4>) {
					out << YAML::Value << YAML::Flow
						<< YAML::BeginSeq << val.x << val.y << val.z << val.w << YAML::EndSeq;
				}
				else if constexpr (std::is_same_v<T, std::string>) {
					out << YAML::Value << val;
				}
				else {
					out << YAML::Value << "unsupported";
				}
			}, value);
		}

		static bool Deserialize(NodeGraph& graph, const YAML::Node& root) {
			auto graphNode = root["NodeGraph"];
			if (!graphNode) return false;

			graph.Clear();

			if (graphNode["Name"])
				graph.SetName(graphNode["Name"].as<std::string>());
			if (graphNode["ID"])
				graph.SetID(UUID(graphNode["ID"].as<uint64_t>()));

			// Parse domain
			if (graphNode["Domain"]) {
				std::string domainStr = graphNode["Domain"].as<std::string>();
				if (domainStr == "Shader") graph.SetDomain(GraphDomain::Shader);
				else if (domainStr == "Animation") graph.SetDomain(GraphDomain::Animation);
				else if (domainStr == "Audio") graph.SetDomain(GraphDomain::Audio);
				else if (domainStr == "Blueprint") graph.SetDomain(GraphDomain::Blueprint);
				else if (domainStr == "BehaviorTree") graph.SetDomain(GraphDomain::BehaviorTree);
				else if (domainStr == "Particle") graph.SetDomain(GraphDomain::Particle);
				else if (domainStr == "PostProcess") graph.SetDomain(GraphDomain::PostProcess);
			}

			NodeID maxNodeID = 0;
			PinID maxPinID = 0;
			LinkID maxLinkID = 0;

			// Load nodes
			if (graphNode["Nodes"]) {
				for (const auto& nodeData : graphNode["Nodes"]) {
					std::string typeName = nodeData["Type"].as<std::string>();
					
					auto node = NodeFactory::Get().CreateNode(typeName, graph);
					if (!node) {
						// Create a placeholder node if the type isn't registered
						node = CreateRef<Node>();
						node->TypeName = typeName;
						node->DisplayName = typeName + " (Unknown)";
						node->Status = NodeStatus::Error;
						node->StatusMessage = "Unknown node type: " + typeName;
					}

					node->ID = nodeData["ID"].as<int32_t>();
					maxNodeID = std::max(maxNodeID, node->ID);

					if (nodeData["Display"])
						node->DisplayName = nodeData["Display"].as<std::string>();

					if (nodeData["Position"]) {
						auto pos = nodeData["Position"];
						node->Position = glm::vec2(pos[0].as<float>(), pos[1].as<float>());
					}

					// Restore input pin values
					if (nodeData["Inputs"]) {
						for (size_t i = 0; i < nodeData["Inputs"].size() && i < node->Inputs.size(); i++) {
							auto& pinData = nodeData["Inputs"][i];
							auto& pin = node->Inputs[i];
							pin.ID = pinData["ID"].as<int32_t>();
							pin.OwnerNodeID = node->ID;
							maxPinID = std::max(maxPinID, pin.ID);

							if (pinData["Value"]) {
								pin.DefaultValue = DeserializePinValue(pinData["Value"], pin.DataType);
							}
						}
					}

					// Restore output pin IDs
					if (nodeData["Outputs"]) {
						for (size_t i = 0; i < nodeData["Outputs"].size() && i < node->Outputs.size(); i++) {
							auto& pinData = nodeData["Outputs"][i];
							node->Outputs[i].ID = pinData["ID"].as<int32_t>();
							node->Outputs[i].OwnerNodeID = node->ID;
							maxPinID = std::max(maxPinID, node->Outputs[i].ID);
						}
					}

					graph.AddNode(node);
				}
			}

			// Load links
			if (graphNode["Links"]) {
				for (const auto& linkData : graphNode["Links"]) {
					LinkID id = linkData["ID"].as<int32_t>();
					PinID startPin = linkData["StartPin"].as<int32_t>();
					PinID endPin = linkData["EndPin"].as<int32_t>();
					
					maxLinkID = std::max(maxLinkID, id);
					graph.AddLink(startPin, endPin);
				}
			}

			// Update ID counters
			graph.SetNextNodeID(maxNodeID + 1);
			graph.SetNextPinID(maxPinID + 1);
			graph.SetNextLinkID(maxLinkID + 1);

			graph.ClearDirty();
			return true;
		}

		static PinValue DeserializePinValue(const YAML::Node& valueNode, PinDataType type) {
			try {
				if (valueNode.IsNull() || (valueNode.IsScalar() && valueNode.as<std::string>() == "null"))
					return std::monostate{};

				switch (type) {
					case PinDataType::Bool:
						return valueNode.as<bool>();
					case PinDataType::Int:
						return valueNode.as<int32_t>();
					case PinDataType::Float:
						return valueNode.as<float>();
					case PinDataType::Vec2:
						return glm::vec2(valueNode[0].as<float>(), valueNode[1].as<float>());
					case PinDataType::Vec3:
					case PinDataType::Color3:
						return glm::vec3(valueNode[0].as<float>(), valueNode[1].as<float>(), valueNode[2].as<float>());
					case PinDataType::Vec4:
					case PinDataType::Color4:
						return glm::vec4(valueNode[0].as<float>(), valueNode[1].as<float>(),
										 valueNode[2].as<float>(), valueNode[3].as<float>());
					case PinDataType::String:
						return valueNode.as<std::string>();
					default:
						return std::monostate{};
				}
			}
			catch (...) {
				return std::monostate{};
			}
		}
	};

} // namespace Lunex::NodeGraph
