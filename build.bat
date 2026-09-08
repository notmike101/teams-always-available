@echo off
setlocal
cd /d "%~dp0"
if /i "%VSCMD_ARG_TGT_ARCH%"=="x64" goto :build
set "VSWHERE=%ProgramFiles(x86)%\Microsoft Visual Studio\Installer\vswhere.exe"
if not exist "%VSWHERE%" goto :missing
for /f "usebackq tokens=*" %%i in (`"%VSWHERE%" -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath`) do set "VS=%%i"
if not defined VS goto :missing
call "%VS%\VC\Auxiliary\Build\vcvars64.bat" >nul
if errorlevel 1 exit /b 1
:build
if not exist build mkdir build
if errorlevel 1 exit /b 1
cl /nologo /O2 /W4 /WX /MT /LD hookdll.cpp /Fe:build\hook2.dll /Fo:build\hook.obj /link user32.lib
if errorlevel 1 exit /b 1
cl /nologo /O2 /W4 /WX /MT app.cpp /Fe:build\app.exe /Fo:build\app.obj
if errorlevel 1 exit /b 1
if /i not "%~1"=="test" exit /b 0
cl /nologo /O2 /W4 /WX /MT test-hook.cpp /Fe:build\test-hook.exe /Fo:build\test-hook.obj /link user32.lib
if errorlevel 1 exit /b 1
build\test-hook.exe
exit /b %errorlevel%
:missing
echo Install Visual Studio C++ Build Tools with the Desktop development with C++ workload and a Windows SDK.
exit /b 1
