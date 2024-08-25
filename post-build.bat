@echo off

SET wd=%~dp0
SET build_dir=%1
echo %build_dir%

echo "copying optick dll to output directory : vendor/Optick/lib/x64/release/OptickCore.dll -> %build_dir%\OptickCore.dll"
ROBOCOPY %wd%vendor\Optick\lib\x64\release %wd%%build_dir% "OptickCore.dll"
IF %ERRORLEVEL% GEQ 2 (echo Error: %ERRORLEVEL% && exit)

echo %wd%
echo "Compiling shaders..."

echo "assets\shaders\Builtin.MaterialShader.vert.glsl -> assets\shaders\Builtin.MaterialShader.vert.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert "%wd%assets\shaders\Builtin.MaterialShader.vert.glsl" -o "%wd%assets\shaders\Builtin.MaterialShader.vert.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "assets\shaders\Builtin.MaterialShader.frag.glsl -> assets\shaders\Builtin.MaterialShader.frag.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag "%wd%assets\shaders\Builtin.MaterialShader.frag.glsl" -o "%wd%assets\shaders\Builtin.MaterialShader.frag.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)


echo "assets\shaders\Builtin.UIShader.vert.glsl -> assets\shaders\Builtin.UIShader.vert.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert "%wd%assets\shaders\Builtin.UIShader.vert.glsl" -o "%wd%assets\shaders\Builtin.UIShader.vert.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "assets\shaders\Builtin.UIShader.frag.glsl -> assets\shaders\Builtin.UIShader.frag.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag "%wd%assets\shaders\Builtin.UIShader.frag.glsl" -o "%wd%assets\shaders\Builtin.UIShader.frag.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "assets\shaders\Builtin.SkyboxShader.vert.glsl -> assets\shaders\Builtin.SkyboxShader.vert.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert "%wd%assets\shaders\Builtin.SkyboxShader.vert.glsl" -o "%wd%assets\shaders\Builtin.SkyboxShader.vert.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "assets\shaders\Builtin.SkyboxShader.frag.glsl -> assets\shaders\Builtin.SkyboxShader.frag.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag "%wd%assets\shaders\Builtin.SkyboxShader.frag.glsl" -o "%wd%assets\shaders\Builtin.SkyboxShader.frag.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "Done."
