#pragma once

/**
 * Physics.h - Main include file for Lunex Physics Module (Bullet3)
 * 
 * Include this header to access all physics functionality:
 * - PhysicsCore (singleton manager)
 * - PhysicsWorld (Bullet world wrapper)
 * - Components (RigidBody, Collider)
 * - Utilities and configuration
 */

// ============================================
// WORKAROUND: NodeArray conflict between Bullet3 and Assimp
// Both libraries define NodeArray, causing C2371 errors
// This undefines any previous NodeArray before including Bullet
// ============================================
#ifdef NodeArray
#undef NodeArray
#endif

// Core
#include "PhysicsCore.h"
#include "PhysicsWorld.h"
#include "PhysicsConfig.h"
#include "PhysicsMaterial.h"
#include "PhysicsUtils.h"

// Components
#include "Components/RigidBodyComponent.h"
#include "Components/ColliderComponent.h"

// Systems
#include "CollisionCallback.h"
#include "CustomMotionState.h"
#include "PhysicsResourceManager.h"

// Debug
#include "Debug/PhysicsDebugDrawer.h"

// Re-define NodeArray for Bullet after all includes (if needed)
// Not required as Bullet uses it internally only
