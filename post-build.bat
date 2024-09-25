@echo off

SET wd=%~dp0
SET build_dir=%1
echo %build_dir%

echo "copying optick dll to output directory : vendor/Optick/lib/x64/release/OptickCore.dll -> %build_dir%OptickCore.dll"
ROBOCOPY %wd%vendor\Optick\lib\x64\release %build_dir% "OptickCore.dll"
IF %ERRORLEVEL% GEQ 2 (echo Error: %ERRORLEVEL% && exit)

echo %wd%
echo "Compiling shaders..."

echo "assets\shaders\sauce\Builtin.MaterialShader.vert.glsl -> assets\shaders\bin\Builtin.MaterialShader.vert.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert "%wd%assets\shaders\sauce\Builtin.MaterialShader.vert.glsl" -o "%wd%assets\shaders\bin\Builtin.MaterialShader.vert.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "assets\shaders\sauce\Builtin.MaterialShader.frag.glsl -> assets\shaders\bin\Builtin.MaterialShader.frag.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag "%wd%assets\shaders\sauce\Builtin.MaterialShader.frag.glsl" -o "%wd%assets\shaders\bin\Builtin.MaterialShader.frag.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "assets\shaders\sauce\Builtin.UIShader.vert.glsl -> assets\shaders\bin\Builtin.UIShader.vert.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert "%wd%assets\shaders\sauce\Builtin.UIShader.vert.glsl" -o "%wd%assets\shaders\bin\Builtin.UIShader.vert.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "assets\shaders\sauce\Builtin.UIShader.frag.glsl -> assets\shaders\bin\Builtin.UIShader.frag.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag "%wd%assets\shaders\sauce\Builtin.UIShader.frag.glsl" -o "%wd%assets\shaders\bin\Builtin.UIShader.frag.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "assets\shaders\sauce\Builtin.SkyboxShader.vert.glsl -> assets\shaders\bin\Builtin.SkyboxShader.vert.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert "%wd%assets\shaders\sauce\Builtin.SkyboxShader.vert.glsl" -o "%wd%assets\shaders\bin\Builtin.SkyboxShader.vert.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "assets\shaders\sauce\Builtin.SkyboxShader.frag.glsl -> assets\shaders\bin\Builtin.SkyboxShader.frag.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag "%wd%assets\shaders\sauce\Builtin.SkyboxShader.frag.glsl" -o "%wd%assets\shaders\bin\Builtin.SkyboxShader.frag.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "assets\shaders\sauce\Builtin.WorldPickShader.vert.glsl -> assets\shaders\bin\Builtin.WorldPickShader.vert.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert "%wd%assets\shaders\sauce\Builtin.WorldPickShader.vert.glsl" -o "%wd%assets\shaders\bin\Builtin.WorldPickShader.vert.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "assets\shaders\sauce\Builtin.WorldPickShader.frag.glsl -> assets\shaders\bin\Builtin.WorldPickShader.frag.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag "%wd%assets\shaders\sauce\Builtin.WorldPickShader.frag.glsl" -o "%wd%assets\shaders\bin\Builtin.WorldPickShader.frag.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "assets\shaders\sauce\Builtin.UIPickShader.vert.glsl -> assets\shaders\bin\Builtin.UIPickShader.vert.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert "%wd%assets\shaders\sauce\Builtin.UIPickShader.vert.glsl" -o "%wd%assets\shaders\bin\Builtin.UIPickShader.vert.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "assets\shaders\sauce\Builtin.UIPickShader.frag.glsl -> assets\shaders\bin\Builtin.UIPickShader.frag.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag "%wd%assets\shaders\sauce\Builtin.UIPickShader.frag.glsl" -o "%wd%assets\shaders\bin\Builtin.UIPickShader.frag.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "Done."
