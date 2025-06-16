workspace "Stellara"
	architecture "x64"

	configurations{
		"Debug",
		"Release",
		"Dist"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

-- Include directories relative to the solution directory
IncludeDir = {}
IncludeDir["GLFW"] = "vendor/glfw/include"

include "vendor/GLFW"

project "Stellara"
	location "Stellara"
	kind "ConsoleApp"
	language "C++"

	targetdir ("bin/%{outputdir}/%{prj.name}")
	objdir ("bin-int/%{outputdir}/%{prj.name}")

	pchheader "stpch.h"
	pchsource "Stellara/src/stpch.cpp"

	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/src/**.hpp",
		"%{prj.name}/src/**.c",
	}

	includedirs
	{
		"%{prj.name}/src",
		"%{sln.name}/vendor/spdlog/include",
		"%{IncludeDir.GLFW}"
	}

	links{
		"GLFW",
		"opengl32.lib"
	}

	filter "system:windows"
		cppdialect "C++20"
		staticruntime "Off"
		systemversion "latest"

	
    filter "system:windows"
        buildoptions { "/utf-8" }

	filter "configurations:Debug"
		defines { "ST_DEBUG" }
		runtime "Debug"
		symbols "On"

	filter "configurations:Release"
	defines { "ST_RELEASE" }
		runtime "Release"
		optimize "On"

	filter "configurations:Dist"
	defines { "ST_DIST" }
		runtime "Release"
		optimize "On"