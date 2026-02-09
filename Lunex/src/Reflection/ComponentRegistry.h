#pragma once

/**
 * @file ComponentRegistry.h
 * @brief AAA Architecture: Component reflection system using EnTT Meta
 *
 * Provides compile-time reflection for native engine components.
 * Works in conjunction with RTTR for script classes.
 */

#include "entt.hpp"
#include <string_view>
#include <functional>

namespace Lunex {

    /**
     * @brief Traits for auto-registering components with EnTT Meta
     */
    template<typename Component>
    struct ComponentTraits {
        static constexpr auto name = entt::type_name<Component>::value();
        static constexpr entt::id_type id = entt::type_hash<Component>::value();

        /**
         * @brief Register component with EnTT Meta system
         */
        static void Register() {
            using namespace entt::literals;

            entt::meta<Component>()
                .type(id)
                .template ctor<>();

            // Call RegisterFields if the component defines it
            if constexpr (requires { Component::RegisterMetaFields(); }) {
                Component::RegisterMetaFields();
            }
        }
    };

    /**
     * @brief Register multiple components at once
     */
    template<typename... Components>
    void RegisterComponents() {
        (ComponentTraits<Components>::Register(), ...);
    }

    /**
     * @brief ComponentRegistry singleton for managing component metadata
     */
    class ComponentRegistry {
    public:
        static ComponentRegistry& Get() {
            static ComponentRegistry instance;
            return instance;
        }

        /**
         * @brief Initialize all component registrations
         * Called once at engine startup
         */
        void Initialize();

        /**
         * @brief Check if a component type is registered
         */
        bool IsRegistered(entt::id_type typeId) const {
            return entt::resolve(typeId).operator bool();
        }

        /**
         * @brief Get component type info by ID
         */
        entt::meta_type GetType(entt::id_type typeId) const {
            return entt::resolve(typeId);
        }

        /**
         * @brief Get component type info by name
         */
        entt::meta_type GetTypeByName(std::string_view name) const {
            return entt::resolve(entt::hashed_string{ name.data() });
        }

        /**
         * @brief Iterate over all registered component types
         */
        template<typename Func>
        void ForEachType(Func&& func) const {
            for (auto [id, type] : entt::resolve()) {
                func(id, type);
            }
        }

    private:
        ComponentRegistry() = default;
        bool m_Initialized = false;
    };

} // namespace Lunex

// ========================================================================
// MACROS FOR COMPONENT FIELD REGISTRATION
// ========================================================================

/**
 * @brief Macro to declare a component with reflection support
 * Use inside component struct definition
 */
#define LUNEX_COMPONENT(ComponentName) \
    static constexpr const char* GetTypeName() { return #ComponentName; } \
    static void RegisterMetaFields()

 /**
  * @brief Macro to register a field within RegisterMetaFields
  * Usage: LUNEX_FIELD(fieldName);
  */
#define LUNEX_FIELD(FieldName) \
    entt::meta<std::decay_t<decltype(*this)>>() \
        .data<&std::decay_t<decltype(*this)>::FieldName>(entt::hashed_string{#FieldName})

  /**
   * @brief Macro to register a field with custom name
   */
#define LUNEX_FIELD_NAMED(FieldName, DisplayName) \
    entt::meta<std::decay_t<decltype(*this)>>() \
        .data<&std::decay_t<decltype(*this)>::FieldName>(entt::hashed_string{DisplayName})
