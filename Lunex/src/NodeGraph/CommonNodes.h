#pragma once

/**
 * @file CommonNodes.h
 * @brief Built-in node types shared across domains
 * 
 * These math and utility nodes are available in all graph domains.
 * Domain-specific nodes should be in their own files under
 * NodeGraph/Domains/<DomainName>/
 */

#include "NodeGraphCore.h"

namespace Lunex::NodeGraph {

	// ============================================================================
	// MATH NODES
	// ============================================================================

	class AddNode : public Node {
	public:
		void Init(NodeGraph& graph) {
			AddInput(graph.AllocatePinID(), "A", PinDataType::Float, 0.0f);
			AddInput(graph.AllocatePinID(), "B", PinDataType::Float, 0.0f);
			AddOutput(graph.AllocatePinID(), "Result", PinDataType::Float);
		}
	};

	class SubtractNode : public Node {
	public:
		void Init(NodeGraph& graph) {
			AddInput(graph.AllocatePinID(), "A", PinDataType::Float, 0.0f);
			AddInput(graph.AllocatePinID(), "B", PinDataType::Float, 0.0f);
			AddOutput(graph.AllocatePinID(), "Result", PinDataType::Float);
		}
	};

	class MultiplyNode : public Node {
	public:
		void Init(NodeGraph& graph) {
			AddInput(graph.AllocatePinID(), "A", PinDataType::Float, 1.0f);
			AddInput(graph.AllocatePinID(), "B", PinDataType::Float, 1.0f);
			AddOutput(graph.AllocatePinID(), "Result", PinDataType::Float);
		}
	};

	class DivideNode : public Node {
	public:
		void Init(NodeGraph& graph) {
			AddInput(graph.AllocatePinID(), "A", PinDataType::Float, 1.0f);
			AddInput(graph.AllocatePinID(), "B", PinDataType::Float, 1.0f);
			AddOutput(graph.AllocatePinID(), "Result", PinDataType::Float);
		}

		bool Validate() override {
			float b = GetPinValue<float>(Inputs[1].DefaultValue, 1.0f);
			if (!Inputs[1].IsConnected && b == 0.0f) {
				Status = NodeStatus::Warning;
				StatusMessage = "Division by zero";
				return false;
			}
			Status = NodeStatus::Valid;
			return true;
		}
	};

	class DotProductNode : public Node {
	public:
		void Init(NodeGraph& graph) {
			AddInput(graph.AllocatePinID(), "A", PinDataType::Vec3, glm::vec3(0.0f));
			AddInput(graph.AllocatePinID(), "B", PinDataType::Vec3, glm::vec3(0.0f));
			AddOutput(graph.AllocatePinID(), "Result", PinDataType::Float);
		}
	};

	class CrossProductNode : public Node {
	public:
		void Init(NodeGraph& graph) {
			AddInput(graph.AllocatePinID(), "A", PinDataType::Vec3, glm::vec3(0.0f));
			AddInput(graph.AllocatePinID(), "B", PinDataType::Vec3, glm::vec3(0.0f));
			AddOutput(graph.AllocatePinID(), "Result", PinDataType::Vec3);
		}
	};

	class NormalizeNode : public Node {
	public:
		void Init(NodeGraph& graph) {
			AddInput(graph.AllocatePinID(), "Vector", PinDataType::Vec3, glm::vec3(0.0f, 1.0f, 0.0f));
			AddOutput(graph.AllocatePinID(), "Result", PinDataType::Vec3);
		}
	};

	class LerpNode : public Node {
	public:
		void Init(NodeGraph& graph) {
			AddInput(graph.AllocatePinID(), "A", PinDataType::Float, 0.0f);
			AddInput(graph.AllocatePinID(), "B", PinDataType::Float, 1.0f);
			AddInput(graph.AllocatePinID(), "T", PinDataType::Float, 0.5f);
			AddOutput(graph.AllocatePinID(), "Result", PinDataType::Float);
		}
	};

	class ClampNode : public Node {
	public:
		void Init(NodeGraph& graph) {
			AddInput(graph.AllocatePinID(), "Value", PinDataType::Float, 0.0f);
			AddInput(graph.AllocatePinID(), "Min", PinDataType::Float, 0.0f);
			AddInput(graph.AllocatePinID(), "Max", PinDataType::Float, 1.0f);
			AddOutput(graph.AllocatePinID(), "Result", PinDataType::Float);
		}
	};

	class AbsNode : public Node {
	public:
		void Init(NodeGraph& graph) {
			AddInput(graph.AllocatePinID(), "Value", PinDataType::Float, 0.0f);
			AddOutput(graph.AllocatePinID(), "Result", PinDataType::Float);
		}
	};

	class PowerNode : public Node {
	public:
		void Init(NodeGraph& graph) {
			AddInput(graph.AllocatePinID(), "Base", PinDataType::Float, 2.0f);
			AddInput(graph.AllocatePinID(), "Exponent", PinDataType::Float, 2.0f);
			AddOutput(graph.AllocatePinID(), "Result", PinDataType::Float);
		}
	};

	class SinNode : public Node {
	public:
		void Init(NodeGraph& graph) {
			AddInput(graph.AllocatePinID(), "Angle", PinDataType::Float, 0.0f);
			AddOutput(graph.AllocatePinID(), "Result", PinDataType::Float);
		}
	};

	class CosNode : public Node {
	public:
		void Init(NodeGraph& graph) {
			AddInput(graph.AllocatePinID(), "Angle", PinDataType::Float, 0.0f);
			AddOutput(graph.AllocatePinID(), "Result", PinDataType::Float);
		}
	};

	// ============================================================================
	// VECTOR NODES
	// ============================================================================

	class MakeVec2Node : public Node {
	public:
		void Init(NodeGraph& graph) {
			AddInput(graph.AllocatePinID(), "X", PinDataType::Float, 0.0f);
			AddInput(graph.AllocatePinID(), "Y", PinDataType::Float, 0.0f);
			AddOutput(graph.AllocatePinID(), "Vector", PinDataType::Vec2);
		}
	};

	class MakeVec3Node : public Node {
	public:
		void Init(NodeGraph& graph) {
			AddInput(graph.AllocatePinID(), "X", PinDataType::Float, 0.0f);
			AddInput(graph.AllocatePinID(), "Y", PinDataType::Float, 0.0f);
			AddInput(graph.AllocatePinID(), "Z", PinDataType::Float, 0.0f);
			AddOutput(graph.AllocatePinID(), "Vector", PinDataType::Vec3);
		}
	};

	class MakeVec4Node : public Node {
	public:
		void Init(NodeGraph& graph) {
			AddInput(graph.AllocatePinID(), "X", PinDataType::Float, 0.0f);
			AddInput(graph.AllocatePinID(), "Y", PinDataType::Float, 0.0f);
			AddInput(graph.AllocatePinID(), "Z", PinDataType::Float, 0.0f);
			AddInput(graph.AllocatePinID(), "W", PinDataType::Float, 1.0f);
			AddOutput(graph.AllocatePinID(), "Vector", PinDataType::Vec4);
		}
	};

	class SplitVec3Node : public Node {
	public:
		void Init(NodeGraph& graph) {
			AddInput(graph.AllocatePinID(), "Vector", PinDataType::Vec3, glm::vec3(0.0f));
			AddOutput(graph.AllocatePinID(), "X", PinDataType::Float);
			AddOutput(graph.AllocatePinID(), "Y", PinDataType::Float);
			AddOutput(graph.AllocatePinID(), "Z", PinDataType::Float);
		}
	};

	class SplitVec4Node : public Node {
	public:
		void Init(NodeGraph& graph) {
			AddInput(graph.AllocatePinID(), "Vector", PinDataType::Vec4, glm::vec4(0.0f));
			AddOutput(graph.AllocatePinID(), "X", PinDataType::Float);
			AddOutput(graph.AllocatePinID(), "Y", PinDataType::Float);
			AddOutput(graph.AllocatePinID(), "Z", PinDataType::Float);
			AddOutput(graph.AllocatePinID(), "W", PinDataType::Float);
		}
	};

	// ============================================================================
	// UTILITY NODES
	// ============================================================================

	class ConstantFloatNode : public Node {
	public:
		void Init(NodeGraph& graph) {
			Flags = NodeFlags::IsConstant | NodeFlags::IsCompact;
			AddOutput(graph.AllocatePinID(), "Value", PinDataType::Float);
			// The value is stored in a "virtual" input for editing
			AddInput(graph.AllocatePinID(), "##value", PinDataType::Float, 0.0f);
			Inputs.back().IsHidden = true;
		}
	};

	class ConstantVec3Node : public Node {
	public:
		void Init(NodeGraph& graph) {
			Flags = NodeFlags::IsConstant;
			AddOutput(graph.AllocatePinID(), "Value", PinDataType::Vec3);
			AddInput(graph.AllocatePinID(), "##value", PinDataType::Vec3, glm::vec3(0.0f));
			Inputs.back().IsHidden = true;
		}
	};

	class ConstantColorNode : public Node {
	public:
		void Init(NodeGraph& graph) {
			Flags = NodeFlags::IsConstant;
			AddOutput(graph.AllocatePinID(), "Color", PinDataType::Color4);
			AddInput(graph.AllocatePinID(), "##value", PinDataType::Color4, glm::vec4(1.0f));
			Inputs.back().IsHidden = true;
		}
	};

	class TimeNode : public Node {
	public:
		void Init(NodeGraph& graph) {
			Flags = NodeFlags::IsConstant | NodeFlags::IsCompact;
			AddOutput(graph.AllocatePinID(), "Time", PinDataType::Float);
			AddOutput(graph.AllocatePinID(), "SinTime", PinDataType::Float);
			AddOutput(graph.AllocatePinID(), "CosTime", PinDataType::Float);
			AddOutput(graph.AllocatePinID(), "DeltaTime", PinDataType::Float);
		}
	};

	class CommentNode : public Node {
	public:
		void Init(NodeGraph& graph) {
			Flags = NodeFlags::IsComment;
			DisplayName = "Comment";
		}

		std::string CommentText = "Double-click to edit";
	};

	// ============================================================================
	// REGISTRATION HELPER
	// ============================================================================

	inline void RegisterCommonNodes() {
		auto& factory = NodeFactory::Get();

		// Math nodes - registered for all domains
		auto registerForAllDomains = [&](auto registerFunc) {
			for (uint8_t d = (uint8_t)GraphDomain::Shader; d < (uint8_t)GraphDomain::Count; d++) {
				registerFunc(static_cast<GraphDomain>(d));
			}
		};

		// We register common nodes globally under GraphDomain::None which means available everywhere
		factory.Register<AddNode>("Common::Add", "Add", "Math", GraphDomain::None, PackColor(60, 120, 60));
		factory.Register<SubtractNode>("Common::Subtract", "Subtract", "Math", GraphDomain::None, PackColor(60, 120, 60));
		factory.Register<MultiplyNode>("Common::Multiply", "Multiply", "Math", GraphDomain::None, PackColor(60, 120, 60));
		factory.Register<DivideNode>("Common::Divide", "Divide", "Math", GraphDomain::None, PackColor(60, 120, 60));
		factory.Register<DotProductNode>("Common::DotProduct", "Dot Product", "Math", GraphDomain::None, PackColor(60, 120, 60));
		factory.Register<CrossProductNode>("Common::CrossProduct", "Cross Product", "Math", GraphDomain::None, PackColor(60, 120, 60));
		factory.Register<NormalizeNode>("Common::Normalize", "Normalize", "Math", GraphDomain::None, PackColor(60, 120, 60));
		factory.Register<LerpNode>("Common::Lerp", "Lerp", "Math", GraphDomain::None, PackColor(60, 120, 60));
		factory.Register<ClampNode>("Common::Clamp", "Clamp", "Math", GraphDomain::None, PackColor(60, 120, 60));
		factory.Register<AbsNode>("Common::Abs", "Abs", "Math", GraphDomain::None, PackColor(60, 120, 60));
		factory.Register<PowerNode>("Common::Power", "Power", "Math", GraphDomain::None, PackColor(60, 120, 60));
		factory.Register<SinNode>("Common::Sin", "Sin", "Math/Trigonometry", GraphDomain::None, PackColor(60, 120, 60));
		factory.Register<CosNode>("Common::Cos", "Cos", "Math/Trigonometry", GraphDomain::None, PackColor(60, 120, 60));

		// Vector nodes
		factory.Register<MakeVec2Node>("Common::MakeVec2", "Make Vec2", "Vector", GraphDomain::None, PackColor(60, 60, 120));
		factory.Register<MakeVec3Node>("Common::MakeVec3", "Make Vec3", "Vector", GraphDomain::None, PackColor(60, 60, 120));
		factory.Register<MakeVec4Node>("Common::MakeVec4", "Make Vec4", "Vector", GraphDomain::None, PackColor(60, 60, 120));
		factory.Register<SplitVec3Node>("Common::SplitVec3", "Split Vec3", "Vector", GraphDomain::None, PackColor(60, 60, 120));
		factory.Register<SplitVec4Node>("Common::SplitVec4", "Split Vec4", "Vector", GraphDomain::None, PackColor(60, 60, 120));

		// Utility nodes
		factory.Register<ConstantFloatNode>("Common::ConstFloat", "Float", "Constants", GraphDomain::None, PackColor(100, 60, 60));
		factory.Register<ConstantVec3Node>("Common::ConstVec3", "Vec3", "Constants", GraphDomain::None, PackColor(100, 60, 60));
		factory.Register<ConstantColorNode>("Common::ConstColor", "Color", "Constants", GraphDomain::None, PackColor(100, 60, 60));
		factory.Register<TimeNode>("Common::Time", "Time", "Utility", GraphDomain::None, PackColor(100, 100, 60));
		factory.Register<CommentNode>("Common::Comment", "Comment", "Utility", GraphDomain::None, PackColor(80, 80, 80));
	}

} // namespace Lunex::NodeGraph
