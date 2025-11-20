@echo off
echo ========================================
echo   Lunex Engine - Setup
echo ========================================
echo.
echo Este script configurara:
echo   - Submodulos de Git (GLFW, ImGui, Box2D, Assimp, etc.)
echo   - Vulkan SDK
echo   - Assimp (carga de modelos 3D)
echo   - Proyectos de Visual Studio
echo.

REM Verificar Python
python --version >nul 2>&1
if errorlevel 1 (
    echo ERROR: Python no esta instalado!
    echo Por favor instala Python desde https://www.python.org/
    pause
    exit /b 1
)

echo [1/4] Verificando dependencias de Python...
python Setup.py

echo.
echo Setup completado!
pause