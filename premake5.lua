workspace "Lunex-Engine"
    architecture "x86_64"
    startproject "Lunex-Editor"

    configurations {
        "Debug",
        "Release",
        "Dist"
    }

    flags {
        "MultiProcessorCompile"
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

VULKAN_SDK = os.getenv("VULKAN_SDK")

-- ============================================================================
-- KTX-Software SDK Detection
-- ============================================================================
KTX_SDK_PATH = nil

-- Try to load KTX config from Python setup script
if os.isfile("ktx_config.lua") then
    dofile("ktx_config.lua")
end

-- If not found, try common installation paths
if KTX_SDK_PATH == nil then
    local ktx_paths = {
        "C:/Program Files/KTX-Software",
        "C:/Program Files/KTX-Software-4.3.2",
        os.getenv("KTX_SDK") or ""
    }
    
    for _, path in ipairs(ktx_paths) do
        if os.isdir(path) and os.isfile(path .. "/include/ktx.h") then
            KTX_SDK_PATH = path
            break
        end
    end
end

-- KTX availability flag
KTX_AVAILABLE = KTX_SDK_PATH ~= nil

if KTX_AVAILABLE then
    print("KTX-Software SDK found: " .. KTX_SDK_PATH)
else
    print("KTX-Software SDK not found - texture compression will use compatibility mode")
end

-- Include directories relative to the solution directory
IncludeDir = {}
IncludeDir["GLFW"]      = "vendor/GLFW/include"
IncludeDir["Glad"]      = "vendor/Glad/include"
IncludeDir["ImGui"]     = "vendor/ImGuiLib"
IncludeDir["glm"]       = "vendor/glm"
IncludeDir["stb_image"] = "vendor/stb_image"
IncludeDir["entt"]      = "vendor/entt/include"
IncludeDir["yaml_cpp"]  = "vendor/yaml-cpp/include"
IncludeDir["Box2D"]     = "vendor/Box2D/include" 
IncludeDir["ImGuizmo"]  = "vendor/ImGuizmo"
IncludeDir["Assimp"]    = "vendor/assimp/include"
IncludeDir["Bullet3"]   = "vendor/Bullet3/src"
IncludeDir["imnodes"]   = "vendor/imnodes"
IncludeDir["Lunex"]     = "Lunex/src"
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"

-- KTX include directory (only if available)
if KTX_AVAILABLE then
    IncludeDir["KTX"] = KTX_SDK_PATH .. "/include"
end

-- ✅ Librerías de Vulkan y Assimp
LibraryDir = {}
LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"
LibraryDir["Assimp"]    = "vendor/assimp/lib"

-- KTX library directory (only if available)
if KTX_AVAILABLE then
    LibraryDir["KTX"] = KTX_SDK_PATH .. "/lib"
end

Library = {}
Library["Vulkan"] = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"

-- ✅ Usar versiones específicas para Debug y Release (corrige nombres)
Library["ShaderC_Debug"] = "%{LibraryDir.VulkanSDK}/shaderc_combinedd.lib"
Library["ShaderC_Release"] = "%{LibraryDir.VulkanSDK}/shaderc_combined.lib"

Library["SPIRV_Cross_Debug"] = "%{LibraryDir.VulkanSDK}/spirv-cross-cored.lib"
Library["SPIRV_Cross_Release"] = "%{LibraryDir.VulkanSDK}/spirv-cross-core.lib"

Library["SPIRV_Cross_GLSL_Debug"] = "%{LibraryDir.VulkanSDK}/spirv-cross-glsld.lib"
Library["SPIRV_Cross_GLSL_Release"] = "%{LibraryDir.VulkanSDK}/spirv-cross-glsl.lib"

-- Assimp libraries
Library["Assimp_Debug_Name"] = "assimp-vc143-mtd"
Library["Assimp_Release_Name"] = "assimp-vc143-mt"

-- KTX-Software libraries
Library["KTX_Name"] = "ktx"

-- ============================================================================
-- DEPENDENCIES
-- ============================================================================
group "Dependencies"

-- GLFW Project
project "GLFW"
    kind "StaticLib"
    language "C"
    staticruntime "off"
    warnings "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "vendor/GLFW/include/GLFW/glfw3.h",
        "vendor/GLFW/include/GLFW/glfw3native.h",
        "vendor/GLFW/src/glfw_config.h",
        "vendor/GLFW/src/context.c",
        "vendor/GLFW/src/init.c",
        "vendor/GLFW/src/input.c",
        "vendor/GLFW/src/monitor.c",
        "vendor/GLFW/src/null_init.c",
        "vendor/GLFW/src/null_joystick.c",
        "vendor/GLFW/src/null_monitor.c",
        "vendor/GLFW/src/null_window.c",
        "vendor/GLFW/src/platform.c",
        "vendor/GLFW/src/vulkan.c",
        "vendor/GLFW/src/window.c",
    }

    filter "system:windows"
        systemversion "latest"

        files
        {
            "vendor/GLFW/src/win32_init.c",
            "vendor/GLFW/src/win32_joystick.c",
            "vendor/GLFW/src/win32_module.c",
            "vendor/GLFW/src/win32_monitor.c",
            "vendor/GLFW/src/win32_time.c",
            "vendor/GLFW/src/win32_thread.c",
            "vendor/GLFW/src/win32_window.c",
            "vendor/GLFW/src/wgl_context.c",
            "vendor/GLFW/src/egl_context.c",
            "vendor/GLFW/src/osmesa_context.c"
        }

        defines 
        { 
            "_GLFW_WIN32",
            "_CRT_SECURE_NO_WARNINGS"
        }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter "configurations:Dist"
        runtime "Release"
        optimize "on"

-- Glad Project
project "Glad"
    kind "StaticLib"
    language "C"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "vendor/Glad/include/glad/glad.h",
        "vendor/Glad/include/KHR/khrplatform.h",
        "vendor/Glad/src/glad.c",
    }

    includedirs
    {
        "vendor/Glad/include",
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter "configurations:Dist"
        runtime "Release"
        optimize "on"

-- ImGui Project
project "ImGui"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "vendor/ImGuiLib/imconfig.h",
        "vendor/ImGuiLib/imgui.h",
        "vendor/ImGuiLib/imgui.cpp",
        "vendor/ImGuiLib/imgui_draw.cpp",
        "vendor/ImGuiLib/imgui_internal.h",
        "vendor/ImGuiLib/imgui_tables.cpp",
        "vendor/ImGuiLib/imgui_widgets.cpp",
        "vendor/ImGuiLib/imstb_rectpack.h",
        "vendor/ImGuiLib/imstb_textedit.h",
        "vendor/ImGuiLib/imstb_truetype.h",
        "vendor/ImGuiLib/imgui_demo.cpp"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter "configurations:Dist"
        runtime "Release"
        optimize "on"

-- yaml-cpp Project
project "yaml-cpp"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "vendor/yaml-cpp/src/**.h",
        "vendor/yaml-cpp/src/**.cpp",
        "vendor/yaml-cpp/include/**.h"
    }

    includedirs
    {
        "vendor/yaml-cpp/include"
    }

    defines
    {
        "YAML_CPP_STATIC_DEFINE"
    }

    filter "system:windows"
        systemversion "latest"

    filter "system:linux"
        pic "On"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter "configurations:Dist"
        runtime "Release"
        optimize "on"

-- Box2D Project
project "Box2D"
    kind "StaticLib"
    language "C"
    cdialect "C11"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "vendor/Box2D/src/**.h",
        "vendor/Box2D/src/**.c",
        "vendor/Box2D/include/**.h"
    }

    includedirs
    {
        "vendor/Box2D/include",
        "vendor/Box2D/src"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter "configurations:Dist"
        runtime "Release"
        optimize "on"

-- Bullet3 Project (actually compiles Bullet2 - the stable version)
project "Bullet3"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"
    warnings "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "vendor/Bullet3/src/BulletCollision/**.cpp",
        "vendor/Bullet3/src/BulletCollision/**.h",
        "vendor/Bullet3/src/BulletDynamics/**.cpp",
        "vendor/Bullet3/src/BulletDynamics/**.h",
        "vendor/Bullet3/src/BulletSoftBody/**.cpp",
        "vendor/Bullet3/src/BulletSoftBody/**.h",
        "vendor/Bullet3/src/LinearMath/**.cpp",
        "vendor/Bullet3/src/LinearMath/**.h"
    }

    includedirs
    {
        "vendor/Bullet3/src"
    }

    defines
    {
        "_CRT_SECURE_NO_WARNINGS"
    }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "speed"

    filter "configurations:Dist"
        runtime "Release"
        optimize "speed"

group ""

-- ============================================================================
-- SCRIPTING CORE
-- ============================================================================

project "Lunex-ScriptCore"
    location "Lunex-ScriptCore"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }

    includedirs
    {
        "%{prj.name}/src",
        "Lunex/src",
        "vendor/spdlog/include",
        "%{IncludeDir.entt}",
        "%{IncludeDir.glm}"
    }

    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8" }

    filter "configurations:Debug"
        runtime "Debug"
        symbols "on"

    filter "configurations:Release"
        runtime "Release"
        optimize "on"

    filter "configurations:Dist"
        runtime "Release"
        optimize "on"

-- ============================================================================
-- MAIN PROJECTS
-- ============================================================================

project "Lunex"
    location "Lunex"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    pchheader "stpch.h"
    pchsource "Lunex/src/stpch.cpp"

    -- LUNEX project files section
    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp",
        "%{prj.name}/vendor/stb_image/**.h",
        "%{prj.name}/vendor/stb_image/**.cpp",
        "%{prj.name}/src/**.hpp",
        "%{prj.name}/src/**.c",
        "%{prj.name}/src/**.inl",
        "vendor/ImGuizmo/ImGuizmo.h",
        "vendor/ImGuizmo/ImGuizmo.cpp",
        "vendor/imnodes/imnodes.h",
        "vendor/imnodes/imnodes_internal.h",
        "vendor/imnodes/imnodes.cpp",
        "%{prj.name}/assets/**.glsl",
        "%{prj.name}/assets/**.lunex",
        "%{prj.name}/assets/**.png"
    }

    defines
    {
        "_CRT_SECURE_NO_WARNINGS",
        "GLFW_INCLUDE_NONE",
        "YAML_CPP_STATIC_DEFINE"
    }

    -- Add KTX define if available
    filter {}
    if KTX_AVAILABLE then
        defines { "LNX_HAS_KTX" }
    end

    -- LUNEX project includedirs section
    includedirs
    {
        "%{prj.name}/src",
        "vendor/spdlog/include",
        "%{IncludeDir.Box2D}",
        "%{IncludeDir.Bullet3}",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.Glad}",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.stb_image}",
        "%{IncludeDir.entt}",
        "%{IncludeDir.yaml_cpp}",
        "%{IncludeDir.ImGuizmo}",
        "%{IncludeDir.imnodes}",
        "%{IncludeDir.Assimp}",
        "%{IncludeDir.VulkanSDK}",
        "Lunex-ScriptCore/src"
    }

    -- Add KTX include if available
    if KTX_AVAILABLE then
        includedirs { IncludeDir["KTX"] }
    end

    links
    {
        "Box2D",
        "Bullet3",
        "GLFW",
        "Glad",
        "ImGui",
        "yaml-cpp",
        "Lunex-ScriptCore",
        "opengl32.lib"
    }

    filter "files:vendor/ImGuizmo/**.cpp"
        flags { "NoPCH" }

    filter "files:vendor/imnodes/**.cpp"
        flags { "NoPCH" }
    
    filter "files:vendor/Bullet3/**.cpp"
        flags { "NoPCH" }

    filter "files:%{prj.name}/src/Physics/**.cpp"
        flags { "NoPCH" }

    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8" }

    filter "configurations:Debug"
        defines { "LN_DEBUG" }
        runtime "Debug"
        symbols "on"
        libdirs { "$(SolutionDir)vendor/assimp/lib/Debug" }
        links
        {
            "%{Library.ShaderC_Debug}",
            "%{Library.SPIRV_Cross_Debug}",
            "%{Library.SPIRV_Cross_GLSL_Debug}",
            "%{Library.Assimp_Debug_Name}"
        }
        -- Add KTX library if available
        if KTX_AVAILABLE then
            libdirs { LibraryDir["KTX"] }
            links { "%{Library.KTX_Name}" }
        end
        postbuildcommands
        {
            "{COPY} $(SolutionDir)vendor/assimp/lib/Debug/assimp-vc143-mtd.dll %{cfg.targetdir}"
        }
        -- Copy KTX DLL if available
        if KTX_AVAILABLE then
            postbuildcommands
            {
                "{COPY} \"" .. KTX_SDK_PATH .. "/bin/ktx.dll\" \"%{cfg.targetdir}\""
            }
        end

    filter "configurations:Release"
        defines { "LN_RELEASE" }
        runtime "Release"
        optimize "on"
        libdirs { "$(SolutionDir)vendor/assimp/lib/Release" }
        links
        {
            "%{Library.ShaderC_Release}",
            "%{Library.SPIRV_Cross_Release}",
            "%{Library.SPIRV_Cross_GLSL_Release}",
            "%{Library.Assimp_Release_Name}"
        }
        -- Add KTX library if available
        if KTX_AVAILABLE then
            libdirs { LibraryDir["KTX"] }
            links { "%{Library.KTX_Name}" }
        end
        postbuildcommands
        {
            "{COPY} $(SolutionDir)vendor/assimp/lib/Release/assimp-vc143-mt.dll %{cfg.targetdir}"
        }
        -- Copy KTX DLL if available
        if KTX_AVAILABLE then
            postbuildcommands
            {
                "{COPY} \"" .. KTX_SDK_PATH .. "/bin/ktx.dll\" \"%{cfg.targetdir}\""
            }
        end

    filter "configurations:Dist"
        defines { "LN_DIST" }
        runtime "Release"
        optimize "on"
        libdirs { "$(SolutionDir)vendor/assimp/lib/Release" }
        links
        {
            "%{Library.ShaderC_Release}",
            "%{Library.SPIRV_Cross_Release}",
            "%{Library.SPIRV_Cross_GLSL_Release}",
            "%{Library.Assimp_Release_Name}"
        }
        -- Add KTX library if available
        if KTX_AVAILABLE then
            libdirs { LibraryDir["KTX"] }
            links { "%{Library.KTX_Name}" }
        end
        postbuildcommands
        {
            "{COPY} $(SolutionDir)vendor/assimp/lib/Release/assimp-vc143-mt.dll %{cfg.targetdir}"
        }
        -- Copy KTX DLL if available
        if KTX_AVAILABLE then
            postbuildcommands
            {
                "{COPY} \"" .. KTX_SDK_PATH .. "/bin/ktx.dll\" \"%{cfg.targetdir}\""
            }
        end

project "Sandbox"
    location "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
    debugdir ("$(SolutionDir)")  -- ← CAMBIADO

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }

    includedirs
    {
        "vendor/spdlog/include",
        "Lunex/src",
        "Lunex/src/Core",
        "vendor/ImGuiLib",
        "%{IncludeDir.glm}",
        "%{IncludeDir.entt}",
        "%{IncludeDir.yaml_cpp}",
        "%{IncludeDir.Assimp}",
        "%{IncludeDir.ImGuizmo}",
        "%{IncludeDir.imnodes}",
        "%{IncludeDir.Bullet3}"
    }

    defines
    {
        "YAML_CPP_STATIC_DEFINE"
    }

    links
    {
        "Lunex"
    }

    filter "system:windows"
        systemversion "latest"
        buildoptions { "/utf-8" }

    filter "configurations:Debug"
        defines { "LN_DEBUG" }
        runtime "Debug"
        symbols "on"
        postbuildcommands
        {
            "{COPY} \"$(SolutionDir)vendor/assimp/lib/Debug/assimp-vc143-mtd.dll\" \"%{cfg.targetdir}\""
        }

    filter "configurations:Release"
        defines { "LN_RELEASE" }
        runtime "Release"
        optimize "on"
        postbuildcommands
        {
            "{COPY} \"$(SolutionDir)vendor/assimp/lib/Release/assimp-vc143-mt.dll\" \"%{cfg.targetdir}\""
        }

    filter "configurations:Dist"
        defines { "LN_DIST" }
        runtime "Release"
        optimize "on"
        postbuildcommands
        {
            "{COPY} \"$(SolutionDir)vendor/assimp/lib/Release/assimp-vc143-mt.dll\" \"%{cfg.targetdir}\""
        }

project "Lunex-Editor"
    location "Lunex-Editor"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"
        
    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
    debugdir ("$(SolutionDir)/Lunex-Editor")  -- ← CAMBIADO de "%{cfg.targetdir}"
        
    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp",
        "%{prj.name}/vendor/stb_image/**.h",
        "%{prj.name}/vendor/stb_image/**.cpp",
        "%{prj.name}/src/**.hpp",
        "%{prj.name}/src/**.c",
        "%{prj.name}/src/**.inl",
        "vendor/ImGuizmo/ImGuizmo.h",
        "vendor/ImGuizmo/ImGuizmo.cpp",
        "vendor/imnodes/imnodes.h",
        "vendor/imnodes/imnodes_internal.h",
        "vendor/imnodes/imnodes.cpp",
        "%{prj.name}/assets/**.glsl",
        "%{prj.name}/assets/**.lunex",
        "%{prj.name}/assets/**.png",
    }

    includedirs
    {
        "vendor/spdlog/include",
        "Lunex/src",
        "Lunex/src/Core",
        "vendor/ImGuiLib",
        "%{IncludeDir.glm}",
        "%{IncludeDir.entt}",
        "%{IncludeDir.yaml_cpp}",
        "%{IncludeDir.Assimp}",
        "%{IncludeDir.ImGuizmo}",
        "%{IncludeDir.imnodes}",
        "%{IncludeDir.Bullet3}"
    }

    defines
    {
        "YAML_CPP_STATIC_DEFINE"
    }

    links
    {
        "Lunex"
    }

    buildoptions { "/utf-8" }

    filter "system:windows"
        systemversion "latest"

    filter "configurations:Debug"
        defines { "LN_DEBUG" }
        runtime "Debug"
        symbols "on"
        postbuildcommands
        {
            "{COPY} \"$(SolutionDir)vendor/assimp/lib/Debug/assimp-vc143-mtd.dll\" \"%{cfg.targetdir}\"",
            -- ✅ AÑADE ESTOS COMANDOS para copiar assets
            "{MKDIR} \"%{cfg.targetdir}/assets\"",
            "{COPYDIR} \"$(SolutionDir)Lunex-Editor/assets\" \"%{cfg.targetdir}/assets\""
        }

    filter "configurations:Release"
        defines { "LN_RELEASE" }
        runtime "Release"
        optimize "on"
        postbuildcommands
        {
            "{COPY} \"$(SolutionDir)vendor/assimp/lib/Release/assimp-vc143-mt.dll\" \"%{cfg.targetdir}\"",
            -- ✅ AÑADE ESTOS COMANDOS
            "{MKDIR} \"%{cfg.targetdir}/assets\"",
            "{COPYDIR} \"$(SolutionDir)Lunex-Editor/assets\" \"%{cfg.targetdir}/assets\""
        }

    filter "configurations:Dist"
        defines { "LN_DIST" }
        runtime "Release"
        optimize "on"
        postbuildcommands
        {
            "{COPY} \"$(SolutionDir)vendor/assimp/lib/Release/assimp-vc143-mt.dll\" \"%{cfg.targetdir}\"",
            -- ✅ AÑADE ESTOS COMANDOS
            "{MKDIR} \"%{cfg.targetdir}/assets\"",
            "{COPYDIR} \"$(SolutionDir)Lunex-Editor/assets\" \"%{cfg.targetdir}/assets\""
        }
