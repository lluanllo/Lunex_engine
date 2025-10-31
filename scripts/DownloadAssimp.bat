@echo off
echo ========================================
echo   Descarga de Assimp Precompilado
echo ========================================
echo.

python scripts\DownloadAssimp.py

if errorlevel 1 (
    echo.
    echo ERROR: Fallo la configuracion de Assimp
    pause
    exit /b 1
)

echo.
echo OK Assimp esta listo!
echo.
pause
