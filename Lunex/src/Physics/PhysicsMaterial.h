#pragma once

namespace Lunex {

    /**
     * PhysicsMaterial - Physical material properties for rigid bodies
     * 
     * Properties:
     * - Mass: Weight of the object (kg) - 0 means static/kinematic
     * - Friction: Surface friction coefficient (0 = ice, 1 = rubber)
     * - Restitution: Bounciness (0 = no bounce, 1 = perfect bounce)
     * - LinearDamping: Air resistance for linear motion (0-1)
     * - AngularDamping: Air resistance for rotation (0-1)
     */
    struct PhysicsMaterial
    {
        // Basic properties
        float Mass = 1.0f;              // kg (0 = static/kinematic)
        float Friction = 0.5f;          // 0-1 (0 = ice, 1 = rubber)
        float Restitution = 0.3f;       // 0-1 (0 = no bounce, 1 = perfect bounce)
        
        // Damping (air resistance)
        float LinearDamping = 0.04f;    // 0-1 (higher = more air resistance)
        float AngularDamping = 0.05f;   // 0-1 (higher = more rotational resistance)

        // CCD (Continuous Collision Detection) - for fast moving objects
        bool UseCCD = false;
        float CcdMotionThreshold = 0.0f;    // Objects moving faster than this use CCD
        float CcdSweptSphereRadius = 0.0f;  // Sphere radius for swept test

        // Collision filtering
        bool IsTrigger = false;         // If true, no collision response (only callbacks)
        
        // Body type flags
        bool IsStatic = false;          // Immovable object (infinite mass)
        bool IsKinematic = false;       // Animated object (controlled by code, not physics)

        // Presets
        static PhysicsMaterial Static()
        {
            PhysicsMaterial mat;
            mat.Mass = 0.0f;
            mat.IsStatic = true;
            return mat;
        }

        static PhysicsMaterial Kinematic()
        {
            PhysicsMaterial mat;
            mat.Mass = 0.0f;
            mat.IsKinematic = true;
            return mat;
        }

        static PhysicsMaterial Wood()
        {
            PhysicsMaterial mat;
            mat.Mass = 10.0f;
            mat.Friction = 0.4f;
            mat.Restitution = 0.2f;
            return mat;
        }

        static PhysicsMaterial Metal()
        {
            PhysicsMaterial mat;
            mat.Mass = 50.0f;
            mat.Friction = 0.3f;
            mat.Restitution = 0.1f;
            return mat;
        }

        static PhysicsMaterial Rubber()
        {
            PhysicsMaterial mat;
            mat.Mass = 5.0f;
            mat.Friction = 0.9f;
            mat.Restitution = 0.8f;
            return mat;
        }

        static PhysicsMaterial Ice()
        {
            PhysicsMaterial mat;
            mat.Mass = 10.0f;
            mat.Friction = 0.05f;
            mat.Restitution = 0.1f;
            return mat;
        }

        static PhysicsMaterial Bouncy()
        {
            PhysicsMaterial mat;
            mat.Mass = 1.0f;
            mat.Friction = 0.5f;
            mat.Restitution = 0.95f;
            return mat;
        }
    };

} // namespace Lunex
