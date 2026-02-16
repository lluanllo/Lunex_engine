#pragma once

/**
 * @file Pin.h
 * @brief Pin (attribute) definition for the node graph system
 * 
 * Pins are the connection points on nodes. They define:
 * - Data type that can flow through them
 * - Direction (input/output)
 * - Default values
 * - Connection constraints
 */

#include "NodeGraphTypes.h"

namespace Lunex::NodeGraph {

	class Pin {
	public:
		Pin() = default;
		Pin(PinID id, const std::string& name, PinDataType dataType, PinDirection direction)
			: ID(id), Name(name), DataType(dataType), Direction(direction) {}

		// ========== IDENTIFICATION =========
		PinID ID = InvalidPinID;
		NodeID OwnerNodeID = InvalidNodeID;
		std::string Name;

		// ========== TYPE INFO ==========
		PinDataType DataType = PinDataType::None;
		PinDirection Direction = PinDirection::Input;

		// ========== DEFAULT VALUE ==========
		PinValue DefaultValue;

		// ========== STATE ==========
		bool IsConnected = false;
		bool IsHidden = false;

		// ========== HELPERS ==========
		
		bool IsInput() const { return Direction == PinDirection::Input; }
		bool IsOutput() const { return Direction == PinDirection::Output; }
		
		bool CanConnectTo(const Pin& other) const {
			// Must be different directions
			if (Direction == other.Direction) return false;
			// Must not be same node
			if (OwnerNodeID == other.OwnerNodeID) return false;
			// Type compatibility
			return AreTypesCompatible(DataType, other.DataType);
		}

		static bool AreTypesCompatible(PinDataType a, PinDataType b) {
			if (a == b) return true;
			if (a == PinDataType::Any || b == PinDataType::Any) return true;

			// Float can promote to Vec2/3/4
			if (a == PinDataType::Float && (b == PinDataType::Vec2 || b == PinDataType::Vec3 || b == PinDataType::Vec4))
				return true;
			if (b == PinDataType::Float && (a == PinDataType::Vec2 || a == PinDataType::Vec3 || a == PinDataType::Vec4))
				return true;

			// Color3 <-> Vec3, Color4 <-> Vec4
			if ((a == PinDataType::Color3 && b == PinDataType::Vec3) ||
				(a == PinDataType::Vec3 && b == PinDataType::Color3))
				return true;
			if ((a == PinDataType::Color4 && b == PinDataType::Vec4) ||
				(a == PinDataType::Vec4 && b == PinDataType::Color4))
				return true;

			// Int <-> Float implicit conversion
			if ((a == PinDataType::Int && b == PinDataType::Float) ||
				(a == PinDataType::Float && b == PinDataType::Int))
				return true;

			return false;
		}

		// Get a color for the pin type (used by UI)
		static uint32_t GetTypeColor(PinDataType type) {
			switch (type) {
				case PinDataType::Bool:        return PackColor(200, 50, 50);
				case PinDataType::Int:         return PackColor(50, 180, 220);
				case PinDataType::Float:       return PackColor(150, 200, 50);
				case PinDataType::Vec2:        return PackColor(100, 220, 100);
				case PinDataType::Vec3:        return PackColor(100, 100, 220);
				case PinDataType::Vec4:        return PackColor(180, 100, 220);
				case PinDataType::Color3:      return PackColor(255, 200, 50);
				case PinDataType::Color4:      return PackColor(255, 180, 50);
				case PinDataType::Mat3:        return PackColor(180, 180, 100);
				case PinDataType::Mat4:        return PackColor(200, 200, 120);
				case PinDataType::Texture2D:   return PackColor(220, 100, 100);
				case PinDataType::TextureCube: return PackColor(200, 120, 120);
				case PinDataType::Sampler:     return PackColor(180, 140, 100);
				case PinDataType::String:      return PackColor(220, 100, 220);
				case PinDataType::Flow:        return PackColor(255, 255, 255);
				case PinDataType::Object:      return PackColor(50, 150, 200);
				case PinDataType::Entity:      return PackColor(50, 200, 150);
				case PinDataType::AudioStream: return PackColor(255, 150, 50);
				case PinDataType::AudioParam:  return PackColor(255, 180, 80);
				case PinDataType::Pose:        return PackColor(50, 200, 255);
				case PinDataType::AnimClip:    return PackColor(100, 180, 255);
				case PinDataType::BlendSpace:  return PackColor(80, 160, 240);
				case PinDataType::Any:         return PackColor(200, 200, 200);
				default:                       return PackColor(128, 128, 128);
			}
		}
	};

} // namespace Lunex::NodeGraph
