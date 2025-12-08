# -*- coding: utf-8 -*-
import os
import subprocess
import sys
from pathlib import Path
import shutil

import Utils

# KTX-Software configuration
LUNEX_KTX_VERSION = '4.3.2'
KTX_SDK_LOCAL_PATH = 'vendor/ktx'

# GitHub source archive URL (contains headers)
KTX_SOURCE_URL = f'https://github.com/KhronosGroup/KTX-Software/archive/refs/tags/v{LUNEX_KTX_VERSION}.zip'

# Windows installer (contains binaries)
KTX_INSTALLER_URL = f'https://github.com/KhronosGroup/KTX-Software/releases/download/v{LUNEX_KTX_VERSION}/KTX-Software-{LUNEX_KTX_VERSION}-Windows-x64.exe'

def GetKTXSDKPath():
    """Busca el SDK de KTX en ubicaciones conocidas"""
    # Buscar en vendor/ktx (instalacion local del proyecto)
    local_ktx = os.path.abspath(KTX_SDK_LOCAL_PATH)
    if os.path.exists(local_ktx) and os.path.exists(os.path.join(local_ktx, 'include', 'ktx.h')):
        return local_ktx
    
    # Verificar variable de entorno
    ktx_sdk = os.environ.get('KTX_SDK')
    if ktx_sdk and os.path.exists(ktx_sdk) and os.path.exists(os.path.join(ktx_sdk, 'include', 'ktx.h')):
        return ktx_sdk
    
    return None

KTX_SDK = GetKTXSDKPath()

def DownloadAndExtractSource():
    """Descarga el source code y extrae los headers"""
    import zipfile
    import urllib.request
    
    zip_path = f'{KTX_SDK_LOCAL_PATH}/ktx-source.zip'
    
    print(f'Descargando KTX-Software source (para headers)...')
    
    os.makedirs(KTX_SDK_LOCAL_PATH, exist_ok=True)
    
    try:
        Utils.DownloadFile(KTX_SOURCE_URL, zip_path)
        print("\nDescarga completada!")
    except Exception as e:
        print(f"\nError al descargar: {e}")
        return False
    
    print(f'\nExtrayendo headers...')
    try:
        with zipfile.ZipFile(zip_path, 'r') as zip_ref:
            # Extract only what we need
            for member in zip_ref.namelist():
                # Extract include folder
                if '/include/' in member or '/lib/' in member:
                    zip_ref.extract(member, KTX_SDK_LOCAL_PATH)
        
        # Move files to correct location
        source_dir = f'{KTX_SDK_LOCAL_PATH}/KTX-Software-{LUNEX_KTX_VERSION}'
        
        # Create include directory
        include_dest = f'{KTX_SDK_LOCAL_PATH}/include'
        if os.path.exists(f'{source_dir}/include'):
            if os.path.exists(include_dest):
                shutil.rmtree(include_dest)
            shutil.move(f'{source_dir}/include', include_dest)
        
        # Create lib directory structure
        lib_dest = f'{KTX_SDK_LOCAL_PATH}/lib'
        os.makedirs(lib_dest, exist_ok=True)
        
        # Cleanup extracted source folder
        if os.path.exists(source_dir):
            shutil.rmtree(source_dir)
        
        print("Headers extraidos!")
        
    except Exception as e:
        print(f"Error al extraer: {e}")
        return False
    finally:
        # Cleanup zip
        if os.path.exists(zip_path):
            os.remove(zip_path)
    
    return True

def DownloadBinaries():
    """Descarga los binarios precompilados desde el instalador"""
    print("\n" + "="*60)
    print("IMPORTANTE: Instalacion de binarios KTX")
    print("="*60)
    print()
    print("El instalador oficial de KTX-Software se abrira.")
    print("Por favor:")
    print("  1. Instala en la ubicacion por defecto (C:\\Program Files\\KTX-Software)")
    print("  2. Completa la instalacion")
    print("  3. Vuelve aqui y presiona Enter")
    print()
    
    installer_path = f'{KTX_SDK_LOCAL_PATH}/KTX-Installer.exe'
    
    try:
        Utils.DownloadFile(KTX_INSTALLER_URL, installer_path)
        print("Instalador descargado, abriendo...")
        os.startfile(os.path.abspath(installer_path))
    except Exception as e:
        print(f"Error: {e}")
        print(f"\nDescarga manualmente desde:")
        print(f"  {KTX_INSTALLER_URL}")
        return False
    
    input("\nPresiona Enter cuando hayas completado la instalacion...")
    
    # Copy DLL and LIB from Program Files
    program_files_ktx = "C:/Program Files/KTX-Software"
    
    if not os.path.exists(f"{program_files_ktx}/bin/ktx.dll"):
        print("\nNo se encontro ktx.dll en la instalacion.")
        print("Asegurate de haber instalado KTX-Software correctamente.")
        return False
    
    # Create bin and lib directories
    bin_dest = f'{KTX_SDK_LOCAL_PATH}/bin'
    lib_dest = f'{KTX_SDK_LOCAL_PATH}/lib'
    os.makedirs(bin_dest, exist_ok=True)
    os.makedirs(lib_dest, exist_ok=True)
    
    # Copy DLL
    print("\nCopiando binarios...")
    shutil.copy(f"{program_files_ktx}/bin/ktx.dll", f"{bin_dest}/ktx.dll")
    print(f"  Copiado: ktx.dll")
    
    # The installer doesn't include .lib files, we need to generate them
    # For now, we'll create a placeholder and note that linking will use the DLL directly
    
    print("\nNOTA: El instalador oficial no incluye ktx.lib")
    print("      Se usara carga dinamica de la DLL.")
    
    return True

def CreateLibFromDLL():
    """Intenta crear ktx.lib desde ktx.dll usando herramientas de VS"""
    lib_dest = f'{KTX_SDK_LOCAL_PATH}/lib'
    bin_path = f'{KTX_SDK_LOCAL_PATH}/bin/ktx.dll'
    
    if not os.path.exists(bin_path):
        return False
    
    # Try to find Visual Studio tools
    vs_paths = [
        r"C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\MSVC",
        r"C:\Program Files\Microsoft Visual Studio\2022\Professional\VC\Tools\MSVC",
        r"C:\Program Files\Microsoft Visual Studio\2022\Enterprise\VC\Tools\MSVC",
    ]
    
    dumpbin_path = None
    lib_tool_path = None
    
    for vs_base in vs_paths:
        if os.path.exists(vs_base):
            # Find latest MSVC version
            versions = sorted(os.listdir(vs_base), reverse=True)
            if versions:
                tools_path = os.path.join(vs_base, versions[0], "bin", "Hostx64", "x64")
                if os.path.exists(os.path.join(tools_path, "dumpbin.exe")):
                    dumpbin_path = os.path.join(tools_path, "dumpbin.exe")
                    lib_tool_path = os.path.join(tools_path, "lib.exe")
                    break
    
    if not dumpbin_path:
        print("  No se encontraron herramientas de Visual Studio para crear .lib")
        return False
    
    try:
        # Generate .def file from DLL exports
        print("  Generando ktx.lib desde ktx.dll...")
        
        result = subprocess.run(
            [dumpbin_path, "/EXPORTS", bin_path],
            capture_output=True,
            text=True
        )
        
        # Parse exports
        exports = []
        in_exports = False
        for line in result.stdout.split('\n'):
            if 'ordinal' in line.lower() and 'name' in line.lower():
                in_exports = True
                continue
            if in_exports:
                parts = line.split()
                if len(parts) >= 4:
                    exports.append(parts[3])
        
        if not exports:
            print("  No se encontraron exports en la DLL")
            return False
        
        # Write .def file
        def_path = f'{lib_dest}/ktx.def'
        with open(def_path, 'w') as f:
            f.write("LIBRARY ktx\n")
            f.write("EXPORTS\n")
            for exp in exports:
                f.write(f"    {exp}\n")
        
        # Generate .lib from .def
        lib_path = f'{lib_dest}/ktx.lib'
        result = subprocess.run(
            [lib_tool_path, f"/DEF:{def_path}", f"/OUT:{lib_path}", "/MACHINE:X64"],
            capture_output=True,
            text=True
        )
        
        if os.path.exists(lib_path):
            print(f"  ktx.lib creado exitosamente!")
            return True
        else:
            print(f"  Error al crear ktx.lib: {result.stderr}")
            return False
            
    except Exception as e:
        print(f"  Error: {e}")
        return False

def InstallKTXSDK():
    """Instala KTX-Software SDK completo"""
    print("Instalando KTX-Software SDK...")
    print()
    
    # Step 1: Download headers from source
    if not DownloadAndExtractSource():
        return False
    
    # Step 2: Download and install binaries
    if not DownloadBinaries():
        return False
    
    # Step 3: Try to create .lib from DLL
    CreateLibFromDLL()
    
    # Verify installation
    global KTX_SDK
    KTX_SDK = GetKTXSDKPath()
    
    if KTX_SDK:
        print(f"\nKTX-Software SDK instalado en: {KTX_SDK}")
        SaveKTXPath(KTX_SDK)
        return True
    else:
        print("\nError: No se pudo verificar la instalacion")
        return False

def InstallKTXPrompt():
    """Pregunta al usuario si desea instalar KTX"""
    print("\nDeseas instalar KTX-Software SDK?")
    print("(Necesario para compresion de texturas GPU - BC7, ASTC, ETC2)")
    print(f"Se instalara en: {os.path.abspath(KTX_SDK_LOCAL_PATH)}")
    install = Utils.YesOrNo()
    if install:
        return InstallKTXSDK()
    else:
        print("\nADVERTENCIA: Sin KTX-Software, la compresion de texturas")
        print("             usara un modo de compatibilidad (sin compresion real).")
        return False

def CheckKTXSDK():
    """Verifica si KTX-Software esta instalado"""
    global KTX_SDK
    KTX_SDK = GetKTXSDKPath()
    
    if KTX_SDK is None:
        print("\n" + "!"*60)
        print("WARNING: KTX-Software SDK no esta instalado!")
        print("!"*60)
        print("\nKTX-Software es OPCIONAL pero recomendado para:")
        print("  - Compresion de texturas GPU (BC7, ASTC, ETC2)")
        print("  - Reduccion de memoria VRAM hasta 75%")
        print("  - Carga mas rapida de texturas")
        return InstallKTXPrompt()
    
    print(f"KTX-Software SDK encontrado: {KTX_SDK}")
    
    # Guardar path para premake
    SaveKTXPath(KTX_SDK)
    
    return True

def SaveKTXPath(ktx_path):
    """Guarda el path de KTX para que premake lo use"""
    config_path = 'ktx_config.lua'
    
    # Normalizar path para Lua (usar forward slashes)
    normalized_path = ktx_path.replace('\\', '/')
    
    with open(config_path, 'w', encoding='utf-8') as f:
        f.write(f'-- Auto-generated by KTX.py\n')
        f.write(f'KTX_SDK_PATH = "{normalized_path}"\n')
    
    print(f"  Configuracion guardada en {config_path}")

def CheckKTXSDKDebugLibs():
    """Verifica las librerias de KTX"""
    if KTX_SDK is None:
        return False
    
    # Verificar include
    include_path = Path(f"{KTX_SDK}/include")
    if not (include_path / "ktx.h").exists():
        print(f"\n  ktx.h no encontrado en: {include_path}")
        return False
    print(f"  Headers encontrados en: {include_path}")
    
    # Verificar DLL
    bin_path = Path(f"{KTX_SDK}/bin")
    if (bin_path / "ktx.dll").exists():
        print(f"  ktx.dll encontrado en: {bin_path}")
    else:
        print(f"  ADVERTENCIA: ktx.dll no encontrado")
    
    # Verificar lib
    lib_path = Path(f"{KTX_SDK}/lib")
    if (lib_path / "ktx.lib").exists():
        print(f"  ktx.lib encontrado en: {lib_path}")
    else:
        print(f"  ADVERTENCIA: ktx.lib no encontrado (se usara carga dinamica)")
    
    return True

if __name__ == "__main__":
    print("="*60)
    print("  KTX-Software SDK Setup")
    print("="*60)
    print()
    
    # Cambiar al directorio raiz
    script_dir = os.path.dirname(os.path.realpath(__file__))
    os.chdir(os.path.join(script_dir, '../'))
    
    if CheckKTXSDK():
        CheckKTXSDKDebugLibs()
        print("\n" + "="*60)
        print("KTX-Software configurado correctamente!")
        print("="*60)
        print("\nSiguientes pasos:")
        print("  1. Ejecuta: vendor\\bin\\premake\\premake5.exe vs2022")
        print("  2. Recompila el proyecto en Visual Studio (Rebuild)")
    else:
        print("\n" + "="*60)
        print("KTX-Software no esta configurado")
        print("="*60)
        print("  La compresion de texturas funcionara en modo compatibilidad")
    
    input("\nPresiona Enter para salir...")
