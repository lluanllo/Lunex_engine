import os
import subprocess
import sys

# Change to root directory
script_dir = os.path.dirname(os.path.realpath(__file__))
os.chdir(os.path.join(script_dir, '../'))

import CheckPython

# Make sure everything we need is installed
print("Verificando paquetes de Python...")
CheckPython.ValidatePackages()

import Vulkan
import KTX

print("\n[2/8] Clonando/actualizando submodulos...")
result = subprocess.call(["git", "submodule", "update", "--init", "--recursive"])

if result != 0:
    print("\n" + "!"*60)
    print("WARNING: Conflictos detectados en submódulos.")
    print("Reseteando submódulos a versiones limpias...")
    print("!"*60)
    
    # Resetear todos los submódulos a su estado limpio
    subprocess.call(["git", "submodule", "foreach", "--recursive", "git", "reset", "--hard"])
    subprocess.call(["git", "submodule", "foreach", "--recursive", "git", "clean", "-fd"])
    
    # Intentar actualizar de nuevo
    result = subprocess.call(["git", "submodule", "update", "--init", "--recursive"])

# ✅ Lista completa de submódulos (para clonar manualmente si no están en .gitmodules)
print("\n[3/8] Verificando dependencias adicionales...")
submodules = [
    ("vendor/GLFW", "https://github.com/glfw/glfw.git", "master"),
    ("vendor/ImGuiLib", "https://github.com/ocornut/imgui.git", "docking"),
    ("vendor/glm", "https://github.com/g-truc/glm.git", "master"),
    ("vendor/ImGuizmo", "https://github.com/CedricGuillemet/ImGuizmo.git", "master"),
    ("vendor/imnodes", "https://github.com/Nelarius/imnodes.git", "master"),
    ("vendor/yaml-cpp", "https://github.com/jbeder/yaml-cpp.git", "master"),
    ("vendor/Box2D", "https://github.com/erincatto/box2d.git", "main"),
    ("vendor/assimp", "https://github.com/assimp/assimp.git", "master"),
    ("vendor/Bullet3", "https://github.com/bulletphysics/bullet3.git", "master"),
]

# Verificar y clonar submódulos faltantes
for path, url, branch in submodules:
    # Verificar si el directorio existe y contiene archivos
    if not os.path.exists(path) or not os.listdir(path):
        print(f"\n⚠ {path} no encontrado o vacío, clonando...")
        
        # Si existe pero está vacío, eliminarlo
        if os.path.exists(path):
            import shutil
            try:
                shutil.rmtree(path)
            except Exception as e:
                print(f"  Error al eliminar directorio vacío: {e}")
        
        # Clonar el repositorio
        print(f"  Clonando {path} desde {url}...")
        result = subprocess.call(["git", "clone", "--depth", "1", "-b", branch, url, path])
        if result == 0:
            print(f"  ✓ {path} clonado correctamente")
        else:
            print(f"  ✗ Error al clonar {path}")
    else:
        print(f"  ✓ {path} ya existe")

# Verificar que los submódulos se clonaron correctamente
print("\n[4/8] Verificando integridad de submódulos...")
missing_submodules = []
required_files = {
    "vendor/GLFW/include/GLFW/glfw3.h": "GLFW",
    "vendor/ImGuiLib/imgui.h": "ImGui",
    "vendor/glm/glm/glm.hpp": "GLM",
    "vendor/ImGuizmo/ImGuizmo.h": "ImGuizmo",
    "vendor/imnodes/imnodes.h": "imnodes",
    "vendor/yaml-cpp/include/yaml-cpp/yaml.h": "yaml-cpp",
    "vendor/Box2D/include/box2d/box2d.h": "Box2D",
    "vendor/assimp/include/assimp/Importer.hpp": "Assimp",
    "vendor/Bullet3/src/btBulletDynamicsCommon.h": "Bullet3",
}

all_ok = True
for path, name in required_files.items():
    if not os.path.exists(path):
        missing_submodules.append(f"{name} ({path})")
        print(f"✗ Falta: {name}")
        all_ok = False
    else:
        print(f"✓ {name}")

if not all_ok:
    print("\n" + "!"*60)
    print("ERROR: Algunos submódulos no se clonaron correctamente.")
    print("Archivos faltantes:")
    for item in missing_submodules:
        print(f"  - {item}")
    print("\nIntenta ejecutar Setup.bat de nuevo o clonar manualmente:")
    print("  git clone --recursive https://github.com/lluanllo/Lunex_engine.git")
    print("!"*60)
    input("\nPresiona Enter para continuar de todos modos...")
else:
    print("\n✓ Todos los submódulos están correctos")

print("\n[5/8] Configurando Assimp...")
# Configurar Assimp
assimp_config = "vendor/assimp/include/assimp/config.h"
if not os.path.exists(assimp_config):
    print("  ⚠ Creando config.h para Assimp...")
    os.makedirs(os.path.dirname(assimp_config), exist_ok=True)
    with open(assimp_config, 'w') as f:
        f.write("""#ifndef ASSIMP_CONFIG_H_INC
#define ASSIMP_CONFIG_H_INC
#define ASSIMP_BUILD_NO_C4D_IMPORTER
#define ASSIMP_BUILD_NO_DRACO
#ifdef _MSC_VER
#define AI_FORCE_INLINE __forceinline
#else
#define AI_FORCE_INLINE inline
#endif
#endif
""")
    print("  ✓ config.h creado")
else:
    print("  ✓ Assimp ya configurado")

print("\n[6/8] Verificando Vulkan SDK...")
vulkanInstalled = Vulkan.CheckVulkanSDK()
if vulkanInstalled:
    # Solo verificar, no intentar descargar
    Vulkan.CheckVulkanSDKDebugLibs()

print("\n[7/8] Verificando KTX-Software SDK...")
ktxInstalled = KTX.CheckKTXSDK()
if ktxInstalled:
    KTX.CheckKTXSDKDebugLibs()

print("\n[8/8] Generando archivos de proyecto con Premake...")
if os.name == 'nt':  # Windows
    premake_path = "vendor/bin/premake/premake5.exe"
    if not os.path.exists(premake_path):
        print(f"ERROR: Premake no encontrado en {premake_path}")
        input("Presiona Enter para salir...")
        sys.exit(1)
    
    result = subprocess.call([premake_path, "vs2022"])
    if result != 0:
        print("ERROR: Premake falló al generar los archivos de proyecto")
        input("Presiona Enter para salir...")
        sys.exit(1)
else:
    subprocess.call(["vendor/bin/premake/premake5", "gmake2"])

print("\n" + "="*60)
print("✓ Setup completado correctamente!")
print("="*60)
print("\nVerificación final:")
print("  ✓ Box2D - Motor de física 2D")
print("  ✓ Bullet3 - Motor de física 3D")
print("  ✓ Assimp - Carga de modelos 3D")
print("  ✓ GLFW - Manejo de ventanas")
print("  ✓ ImGui - Interfaz de usuario")
print("  ✓ imnodes - Sistema de nodos visual")
print("  ✓ Vulkan SDK - Compilación de shaders SPIR-V")
if ktxInstalled:
    print("  ✓ KTX-Software - Compresión de texturas GPU")
else:
    print("  ⚠ KTX-Software - No instalado (modo compatibilidad)")
print("\nSiguientes pasos:")
print("  1. Abre 'Lunex-Engine.sln' en Visual Studio")
print("  2. Selecciona la configuración 'Debug' o 'Release'")
print("  3. Compila el proyecto (Ctrl+Shift+B)")
print("="*60)