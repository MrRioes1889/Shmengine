require "cmake"

workspace "Shmengine"
    architecture "x64"
    configurations {"Debug", "ODebug", "Release"}
    location "../"
    startproject "Sandbox"

outputdir = "%{cfg.buildcfg}-%{cfg.system}-%{cfg.architecture}/Sandbox/"
engine_name = "Shmengine"
app_name = "Application"
tests_name = "Tests"
vulkan_renderer_module_name = "M_VulkanRenderer"
sandbox_app_module_name = "A_Sandbox"
compiler = "msc"
project_location = "%{wks.location}"

global_msvc_build_options = {"/fp:except", "/wd4005", "/wd4100", "/wd4189", "/wd4201", "/wd4505", "/wd6011", "/wd4251"}
global_clang_build_options = {
    "-Wno-missing-braces", 
    "-Wno-reorder-ctor", 
    "-Wno-unused-variable", 
    "-Wno-unused-but-set-variable", 
    "-Wno-nonportable-include-path", 
    "-Wno-unused-function", 
    "-Wno-unused-parameter", 
    "-Wno-missing-field-initializers"}

IncludeDir = {}

----------------------------------------------------------------------------------------------------------------------------------------------

project  (engine_name)
    kind "SharedLib"
    language "C++"
    toolset (compiler)
    location (project_location .. "/" .. engine_name)

    targetdir (project_location .. "/bin/" .. outputdir)
	objdir    (project_location .. "/bin-int/")

    -- pchheader "mpch.hpp"
    -- pchsource project_location .. "/%{prj.name}/sauce/mpch.cpp"

    files
	{
        project_location .. "/" .. engine_name .. "/sauce/**.h",
		project_location .. "/" .. engine_name .. "/sauce/**.hpp",
        project_location .. "/" .. engine_name .. "/sauce/**.c",
		project_location .. "/" .. engine_name .. "/sauce/**.cpp",
	}

    includedirs
	{
        project_location .. "/" .. engine_name .. "/sauce",
        project_location .. "/vendor/Optick/include", 
	}

    links
    {
        "Winmm.lib",
        "user32.lib",     
        project_location .. "/vendor/Optick/lib/x64/release/OptickCore.lib",   
    } 

    flags("FatalWarnings")

    filter "system:windows"
        defines {"LIB_COMPILE", "PLATFORM_WINDOWS", "_WIN32", "SHMEXPORT"}
        warnings "High"
        inlining ("Explicit")
        systemversion "latest"
		cppdialect "C++20"
        staticruntime "on"

        buildmessage 'Post build events'
        buildcommands 
        {
            '$(SolutionDir)/post-build.bat $(OutDirFullPath)',
        }
        -- Output file does not get generated. Only there so that post build event is triggered every time.
        buildoutputs { '$(SolutionDir)/dummy_target_file' }

    if compiler == "clang" then
        buildoptions (global_clang_build_options)
    else
        buildoptions (global_msvc_build_options)
    end

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

project  (vulkan_renderer_module_name)
    dependson (engine_name)
    kind "SharedLib"
    language "C++"
    toolset (compiler)
    location (project_location .. "/modules/renderer/" .. vulkan_renderer_module_name)

    targetdir (project_location .. "/bin/" .. outputdir)
	objdir    (project_location .. "/bin-int/")

    -- pchheader "mpch.hpp"
    -- pchsource project_location .. "/%{prj.name}/sauce/mpch.cpp"

    files
	{
        project_location .. "/modules/renderer/" .. vulkan_renderer_module_name .. "/sauce/**.h",
		project_location .. "/modules/renderer/" .. vulkan_renderer_module_name .. "/sauce/**.hpp",
        project_location .. "/modules/renderer/" .. vulkan_renderer_module_name .. "/sauce/**.c",
		project_location .. "/modules/renderer/" .. vulkan_renderer_module_name .. "/sauce/**.cpp",
	}

    includedirs
	{
        project_location .. "/" .. engine_name .. "/sauce",        
        "$(VULKAN_SDK)/Include",
        project_location .. "/vendor/Optick/include", 
	}

    links
    {
        "$(VULKAN_SDK)/Lib/vulkan-1.lib",
        "Winmm.lib",
        "user32.lib",
        project_location .. "/bin/" .. outputdir .. "Shmengine.lib",
        project_location .. "/vendor/Optick/lib/x64/release/OptickCore.lib",   
    } 

    flags("FatalWarnings")

    filter "system:windows"
        defines {"LIB_COMPILE", "PLATFORM_WINDOWS", "_WIN32", "SHMEXPORT"}
        warnings "High"
        inlining ("Explicit")
        systemversion "latest"
		cppdialect "C++20"
        staticruntime "on"

    if compiler == "clang" then
        buildoptions (global_clang_build_options)
    else
        buildoptions (global_msvc_build_options)
    end

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

project  (sandbox_app_module_name)
    dependson (engine_name)
    kind "SharedLib"
    language "C++"
    toolset (compiler)
    location (project_location .. "/modules/app/" .. sandbox_app_module_name)

    targetdir (project_location .. "/bin/" .. outputdir)
	objdir    (project_location .. "/bin-int/")

    -- pchheader "mpch.hpp"
    -- pchsource project_location .. "/%{prj.name}/sauce/mpch.cpp"

    files
	{
        project_location .. "/modules/app/" .. sandbox_app_module_name .. "/sauce/**.h",
		project_location .. "/modules/app/" .. sandbox_app_module_name .. "/sauce/**.hpp",
        project_location .. "/modules/app/" .. sandbox_app_module_name .. "/sauce/**.c",
		project_location .. "/modules/app/" .. sandbox_app_module_name .. "/sauce/**.cpp",
	}

    includedirs
	{
        project_location .. "/" .. engine_name .. "/sauce",
        project_location .. "/vendor/Optick/include", 
	}

    links
    {
        "Winmm.lib",
        project_location .. "/bin/" .. outputdir .. "Shmengine.lib",
        project_location .. "/vendor/Optick/lib/x64/release/OptickCore.lib",   
    } 

    flags("FatalWarnings")

    filter "system:windows"
        defines {"LIB_COMPILE", "PLATFORM_WINDOWS", "_WIN32", "SHMEXPORT"}
        warnings "High"
        inlining ("Explicit")
        systemversion "latest"
		cppdialect "C++20"
        staticruntime "on"

    if compiler == "clang" then
        buildoptions (global_clang_build_options)
    else
        buildoptions (global_msvc_build_options)
    end

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
    dependson (vulkan_renderer_module_name)
    dependson (sandbox_app_module_name)
    kind "WindowedApp"
    language "C++"
    toolset (compiler)
    location (project_location .. "/%{prj.name}")

    targetdir (project_location .. "/bin/" .. outputdir)
	objdir    (project_location .. "/bin-int/")

    -- pchheader "mpch.hpp"
    -- pchsource project_location .. "/%{prj.name}/sauce/mpch.cpp"

    files
	{
        project_location .. "/" .. app_name .. "/sauce/**.h",
		project_location .. "/" .. app_name .. "/sauce/**.hpp",
        project_location .. "/" .. app_name .. "/sauce/**.c",
		project_location .. "/" .. app_name .. "/sauce/**.cpp",
	}

    includedirs
	{
		project_location .. "/" .. app_name .. "/sauce",
        project_location .. "/" .. engine_name .. "/sauce",
        project_location .. "/vendor/Optick/include", 
	}

    links
    {
        "user32.lib",
        "Gdi32.lib",
        "Winmm.lib",
        project_location .. "/vendor/Optick/lib/x64/release/OptickCore.lib",   
        project_location .. "/bin/" .. outputdir .. "Shmengine.lib",   
    }

    flags("FatalWarnings")

    filter "system:windows"
        defines {"PLATFORM_WINDOWS", "_WIN32"}
        warnings "High"
        inlining ("Explicit")
        systemversion "latest"
		cppdialect "C++20"
        staticruntime "on"

    if compiler == "clang" then
        buildoptions (global_clang_build_options)
    else
        buildoptions (global_msvc_build_options)
    end

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




