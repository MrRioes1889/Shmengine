@echo off

SET wd=%~dp0

if not exist "%wd%bin\assets\shaders" mkdir "%wd%bin\assets\shaders"

echo %wd%
echo "Compiling shaders..."

echo "assets\shaders\Builtin.ObjectShader.vert.glsl -> assets\shaders\Builtin.ObjectShader.vert.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert "%wd%assets\shaders\Builtin.ObjectShader.vert.glsl" -o "%wd%assets\shaders\Builtin.ObjectShader.vert.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "assets\shaders\Builtin.ObjectShader.frag.glsl -> assets\shaders\Builtin.ObjectShader.frag.spv"
%VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag "%wd%assets\shaders\Builtin.ObjectShader.frag.glsl" -o "%wd%assets\shaders\Builtin.ObjectShader.frag.spv"
IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit)

echo "Copying assets..."
echo xcopy "%wd%assets" "%wd%bin\assets" /h /i /c /k /e /r /y
xcopy "%wd%assets" "%wd%bin\assets" /h /i /c /k /e /r /y

echo "Done."
