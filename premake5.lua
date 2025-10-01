workspace "Lunex-Engine"
    architecture "x86_64"
    startproject "Sandbox"

    configurations {
        "Debug",
        "Release",
        "Dist"
    }

	flags {
		"MultiProcessorCompile"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include directories relative to the solution directory
IncludeDir = {}
IncludeDir["GLFW"]      = "vendor/GLFW/include"
IncludeDir["Glad"]      = "vendor/Glad/include"
IncludeDir["ImGui"]     = "vendor/ImGuiLib"
IncludeDir["glm"]       = "vendor/glm"
IncludeDir["stb_image"] = "vendor/stb_image"
IncludeDir["Lunex"]     = "Lunex/src"

group "Dependencies"
    include "vendor/GLFW"
    include "vendor/Glad"
    include "vendor/ImGuiLib"
group ""

project "Lunex"
    location "Lunex"
    kind "StaticLib"
    language "C++"
    cppdialect "C++20"
    staticruntime "on"

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
        "%{prj.name}/assets/**.glsl",
        "%{prj.name}/assets/**.png",
    }

    defines
	{
		"_CRT_SECURE_NO_WARNINGS",
		"GLFW_INCLUDE_NONE"
	}

    includedirs
    {
        "%{prj.name}/src",
        "vendor/spdlog/include",
        "%{IncludeDir.GLFW}",
        "%{IncludeDir.Glad}",
        "%{IncludeDir.ImGui}",
        "%{IncludeDir.glm}",
        "%{IncludeDir.stb_image}"
    }

    links{
        "GLFW",
        "Glad",
        "ImGui",
        "opengl32.lib"
    }

    filter "system:windows"
        systemversion "latest"

        defines{
        }

        buildoptions { "/utf-8" }

    filter "configurations:Debug"
        defines { "LN_DEBUG" }
        buildoptions { "/MDd"}
        symbols "on"

    filter "configurations:Release"
        defines { "LN_RELEASE" }
        buildoptions { "/MD"}
        optimize "on"

    filter "configurations:Dist"
        defines { "LN_DIST" }
        buildoptions { "/MD"}
        optimize "on"

project "Sandbox"
    location "Sandbox"
    kind "ConsoleApp"
    language "C++"
    cppdialect "C++20"
    staticruntime "on"

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
        "Lunex/src",
        "Lunex/src/Core",
        "vendor/ImGuiLib",
        "%{IncludeDir.glm}"
    }

    links
    {
        "Lunex"
    }

    filter "system:windows"
        cppdialect "C++20"
        staticruntime "On"
        systemversion "latest"

    buildoptions { "/utf-8" }

    filter "configurations:Debug"
        defines { "LN_DEBUG" }
        buildoptions { "/MDd"}
        symbols "on"

    filter "configurations:Release"
        defines { "LN_RELEASE" }
        buildoptions { "/MD"}
        optimize "on"

    filter "configurations:Dist"
        defines { "LN_DIST" }
        buildoptions { "/MD"}
        optimize "on"

project "Lunex-Editor"
	location "Lunex-Editor"
	kind "ConsoleApp"
	language "C++"
	cppdialect "C++20"
	staticruntime "on"

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
        "Lunex/src",
        "Lunex/src/Core",
        "vendor/ImGuiLib",
        "%{IncludeDir.glm}"
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
        buildoptions { "/MDd"}
        symbols "on"

    filter "configurations:Release"
        defines { "LN_RELEASE" }
        buildoptions { "/MD"}
        optimize "on"

    filter "configurations:Dist"
        defines { "LN_DIST" }
        buildoptions { "/MD"}
        optimize "on"