#pragma once

/**
 * @file Link.h
 * @brief Link (connection) between two pins in a node graph
 */

#include "NodeGraphTypes.h"

namespace Lunex::NodeGraph {

	struct Link {
		LinkID ID = InvalidLinkID;
		PinID StartPinID = InvalidPinID;  // Output pin
		PinID EndPinID = InvalidPinID;    // Input pin
		uint32_t Color = PackColor(200, 200, 200);

		Link() = default;
		Link(LinkID id, PinID startPin, PinID endPin)
			: ID(id), StartPinID(startPin), EndPinID(endPin) {}
	};

} // namespace Lunex::NodeGraph
