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
	kind "SharedLib"
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
		"../vendor/spdlog/include",
		"%{IncludeDir.GLFW}"
	}

	links{
		"GLFW",
		"opengl32.lib"
	}

	filter "system:windows"
		cppdialect "C++20"
		staticruntime "On"
		systemversion "latest"

		defines{
			"ST_PLATFORM_WINDOWS",
			"ST_BUILD_DLL"
		}

		postbuildcommands{
			("{COPY} %{cfg.buildtarget.relpath} ../bin/" .. outputdir .. "/Stellara")
		}
	
    filter "system:windows"
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
		"../vendor/spdlog/include",
		"Stellara/src/Core"
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