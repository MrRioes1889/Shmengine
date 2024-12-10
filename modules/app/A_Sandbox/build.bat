@echo off
setlocal enabledelayedexpansion

if "%~1"=="init" @call "D:\Programme\Microsoft VS\Microsoft Visual Studio Community 2022\VC\Auxiliary\Build\vcvars64.bat"

set compiler=cl

set includes=/I"..\..\..\Shmengine\sauce" /I"..\..\..\vendor\Optick\include"
set defines=-D_WIN32=1 -DLIB_COMPILE=1 -DPLATFORM_WINDOWS=1 -DSHMEXPORT=1 -DDEBUG=1 -D_WINDLL=1 -D_UNICODE=1 -DUNICODE=1
set disabled_warnings=/wd4005 /wd4100 /wd4189 /wd4201 /wd4505 /wd6011 /wd4251
set compiler_flags=/LDd /MDd /GS /W4 /Zc:wchar_t /Gm- /Od /Ob1 /Zc:inline /fp:precise /fp:except /errorReport:prompt /WX /Zc:forScope /RTC1 /Gd /std:c++20 /FC /WX /W4 /FC /Zi /EHsc /nologo %includes% %defines% %disabled_warnings%

set libs=/DYNAMICBASE "Shmengine.lib" "..\..\..\vendor\Optick\lib\x64\release\OptickCore.lib"

set linker_flags=/DEBUG /incremental:no /opt:ref %libs% /MACHINE:X64 /WX /SUBSYSTEM:WINDOWS /ERRORREPORT:PROMPT /NOLOGO

set int_dir=..\..\..\bin-int\Debug\A_Sandbox
IF NOT EXIST %int_dir% mkdir %int_dir%
pushd %int_dir%

set cdir=..\..\..\modules\app\A_Sandbox\sauce

:: List of source files
set SOURCES="%cdir%\Sandbox.cpp" "%cdir%\DebugConsole.cpp" "%cdir%\Keybinds.cpp"

:: Compile each source file into an object (.obj) file
for %%f in (%SOURCES%) do (
    %COMPILER% /c %%f %compiler_flags%
)

popd

:: Linking

set pdb_name=A_Sandbox_%date:~-4,4%%date:~-10,2%%date:~-7,2%_%time:~0,2%%time:~3,2%%time:~6,2%.pdb

set bin_dir=..\..\..\bin\Debug-windows-x86_64\Sandbox
IF NOT EXIST %bin_dir% mkdir %bin_dir%
pushd %bin_dir%

%COMPILER% "%int_dir%\*.obj" /link /DLL %linker_flags% /OUT:"A_Sandbox.dll" /PDB:"%pdb_name%"

set count=0
FOR %%f IN (A_Sandbox_*.pdb) DO (
    set /a count+=1
)

if !count! geq 3 (
    FOR /F "tokens=1,* delims= " %%f IN ('dir /T:C /O:D /B "A_Sandbox_*.pdb"') DO (
        if !count! geq 3 (
            echo deleting %%f
            del %%f
            set /a count-=1
        )
    )
)

popd





