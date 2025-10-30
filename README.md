# ?? Lunex Engine

<div align="center">

![Lunex Engine](https://img.shields.io/badge/version-0.1.0-blue.svg)
![C++](https://img.shields.io/badge/C++-20-00599C?logo=cplusplus)
![OpenGL](https://img.shields.io/badge/OpenGL-4.5-5586A4?logo=opengl)
![Platform](https://img.shields.io/badge/platform-Windows-0078D6?logo=windows)
![License](https://img.shields.io/badge/license-MIT-green.svg)

**Un motor de juegos 2D/3D moderno y extensible escrito en C++20**

[Características](#-características) •
[Instalación](#-instalación) •
[Uso](#-uso-rápido) •
[Arquitectura](#-arquitectura) •
[Documentación](#-documentación) •
[Roadmap](#-roadmap)

</div>

---

## ?? Tabla de Contenidos

- [Acerca de Lunex](#-acerca-de-lunex)
- [Características](#-características)
- [Instalación](#-instalación)
- [Uso Rápido](#-uso-rápido)
- [Arquitectura](#-arquitectura)
- [Estructura del Proyecto](#-estructura-del-proyecto)
- [Tecnologías](#-tecnologías-utilizadas)
- [Roadmap](#-roadmap)
- [Contribuir](#-contribuir)
- [Licencia](#-licencia)

---

## ?? Acerca de Lunex

**Lunex Engine** es un motor de juegos 2D/3D de código abierto diseñado para ser **modular**, **extensible** y **fácil de usar**. Inspirado en motores modernos como Unity y Unreal Engine, Lunex proporciona un editor visual completo y un sistema de renderizado avanzado basado en OpenGL.

### ¿Por qué Lunex?

- ? **Arquitectura modular** - Sistema de componentes flexible (ECS con entt)
- ? **Editor visual profesional** - Interfaz estilo Unreal Engine 5 con ImGui
- ? **Renderizado 2D/3D** - Pipeline de renderizado optimizado con soporte PBR
- ? **Física 2D integrada** - Motor Box2D v3.0 para simulaciones físicas realistas
- ? **Sistema de escenas** - Serialización/deserialización en formato YAML
- ? **Hot-reload de shaders** - Compilación SPIR-V y caché de shaders
- ? **Profiling integrado** - Análisis de rendimiento en tiempo real

---

## ? Características

### ?? Editor Visual (Lunex-Editor)

<details>
<summary><b>Ver detalles del Editor</b></summary>

El editor de Lunex proporciona una experiencia moderna y profesional:

- **Viewport 3D interactivo** - Navegación estilo Unity (Alt + Mouse)
- **Scene Hierarchy Panel** - Gestión jerárquica de entidades con búsqueda y filtros
- **Properties Panel** - Inspector de componentes con edición en tiempo real
- **Content Browser** - Explorador de assets con vista de carpetas
- **Stats Panel** - Estadísticas de renderizado (Draw Calls, Vértices, FPS)
- **Settings Panel** - Configuración del editor y visualización de colliders
- **Toolbar** - Controles de Play/Stop/Simulate
- **Gizmos** - Manipulación visual de transformaciones (Translate, Rotate, Scale)

#### Modos de Trabajo

- **Edit Mode** - Edición de escenas con EditorCamera
- **Play Mode** - Ejecución con cámara de escena y física activa
- **Simulate Mode** - Simulación de física sin scripts

</details>

### ?? Sistema de Renderizado

<details>
<summary><b>Renderer 2D</b></summary>

- **Batch Rendering** - Optimización automática de draw calls
- **Sprites y Textures** - Soporte de texturas con tiling y offset
- **Primitivas** - Quads, Círculos, Líneas
- **Sistema de Capas** - Z-ordering y transparencia
- **Text Rendering** - (Planeado)

</details>

<details>
<summary><b>Renderer 3D</b></summary>

- **Pipeline PBR** - Physically Based Rendering con metallic/roughness workflow
- **Mesh System** - Importación y renderizado de geometría 3D
- **Material System** - Materiales con texturas (Albedo, Normal, Metallic, Roughness)
- **Lighting** - Sistema de luces (Directional, Point, Spot)
- **Shadow Mapping** - (En desarrollo)
- **Post-Processing** - (Planeado: Bloom, SSAO, HDR)

#### Formas 3D Básicas

```cpp
// Crear meshes básicas
Ref<Mesh> cube = Mesh::CreateCube();
Ref<Mesh> sphere = Mesh::CreateSphere(32);
Ref<Mesh> plane = Mesh::CreatePlane();
Ref<Mesh> quad = Mesh::CreateQuad();
```

</details>

### ?? Sistema de Componentes (ECS)

Lunex usa **entt** para gestionar entidades y componentes:

```cpp
// Componentes disponibles
- TransformComponent      // Posición, rotación, escala
- TagComponent       // Nombre de la entidad
- SpriteRendererComponent // Renderizado 2D
- CameraComponent         // Cámara ortográfica/perspectiva
- MeshComponent  // Geometría 3D
- MaterialComponent       // Material PBR
- Rigidbody2DComponent    // Física 2D (Box2D)
- BoxCollider2DComponent  // Collider de caja 2D
- CircleCollider2DComponent // Collider circular 2D
- NativeScriptComponent   // Scripts nativos en C++
```

### ?? Sistema de Física 2D

Integración completa de **Box2D v3.0**:

- **Rigidbodies** - Static, Dynamic, Kinematic
- **Colliders** - Box, Circle
- **Forces & Impulses** - Control preciso de movimiento
- **Collision Detection** - Callbacks de colisiones
- **Debug Rendering** - Visualización de colliders en el editor

### ?? Sistema de Assets

- **Textures** - PNG, JPG (usando stb_image)
- **Shaders** - GLSL con compilación SPIR-V
- **Scenes** - Formato `.lunex` (YAML)
- **Materials** - Serialización de propiedades PBR

### ?? Herramientas de Desarrollo

- **Profiling** - Instrumentación de rendimiento con Chrome Tracing
- **Logging** - Sistema de logs con spdlog (Core, Client)
- **Asserts** - Macros de debugging para desarrollo
- **Hot-Reload** - Recarga automática de shaders y assets

---

## ?? Instalación

### Requisitos Previos

| Componente | Versión Mínima | Notas |
|------------|----------------|-------|
| **Sistema Operativo** | Windows 10/11 | Soporte para Linux/macOS planeado |
| **Compilador** | MSVC 2022 | C++20 requerido |
| **CMake** | 3.20+ | Para generar proyectos (opcional) |
| **Vulkan SDK** | 1.3.290 | Para compilación de shaders SPIR-V |
| **Git** | 2.0+ | Para clonar submódulos |

### Pasos de Instalación

#### 1?? Clonar el Repositorio

```bash
git clone --recursive https://github.com/lluanllo/Lunex_engine.git
cd Lunex_engine
```

> **Nota**: El flag `--recursive` es importante para clonar los submódulos (GLFW, ImGui, Box2D, etc.)

#### 2?? Ejecutar Setup

**Windows:**
```cmd
Setup.bat
```

Esto hará:
- ? Verificar/instalar Vulkan SDK
- ? Clonar submódulos (GLFW, ImGui, Box2D, yaml-cpp, etc.)
- ? Generar archivos de proyecto con Premake5

#### 3?? Compilar

1. Abre `Lunex-Engine.sln` en **Visual Studio 2022**
2. Selecciona la configuración:
   - **Debug** - Debugging con símbolos
   - **Release** - Optimizado para rendimiento
   - **Dist** - Build de distribución
3. Compila (`Ctrl+Shift+B`)

#### 4?? Ejecutar

- **Editor**: Proyecto `Lunex-Editor` (Startup Project por defecto)
- **Sandbox**: Proyecto `Sandbox` (Aplicación de prueba simple)

---

## ?? Uso Rápido

### Crear una Aplicación Básica

```cpp
#include <Lunex.h>
#include <Core/EntryPoint.h>

class MyGameLayer : public Lunex::Layer {
public:
void OnUpdate(Lunex::Timestep ts) override {
        // Lógica de juego
        Lunex::RenderCommand::SetClearColor({ 0.1f, 0.1f, 0.1f, 1.0f });
        Lunex::RenderCommand::Clear();
        
 // Renderizar un quad
        Lunex::Renderer2D::BeginScene(m_Camera);
        Lunex::Renderer2D::DrawQuad({ 0.0f, 0.0f }, { 1.0f, 1.0f }, { 1.0f, 0.0f, 0.0f, 1.0f });
     Lunex::Renderer2D::EndScene();
    }
    
private:
    Lunex::OrthographicCamera m_Camera{-1.6f, 1.6f, -0.9f, 0.9f};
};

class MyApp : public Lunex::Application {
public:
    MyApp(Lunex::ApplicationCommandLineArgs args)
        : Application("My Game", args) {
        PushLayer(new MyGameLayer());
    }
};

namespace Lunex {
    Ref<Application> CreateApplication(ApplicationCommandLineArgs args) {
        return CreateRef<MyApp>(args);
    }
}
```

### Usar el Editor

1. **Crear Entidad**: Click derecho en Scene Hierarchy ? `Create`
2. **Añadir Componentes**: Click en `+ Add` en Properties Panel
3. **Manipular Objetos**: 
   - `Q` - Sin gizmo
   - `W` - Translate
   - `E` - Rotate
   - `R` - Scale
4. **Cámara del Editor**:
   - `Alt + Left Mouse` - Orbitar
   - `Alt + Middle Mouse` - Pan
   - `Mouse Wheel` - Zoom
5. **Ejecutar**: Click en ?? Play
6. **Guardar Escena**: `Ctrl+S`

---

## ??? Arquitectura

### Diagrama de Arquitectura General

```
????????????????????????????????????????????????????????????
?  LUNEX ENGINE  ?
?         ?
?  ?????????????????????????????????????????????????????? ?
?  ?     Application Layer           ? ?
?  ?  - Window Management (GLFW)       ? ?
?  ?  - Event System (Keyboard, Mouse, Window)          ? ?
?  ?  - Layer Stack          ? ?
?  ?????????????????????????????????????????????????????? ?
?     ?      ?
?  ?????????????????????????????????????????????????????? ?
?  ?              Scene Layer    ? ?
?  ?  - Entity Component System (entt)      ? ?
?  ?  - Scene Serialization (YAML)  ? ?
?  ?  - Scene Runtime / Editor   ? ?
?  ?????????????????????????????????????????????????????? ?
?           ?         ?
?  ?????????????????????????????????????????????????????? ?
?  ?  Rendering Layer       ? ?
?  ?  ????????????????  ????????????????    ? ?
?  ?  ? Renderer2D   ?  ? Renderer3D   ?         ? ?
?  ?  ? - Batching   ?  ? - PBR        ?     ? ?
?  ?  ? - Sprites    ?  ? - Meshes     ?            ? ?
?  ?  ? - Quads      ?  ? - Materials  ?     ? ?
?  ?  ????????????????  ????????????????        ? ?
?  ?   Renderer Abstraction      ? ?
?  ?  - Shader, VertexArray, Framebuffer, etc.         ? ?
?  ?????????????????????????????????????????????????????? ?
?        ?       ?
?  ?????????????????????????????????????????????????????? ?
?  ?              Platform Layer            ? ?
?  ?  - OpenGL (Windows/Linux/macOS)         ? ?
?  ?  - Vulkan (Planned)  ? ?
?  ?  - DirectX 12 (Planned)     ? ?
?  ?????????????????????????????????????????????????????? ?
????????????????????????????????????????????????????????????
```

### Flujo de Renderizado 3D

```
Scene::OnUpdateEditor()
    ?
Renderer3D::BeginScene(EditorCamera)
    ?
RendererPipeline3D::BeginScene()
    - Upload Camera UBO (ViewProjection, Position)
    ?
Renderer3D::DrawMesh(Mesh, Material, Transform)
    ?
RendererPipeline3D::SubmitMesh()
    - Add to submission queue
    ?
Renderer3D::EndScene()
    ?
RendererPipeline3D::EndScene()
    ?
RendererPipeline3D::ExecuteGeometryPass()
    - Bind Material (Shader + Uniforms)
    - Upload Transform UBO
    - Draw Mesh (glDrawElements)
    ?
Final Render to Framebuffer
```

### Sistema de Materiales PBR

```cpp
Material
??? Shader (Basic3D.glsl)
??? PBR Properties
?   ??? Albedo (vec3)
?   ??? Metallic (float)
?   ??? Roughness (float)
?   ??? Emission (vec3)
??? Texture Maps (optional)
    ??? AlbedoMap (Texture2D)
    ??? NormalMap (Texture2D)
    ??? MetallicMap (Texture2D)
    ??? RoughnessMap (Texture2D)

MaterialInstance
??? BaseMaterial (ref)
??? Property Overrides
    ??? Override Albedo
    ??? Override Metallic
    ??? Override Roughness
    ??? Override Emission
```

---

## ?? Estructura del Proyecto

```
Lunex_engine/
??? ?? Lunex/                # Motor principal (Static Library)
?   ??? ?? src/
?   ?   ??? ?? Core/             # Núcleo del motor
?   ?   ?   ??? Application.cpp  # Clase Application
?   ?   ?   ??? Layer.cpp        # Sistema de capas
?   ?   ?   ??? Input.cpp        # Sistema de input
?   ?   ?   ??? ...
?   ?   ??? ?? Renderer/         # Sistema de renderizado
?   ?   ?   ??? ?? Renderer2D/   # Renderizado 2D
?   ?   ?   ??? ?? Renderer3D/   # Renderizado 3D
?   ?   ?   ?   ??? Mesh.cpp
?   ?   ?   ?   ??? Material.cpp
?   ?   ?   ?   ??? RendererPipeline3D.cpp
?   ?   ?   ??? ?? RenderCore/   # Abstracción de render
?   ?   ?   ??? ?? Buffer/       # Vertex/Index buffers
?   ?   ?   ??? ?? CameraTypes/  # Cámaras (Ortho, Perspective, Editor)
?   ?   ??? ?? Scene/ # Sistema de escenas ECS
?   ?   ?   ??? Scene.cpp
?   ?   ?   ??? Entity.cpp
?   ?   ?   ??? Components.h     # Todos los componentes
?   ?   ?   ??? SceneSerializer.cpp
?   ?   ??? ?? Events/           # Sistema de eventos
?   ?   ??? ?? ImGui/  # Integración ImGui
?   ?   ??? ?? Platform/         # Código específico de plataforma
? ?       ??? ?? OpenGL/       # Implementación OpenGL
?   ??? ?? assets/             # Assets del motor
?
??? ?? Lunex-Editor/  # Editor visual (Executable)
?   ??? ?? src/
?   ?   ??? ?? Editor/
?   ? ?   ??? EditorLayer.cpp  # Capa principal del editor
?   ?   ?   ??? LunexEditorApp.cpp
?   ?   ??? ?? Panels/  # Paneles del editor
?   ?     ??? SceneHierarchyPanel.cpp
?   ?    ??? PropertiesPanel.cpp
?   ?       ??? ContentBrowserPanel.cpp
?   ?       ??? ViewportPanel.cpp
?   ?       ??? StatsPanel.cpp
?   ?       ??? SettingsPanel.cpp
?   ??? ?? assets/             # Assets del editor
?       ??? ?? shaders/          # Shaders del motor
?   ?   ??? Basic3D.glsl# Shader PBR
?       ?   ??? Renderer2D_Quad.glsl
??   ??? ...
?       ??? ?? textures/         # Texturas
?       ??? ?? icons/          # Iconos del editor
?       ??? ?? scenes/        # Escenas de ejemplo
?
??? ?? Sandbox/       # Aplicación de prueba (Executable)
?   ??? ?? src/
?       ??? SandboxApp.cpp
?       ??? Sandbox2D.cpp
?
??? ?? vendor/   # Dependencias externas
?   ??? ?? GLFW/           # Windowing
?   ??? ?? Glad/# OpenGL Loader
?   ??? ?? ImGuiLib/    # Dear ImGui (docking branch)
?   ??? ?? glm/           # GLM Math Library
?   ??? ?? entt/        # Entity Component System
? ??? ?? Box2D/         # Física 2D
?   ??? ?? yaml-cpp/             # Serialización YAML
?   ??? ?? ImGuizmo/          # Gizmos para transformaciones
?   ??? ?? spdlog/            # Logging
?   ??? ?? stb_image/        # Carga de imágenes
?
??? ?? scripts/     # Scripts de setup
?   ??? Setup.py        # Script principal de setup
?   ??? Vulkan.py    # Setup de Vulkan SDK
?   ??? ...
?
??? premake5.lua       # Configuración de proyectos
??? Setup.bat          # Script de setup para Windows
??? README.md     # Este archivo
```

---

## ??? Tecnologías Utilizadas

### Lenguajes y Estándares

- **C++20** - Lenguaje principal del motor
- **GLSL 4.50** - Shaders (OpenGL)
- **Python 3** - Scripts de setup y herramientas

### Librerías Principales

| Librería | Versión | Propósito |
|----------|---------|-----------|
| **GLFW** | 3.4 | Gestión de ventanas y contexto OpenGL |
| **Glad** | 0.1.36 | Loader de OpenGL 4.5 |
| **Dear ImGui** | 1.90 (docking) | Interfaz de usuario del editor |
| **GLM** | 0.9.9 | Matemáticas (vectores, matrices, quaternions) |
| **entt** | 3.13 | Entity Component System |
| **Box2D** | 3.0 | Motor de física 2D |
| **yaml-cpp** | 0.8.0 | Serialización de escenas |
| **spdlog** | 1.12 | Sistema de logging |
| **stb_image** | 2.28 | Carga de texturas |
| **ImGuizmo** | 1.83 | Gizmos de transformación |
| **Vulkan SDK** | 1.3.290 | Compilación SPIR-V (shaderc, spirv-cross) |

### Herramientas de Desarrollo

- **Visual Studio 2022** - IDE principal
- **Premake5** - Generación de proyectos
- **RenderDoc** - Depuración gráfica (recomendado)
- **Chrome Tracing** - Profiling (chrome://tracing)

---

## ??? Roadmap

### ? Versión 0.1 (Actual)

- [x] Editor visual funcional
- [x] Renderizado 2D con batching
- [x] Renderizado 3D con PBR básico
- [x] Sistema de componentes (ECS)
- [x] Física 2D (Box2D)
- [x] Serialización de escenas
- [x] Material System con texturas
- [x] Gizmos de transformación

### ?? Versión 0.2 (En desarrollo)

- [ ] **Shadow Mapping** - Sombras en tiempo real
- [ ] **Multiple Lights** - Sistema de luces dinámicas
- [ ] **Skybox** - Renderizado de cielos
- [ ] **Particle System** - Sistema de partículas 2D/3D
- [ ] **Audio System** - Integración de OpenAL
- [ ] **Scripting C#** - Soporte de scripts en C# (Mono)

### ?? Versión 0.3 (Planeado)

- [ ] **Post-Processing** - Bloom, SSAO, HDR, Tonemapping
- [ ] **Model Import** - Assimp para FBX, OBJ, GLTF
- [ ] **Animation System** - Skeletal animation
- [ ] **Terrain System** - Heightmaps y LOD
- [ ] **Prefabs** - Sistema de prefabs reutilizables
- [ ] **Build System** - Exportación de ejecutables

### ?? Versión 1.0 (Objetivo)

- [ ] **Vulkan Backend** - Renderizado con Vulkan
- [ ] **DirectX 12 Backend** - Soporte para Windows
- [ ] **Linux/macOS Support** - Multiplataforma completo
- [ ] **Physics 3D** - Integración de Jolt Physics
- [ ] **Networking** - Multiplayer básico
- [ ] **Asset Pipeline** - Editor de materiales, texturas, etc.

---

## ?? Contribuir

¡Las contribuciones son bienvenidas! Si quieres contribuir a Lunex:

1. **Fork** el repositorio
2. Crea una rama para tu feature (`git checkout -b feature/AmazingFeature`)
3. Commit tus cambios (`git commit -m 'Add some AmazingFeature'`)
4. Push a la rama (`git push origin feature/AmazingFeature`)
5. Abre un **Pull Request**

### Guidelines

- Sigue el estilo de código existente
- Añade tests para nuevas funcionalidades
- Actualiza la documentación si es necesario
- Asegúrate de que el código compile sin warnings

### Reportar Bugs

Usa el [Issue Tracker](https://github.com/lluanllo/Lunex_engine/issues) para reportar bugs. Por favor incluye:

- Descripción detallada del problema
- Pasos para reproducir
- Comportamiento esperado vs comportamiento actual
- Logs y screenshots si es posible
- Información del sistema (OS, GPU, drivers)

---

## ?? Documentación

- **Wiki** (Próximamente) - Guías y tutoriales
- **API Reference** (Próximamente) - Documentación de la API
- **Examples** - Ver carpeta `Sandbox/` para ejemplos

### Recursos Útiles

- [LearnOpenGL](https://learnopengl.com/) - Tutorial de OpenGL
- [entt Documentation](https://github.com/skypjack/entt) - ECS
- [Dear ImGui Wiki](https://github.com/ocornut/imgui/wiki) - ImGui
- [Box2D Manual](https://box2d.org/documentation/) - Física 2D

---

## ?? Licencia

Este proyecto está licenciado bajo la licencia **MIT** - ver el archivo [LICENSE](LICENSE) para más detalles.

```
MIT License

Copyright (c) 2025 Lunex Engine Contributors

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
```

---

## ?? Autores

- **lluanllo** - *Creador y desarrollador principal* - [GitHub](https://github.com/lluanllo)

Ver también la lista de [contribuidores](https://github.com/lluanllo/Lunex_engine/contributors) que participaron en este proyecto.

---

## ?? Agradecimientos

- **The Cherno** - Por su serie de tutoriales "Game Engine" en YouTube
- **Hazel Engine** - Inspiración para la arquitectura del motor
- **entt** - Por el increíble sistema ECS
- **Dear ImGui** - Por la biblioteca de UI
- **Box2D** - Por el motor de física
- Todas las librerías open source utilizadas en este proyecto

---

<div align="center">

**? Si te gusta Lunex Engine, dale una estrella en GitHub! ?**

[? Volver arriba](#-lunex-engine)

</div>
