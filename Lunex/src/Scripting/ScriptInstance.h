#pragma once

/**
 * @file ScriptInstance.h
 * @brief AAA Architecture: Script instance storage with state management
 *
 * ScriptInstance holds the actual script object and metadata for
 * serialization, profiling, and hot-reload support.
 */

#include "ScriptComponents.h"
#include "../../Lunex-ScriptCore/src/LunexScriptingAPI.h"
#include "../../Lunex-ScriptCore/src/ScriptPlugin.h"
#include <memory>
#include <vector>
#include <string>

namespace Lunex {

    /**
     * @struct ScriptInstance
     * @brief Container for a loaded script instance
     */
    struct ScriptInstance {
        // ========== CORE DATA ==========

        // The loaded script plugin (owns the DLL handle and module)
        std::unique_ptr<ScriptPlugin> plugin;

        // Path to the source script file (.cpp)
        std::string sourcePath;

        // Path to the compiled DLL
        std::string dllPath;

        // Entity this script is attached to (for context injection)
        entt::entity ownerEntity = entt::null;

        // ========== STATE SERIALIZATION ==========

        // Serialized state for hot-reload
        std::vector<uint8_t> serializedState;

        // Property snapshots (name -> serialized value)
        struct PropertySnapshot {
            std::string name;
            VarType type;
            std::vector<uint8_t> data;
        };
        std::vector<PropertySnapshot> propertySnapshots;

        // ========== PROFILING ==========

        ScriptProfilingData profiling;

        // ========== METHODS ==========

        /**
         * @brief Check if the script is loaded and valid
         */
        bool IsLoaded() const {
            return plugin && plugin->IsLoaded();
        }

        /**
         * @brief Serialize current property state for hot-reload
         */
        void SerializeState() {
            if (!plugin || !plugin->IsLoaded()) return;

            propertySnapshots.clear();

            // Get public variables from the script
            IScriptModule* module = nullptr; // Can't access directly, would need API

            // For now, just clear - full implementation would iterate
            // through GetPublicVariables() and serialize each
        }

        /**
         * @brief Restore property state after hot-reload
         */
        void DeserializeState() {
            if (!plugin || !plugin->IsLoaded() || propertySnapshots.empty()) return;

            // Restore each property by name
            // Full implementation would match property names and restore values
        }

        /**
         * @brief Clear all cached state
         */
        void ClearState() {
            serializedState.clear();
            propertySnapshots.clear();
            profiling.Reset();
        }
    };

    /**
     * @struct ScriptInstancePool
     * @brief Object pool for script instances to reduce allocations
     */
    class ScriptInstancePool {
    public:
        /**
         * @brief Acquire a script instance from the pool
         * @return Unique ID and reference to the instance
         */
        std::pair<uint32_t, ScriptInstance*> Acquire() {
            uint32_t id;

            if (!m_FreeList.empty()) {
                id = m_FreeList.back();
                m_FreeList.pop_back();
                m_Instances[id] = ScriptInstance{};
            }
            else {
                id = m_NextId++;
                m_Instances[id] = ScriptInstance{};
            }

            return { id, &m_Instances[id] };
        }

        /**
         * @brief Release a script instance back to the pool
         */
        void Release(uint32_t id) {
            auto it = m_Instances.find(id);
            if (it != m_Instances.end()) {
                it->second = ScriptInstance{}; // Reset
                m_FreeList.push_back(id);
            }
        }

        /**
         * @brief Get an instance by ID
         */
        ScriptInstance* Get(uint32_t id) {
            auto it = m_Instances.find(id);
            return (it != m_Instances.end()) ? &it->second : nullptr;
        }

        /**
         * @brief Get an instance by ID (const)
         */
        const ScriptInstance* Get(uint32_t id) const {
            auto it = m_Instances.find(id);
            return (it != m_Instances.end()) ? &it->second : nullptr;
        }

        /**
         * @brief Clear all instances
         */
        void Clear() {
            m_Instances.clear();
            m_FreeList.clear();
            m_NextId = 1;
        }

        /**
         * @brief Get number of active instances
         */
        size_t GetActiveCount() const {
            return m_Instances.size() - m_FreeList.size();
        }

        /**
         * @brief Iterate over all active instances
         */
        template<typename Func>
        void ForEach(Func&& func) {
            for (auto& [id, instance] : m_Instances) {
                if (std::find(m_FreeList.begin(), m_FreeList.end(), id) == m_FreeList.end()) {
                    func(id, instance);
                }
            }
        }

    private:
        std::unordered_map<uint32_t, ScriptInstance> m_Instances;
        std::vector<uint32_t> m_FreeList;
        uint32_t m_NextId = 1;
    };

} // namespace Lunex
