@echo off
echo ========================================
echo   Compilando Assimp desde Source
echo ========================================
echo.

REM Verificar que CMake esta instalado
cmake --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: CMake no esta instalado!
    echo Por favor instala CMake desde https://cmake.org/download/
    pause
    exit /b 1
)

REM Ir al directorio raiz del proyecto
cd /d "%~dp0.."
echo Directorio actual: %CD%

echo.
echo [1/4] Configurando proyecto con CMake...
cd vendor\assimp

REM Verificar que estamos en el directorio correcto
if not exist "CMakeLists.txt" (
  echo ERROR: No se encuentra CMakeLists.txt en vendor\assimp
    echo Asegurate de que el submodulo de Assimp este clonado correctamente
    cd ..\..
    pause
    exit /b 1
)

REM Limpiar cache anterior de CMake si existe (necesario al cambiar de version de VS)
if exist "build\CMakeCache.txt" (
    echo Limpiando cache de CMake anterior...
    del /Q "build\CMakeCache.txt" 2>nul
    rmdir /S /Q "build\CMakeFiles" 2>nul
)

REM Crear directorio de build
if not exist "build" mkdir build
cd build

echo.
echo Configurando con CMake (esto puede tardar unos minutos)...
cmake .. ^
    -G "Visual Studio 18 2026" ^
    -A x64 ^
-DCMAKE_BUILD_TYPE=Release ^
    -DASSIMP_BUILD_TESTS=OFF ^
    -DASSIMP_BUILD_ASSIMP_TOOLS=OFF ^
    -DASSIMP_BUILD_SAMPLES=OFF ^
    -DASSIMP_INSTALL=OFF ^
    -DBUILD_SHARED_LIBS=ON ^
    -DASSIMP_BUILD_ZLIB=ON ^
    -DASSIMP_NO_EXPORT=ON ^
    -DASSIMP_BUILD_ALL_IMPORTERS_BY_DEFAULT=OFF ^
    -DASSIMP_BUILD_OBJ_IMPORTER=ON ^
    -DASSIMP_BUILD_FBX_IMPORTER=ON ^
    -DASSIMP_BUILD_GLTF_IMPORTER=ON

if errorlevel 1 (
    echo.
    echo ERROR: CMake fallo al configurar el proyecto
    echo.
    echo Posibles soluciones:
    echo   1. Verifica que CMake este instalado correctamente
    echo   2. Verifica que Visual Studio 2026 este instalado
    echo   3. Ejecuta: git submodule update --init --recursive
    cd ..\..\..
    pause
    exit /b 1
)

echo.
echo [2/4] Compilando Assimp (Debug)...
echo Esto puede tardar varios minutos...
cmake --build . --config Debug --parallel

if errorlevel 1 (
    echo.
  echo ERROR: Fallo al compilar Debug
    cd ..\..\..
    pause
    exit /b 1
)

echo.
echo [3/4] Compilando Assimp (Release)...
echo Esto puede tardar varios minutos...
cmake --build . --config Release --parallel

if errorlevel 1 (
    echo.
    echo ERROR: Fallo al compilar Release
    cd ..\..\..
    pause
    exit /b 1
)

echo.
echo [4/4] Copiando archivos compilados...

REM Volver al directorio de assimp
cd ..

REM Crear directorios de destino
if not exist "lib\Debug" mkdir lib\Debug
if not exist "lib\Release" mkdir lib\Release

REM Buscar y copiar archivos compilados
echo.
echo Buscando archivos compilados...

REM Para Debug
if exist "build\bin\Debug\assimp-vc145-mtd.dll" (
    copy /Y "build\bin\Debug\assimp-vc145-mtd.dll" "lib\Debug\"
    echo   OK Copiado: assimp-vc145-mtd.dll (Debug)
) else if exist "build\bin\Debug\assimp.dll" (
    copy /Y "build\bin\Debug\assimp.dll" "lib\Debug\assimp-vc145-mtd.dll"
    echo   OK Copiado: assimp.dll como assimp-vc145-mtd.dll (Debug)
) else (
    echo   ! No se encontro assimp-vc145-mtd.dll en build\bin\Debug\
)

if exist "build\lib\Debug\assimp-vc145-mtd.lib" (
    copy /Y "build\lib\Debug\assimp-vc145-mtd.lib" "lib\Debug\"
    echo   OK Copiado: assimp-vc145-mtd.lib (Debug)
) else if exist "build\lib\Debug\assimp.lib" (
    copy /Y "build\lib\Debug\assimp.lib" "lib\Debug\assimp-vc145-mtd.lib"
    echo   OK Copiado: assimp.lib como assimp-vc145-mtd.lib (Debug)
) else (
    echo   ! No se encontro assimp-vc145-mtd.lib en build\lib\Debug\
)

REM Para Release
if exist "build\bin\Release\assimp-vc145-mt.dll" (
  copy /Y "build\bin\Release\assimp-vc145-mt.dll" "lib\Release\"
    echo   OK Copiado: assimp-vc145-mt.dll (Release)
) else if exist "build\bin\Release\assimp.dll" (
  copy /Y "build\bin\Release\assimp.dll" "lib\Release\assimp-vc145-mt.dll"
    echo   OK Copiado: assimp.dll como assimp-vc145-mt.dll (Release)
) else (
    echo   ! No se encontro assimp-vc145-mt.dll en build\bin\Release\
)

if exist "build\lib\Release\assimp-vc145-mt.lib" (
    copy /Y "build\lib\Release\assimp-vc145-mt.lib" "lib\Release\"
    echo   OK Copiado: assimp-vc145-mt.lib (Release)
) else if exist "build\lib\Release\assimp.lib" (
    copy /Y "build\lib\Release\assimp.lib" "lib\Release\assimp-vc145-mt.lib"
    echo   OK Copiado: assimp.lib como assimp-vc145-mt.lib (Release)
) else (
    echo   ! No se encontro assimp-vc145-mt.lib en build\lib\Release\
)

cd ..\..

echo.
echo ========================================
echo   Assimp compilado exitosamente!
echo ========================================
echo.
echo Archivos generados:
dir /B vendor\assimp\lib\Debug\*.lib 2>nul
dir /B vendor\assimp\lib\Debug\*.dll 2>nul
dir /B vendor\assimp\lib\Release\*.lib 2>nul
dir /B vendor\assimp\lib\Release\*.dll 2>nul
echo.
echo Ubicacion: vendor\assimp\lib\
echo.
echo Siguiente paso:
echo   1. Ejecuta: vendor\bin\premake\premake5.exe vs2026
echo   2. Abre Lunex-Engine.sln y compila
echo.
pause
