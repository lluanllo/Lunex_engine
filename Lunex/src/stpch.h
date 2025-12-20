#pragma once

#include <iostream>
#include <memory>
#include <utility>
#include <algorithm>
#include <functional>

#include <string>
#include <sstream>
#include <array>
#include <vector>
#include <unordered_map>
#include <unordered_set>

// ============================================
// GLM - Include FIRST to prevent forward declaration conflicts
// ============================================
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Log/Log.h"

#include "Debug/Instrumentor.h"

// ============================================
// WORKAROUND: NodeArray conflict between Bullet3 and Assimp
// Both define NodeArray typedef, causing C2371 errors
// Solution: Undef after Assimp if Bullet is included later
// ============================================

#ifdef LNX_PLATFORM_WINDOWS
	#include <Windows.h>
#endif