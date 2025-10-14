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

print("\n[2/4] Clonando submodulos...")
subprocess.call(["git", "submodule", "update", "--init", "--recursive"])

print("\n[3/4] Verificando Vulkan SDK...")
vulkanInstalled = Vulkan.CheckVulkanSDK()
if vulkanInstalled:
    Vulkan.CheckVulkanSDKDebugLibs()

print("\n[4/4] Generando archivos de proyecto con Premake...")
if os.name == 'nt':  # Windows
    subprocess.call(["vendor/bin/premake5.exe", "vs2022"])
else:
    subprocess.call(["vendor/bin/premake5", "gmake2"])

print("\n" + "="*50)
print("Setup completado correctamente!")
print("Abre 'Lunex-Engine.sln' para comenzar a trabajar.")
print("="*50)