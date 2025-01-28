@echo off

SET wd=%~dp0
IF "%1"=="" (echo Error: need to provide build directory as first argument && exit /B)
SET build_dir=%1

IF NOT EXIST %build_dir%OptickCore.dll (
    echo "copying optick dll to output directory : %wd%vendor\Optick\lib\x64\debug -> %build_dir%"
    ROBOCOPY "%wd%vendor\Optick\lib\x64\debug " "%build_dir% " "OptickCore.dll"
    IF %ERRORLEVEL% GEQ 2 (echo Error: %ERRORLEVEL% && exit)
)

echo %wd%
echo "Compiling shaders..."

IF NOT EXIST "%wd%assets\shaders\bin\" mkdir "%wd%assets\shaders\bin"

echo "assets\shaders\sauce\Builtin.MaterialPhong.vert.glsl -> assets\shaders\bin\Builtin.MaterialPhong.vert.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert "%wd%assets\shaders\sauce\Builtin.MaterialPhong.vert.glsl" -o "%wd%assets\shaders\bin\Builtin.MaterialPhong.vert.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit /B)

echo "assets\shaders\sauce\Builtin.MaterialPhong.frag.glsl -> assets\shaders\bin\Builtin.MaterialPhong.frag.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag "%wd%assets\shaders\sauce\Builtin.MaterialPhong.frag.glsl" -o "%wd%assets\shaders\bin\Builtin.MaterialPhong.frag.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit /B)

echo "assets\shaders\sauce\Builtin.Terrain.vert.glsl -> assets\shaders\bin\Builtin.Terrain.vert.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert "%wd%assets\shaders\sauce\Builtin.Terrain.vert.glsl" -o "%wd%assets\shaders\bin\Builtin.Terrain.vert.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit /B)

echo "assets\shaders\sauce\Builtin.Terrain.frag.glsl -> assets\shaders\bin\Builtin.Terrain.frag.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag "%wd%assets\shaders\sauce\Builtin.Terrain.frag.glsl" -o "%wd%assets\shaders\bin\Builtin.Terrain.frag.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit /B)

echo "assets\shaders\sauce\Builtin.UI.vert.glsl -> assets\shaders\bin\Builtin.UI.vert.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert "%wd%assets\shaders\sauce\Builtin.UI.vert.glsl" -o "%wd%assets\shaders\bin\Builtin.UI.vert.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit /B)

echo "assets\shaders\sauce\Builtin.UI.frag.glsl -> assets\shaders\bin\Builtin.UI.frag.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag "%wd%assets\shaders\sauce\Builtin.UI.frag.glsl" -o "%wd%assets\shaders\bin\Builtin.UI.frag.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit /B)

echo "assets\shaders\sauce\Builtin.Skybox.vert.glsl -> assets\shaders\bin\Builtin.Skybox.vert.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert "%wd%assets\shaders\sauce\Builtin.Skybox.vert.glsl" -o "%wd%assets\shaders\bin\Builtin.Skybox.vert.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit /B)

echo "assets\shaders\sauce\Builtin.Skybox.frag.glsl -> assets\shaders\bin\Builtin.Skybox.frag.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag "%wd%assets\shaders\sauce\Builtin.Skybox.frag.glsl" -o "%wd%assets\shaders\bin\Builtin.Skybox.frag.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit /B)

echo "assets\shaders\sauce\Builtin.Pick3D.vert.glsl -> assets\shaders\bin\Builtin.Pick3D.vert.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert "%wd%assets\shaders\sauce\Builtin.Pick3D.vert.glsl" -o "%wd%assets\shaders\bin\Builtin.Pick3D.vert.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit /B)

echo "assets\shaders\sauce\Builtin.Pick2D.vert.glsl -> assets\shaders\bin\Builtin.Pick2D.vert.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert "%wd%assets\shaders\sauce\Builtin.Pick2D.vert.glsl" -o "%wd%assets\shaders\bin\Builtin.Pick2D.vert.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit /B)

echo "assets\shaders\sauce\Builtin.Pick.frag.glsl -> assets\shaders\bin\Builtin.Pick.frag.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag "%wd%assets\shaders\sauce\Builtin.Pick.frag.glsl" -o "%wd%assets\shaders\bin\Builtin.Pick.frag.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit /B)

echo "assets\shaders\sauce\Builtin.Color3D.vert.glsl -> assets\shaders\bin\Builtin.Color3D.vert.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert "%wd%assets\shaders\sauce\Builtin.Color3D.vert.glsl" -o "%wd%assets\shaders\bin\Builtin.Color3D.vert.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit /B)

echo "assets\shaders\sauce\Builtin.Color3D.frag.glsl -> assets\shaders\bin\Builtin.Color3D.frag.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag "%wd%assets\shaders\sauce\Builtin.Color3D.frag.glsl" -o "%wd%assets\shaders\bin\Builtin.Color3D.frag.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit /B)

echo "assets\shaders\sauce\Builtin.CoordinateGrid.vert.glsl -> assets\shaders\bin\Builtin.CoordinateGrid.vert.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert "%wd%assets\shaders\sauce\Builtin.CoordinateGrid.vert.glsl" -o "%wd%assets\shaders\bin\Builtin.CoordinateGrid.vert.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit /B)

echo "assets\shaders\sauce\Builtin.CoordinateGrid.frag.glsl -> assets\shaders\bin\Builtin.CoordinateGrid.frag.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag "%wd%assets\shaders\sauce\Builtin.CoordinateGrid.frag.glsl" -o "%wd%assets\shaders\bin\Builtin.CoordinateGrid.frag.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit /B)

echo "Done."
