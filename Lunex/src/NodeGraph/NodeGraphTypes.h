#pragma once

/**
 * @file NodeGraphTypes.h
 * @brief Core type definitions for the Lunex Node Graph System
 * 
 * This file defines the fundamental types used across all node graph domains:
 * - Shader graphs
 * - Animation graphs (state machines, blend trees)
 * - Audio graphs (mixer chains, DSP)
 * - Blueprint/visual scripting graphs
 * - Behavior trees (future)
 * 
 * Architecture: Domain-agnostic base layer. Each domain (Shader, Animation, etc.)
 * extends these types with specialized nodes and pin types.
 */

#include "Core/Core.h"
#include "Core/UUID.h"

#include <string>
#include <vector>
#include <unordered_map>
#include <functional>
#include <variant>
#include <any>
#include <optional>
#include <cstdint>

#include <glm/glm.hpp>

namespace Lunex::NodeGraph {

	// ============================================================================
	// ID TYPES
	// ============================================================================

	using NodeID = int32_t;
	using PinID = int32_t;
	using LinkID = int32_t;

	constexpr NodeID InvalidNodeID = -1;
	constexpr PinID InvalidPinID = -1;
	constexpr LinkID InvalidLinkID = -1;

	// ============================================================================
	// GRAPH DOMAIN
	// ============================================================================

	enum class GraphDomain : uint8_t {
		None = 0,
		Shader,
		Animation,
		Audio,
		Blueprint,
		BehaviorTree,
		Particle,
		PostProcess,

		Count
	};

	inline const char* GraphDomainToString(GraphDomain domain) {
		switch (domain) {
			case GraphDomain::Shader:       return "Shader";
			case GraphDomain::Animation:    return "Animation";
			case GraphDomain::Audio:        return "Audio";
			case GraphDomain::Blueprint:    return "Blueprint";
			case GraphDomain::BehaviorTree: return "BehaviorTree";
			case GraphDomain::Particle:     return "Particle";
			case GraphDomain::PostProcess:  return "PostProcess";
			default:                        return "Unknown";
		}
	}

	// ============================================================================
	// PIN DATA TYPES - What kind of data flows through a pin
	// ============================================================================

	enum class PinDataType : uint16_t {
		None = 0,

		// Scalar types
		Bool,
		Int,
		Float,
		
		// Vector types
		Vec2,
		Vec3,
		Vec4,
		
		// Matrix types
		Mat3,
		Mat4,
		
		// Texture/Sampler types (shader domain)
		Texture2D,
		TextureCube,
		Sampler,
		
		// Color
		Color3,
		Color4,

		// String
		String,

		// Flow control (blueprint domain)
		Flow,

		// Object/reference types
		Object,
		Entity,
		
		// Audio types
		AudioStream,
		AudioParam,

		// Animation types
		Pose,
		AnimClip,
		BlendSpace,

		// Wildcard - can connect to anything compatible
		Any,

		// Custom domain-specific types start here
		UserDefined = 1000
	};

	inline const char* PinDataTypeToString(PinDataType type) {
		switch (type) {
			case PinDataType::Bool:       return "Bool";
			case PinDataType::Int:        return "Int";
			case PinDataType::Float:      return "Float";
			case PinDataType::Vec2:       return "Vec2";
			case PinDataType::Vec3:       return "Vec3";
			case PinDataType::Vec4:       return "Vec4";
			case PinDataType::Mat3:       return "Mat3";
			case PinDataType::Mat4:       return "Mat4";
			case PinDataType::Texture2D:  return "Texture2D";
			case PinDataType::TextureCube:return "TextureCube";
			case PinDataType::Sampler:    return "Sampler";
			case PinDataType::Color3:     return "Color3";
			case PinDataType::Color4:     return "Color4";
			case PinDataType::String:     return "String";
			case PinDataType::Flow:       return "Flow";
			case PinDataType::Object:     return "Object";
			case PinDataType::Entity:     return "Entity";
			case PinDataType::AudioStream:return "AudioStream";
			case PinDataType::AudioParam: return "AudioParam";
			case PinDataType::Pose:       return "Pose";
			case PinDataType::AnimClip:   return "AnimClip";
			case PinDataType::BlendSpace: return "BlendSpace";
			case PinDataType::Any:        return "Any";
			default:                      return "Unknown";
		}
	}

	// ============================================================================
	// PIN DIRECTION
	// ============================================================================

	enum class PinDirection : uint8_t {
		Input = 0,
		Output
	};

	// ============================================================================
	// PIN VALUE - Runtime value storage for pin defaults and evaluation
	// ============================================================================

	using PinValue = std::variant<
		std::monostate,     // None
		bool,               // Bool
		int32_t,            // Int
		float,              // Float
		glm::vec2,          // Vec2
		glm::vec3,          // Vec3 / Color3
		glm::vec4,          // Vec4 / Color4
		glm::mat3,          // Mat3
		glm::mat4,          // Mat4
		std::string          // String / path references
	>;

	// Helper to get a typed value from PinValue
	template<typename T>
	inline T GetPinValue(const PinValue& value, const T& defaultVal = T{}) {
		if (auto* ptr = std::get_if<T>(&value))
			return *ptr;
		return defaultVal;
	}

	// ============================================================================
	// NODE CATEGORY - For organizing nodes in creation menus
	// ============================================================================

	struct NodeCategory {
		std::string Name;
		std::string Icon;
		uint32_t Color = 0xFF404040;
		std::vector<NodeCategory> SubCategories;
	};

	// ============================================================================
	// NODE FLAGS
	// ============================================================================

	enum class NodeFlags : uint32_t {
		None          = 0,
		IsOutput      = 1 << 0,   // This is an output/result node
		IsInput       = 1 << 1,   // This is a graph input/parameter node
		IsConstant    = 1 << 2,   // Value doesn't change
		HasPreview    = 1 << 3,   // Can render a preview
		IsComment     = 1 << 4,   // Comment/reroute node
		IsCompact     = 1 << 5,   // Render in compact mode
		NoDelete      = 1 << 6,   // Cannot be deleted
		NoDuplicate   = 1 << 7,   // Cannot be duplicated
		IsCollapsed   = 1 << 8,   // Currently collapsed
	};

	inline NodeFlags operator|(NodeFlags a, NodeFlags b) {
		return static_cast<NodeFlags>(static_cast<uint32_t>(a) | static_cast<uint32_t>(b));
	}
	inline NodeFlags operator&(NodeFlags a, NodeFlags b) {
		return static_cast<NodeFlags>(static_cast<uint32_t>(a) & static_cast<uint32_t>(b));
	}
	inline NodeFlags& operator|=(NodeFlags& a, NodeFlags b) { return a = a | b; }
	inline bool HasFlag(NodeFlags flags, NodeFlags flag) {
		return (static_cast<uint32_t>(flags) & static_cast<uint32_t>(flag)) != 0;
	}

	// ============================================================================
	// COMPILATION/EVALUATION STATUS
	// ============================================================================

	enum class NodeStatus : uint8_t {
		None = 0,
		Valid,
		Warning,
		Error
	};

	struct NodeMessage {
		NodeStatus Level = NodeStatus::None;
		std::string Text;
	};

	// ============================================================================
	// COLOR UTILITY
	// ============================================================================

	// Pack RGBA into uint32_t (ABGR layout, compatible with ImGui's IM_COL32)
	inline constexpr uint32_t PackColor(uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255) {
		return ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)g << 8) | (uint32_t)r;
	}

} // namespace Lunex::NodeGraph
