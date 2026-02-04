#pragma once

/**
 * @file Scripting.h
 * @brief AAA Architecture: Main include for the Lunex Scripting System
 * 
 * Include this header to access the full scripting system API:
 * - ScriptSystem: ISceneSystem implementation for ECS integration
 * - ScriptCompiler: Automated C++ script compilation
 * - ScriptHotReloader: File watching and hot-reload support
 * - ScriptInstance: Script state management
 * - ScriptComponents: POD components for ECS
 * 
 * For DLL script development, include LunexScriptingAPI.h instead.
 */

// Core scripting components
#include "ScriptComponents.h"
#include "ScriptInstance.h"
#include "ScriptSystem.h"
#include "ScriptingEngine.h"

// Compilation and hot-reload
#include "ScriptCompiler.h"
#include "ScriptHotReloader.h"

// Utilities
#include "DLLLoader.h"

namespace Lunex {

    /**
     * @brief Initialize the scripting subsystem
     * 
     * Call this once at engine startup before using any scripting features.
     * Detects compiler installation and sets up default paths.
     */
    inline void InitializeScripting() {
        // Detect Visual Studio / compiler
        ScriptCompiler compiler;
        if (!compiler.DetectVisualStudio()) {
            LNX_LOG_WARN("[Scripting] Visual Studio not detected. Script compilation will not be available.");
        }
    }

} // namespace Lunex

/* ============================================================================
   AAA SCRIPTING SYSTEM ARCHITECTURE OVERVIEW
   ============================================================================

The Lunex Scripting System provides a Unity/Unreal-style scripting experience
with the following key features:

1. HOT-RELOAD SUPPORT
   - Scripts can be modified and recompiled while the game is running
   - Script state (properties) is preserved across reloads
   - Automatic file change detection

2. DATA-ORIENTED DESIGN
   - ScriptMetadata is a tiny POD component (8 bytes) for cache efficiency
   - Actual script instances are stored separately for better memory layout
   - Batch processing support for updating many scripts efficiently

3. REFLECTION SYSTEM
   - Public script variables can be exposed to the editor
   - Properties support types: float, int, bool, Vec2, Vec3, string
   - Range constraints and tooltips for editor UI

4. PHYSICS INTEGRATION
   - OnCollisionEnter/Stay/Exit callbacks
   - OnTriggerEnter/Stay/Exit callbacks
   - Full Rigidbody2D and Rigidbody3D API access

5. PROFILING
   - Per-script execution time tracking
   - Average, max, and total update time metrics
   - Frame-level script overhead measurement

SYSTEM HIERARCHY:

    Scene
      ??? ScriptSystem (ISceneSystem)
            ??? ScriptInstancePool
            ?     ??? ScriptInstance[]
            ?           ??? ScriptPlugin (DLL handle)
            ?           ??? State serialization
            ?           ??? Profiling data
            ??? ScriptingEngine
            ?     ??? EngineContext (API callbacks)
            ??? ScriptCompiler
            ??? ScriptHotReloader

LIFECYCLE:

    1. Scene::OnRuntimeStart()
       ??? ScriptSystem::OnRuntimeStart()
             ??? Compile all ScriptComponents (parallel)
             ??? Load DLLs
             ??? Call OnPlayModeEnter on each script

    2. Scene::OnUpdateRuntime(ts)
       ??? ScriptSystem::OnUpdate(ts)
             ??? For each active script:
                   ??? Prepare context (entity, deltaTime)
                   ??? Call Update()
                   ??? Record profiling

    3. Scene::OnRuntimeStop()
       ??? ScriptSystem::OnRuntimeStop()
             ??? Call OnPlayModeExit on each script
             ??? Unload all DLLs
             ??? Clear instance pool

   ============================================================================
*/
