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
sandbox2D_app_module_name = "A_Sandbox2D"
compiler = "msc"
workspace_dir = "%{wks.location}"

common_premake_flags = {"FatalWarnings", "MultiProcessorCompile"}

common_msvc_compiler_flags = {"/permissive-", "/fp:except", "/wd4005", "/wd4100", "/wd4189", "/wd4201", "/wd4505", "/wd6011", "/wd4251"}
common_clang_compiler_flags = {
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
    location (workspace_dir .. "/" .. engine_name)

    targetdir (workspace_dir .. "/bin/" .. outputdir)
	objdir    (workspace_dir .. "/bin-int/")

    -- pchheader "mpch.hpp"
    -- pchsource workspace_dir .. "/%{prj.name}/sauce/mpch.cpp"

    files
	{
        workspace_dir .. "/" .. engine_name .. "/sauce/**.h",
		workspace_dir .. "/" .. engine_name .. "/sauce/**.hpp",
        workspace_dir .. "/" .. engine_name .. "/sauce/**.c",
		workspace_dir .. "/" .. engine_name .. "/sauce/**.cpp",
	}

    includedirs
	{
        workspace_dir .. "/" .. engine_name .. "/sauce",
        workspace_dir .. "/vendor/Optick/include", 
	}

    links
    {
        "Winmm.lib",
        "user32.lib",     
        workspace_dir .. "/vendor/Optick/lib/x64/release/OptickCore.lib",   
    } 

    flags (common_premake_flags)

    filter "system:windows"
        defines {"LIB_COMPILE", "PLATFORM_WINDOWS", "_WIN32", "SHMEXPORT"}
        warnings "High"
        inlining ("Explicit")
        systemversion "latest"
		cppdialect "C++20"
        staticruntime "off"

        buildmessage 'Post build events'
        buildcommands 
        {
            '$(SolutionDir)/post-build.bat $(OutDirFullPath)',
        }
        -- Output file does not get generated. Only there so that post build event is triggered every time.
        buildoutputs { '$(SolutionDir)/dummy_target_file' }

    if compiler == "clang" then
        buildoptions (common_clang_compiler_flags)
    else
        buildoptions (common_msvc_compiler_flags)
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
    location (workspace_dir .. "/modules/renderer/" .. vulkan_renderer_module_name)

    targetdir (workspace_dir .. "/bin/" .. outputdir)
	objdir    (workspace_dir .. "/bin-int/")

    -- pchheader "mpch.hpp"
    -- pchsource workspace_dir .. "/%{prj.name}/sauce/mpch.cpp"

    files
	{
        workspace_dir .. "/modules/renderer/" .. vulkan_renderer_module_name .. "/sauce/**.h",
		workspace_dir .. "/modules/renderer/" .. vulkan_renderer_module_name .. "/sauce/**.hpp",
        workspace_dir .. "/modules/renderer/" .. vulkan_renderer_module_name .. "/sauce/**.c",
		workspace_dir .. "/modules/renderer/" .. vulkan_renderer_module_name .. "/sauce/**.cpp",
	}

    includedirs
	{
        workspace_dir .. "/modules/renderer/" .. vulkan_renderer_module_name .. "/sauce",
        workspace_dir .. "/" .. engine_name .. "/sauce",        
        "$(VULKAN_SDK)/Include",
        workspace_dir .. "/vendor/Optick/include", 
	}

    links
    {
        "$(VULKAN_SDK)/Lib/vulkan-1.lib",
        "Winmm.lib",
        "user32.lib",
        workspace_dir .. "/bin/" .. outputdir .. "Shmengine.lib",
        workspace_dir .. "/vendor/Optick/lib/x64/release/OptickCore.lib",   
    } 

    flags (common_premake_flags)

    filter "system:windows"
        defines {"LIB_COMPILE", "PLATFORM_WINDOWS", "_WIN32", "SHMEXPORT"}
        warnings "High"
        inlining ("Explicit")
        systemversion "latest"
		cppdialect "C++20"
        staticruntime "off"

    if compiler == "clang" then
        buildoptions (common_clang_compiler_flags)
    else
        buildoptions (common_msvc_compiler_flags)
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
    location (workspace_dir .. "/modules/app/" .. sandbox_app_module_name)

    targetdir (workspace_dir .. "/bin/" .. outputdir)
	objdir    (workspace_dir .. "/bin-int/")

    -- pchheader "mpch.hpp"
    -- pchsource workspace_dir .. "/%{prj.name}/sauce/mpch.cpp"

    files
	{
        workspace_dir .. "/modules/app/" .. sandbox_app_module_name .. "/sauce/**.h",
		workspace_dir .. "/modules/app/" .. sandbox_app_module_name .. "/sauce/**.hpp",
        workspace_dir .. "/modules/app/" .. sandbox_app_module_name .. "/sauce/**.c",
		workspace_dir .. "/modules/app/" .. sandbox_app_module_name .. "/sauce/**.cpp",
        workspace_dir .. "/modules/app/Common/sauce/**.h",
		workspace_dir .. "/modules/app/Common/sauce/**.hpp",
        workspace_dir .. "/modules/app/Common/sauce/**.c",
		workspace_dir .. "/modules/app/Common/sauce/**.cpp",
	}

    includedirs
	{
        workspace_dir .. "/modules/app/" .. sandbox_app_module_name .. "/sauce",
        workspace_dir .. "/" .. engine_name .. "/sauce",
        workspace_dir .. "/vendor/Optick/include", 
		workspace_dir .. "/modules/app/Common/sauce",
	}

    links
    {
        "Winmm.lib",
        workspace_dir .. "/bin/" .. outputdir .. "Shmengine.lib",
        workspace_dir .. "/vendor/Optick/lib/x64/release/OptickCore.lib",   
    } 

    flags (common_premake_flags)

    filter "system:windows"
        defines {"LIB_COMPILE", "PLATFORM_WINDOWS", "_WIN32", "SHMEXPORT"}
        warnings "High"
        inlining ("Explicit")
        systemversion "latest"
		cppdialect "C++20"
        staticruntime "off"

    if compiler == "clang" then
        buildoptions (common_clang_compiler_flags)
    else
        buildoptions (common_msvc_compiler_flags)
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

project  (sandbox2D_app_module_name)
    dependson (engine_name)
    kind "SharedLib"
    language "C++"
    toolset (compiler)
    location (workspace_dir .. "/modules/app/" .. sandbox2D_app_module_name)

    targetdir (workspace_dir .. "/bin/" .. outputdir)
	objdir    (workspace_dir .. "/bin-int/")

    -- pchheader "mpch.hpp"
    -- pchsource workspace_dir .. "/%{prj.name}/sauce/mpch.cpp"

    files
	{
        workspace_dir .. "/modules/app/" .. sandbox2D_app_module_name .. "/sauce/**.h",
		workspace_dir .. "/modules/app/" .. sandbox2D_app_module_name .. "/sauce/**.hpp",
        workspace_dir .. "/modules/app/" .. sandbox2D_app_module_name .. "/sauce/**.c",
		workspace_dir .. "/modules/app/" .. sandbox2D_app_module_name .. "/sauce/**.cpp",
        workspace_dir .. "/modules/app/Common/sauce/**.h",
		workspace_dir .. "/modules/app/Common/sauce/**.hpp",
        workspace_dir .. "/modules/app/Common/sauce/**.c",
		workspace_dir .. "/modules/app/Common/sauce/**.cpp",
	}

    includedirs
	{
        workspace_dir .. "/modules/app/" .. sandbox2D_app_module_name .. "/sauce",
        workspace_dir .. "/" .. engine_name .. "/sauce",
        workspace_dir .. "/vendor/Optick/include", 
		workspace_dir .. "/modules/app/Common/sauce",
	}

    links
    {
        "Winmm.lib",
        workspace_dir .. "/bin/" .. outputdir .. "Shmengine.lib",
        workspace_dir .. "/vendor/Optick/lib/x64/release/OptickCore.lib",   
    } 

    flags (common_premake_flags)

    filter "system:windows"
        defines {"LIB_COMPILE", "PLATFORM_WINDOWS", "_WIN32", "SHMEXPORT"}
        warnings "High"
        inlining ("Explicit")
        systemversion "latest"
		cppdialect "C++20"
        staticruntime "off"

    if compiler == "clang" then
        buildoptions (common_clang_compiler_flags)
    else
        buildoptions (common_msvc_compiler_flags)
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
    dependson (sandbox2D_app_module_name)
    kind "WindowedApp"
    language "C++"
    toolset (compiler)
    location (workspace_dir .. "/%{prj.name}")

    targetdir (workspace_dir .. "/bin/" .. outputdir)
	objdir    (workspace_dir .. "/bin-int/")

    -- pchheader "mpch.hpp"
    -- pchsource workspace_dir .. "/%{prj.name}/sauce/mpch.cpp"

    files
	{
        workspace_dir .. "/" .. app_name .. "/sauce/**.h",
		workspace_dir .. "/" .. app_name .. "/sauce/**.hpp",
        workspace_dir .. "/" .. app_name .. "/sauce/**.c",
		workspace_dir .. "/" .. app_name .. "/sauce/**.cpp",
	}

    includedirs
	{
		workspace_dir .. "/" .. app_name .. "/sauce",
        workspace_dir .. "/" .. engine_name .. "/sauce",
        workspace_dir .. "/vendor/Optick/include", 
	}

    links
    {
        "user32.lib",
        "Gdi32.lib",
        "Winmm.lib",
        workspace_dir .. "/vendor/Optick/lib/x64/release/OptickCore.lib",   
        workspace_dir .. "/bin/" .. outputdir .. "Shmengine.lib",   
    }

    flags (common_premake_flags)

    filter "system:windows"
        defines {"PLATFORM_WINDOWS", "_WIN32"}
        warnings "High"
        inlining ("Explicit")
        systemversion "latest"
		cppdialect "C++20"
        staticruntime "off"

    if compiler == "clang" then
        buildoptions (common_clang_compiler_flags)
    else
        buildoptions (common_msvc_compiler_flags)
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




