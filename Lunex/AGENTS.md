# AGENTS.md — Lunex Engine: Reglas y Convenciones de Desarrollo

> Este documento define las restricciones, convenciones y normas que el agente de IA **debe cumplir siempre** al trabajar con el código de Lunex Engine. Cualquier propuesta que las viole debe ser rechazada o cuestionada antes de implementarse.

---

## 1. Idioma y Comunicación

- **Responder siempre en español**, independientemente del idioma del código o de la pregunta.
- Antes de implementar cualquier cambio, **explicar qué se va a hacer y por qué**, con justificación técnica.
- Si una propuesta del usuario parece incorrecta, subóptima o incompatible con la arquitectura del engine, **el agente debe señalarlo, cuestionarlo y proponer alternativas** antes de tocar el código. No se implementa nada hasta que haya acuerdo.
- Todo cambio en el código debe estar **justificado explícitamente**: qué problema resuelve, por qué se eligió esa solución y qué impacto tiene en el sistema.
- Nunca hacer cambios en el VCS (no commits, no branches, no push, no tags). La gestión de Git la hace exclusivamente el propietario del repositorio.

---

## 2. Estructura de Proyectos de la Solución

La solución `Lunex-Engine.sln` contiene exactamente estos proyectos:

| Proyecto | Tipo | Descripción |
|---|---|---|
| `Lunex` | `StaticLib` | Núcleo del engine (toda la lógica, RHI, renderer, ECS…) |
| `Lunex-Editor` | `ConsoleApp` | Editor visual; depende de `Lunex` |
| `Lunex-ScriptCore` | `StaticLib` | Biblioteca base para scripts C++ de usuario |
| `Sandbox` | `ConsoleApp` | Proyecto de pruebas; depende de `Lunex` |
| Dependencias | `StaticLib` | GLFW, Glad, ImGui, yaml-cpp, Box2D, Bullet3 |

### Reglas de dependencia:
- `Lunex-Editor` y `Sandbox` **solo** pueden incluir cabeceras de `Lunex/src/` y `vendor/`.
- `Lunex` no puede depender de `Lunex-Editor` ni de `Sandbox`.
- `Lunex-ScriptCore` puede incluir cabeceras de `Lunex/src/` pero no el contrario (evitar ciclos).
- Todo código nuevo del engine va en `Lunex/src/`. Todo panel o herramienta de editor va en `Lunex-Editor/src/`.

---

## 3. Arquitectura RHI — Regla de Oro

El engine usa una capa de abstracción de hardware de renderizado (**RHI — Rendering Hardware Interface**) localizada en `Lunex/src/RHI/`. Esta abstracción es **obligatoria**.

### Prohibido absolutamente:
- Llamar funciones OpenGL (`gl*`) directamente fuera de `Lunex/src/RHI/OpenGL/`.
- Incluir `<glad/glad.h>` o cualquier cabecera OpenGL/Vulkan fuera del directorio RHI.
- Crear VBOs, VAOs, FBOs, shaders, texturas, etc. con la API gráfica directamente.

### Correcto:
- Usar los tipos y métodos expuestos por `RHICommandList`, `RHIBuffer`, `RHITexture`, `RHIFramebuffer`, `RHIPipeline`, `RHIDevice`, etc.
- Todo renderizado pasa por `RHICommandList::Begin()` / comandos / `RHICommandList::End()`.
- Las implementaciones concretas para OpenGL viven en `Lunex/src/RHI/OpenGL/` y son los únicos archivos que pueden contener código OpenGL raw.

### Punteros inteligentes en RHI:
- Usar `Ref<T>` (`std::shared_ptr`) para recursos compartidos.
- Usar `Scope<T>` (`std::unique_ptr`) para recursos de propiedad exclusiva.
- Crear con `CreateRef<T>(...)` y `CreateScope<T>(...)` (definidos en `Core/Core.h`).

---

## 4. Sistema de Shaders

### Formato de archivo unificado (un único `.glsl` por shader):
Los shaders de Lunex usan un **único archivo `.glsl`** que contiene todos los stages del shader separados por `#ifdef`. El preprocesador inyecta el `#define` adecuado antes de compilar cada stage.

```glsl
#version 450 core

#ifdef VERTEX
// ... código del vertex shader ...
#elif defined(FRAGMENT)
// ... código del fragment shader ...
#elif defined(GEOMETRY)
// ... código del geometry shader ...
#elif defined(COMPUTE)
// ... código del compute shader ...
#endif
```

### Stages soportados y su `#define`:
| Stage | Define a inyectar | Tipo GL |
|---|---|---|
| Vertex | `#define VERTEX` | `GL_VERTEX_SHADER` |
| Fragment | `#define FRAGMENT` | `GL_FRAGMENT_SHADER` |
| Geometry | `#define GEOMETRY` | `GL_GEOMETRY_SHADER` |
| Compute | `#define COMPUTE` | `GL_COMPUTE_SHADER` |

> **Regla:** Si se añade soporte para geometry shaders en `OpenGLRHIShader`, seguir exactamente el mismo patrón que vertex/fragment: inyectar `#define GEOMETRY` en `InsertDefineAfterVersion()` y añadir `GL_GEOMETRY_SHADER` como entrada en todos los mapas de stages.

### Caché de shaders — regla crítica:
- Los shaders compilados a SPIR-V se cachean en `assets/cache/shader/rhi/`.
- Hay **dos niveles** de caché: Vulkan SPIR-V (`.cached_vulkan.<stage>`) y OpenGL SPIR-V (`.cached_opengl.<stage>`).
- **Si se modifica el código fuente de un shader `.glsl`, hay que borrar los archivos de caché correspondientes** para que se recompile. La función `OpenGLRHIShader::Reload()` hace esto automáticamente, pero si el shader se elimina y se recrea (o se cambia la ruta) hay que borrar la caché manualmente.
- Localización de la caché: `<directorio de trabajo>/assets/cache/shader/rhi/`.
- El método correcto para forzar la recompilación desde código es llamar `shader->Reload()`.
- **Nunca** eliminar la carpeta `assets/cache/` completa sin avisar — podría haber caché de texturas también.

### Creación de shaders:
```cpp
// ? Correcto — siempre a través del factory RHI o Shader::Create
Ref<Shader> myShader = Shader::Create("assets/shaders/MyShader.glsl");
Ref<Shader> computeShader = Shader::CreateCompute("assets/shaders/MyCompute.glsl");

// ? Incorrecto — nunca instanciar OpenGLRHIShader directamente fuera del RHI
auto s = new OpenGLRHIShader("...");
```

### Ubicación de los shaders:
- Todos los shaders del editor/engine en: `Lunex-Editor/assets/shaders/`
- El directorio de trabajo en debug/release del editor apunta a `$(SolutionDir)/Lunex-Editor/`

---

## 5. Framework de UI — Lunex UI (Editor)

Todo el código de interfaz de usuario del editor (`Lunex-Editor/`) **debe usar el framework `Lunex::UI`**, nunca ImGui directamente.

### Prohibido absolutamente:
- Llamar `ImGui::Button(...)`, `ImGui::Text(...)`, `ImGui::Begin(...)`, etc. directamente en paneles o componentes del editor.
- Incluir `<imgui.h>` en paneles del editor salvo que sea para tipos de bajo nivel estrictamente necesarios (p. ej. `ImGuiWindowFlags`), y siempre con justificación.
- Usar `ImGui::PushStyleColor` / `ImGui::PushStyleVar` directamente — usar `ScopedColor` y `ScopedStyle`.

### Archivos del framework (en `Lunex-Editor/src/UI/`):

| Archivo | Contenido |
|---|---|
| `UICore.h` | `Color`, `Size`, `ScopedColor`, `ScopedStyle`, `ScopedID`, `ScopedDisabled`, `ScopedFont`, constantes de spacing, variantes de botón/texto/input, payloads drag&drop |
| `UIComponents.h` | Agrega todos los controles: `UIText`, `UIButton`, `UIInput`, `UIProperty`, `UIImage`, `UIState`, `LogOutput`, `ToolbarButton`, `StatItem` |
| `UILayout.h` | Paneles, hijos, cards, secciones, grids, tabs, árboles, popups, modales, menús, tooltips |
| `Controls/UIText.h` | Texto con variantes semánticas |
| `Controls/UIButton.h` | Botones con variantes y tamaños |
| `Controls/UIInput.h` | Inputs de texto, numéricos, etc. |
| `Controls/UIProperty.h` | Grid de propiedades (label:valor) |
| `Controls/UIImage.h` | Imágenes y texturas |
| `Controls/UIState.h` | Estado y utilidades |
| `Components/LogOutput.h` | Panel de consola/log |
| `Components/ToolbarButton.h` | Botón de icono para toolbar |
| `Components/StatItem.h` | Item de estadísticas |

### Hoja de estilos actual de Lunex UI:

#### Paleta de colores (`UI::Colors::*`):

| Función | Hex / valor | Uso |
|---|---|---|
| `Primary()` | `#0EA5C4` | Acento principal teal/cyan |
| `PrimaryHover()` | `#22BDD8` | Hover del acento |
| `PrimaryActive()` | `#0990AD` | Pressed del acento |
| `Success()` | `#4CAF50` | Verde material |
| `Warning()` | `#FFA726` | Ámbar |
| `Danger()` | `#EF5350` | Rojo material |
| `Info()` | `#42A5F5` | Azul claro |
| `TextPrimary()` | `(0.88, 0.90, 0.92)` | Texto principal (casi blanco, tono frío) |
| `TextSecondary()` | `(0.50, 0.54, 0.58)` | Texto secundario (gris medio) |
| `TextMuted()` | `(0.34, 0.38, 0.42)` | Texto atenuado |
| `TextDisabled()` | `(0.26, 0.28, 0.32)` | Texto deshabilitado |
| `BgDark()` | `#151A21` | Fondo más oscuro |
| `BgMedium()` | `#1A2028` | Fondo principal de paneles |
| `BgLight()` | `#212830` | Paneles elevados |
| `BgCard()` | `#1E2530` | Fondo de cards |
| `BgHover()` | `#252D38` | Hover de items |
| `Border()` | `#1A2028` | Borde oscuro |
| `BorderLight()` | `#2A3340` | Borde sutil |
| `BorderFocus()` | `(0.05, 0.65, 0.77, 0.50)` | Glow teal en foco |
| `AxisX()` | `(0.89, 0.22, 0.21)` | Rojo — eje X |
| `AxisY()` | `(0.27, 0.75, 0.27)` | Verde — eje Y |
| `AxisZ()` | `(0.22, 0.46, 0.93)` | Azul — eje Z |
| `Selected()` | `(0.05, 0.65, 0.77, 0.18)` | Selección teal semitransparente |
| `Shadow()` | `(0, 0, 0, 0.50)` | Sombra |

#### Constantes de spacing (`UI::SpacingValues::*`):

| Constante | Valor | Uso típico |
|---|---|---|
| `XS` | 2px | Espaciado mínimo |
| `SM` | 4px | Espaciado pequeño |
| `MD` | 8px | Espaciado estándar |
| `LG` | 12px | Espaciado grande |
| `XL` | 16px | Espaciado muy grande |
| `XXL` | 24px | Secciones |
| `IconSM/MD/LG/XL` | 16/20/24/32px | Tamaños de iconos |
| `ButtonHeight` | 28px | Altura de botón estándar |
| `ButtonHeightLG` | 35px | Altura de botón grande |
| `InputHeight` | 24px | Altura de input |
| `ThumbnailSM/MD/LG/XL` | 48/64/96/128px | Tamaños de thumbnail |
| `PropertyLabelWidth` | 120px | Ancho de etiqueta en property grid |
| `SectionIndent` | 12px | Sangría de sección |
| `CardRounding` | 4px | Radio de esquinas de card |
| `ButtonRounding` | 4px | Radio de esquinas de botón |
| `InputRounding` | 3px | Radio de esquinas de input |

#### Variantes de componentes:

```cpp
// Botones
enum class ButtonVariant { Default, Primary, Success, Warning, Danger, Ghost, Outline };
enum class ButtonSize    { Small, Medium, Large };

// Texto
enum class TextVariant { Default, Primary, Secondary, Muted, Success, Warning, Danger };

// Inputs
enum class InputVariant { Default, Filled, Outline };
```

#### Payloads de drag & drop:
```cpp
PAYLOAD_CONTENT_BROWSER_ITEM   = "CONTENT_BROWSER_ITEM"
PAYLOAD_CONTENT_BROWSER_ITEMS  = "CONTENT_BROWSER_ITEMS"
PAYLOAD_ENTITY_NODE            = "ENTITY_NODE"
PAYLOAD_TEXTURE                = "TEXTURE_ASSET"
PAYLOAD_MATERIAL               = "MATERIAL_ASSET"
PAYLOAD_MESH                   = "MESH_ASSET"
```

#### Reglas específicas de `ScopedStyle`:
- Un `ScopedStyle` **no puede mezclar** argumentos `float` e `ImVec2` en el mismo constructor.
- Para vars de diferente tipo, crear instancias separadas.

```cpp
// ? Correcto
ScopedStyle padding(ImGuiStyleVar_WindowPadding, ImVec2(12, 12));
ScopedStyle rounding(ImGuiStyleVar_WindowRounding, 8.0f);

// ? Incorrecto — tipos mezclados en el mismo constructor
ScopedStyle s(ImGuiStyleVar_WindowPadding, ImVec2(12, 12), ImGuiStyleVar_WindowRounding, 8.0f);
```

#### Colores:
- Usar siempre `UI::Color` o `UI::Colors::*`. Nunca `ImVec4` en código de paneles.

---

## 6. Sistema de Logging

- Usar siempre las macros de `Lunex/src/Log/Log.h`:
  ```cpp
  LNX_LOG_TRACE(...)
  LNX_LOG_INFO(...)
  LNX_LOG_WARN(...)
  LNX_LOG_ERROR(...)
  LNX_LOG_CRITICAL(...)
  LNX_LOG_DEBUG(...)
  ```
- Nunca `std::cout`, `printf`, `fprintf` ni `OutputDebugString` en código del engine o el editor.
- Los mensajes deben incluir el sistema de origen (p. ej. `"MaterialRegistry: ..."`, `"MeshComponent: ..."`).

---

## 7. Estándar de C++20 — Convenciones de Código (Estilo Unreal Engine)

Todo el código de Lunex sigue las convenciones de nomenclatura de **Unreal Engine** adaptadas a C++20.

### Nomenclatura:

| Elemento | Convención | Ejemplo |
|---|---|---|
| Clases / Structs de engine | `PascalCase` | `MaterialComponent`, `RHICommandList` |
| Interfaces abstractas | Prefijo `I` + `PascalCase` | `IRenderable`, `ISerializable` |
| Tipos / `typedef` / `using` | `PascalCase` o `T` + `PascalCase` | `Ref<T>`, `Scope<T>`, `UUID` |
| Métodos / Funciones miembro | `PascalCase` | `GetComponent<T>()`, `OnImGuiRender()` |
| Funciones libres / helpers | `PascalCase` | `CompareFuncToGL()` |
| Variables miembro privadas/protegidas | `m_` + `PascalCase` | `m_SelectionContext`, `m_IsRenaming` |
| Variables miembro públicas de struct | `PascalCase` | `Translation`, `MeshModel` |
| Variables locales / parámetros | `camelCase` | `totalHeight`, `lightCount` |
| Parámetros de función | `camelCase` | `filePath`, `entityID` |
| Constantes / `constexpr` estáticas | `k` + `PascalCase` o `ALL_CAPS` para macros | `kMaxLights`, `LNX_LOG_INFO` |
| Constantes de namespace/inline | `PascalCase` | `SpacingValues::MD` |
| Namespaces | `PascalCase` | `Lunex::UI`, `Lunex::RHI` |
| Enums (tipo) | `PascalCase` + sufijo según contexto | `ButtonVariant`, `ModelType` |
| Enums (valores, `enum class`) | `PascalCase` | `ButtonVariant::Primary`, `ModelType::Cube` |
| Macros del engine | `LNX_` + `ALL_CAPS` | `LNX_LOG_INFO`, `LNX_ASSERT`, `LNX_BIND_EVENT_FN` |
| Archivos `.h` / `.cpp` | `PascalCase` | `MaterialInstance.h`, `OpenGLRHIShader.cpp` |
| Archivos de shader | `PascalCase` + `.glsl` | `Mesh3D.glsl`, `ShadowDepth.glsl` |

### Reglas de código adicionales:
- `nullptr` sobre `NULL` o `0` para punteros.
- `enum class` sobre `enum`.
- No usar `new`/`delete` directamente: usar `CreateRef<T>` / `CreateScope<T>`.
- `[[nodiscard]]` en funciones que devuelven recursos o resultados críticos.
- `constexpr` en todo lo que pueda evaluarse en compilación.
- Preferir `auto` cuando el tipo es obvio por el contexto; nunca cuando sacrifica legibilidad.
- Structured bindings (`auto& [key, val]`) para iteraciones sobre mapas.
- Secciones dentro de archivos separadas con: `// ============================================================================`.
- Cabecera de archivo pública con `@file` y `@brief` al inicio.
- El precompilado `stpch.h` solo se incluye en `.cpp`, nunca en `.h`.

---

## 8. Sistema ECS — Componentes y Entidades

El engine usa **EnTT** como backend ECS. Las entidades y componentes están en `Lunex/src/Scene/`.

### Reglas:
- Acceder a componentes mediante `entity.GetComponent<T>()` — **siempre con `()`**.
- Verificar existencia con `entity.HasComponent<T>()` antes de acceder si no se garantiza su presencia.
- **Bug conocido a evitar:** `entity.GetComponent<T>` sin `()` toma la dirección de la función de plantilla en lugar del componente. Siempre añadir los paréntesis:
  ```cpp
  // ? Correcto
  newEntity.GetComponent<MaterialComponent>() = entity.GetComponent<MaterialComponent>();

  // ? Incorrecto — toma puntero a función
  newEntity.GetComponent<MaterialComponent>() = entity.GetComponent<MaterialComponent>;
  ```
- Los nuevos componentes deben seguir la convención de `Components.h`:
  - Struct con constructor default y copy explícitos.
  - Datos de runtime (punteros void a física, etc.) marcados con comentario `// Runtime data (not serialized)`.
  - Si el componente tiene datos que se persisten en escena, añadir serialización/deserialización en `SceneSerializer.cpp`.
  - Añadir el tipo nuevo al alias `AllComponents` al final de `Components.h`.
- `TextureComponent` está **deprecado**: usar `MaterialComponent` con `MaterialInstance`.

### Patrón para añadir un componente nuevo:
1. Definir el struct en `Lunex/src/Scene/Components.h` con constructores default/copy.
2. Añadirlo al alias `AllComponents` al final del archivo.
3. Añadir `SerializeEntity` en `SceneSerializer.cpp` para serializar sus campos a YAML.
4. Añadir la carga en `Deserialize()` en `SceneSerializer.cpp`.
5. Añadir su panel/sección en `PropertiesPanel.cpp` usando el framework Lunex UI.

---

## 9. Sistema de Materiales

- Arquitectura AAA en capas:
  - `MaterialAsset` (`.lumat`) — definición en disco
  - `MaterialInstance` — instancia runtime con overrides locales
  - `MaterialComponent` — componente ECS que contiene un `MaterialInstance`
  - `MaterialRegistry` — registro global (`MaterialRegistry::Get()`)
- No crear shaders ni pasar uniforms manualmente en paneles o escena: todo pasa por `MaterialInstance::Set*`.
- Material por defecto: `MaterialRegistry::Get().GetDefaultMaterial()`.

---

## 10. Sistema de Escenas y Serialización

- Formato de serialización: **YAML** (via `yaml-cpp`), extensión `.lunex`.
- Versión actual del formato: `SCENE_FORMAT_VERSION = 2` (en `SceneSerializer.h`).
- La serialización de una escena incluye:
  - Bloque `GlobalSkybox` con configuración del HDRI global (`SkyboxRenderer`).
  - Bloque `GlobalShadows` con toda la configuración del `ShadowSystem`.
  - Secuencia `Entities` con cada entidad y sus componentes.
- Compatibilidad hacia atrás: el deserializador debe ser tolerante a campos ausentes (usar `if (node["Key"])` antes de `.as<T>()`).
- Los IDs de entidad son `UUID` (uint64). Si se detecta un UUID duplicado al cargar, se genera uno nuevo automáticamente.
- Los datos de runtime (punteros a física, etc.) **no se serializan**.

---

## 11. Build System — Premake y Scripts Python

### Premake:
- Archivo de configuración: `premake5.lua` en la raíz.
- Para regenerar proyectos: `vendor/bin/premake/premake5.exe vs2022` (Windows).
- Configuraciones válidas: `Debug`, `Release`, `Dist`.
- Salida de binarios: `bin/<cfg>-<os>-<arch>/<proyecto>/`.
- Objetos intermedios: `bin-int/`.
- Todo proyecto nuevo debe añadirse al `premake5.lua` siguiendo la estructura existente.

### Scripts Python:
- Setup del entorno: `Setup.bat` ? `Setup.py`.
- Las dependencias (submódulos git) se gestionan en `Setup.py`.
- Para añadir una nueva dependencia: agregarla a la lista `submodules` en `Setup.py` **y** al `premake5.lua`.
- Scripts auxiliares: `CheckPython.py`, `Vulkan.py`, `KTX.py`, `Utils.py`.

### Dependencias actuales (`vendor/`):
| Librería | URL | Rama |
|---|---|---|
| GLFW | github.com/glfw/glfw | master |
| ImGuiLib | github.com/ocornut/imgui | docking |
| glm | github.com/g-truc/glm | master |
| ImGuizmo | github.com/CedricGuillemet/ImGuizmo | master |
| yaml-cpp | github.com/jbeder/yaml-cpp | master |
| Box2D | github.com/erincatto/box2d | main |
| assimp | github.com/assimp/assimp | master |
| Bullet3 | github.com/bulletphysics/bullet3 | master |

---

## 12. Estructura de Directorios

```
Lunex-Engine/
??? Lunex/                        # Librería estática del engine
?   ??? src/
?       ??? Core/                 # Core.h, UUID, punteros inteligentes, JobSystem
?       ??? RHI/                  # Abstracción gráfica
?       ?   ??? OpenGL/           # Implementación OpenGL (único lugar con gl*)
?       ??? Renderer/             # Renderer3D, DeferredRenderer, Skybox, Shadows…
?       ??? Resources/            # Mesh/Model, Render/MaterialInstance
?       ??? Assets/               # MeshAsset, MaterialAsset, MaterialImporter
?       ??? AssetPipeline/        # TextureCooker, TextureImporter
?       ??? Scene/                # ECS, Components, SceneSerializer, Camera, Lighting
?       ??? Physics/              # Box2D (2D), Bullet3 (3D)
?       ??? Input/                # Input system
?       ??? Log/                  # Logging (spdlog + CallbackSink)
?       ??? Events/               # Sistema de eventos
?       ??? Scripting/            # Sistema de scripts C++
??? Lunex-Editor/                 # Editor (ejecutable)
?   ??? assets/
?   ?   ??? shaders/              # Todos los archivos .glsl del engine
?   ?   ??? cache/shader/rhi/     # Caché SPIR-V generada automáticamente
?   ??? src/
?       ??? Editor/               # EditorLayer, Application
?       ??? Panels/               # Todos los paneles del editor
?       ??? UI/                   # Framework Lunex UI
?           ??? UICore.h
?           ??? UIComponents.h
?           ??? UILayout.h
?           ??? Controls/
?           ??? Components/
??? Lunex-ScriptCore/             # Librería base para scripts de usuario
??? Sandbox/                      # Proyecto de pruebas
??? vendor/                       # Dependencias de terceros
??? scripts/                      # Scripts Python de setup
??? premake5.lua
??? Setup.bat / Setup.py
```

---

## 13. Abstracción: Regla de No Romper Funcionalidad

Cuando se abstrae o refactoriza código existente:
- **No cambiar la funcionalidad observable** — solo mejorar rendimiento, usabilidad del código o reducir acoplamiento.
- Si la propuesta cambia comportamiento, señalarlo explícitamente y acordarlo antes de implementar.
- Las abstracciones deben ser aditivas: no eliminar API pública sin deprecarla primero.

---

## 14. Verificación Antes de Completar

Antes de dar una tarea por terminada, el agente debe confirmar:

1. El código **compila sin errores** usando las herramientas disponibles.
2. No se usan llamadas OpenGL, ImGui raw ni `std::cout` fuera de las capas correspondientes.
3. Los tipos de `ScopedStyle` son coherentes (no mezclar `float` e `ImVec2` en el mismo constructor).
4. `GetComponent<T>()` siempre tiene paréntesis.
5. Todo puntero inteligente usa `Ref<T>`/`Scope<T>` con `CreateRef`/`CreateScope`.
6. Si se modificó un shader `.glsl`, se ha tenido en cuenta la invalidación de la caché.
7. Si se añadió un componente nuevo, tiene serialización/deserialización en `SceneSerializer.cpp` y entrada en `AllComponents`.
8. Todo código nuevo del editor usa el framework Lunex UI.

---

## 15. Lo que el Agente NO Debe Hacer

- Hacer commits, branches, merges, push o cualquier operación de Git.
- Instalar paquetes de sistema, SDKs o modificar variables de entorno.
- Crear nuevas librerías de terceros sin discutirlo primero.
- Cambiar versiones de dependencias sin justificación técnica explícita.
- Añadir `#include <windows.h>` o APIs específicas de plataforma fuera de `Lunex/src/Platform/`.
- Añadir código de debug temporal (`printf`, `std::cout`) sin marcarlo claramente como temporal.
- Crear archivos `.md` de documentación no solicitados.
- Eliminar la caché de shaders o texturas sin advertir de las consecuencias.
- **Hardcodear teclas de teclado directamente** en ningún sistema del editor — usar siempre el sistema de acciones descrito en la sección 16.

---

## 16. Sistema de Inputs — Acciones y Key Bindings

Todo nuevo atajo de teclado del editor **debe registrarse en el sistema de acciones** (`InputManager` + `ActionRegistry` + `KeyMap`). Está **prohibido** leer teclas directamente con `Input::IsKeyPressed(Key::X)` o similar para lógica de editor sin pasar por este sistema.

### Archivos del sistema (en `Lunex/src/Input/`):

| Archivo | Contenido |
|---|---|
| `InputManager.h/.cpp` | Singleton principal; gestiona `KeyMap`, `ActionRegistry`, estados y persistencia |
| `Action.h` | Interfaz `Action`, `FunctionAction`, `ActionState`, `ActionContext` |
| `ActionRegistry.h` | Registro global de acciones por nombre (singleton) |
| `KeyMap.h` | Tabla `KeyBinding ? actionName`; permite bind/unbind/rebind |
| `KeyBinding.h` | Struct `KeyBinding` (KeyCode + Modifiers); `KeyModifiers::*` bitflags |
| `InputConfig.h/.cpp` | Serialización/deserialización del `KeyMap` a YAML |

### Anatomía de una acción:

```cpp
// ActionContext define cuándo se ejecuta
enum class ActionContext {
    Pressed,  // Una sola vez al pulsar
    Released, // Una sola vez al soltar
    Held,     // Cada frame mientras esté pulsada
    Any       // Cualquier cambio de estado
};

// FunctionAction: forma rápida de crear una acción con lambda
CreateRef<FunctionAction>(
    "MiSistema.MiAccion",    // Nombre único (categoría.Acción)
    ActionContext::Pressed,
    [](const ActionState&) { /* lógica */ },
    "Descripción legible",   // Se muestra en InputSettingsPanel
    true                     // ¿Es remappable por el usuario?
);
```

### Convención de nombres de acciones:
Los nombres siguen el patrón `Categoría.Nombre` en `PascalCase`:

| Categoría | Ejemplos |
|---|---|
| `Camera.*` | `Camera.MoveForward`, `Camera.MoveUp` |
| `Editor.*` | `Editor.SaveScene`, `Editor.DuplicateEntity` |
| `Gizmo.*` | `Gizmo.Translate`, `Gizmo.Rotate` |
| `Debug.*` | `Debug.ToggleStats`, `Debug.ToggleConsole` |
| `Preferences.*` | `Preferences.InputSettings` |

### Patrón completo para añadir un nuevo atajo de teclado:

**Paso 1 — Registrar la acción** en `InputManager::RegisterDefaultActions()` (`InputManager.cpp`):
```cpp
registry.Register("Editor.MiNuevaAccion", CreateRef<FunctionAction>(
    "Editor.MiNuevaAccion",
    ActionContext::Pressed,
    [](const ActionState&) { /* placeholder — EditorLayer lo sobreescribirá */ },
    "Descripción que aparece en el panel de configuración",
    true  // remappable = true para que el usuario pueda cambiarlo
));
```

**Paso 2 — Asignar la tecla por defecto** en `InputManager::ResetToDefaults()` (`InputManager.cpp`):
```cpp
m_KeyMap.Bind(Key::M, KeyModifiers::Ctrl, "Editor.MiNuevaAccion");
// Para combinaciones: KeyModifiers::Ctrl | KeyModifiers::Shift
```

**Paso 3 — Sobreescribir el callback real** en `EditorLayer` (o el sistema correspondiente):
```cpp
ActionRegistry::Get().Register("Editor.MiNuevaAccion", CreateRef<FunctionAction>(
    "Editor.MiNuevaAccion",
    ActionContext::Pressed,
    [this](const ActionState&) { MiLogica(); },
    "Descripción",
    true
));
```

**Paso 4 — Verificar estado si se necesita en runtime** (en `OnUpdate` o similar):
```cpp
// ? Correcto — consultar por nombre de acción
if (InputManager::Get().IsActionJustPressed("Editor.MiNuevaAccion")) { ... }

// ? Incorrecto — leer la tecla directamente
if (Input::IsKeyPressed(Key::M)) { ... }
```

### Modificadores disponibles (`KeyModifiers::*`):
```cpp
KeyModifiers::None   // Sin modificador
KeyModifiers::Ctrl   // Control
KeyModifiers::Shift  // Shift
KeyModifiers::Alt    // Alt
KeyModifiers::Super  // Super / Win key

// Combinaciones con OR
uint8_t ctrlShift = KeyModifiers::Ctrl | KeyModifiers::Shift;
```

### Persistencia:
- Los bindings se guardan automáticamente en `assets/InputConfigs/EditorInputBindings.yaml` al cerrar el editor y al hacer clic en "Save" en `InputSettingsPanel`.
- Al arrancar, `InputManager::Initialize()` carga ese archivo. Si no existe, llama a `ResetToDefaults()`.
- Si se añade una acción nueva con tecla por defecto y el usuario ya tiene un archivo de bindings guardado sin esa nueva entrada, la acción quedará sin binding hasta que el usuario pulse "Reset to Defaults" o la asigne manualmente. Tenerlo en cuenta al diseñar nuevas acciones.

### El panel `InputSettingsPanel`:
- Muestra todas las acciones registradas en `ActionRegistry`, agrupadas por categoría (`parte antes del `.`).
- Solo las acciones con `IsRemappable() == true` son editables por el usuario.
- Las acciones con `IsRemappable() == false` se muestran como texto estático.
- Accesible con `Ctrl+K` (acción `Preferences.InputSettings`).
