@echo off
echo ========================================
echo   Limpieza de Assimp
echo ========================================
echo.
echo Este script limpiara la configuracion de Assimp
echo y te permitira volver a configurarlo desde cero.
echo.
pause

REM Eliminar config.h generado
if exist "vendor\assimp\include\assimp\config.h" (
    echo Eliminando config.h...
del "vendor\assimp\include\assimp\config.h"
    echo   OK
)

REM Limpiar el submodulo
echo.
echo Limpiando submodulo de Assimp...
git submodule deinit -f vendor/assimp
git rm -rf --cached vendor/assimp
if exist "vendor\assimp" (
    rmdir /s /q "vendor\assimp"
)
if exist ".git\modules\vendor\assimp" (
    rmdir /s /q ".git\modules\vendor\assimp"
)

echo.
echo Assimp ha sido limpiado completamente.
echo Para reinstalarlo, ejecuta Setup.bat
echo.
pause
