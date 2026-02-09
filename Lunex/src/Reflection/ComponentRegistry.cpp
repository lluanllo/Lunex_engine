#include "stpch.h"
#include "ComponentRegistry.h"
#include "Scene/Components.h"

namespace Lunex {

    void ComponentRegistry::Initialize() {
        if (m_Initialized) return;

        LNX_LOG_INFO("Initializing Component Registry...");

        // Register core components with EnTT Meta
        RegisterComponents<
            IDComponent,
            TagComponent,
            TransformComponent,
            RelationshipComponent
        >();

        // Register rendering components
        RegisterComponents<
            SpriteRendererComponent,
            CircleRendererComponent,
            MeshComponent,
            MaterialComponent,
            LightComponent,
            CameraComponent,
            EnvironmentComponent
        >();

        // Register physics components
        RegisterComponents<
            Rigidbody2DComponent,
            BoxCollider2DComponent,
            CircleCollider2DComponent,
            Rigidbody3DComponent,
            BoxCollider3DComponent,
            SphereCollider3DComponent,
            CapsuleCollider3DComponent,
            MeshCollider3DComponent
        >();

        // Register scripting components
        RegisterComponents<
            NativeScriptComponent,
            ScriptComponent
        >();

        m_Initialized = true;

        // Log registered types count
        size_t count = 0;
        ForEachType([&count](auto, auto) { ++count; });
        LNX_LOG_INFO("Component Registry initialized with {0} types", count);
    }

} // namespace Lunex
