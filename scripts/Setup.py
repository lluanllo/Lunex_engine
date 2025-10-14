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

print("\n[2/4] Clonando/actualizando submodulos...")

# Primero intentar inicializar submódulos
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
    
    if result != 0:
        print("\n" + "!"*60)
        print("ERROR: No se pudieron clonar los submódulos automáticamente.")
        print("Intentando clonar manualmente...")
        print("!"*60)
        
        # Clonar manualmente cada submódulo
        submodules = [
            ("vendor/GLFW", "https://github.com/glfw/glfw.git", "master"),
            ("vendor/ImGuiLib", "https://github.com/ocornut/imgui.git", "docking"),
            ("vendor/glm", "https://github.com/g-truc/glm.git", "master"),
            ("vendor/ImGuizmo", "https://github.com/CedricGuillemet/ImGuizmo.git", "master"),
            ("vendor/yaml-cpp", "https://github.com/jbeder/yaml-cpp.git", "master"),
        ]
        
        for path, url, branch in submodules:
            if os.path.exists(path):
                print(f"\nLimpiando {path}...")
                import shutil
                try:
                    shutil.rmtree(path)
                except Exception as e:
                    print(f"  Error al eliminar: {e}")
            
            print(f"\nClonando {path}...")
            result = subprocess.call(["git", "clone", "--depth", "1", "-b", branch, url, path])
            if result == 0:
                print(f"  ✓ {path} clonado correctamente")
            else:
                print(f"  ✗ Error al clonar {path}")

# Verificar que los submódulos se clonaron correctamente
print("\nVerificando submódulos...")
missing_submodules = []
required_files = {
    "vendor/GLFW/include/GLFW/glfw3.h": "GLFW",
    "vendor/ImGuiLib/imgui.h": "ImGui",
    "vendor/glm/glm/glm.hpp": "GLM",
    "vendor/ImGuizmo/ImGuizmo.h": "ImGuizmo",
    "vendor/yaml-cpp/include/yaml-cpp/yaml.h": "yaml-cpp",
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
    print("\nIntenta clonar el repositorio de nuevo con:")
    print("  git clone --recursive https://github.com/lluanllo/Stellara_engine.git")
    print("!"*60)
    input("\nPresiona Enter para continuar de todos modos...")
else:
    print("\n✓ Todos los submódulos están correctos")

print("\n[3/4] Verificando Vulkan SDK...")
vulkanInstalled = Vulkan.CheckVulkanSDK()
if vulkanInstalled:
    Vulkan.CheckVulkanSDKDebugLibs()

print("\n[4/4] Generando archivos de proyecto con Premake...")
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
print("\nSiguientes pasos:")
print("  1. Abre 'Lunex-Engine.sln' en Visual Studio")
print("  2. Selecciona la configuración 'Debug' o 'Release'")
print("  3. Compila el proyecto (Ctrl+Shift+B)")
print("="*60)