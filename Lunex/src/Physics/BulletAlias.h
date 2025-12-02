#pragma once

// Temporary workaround for NodeArray conflict between Assimp and Bullet3
// Rename Bullet's NodeArray typedef via macros before including Bullet headers

// We avoid including Bullet headers directly here; instead, each .cpp that includes btBulletDynamicsCommon.h
// should include this header first.

// No-op unless Bullet internally defines NodeArray; this avoids conflict with Assimp's using NodeArray
#ifdef NodeArray
#undef NodeArray
#endif

// After including Bullet headers, we can restore or alias if needed (not required here)
