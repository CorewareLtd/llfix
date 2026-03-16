@echo off

:: Use vswhere to find the latest installed Visual Studio
for /f "usebackq tokens=*" %%a in (`"%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do (
    set "VS_INSTALL_DIR=%%a"
)

:: Check if the variable was set
if not defined VS_INSTALL_DIR (
    echo Visual Studio installation not found.
    exit /b 1
)

:: Call the developer command prompt
call "%VS_INSTALL_DIR%\Common7\Tools\VsDevCmd.bat" -arch=x64

set "EXECUTABLE_NAME=sequence_store_manager"
set "TRANSLATION_UNIT_NAMES="sequence_store_manager.cpp""
set "INCLUDE_PATHS=/I"./../../include""
set "LINK_LIBS=Advapi32.lib user32.lib"
set "CPP_STANDARD=/std:c++17"

REM Set the console colour to yellow
color 0E

REM Build the C++ files using MSVC, no O3 in MSVC
cl.exe /EHsc %INCLUDE_PATHS% %CPP_STANDARD% /MD /D NDEBUG /O2 %TRANSLATION_UNIT_NAMES% /Fe:%EXECUTABLE_NAME%.exe /link /subsystem:console %LINK_LIBS%

REM Delete the object file generated during compilation
del %EXECUTABLE_NAME%.obj

REM Pause the script so you can see the build output
pause
