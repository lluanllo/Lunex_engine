#pragma once

/**
 * @file SceneState.h
 * @brief Editor scene state (legacy compatibility)
 * 
 * AAA Architecture: This header is DEPRECATED.
 * Use Scene/Core/SceneMode.h directly in new code.
 * 
 * This file exists only for backwards compatibility with existing editor code.
 */

// Simply include SceneMode - SceneState enum no longer exists here
// The editor code that uses SceneState will work because it matches SceneMode values
#include "Scene/Core/SceneMode.h"

// Note: SceneState was:
//   enum class SceneState { Edit = 0, Play = 1, Simulate = 2 };
// 
// SceneMode is:
//   enum class SceneMode : uint8_t { Edit = 0, Play = 1, Simulate = 2, Paused = 3 };
//
// The values are compatible, so existing code will work.
