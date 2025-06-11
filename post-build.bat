@echo off
setlocal

SET wd=%~dp0
IF "%1"=="" (echo Error: need to provide build directory as first argument && exit /B)
SET build_dir=%1

IF NOT EXIST "%build_dir%OptickCore.dll" (
    echo "copying optick dll to output directory : %wd%vendor\Optick\lib\x64\debug -> %build_dir%"
    ROBOCOPY "%wd%vendor\Optick\lib\x64\debug " "%build_dir% " "OptickCore.dll"
    IF %ERRORLEVEL% GEQ 2 (echo Error: %ERRORLEVEL% && exit)
)

echo %wd%
echo "Compiling shaders..."

IF NOT EXIST "%wd%assets\shaders\bin\" mkdir "%wd%assets\shaders\bin"
IF NOT EXIST "%wd%assets\shaders\sauce\" mkdir "%wd%assets\shaders\sauce"

cd "%wd%assets\shaders\sauce"

for %%f in (*vert.glsl) do (
    echo "%%f -> %%~nf.spv"
    %VULKAN_SDK%\bin\glslc.exe -fshader-stage=vert "%CD%\%%f" -o "%CD%\..\bin\%%~nf.spv"
    IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit /B)
)

for %%f in (*frag.glsl) do (
    echo "%%f -> %%~nf.spv"
    %VULKAN_SDK%\bin\glslc.exe -fshader-stage=frag "%CD%\%%f" -o "%CD%\..\bin\%%~nf.spv"
    IF %ERRORLEVEL% NEQ 0 (echo Error: %ERRORLEVEL% && exit /B)
)

endlocal

echo "Done."
