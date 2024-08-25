workspace "Shmengine"
    architecture "x64"
    configurations {"Debug", "ODebug", "Release"}
    location ""
    startproject "Sandbox"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/Sandbox/"
engine_name = "Shmengine"
app_name = "Sandbox"
tests_name = "Tests"

IncludeDir = {}

----------------------------------------------------------------------------------------------------------------------------------------------

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
        "$(VULKAN_SDK)/Include",
        "%{wks.location}/vendor/Optick/include", 
	}

    links
    {
        "$(VULKAN_SDK)/Lib/vulkan-1.lib",
        "Winmm.lib",
        "%{wks.location}/vendor/Optick/lib/x64/release/OptickCore.lib",   
    } 

    flags("FatalWarnings")

    filter "system:windows"
        defines {"LIB_COMPILE", "PLATFORM_WINDOWS", "_WIN32", "SHMEXPORT"}
        warnings "High"
        inlining ("Explicit")
        buildoptions {"/fp:except", "/wd4005", "/wd4100", "/wd4189", "/wd4201", "/wd4505", "/wd6011"}
        systemversion "latest"
		cppdialect "C++20"
        staticruntime "on"

        buildmessage 'Post build events'
        buildcommands 
        {
            '$(SolutionDir)/post-build.bat bin\\Debug-windows-x86_64\\Sandbox',
        }
        -- Output file does not get generated. Only there so that post build event is triggered every time.
        buildoutputs { '$(SolutionDir)/dummy_target_file' }

    filter "configurations:Debug"
        defines {"DEBUG"}
        symbols "On"

    filter "configurations:ODebug"
        defines {"DEBUG"}
        symbols "On"
        optimize "On"

    filter "configurations:Release"
        defines {"NDEBUG"}
        optimize "On"

--------------------------------------------------------------------------------------------------------------------------------

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
		"%{wks.location}/" .. app_name .. "/sauce",
        "%{wks.location}/" .. engine_name .. "/sauce",
        "%{wks.location}/vendor/Optick/include", 
	}

    links
    {
        "user32.lib",
        "Gdi32.lib",
        "Winmm.lib",
        "%{wks.location}/vendor/Optick/lib/x64/release/OptickCore.lib",   
        "%{wks.location}/bin/" .. outputdir .. "Shmengine.lib",   
    }

    flags("FatalWarnings")

    filter "system:windows"
        defines {"PLATFORM_WINDOWS", "_WIN32"}
        warnings "High"
        inlining ("Explicit")
        buildoptions {"/fp:except", "/wd4005", "/wd4100", "/wd4189", "/wd4201", "/wd4505", "/wd6011"}
        systemversion "latest"
		cppdialect "C++20"
        staticruntime "on"

    filter "configurations:Debug"
        defines {"DEBUG"}
        symbols "On"

    filter "configurations:ODebug"
        defines {"DEBUG"}
        symbols "On"
        optimize "On"

    filter "configurations:Release"
        defines {"NDEBUG"}
        optimize "On"

----------------------------------------------------------------------------------------------------------------------------------------------

project (tests_name)
    dependson (engine_name)
    kind "ConsoleApp"
    language "C++"
    location "%{wks.location}/%{prj.name}"

    targetdir ("%{wks.location}/bin/" .. outputdir)
	objdir    ("%{wks.location}/bin-int/" .. outputdir)

    -- pchheader "mpch.hpp"
    -- pchsource "%{wks.location}/%{prj.name}/sauce/mpch.cpp"

    files
	{
		"%{wks.location}/" .. tests_name .. "/sauce/**.h",
		"%{wks.location}/" .. tests_name .. "/sauce/**.hpp",
        "%{wks.location}/" .. tests_name .. "/sauce/**.c",
		"%{wks.location}/" .. tests_name .. "/sauce/**.cpp",
	}

    includedirs
	{
		"%{wks.location}/" .. tests_name .. "/sauce",
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
        buildoptions {"/fp:except", "/wd4005", "/wd4100", "/wd4189", "/wd4201", "/wd4505", "/wd6011"}
        systemversion "latest"
		cppdialect "C++20"
        staticruntime "on"

    filter "configurations:Debug"
        defines {"DEBUG"}
        symbols "On"

    filter "configurations:ODebug"
        defines {"DEBUG"}
        symbols "On"
        optimize "On"

    filter "configurations:Release"
        defines {"NDEBUG"}
        optimize "On"




