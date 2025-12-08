import os
import sys
import urllib.request
import zipfile
import shutil

def download_file(url, destination):
    """Descarga un archivo con barra de progreso"""
    print(f"Descargando desde: {url}")
    
    def reporthook(count, block_size, total_size):
        percent = int(count * block_size * 100 / total_size)
        sys.stdout.write(f"\r{percent}% completado")
        sys.stdout.flush()
    
    urllib.request.urlretrieve(url, destination, reporthook)
    print("\n? Descarga completada")

def extract_zip(zip_path, extract_to):
    """Extrae un archivo ZIP"""
    print(f"Extrayendo a: {extract_to}")
    with zipfile.ZipFile(zip_path, 'r') as zip_ref:
        zip_ref.extractall(extract_to)
    print("? Extracción completada")

def setup_assimp_prebuilt():
    print("="*60)
    print("  Descarga de Assimp Precompilado")
    print("="*60)
    print()
    
    # Cambiar al directorio raíz del proyecto
    script_dir = os.path.dirname(os.path.realpath(__file__))
    os.chdir(os.path.join(script_dir, '../'))
    
    assimp_dir = "vendor/assimp"
    lib_dir = f"{assimp_dir}/lib"
    temp_dir = "temp_assimp"
    
    # URL de descarga (Assimp 5.3.1 precompilado para VS2022)
    # Nota: Esta es una URL de ejemplo. Deberás encontrar la URL correcta de las releases
    assimp_version = "5.3.1"
    
    print("IMPORTANTE:")
    print("Este script necesita que descargues manualmente las librerías precompiladas.")
    print()
    print("Sigue estos pasos:")
    print()
    print("1. Ve a: https://github.com/assimp/assimp/releases/latest")
    print()
    print("2. Busca y descarga uno de estos archivos:")
    print("   - assimp-windows-x64.zip")
    print("   - assimp-sdk-{version}-setup.exe (ejecutalo y anota donde se instala)")
    print("   - O busca archivos que contengan 'vc143' o 'vs2022'")
    print()
    print("3. Necesitas estos archivos específicos:")
    print("   Debug:")
    print("     - assimp-vc143-mtd.lib")
    print("     - assimp-vc143-mtd.dll")
    print("   Release:")
    print("     - assimp-vc143-mt.lib")
    print("     - assimp-vc143-mt.dll")
    print()
    print("4. Cópialos a:")
    print(f"   {os.path.abspath(lib_dir)}/Debug/")
    print(f"   {os.path.abspath(lib_dir)}/Release/")
    print()
    
    # Crear directorios si no existen
    os.makedirs(f"{lib_dir}/Debug", exist_ok=True)
    os.makedirs(f"{lib_dir}/Release", exist_ok=True)
    
    print("="*60)
    print()
    
    input("Presiona Enter cuando hayas copiado los archivos...")
    
    # Verificar que los archivos existan
    required_files = [
        f"{lib_dir}/Debug/assimp-vc143-mtd.lib",
        f"{lib_dir}/Debug/assimp-vc143-mtd.dll",
        f"{lib_dir}/Release/assimp-vc143-mt.lib",
        f"{lib_dir}/Release/assimp-vc143-mt.dll"
    ]
    
    missing_files = []
    for file in required_files:
        if not os.path.exists(file):
            missing_files.append(file)
    
    if missing_files:
        print()
        print("? ERROR: Faltan algunos archivos:")
        for file in missing_files:
            print(f"  - {file}")
        print()
        print("Por favor, copia todos los archivos necesarios y vuelve a ejecutar este script.")
        return False
    
    print()
    print("="*60)
    print("? Assimp configurado correctamente!")
    print("="*60)
    print()
    print("Archivos encontrados:")
    for file in required_files:
        print(f"  ? {os.path.basename(file)}")
    print()
    print("Siguiente paso:")
    print("  1. Ejecuta: vendor\\bin\\premake\\premake5.exe vs2022")
    print("  2. Abre Lunex-Engine.sln y compila")
    print()
    
    return True

if __name__ == "__main__":
    try:
        setup_assimp_prebuilt()
    except Exception as e:
        print(f"\n? Error: {e}")
        import traceback
        traceback.print_exc()
    
    input("\nPresiona Enter para salir...")
