#pragma once

/**
 * BulletPhysics.h - Wrapper for Bullet3 to avoid NodeArray conflict with Assimp
 * 
 * This header includes Bullet within a namespace to prevent symbol collision
 * with Assimp's NodeArray typedef.
 * 
 * Usage: Include this instead of <btBulletDynamicsCommon.h> in your code.
 */

// Push current namespace state
#pragma push_macro("NodeArray")
#undef NodeArray

// Include Bullet headers
#include <btBulletDynamicsCommon.h>
#include <BulletCollision/CollisionShapes/btShapeHull.h>

// Restore macro state
#pragma pop_macro("NodeArray")

// Bring Bullet types into Lunex namespace for convenience
namespace Lunex {
    // Common Bullet types
    using btVector3 = ::btVector3;
    using btQuaternion = ::btQuaternion;
    using btTransform = ::btTransform;
    using btMatrix3x3 = ::btMatrix3x3;
    
    using btCollisionShape = ::btCollisionShape;
    using btBoxShape = ::btBoxShape;
    using btSphereShape = ::btSphereShape;
    using btCapsuleShape = ::btCapsuleShape;
    using btCylinderShape = ::btCylinderShape;
    using btConeShape = ::btConeShape;
    using btConvexHullShape = ::btConvexHullShape;
    using btBvhTriangleMeshShape = ::btBvhTriangleMeshShape;
    using btCompoundShape = ::btCompoundShape;
    using btTriangleIndexVertexArray = ::btTriangleIndexVertexArray;
    
    using btRigidBody = ::btRigidBody;
    using btMotionState = ::btMotionState;
    using btDefaultMotionState = ::btDefaultMotionState;
    
    using btDiscreteDynamicsWorld = ::btDiscreteDynamicsWorld;
    using btCollisionConfiguration = ::btCollisionConfiguration;
    using btDefaultCollisionConfiguration = ::btDefaultCollisionConfiguration;
    using btCollisionDispatcher = ::btCollisionDispatcher;
    using btBroadphaseInterface = ::btBroadphaseInterface;
    using btDbvtBroadphase = ::btDbvtBroadphase;
    using btSequentialImpulseConstraintSolver = ::btSequentialImpulseConstraintSolver;
    
    using btIDebugDraw = ::btIDebugDraw;
}
