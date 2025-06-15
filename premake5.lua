workspace "Stellara"
	architecture "x64"

	configurations{
		"Debug",
		"Release",
		"Dist"
	}

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}"

project "Stellara"
	location "Stellara"
	kind "ConsoleApp"
	language "C++"

	targetdir ("bin/%{outputdir}/%{prj.name}")
	objdir ("bin-int/%{outputdir}/%{prj.name}")


	files
	{
		"%{prj.name}/src/**.h",
		"%{prj.name}/src/**.cpp",
		"%{prj.name}/src/**.hpp",
		"%{prj.name}/src/**.c",
	}

	includedirs
	{
		"%{sln.name}/vendor/spdlog/include",
	}

	filter "system:windows"
		cppdialect "C++17"
		staticruntime "On"
		systemversion "10.0.22621.0"

	
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