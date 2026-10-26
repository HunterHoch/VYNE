@echo off
setlocal
cd /d "%~dp0.."
where cl.exe >nul 2>nul
if errorlevel 1 (
  echo Run this script from an x64 Visual Studio Developer Command Prompt.
  exit /b 1
)
if not exist build mkdir build
cl /nologo /std:c++17 /EHsc /W4 /WX /MT /LD /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX src\bootstrap.cpp /Fobuild\bootstrap.obj /link /OUT:build\VYNE.asi /IMPLIB:build\VYNE.lib version.lib
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /WX /MT tests\diagnostics_host.cpp /Fobuild\diagnostics_host.obj /Febuild\diagnostics_host.exe
if errorlevel 1 exit /b 1
build\diagnostics_host.exe
exit /b %errorlevel%
