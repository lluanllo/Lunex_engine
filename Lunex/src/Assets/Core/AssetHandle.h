#pragma once

/**
 * @file AssetHandle.h
 * @brief Type-safe asset references
 */

#include "Asset.h"

namespace Lunex {

	/**
	 * @struct AssetHandle
	 * @brief Lightweight asset reference (can be serialized)
	 */
	template<typename T>
	struct AssetHandle {
		UUID ID;
		
		AssetHandle() : ID(0) {}
		AssetHandle(UUID id) : ID(id) {}
		AssetHandle(const Ref<T>& asset) : ID(asset ? asset->GetID() : UUID(0)) {}
		
		bool IsValid() const { return ID != 0; }
		operator bool() const { return IsValid(); }
		operator UUID() const { return ID; }
		
		bool operator==(const AssetHandle& other) const { return ID == other.ID; }
		bool operator!=(const AssetHandle& other) const { return ID != other.ID; }
		
		// Get the actual asset (from registry)
		Ref<T> Get() const;
	};

} // namespace Lunex
