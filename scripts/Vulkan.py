import os
import subprocess
import sys
from pathlib import Path

import Utils

from io import BytesIO
from urllib.request import urlopen
from zipfile import ZipFile

VULKAN_SDK = os.environ.get('VULKAN_SDK')
VULKAN_SDK_INSTALLER_URL = 'https://sdk.lunarg.com/sdk/download/1.3.290.0/windows/VulkanSDK-1.3.290.0-Installer.exe'
LUNEX_VULKAN_VERSION = '1.3.290'
VULKAN_SDK_EXE_PATH = 'Lunex/vendor/VulkanSDK/VulkanSDK.exe'

def InstallVulkanSDK():
    print(f'Downloading {VULKAN_SDK_INSTALLER_URL} to {VULKAN_SDK_EXE_PATH}')
    os.makedirs(os.path.dirname(VULKAN_SDK_EXE_PATH), exist_ok=True)
    Utils.DownloadFile(VULKAN_SDK_INSTALLER_URL, VULKAN_SDK_EXE_PATH)
    print("Done!")
    print("Running Vulkan SDK installer...")
    os.startfile(os.path.abspath(VULKAN_SDK_EXE_PATH))
    print("\n" + "="*60)
    print("IMPORTANTE: Despues de instalar Vulkan SDK:")
    print("  1. Cierra esta ventana")
    print("  2. Reinicia tu PC o abre una nueva terminal")
    print("  3. Vuelve a ejecutar Setup.bat")
    print("="*60)
    input("\nPresiona Enter para salir...")
    sys.exit(0)

def InstallVulkanPrompt():
    print("\nDeseas instalar el Vulkan SDK?")
    install = Utils.YesOrNo()
    if install:
        InstallVulkanSDK()
    else:
        print("\nADVERTENCIA: El proyecto requiere Vulkan SDK para compilar.")
        print("Puedes instalarlo manualmente desde: https://vulkan.lunarg.com/")
        return False
    return True

def CheckVulkanSDK():
    if VULKAN_SDK is None:
        print("\n" + "!"*60)
        print("ERROR: Vulkan SDK no esta instalado!")
        print("!"*60)
        return InstallVulkanPrompt()
    elif LUNEX_VULKAN_VERSION not in VULKAN_SDK:
        print(f"\nVulkan SDK encontrado en: {VULKAN_SDK}")
        print(f"ADVERTENCIA: Version incorrecta detectada!")
        print(f"  - Requerida: {LUNEX_VULKAN_VERSION}")
        print(f"  - Actual: {os.path.basename(VULKAN_SDK)}")
        print("\nPuedes continuar, pero pueden haber problemas de compatibilidad.")
        print("Deseas instalar la version correcta?")
        return InstallVulkanPrompt()
    
    print(f"✓ Vulkan SDK correcto encontrado: {VULKAN_SDK}")
    return True

def CheckVulkanSDKDebugLibs():
    """
    Las librerías de debug vienen incluidas en el instalador principal del Vulkan SDK.
    Esta función verifica que el SDK esté instalado correctamente.
    """
    if VULKAN_SDK is None:
        print("\n⚠️ Vulkan SDK no está instalado.")
        print("Las librerías de Vulkan no estarán disponibles hasta que lo instales.")
        return False
    
    # Verificar que existan las librerías básicas
    sdk_lib_path = Path(f"{VULKAN_SDK}/Lib")
    if not sdk_lib_path.exists():
        print(f"\n⚠️ Carpeta de librerías no encontrada: {sdk_lib_path}")
        print("Es posible que el Vulkan SDK no se haya instalado correctamente.")
        return False
    
    # Verificar algunas librerías clave
    required_libs = [
        "vulkan-1.lib",
        "shaderc_combined.lib"
    ]
    
    missing_libs = []
    for lib in required_libs:
        if not (sdk_lib_path / lib).exists():
            missing_libs.append(lib)
    
    if missing_libs:
        print(f"\n⚠️ Algunas librerías de Vulkan no están disponibles:")
        for lib in missing_libs:
            print(f"  - {lib}")
        print("\nNOTA: Esto no afectará la compilación actual ya que las librerías")
        print("      de Vulkan están deshabilitadas en premake5.lua")
        return False
    
    print(f"✓ Librerías de Vulkan SDK encontradas en: {sdk_lib_path}")
    return True