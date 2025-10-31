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
IncludeDir["Lunex"]     = "Lunex/src"
IncludeDir["VulkanSDK"] = "%{VULKAN_SDK}/Include"

-- ✅ Librerías de Vulkan y Assimp
LibraryDir = {}
LibraryDir["VulkanSDK"] = "%{VULKAN_SDK}/Lib"
LibraryDir["Assimp"]    = "vendor/assimp/lib"

Library = {}
Library["Vulkan"] = "%{LibraryDir.VulkanSDK}/vulkan-1.lib"

-- ✅ Usar versiones específicas para Debug y Release (corrige nombres)
Library["ShaderC_Debug"] = "%{LibraryDir.VulkanSDK}/shaderc_combinedd.lib"
Library["ShaderC_Release"] = "%{LibraryDir.VulkanSDK}/shaderc_combined.lib"

Library["SPIRV_Cross_Debug"] = "%{LibraryDir.VulkanSDK}/spirv-cross-cored.lib"
Library["SPIRV_Cross_Release"] = "%{LibraryDir.VulkanSDK}/spirv-cross-core.lib"

Library["SPIRV_Cross_GLSL_Debug"] = "%{LibraryDir.VulkanSDK}/spirv-cross-glsld.lib"
Library["SPIRV_Cross_GLSL_Release"] = "%{LibraryDir.VulkanSDK}/spirv-cross-glsl.lib"

-- Assimp libraries (usaremos libdirs + nombres de lib)
Library["Assimp_Debug_Name"] = "assimp-vc143-mtd"
Library["Assimp_Release_Name"] = "assimp-vc143-mt"

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

group ""

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

    includedirs
    {
        "%{prj.name}/src",
        "vendor/spdlog/include",
        "%{IncludeDir.Box2D}",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.Glad}",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.stb_image}",
        "%{IncludeDir.entt}",
        "%{IncludeDir.yaml_cpp}",
        "%{IncludeDir.ImGuizmo}",
        "%{IncludeDir.Assimp}",
        "%{IncludeDir.VulkanSDK}"
    }

    links
    {
        "Box2D",
        "GLFW",
        "Glad",
        "ImGui",
        "yaml-cpp",
        "opengl32.lib"
    }

    filter "files:vendor/ImGuizmo/**.cpp"
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
        postbuildcommands
        {
            "{COPY} $(SolutionDir)vendor/assimp/lib/Debug/assimp-vc143-mtd.dll %{cfg.targetdir}"
        }

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
        postbuildcommands
        {
            "{COPY} $(SolutionDir)vendor/assimp/lib/Release/assimp-vc143-mt.dll %{cfg.targetdir}"
        }

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
        postbuildcommands
        {
            "{COPY} $(SolutionDir)vendor/assimp/lib/Release/assimp-vc143-mt.dll %{cfg.targetdir}"
        }

project "Sandbox"
    location "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "off"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")
    debugdir ("%{cfg.targetdir}")

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
        "%{IncludeDir.ImGuizmo}",
        "%{IncludeDir.Box2D}"  -- ✅ AÑADIDO
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
    debugdir ("%{cfg.targetdir}")

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
        "%{IncludeDir.ImGuizmo}"
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