workspace "Stellara"
    architecture "x64"
    startproject "Sandbox"

    configurations{
        "Debug",
        "Release",
        "Dist"
    }

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include directories relative to the solution directory
IncludeDir = {}
IncludeDir["GLFW"] = "vendor/glfw/include"
IncludeDir["Glad"] = "vendor/Glad/include"
IncludeDir["ImGui"] = "vendor/imgui/include"
IncludeDir["Stellara"] = "Stellara/src/Stellara"

include "vendor/GLFW"
include "vendor/Glad"
include "vendor/imgui"

project "Stellara"
    location "Stellara"
    kind "SharedLib"
    language "C++"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    pchheader "stpch.h"
    pchsource "Stellara/src/Stellara/stpch.cpp"

    files
    {
        "%{prj.name}/src/Stellara/**.h",
        "%{prj.name}/src/Stellara/**.cpp",
        "%{prj.name}/src/Stellara/**.hpp",
        "%{prj.name}/src/Stellara/**.c",
    }

    includedirs
    {
        "%{prj.name}/src/Stellara",
        "vendor/spdlog/include",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.Glad}",
        "%{IncludeDir.ImGui}",
    }

    links{
        "GLFW",
        "Glad",
        "ImGui",
        "opengl32.lib"
    }

    filter "system:windows"
        cppdialect "C++20"
        staticruntime "On"
        systemversion "latest"

        defines{
            "ST_PLATFORM_WINDOWS",
            "ST_BUILD_DLL",
            "GLFW_INCLUDE_NONE"
        }

postbuildcommands {
    ("{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/Sandbox")
}

        buildoptions { "/utf-8" }

    filter "configurations:Debug"
        defines { "ST_DEBUG" }
        buildoptions { "/MDd"}
        symbols "On"

    filter "configurations:Release"
        defines { "ST_RELEASE" }
        buildoptions { "/MD"}
        optimize "On"

    filter "configurations:Dist"
        defines { "ST_DIST" }
        buildoptions { "/MD"}
        optimize "On"

project "Sandbox"
    location "Sandbox"
    kind "ConsoleApp"
    language "C++"

    targetdir ("bin/" .. outputdir .. "/%{prj.name}")
    objdir ("bin-int/" .. outputdir .. "/%{prj.name}")

    files
    {
        "%{prj.name}/src/**.h",
        "%{prj.name}/src/**.cpp"
    }

    includedirs
    {
        "vendor/spdlog/include",
        "Stellara/src/Stellara",
        "Stellara/src/Stellara/Core"
    }

    links
    {
        "Stellara"
    }

    filter "system:windows"
        cppdialect "C++20"
        staticruntime "On"
        systemversion "latest"

        defines
        {
            "ST_PLATFORM_WINDOWS"
        }

    buildoptions { "/utf-8" }

    filter "configurations:Debug"
        defines "ST_DEBUG"
        buildoptions "/MDd"
        symbols "On"

    filter "configurations:Release"
        defines "ST_RELEASE"
        buildoptions "/MD"
        optimize "On"

    filter "configurations:Dist"
        defines "ST_DIST"
        buildoptions "/MD"
        optimize "On"
