workspace "Shmengine"
    architecture "x64"
    configurations {"Debug", "Release"}
    location ""

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/Sandbox/"
engine_name = "Shmengine"
app_name = ("Sandbox")

IncludeDir = {}

project  (engine_name)
    kind "SharedLib"
    language "C++"
    location ("%{wks.location}/" .. engine_name)

    targetdir ("%{wks.location}/bin/" .. outputdir)
	objdir    ("%{wks.location}/bin-int/" .. outputdir)

    -- pchheader "mpch.hpp"
    -- pchsource "%{wks.location}/%{prj.name}/sauce/mpch.cpp"

    files
	{
		"%{wks.location}/" .. engine_name .. "/sauce/**.h",
		"%{wks.location}/" .. engine_name .. "/sauce/**.hpp",
        "%{wks.location}/" .. engine_name .. "/sauce/**.c",
		"%{wks.location}/" .. engine_name .. "/sauce/**.cpp",
	}

    includedirs
	{
        "%{wks.location}/" .. engine_name .. "/sauce",
        "$(VULKAN_SDK)/Include"
	}

    links
    {
        "$(VULKAN_SDK)/Lib/vulkan-1.lib"  
    }

    flags("FatalWarnings")

    filter "system:windows"
        defines {"LIB_COMPILE", "PLATFORM_WINDOWS", "_WIN32", "SHMEXPORT"}
        warnings "High"
        inlining ("Explicit")
        buildoptions {"/wd4100", "/wd4189", "/wd4201", "/wd4505", "/wd4127", "/wd4390", "/wd4005", "/wd4554"}
        systemversion "latest"
		cppdialect "C++20"
        staticruntime "on"

    filter "configurations:Debug"
        defines {"DEBUG"}
        symbols "On"

    filter "configurations:Release"
        defines {"NDEBUG"}
        optimize "On"


project (app_name)
    dependson (engine_name)
    kind "WindowedApp"
    language "C++"
    location "%{wks.location}/%{prj.name}"

    targetdir ("%{wks.location}/bin/" .. outputdir)
	objdir    ("%{wks.location}/bin-int/" .. outputdir)

    -- pchheader "mpch.hpp"
    -- pchsource "%{wks.location}/%{prj.name}/sauce/mpch.cpp"

    files
	{
		"%{wks.location}/" .. app_name .. "/sauce/**.h",
		"%{wks.location}/" .. app_name .. "/sauce/**.hpp",
        "%{wks.location}/" .. app_name .. "/sauce/**.c",
		"%{wks.location}/" .. app_name .. "/sauce/**.cpp",
	}

    includedirs
	{
		"%{wks.location}/%{prj.name}/sauce",
        "%{wks.location}/" .. engine_name .. "/sauce",
	}

    links
    {
        "user32.lib",
        "Gdi32.lib",
        "Winmm.lib",
        "%{wks.location}/bin/" .. outputdir .. "Shmengine.lib",   
    }

    flags("FatalWarnings")

    filter "system:windows"
        defines {"PLATFORM_WINDOWS", "_WIN32"}
        warnings "High"
        inlining ("Explicit")
        buildoptions {"/wd4100", "/wd4189", "/wd4201", "/wd4505", "/wd4127", "/wd4390", "/wd4005", "/wd4554"}
        systemversion "latest"
		cppdialect "C++20"
        staticruntime "on"

    filter "configurations:Debug"
        defines {"DEBUG"}
        symbols "On"

    filter "configurations:Release"
        defines {"NDEBUG"}
        optimize "On"




