@echo off
setlocal
cd /d "%~dp0.."
where cl.exe >nul 2>nul
if errorlevel 1 (
  echo Run this script from an x64 Visual Studio Developer Command Prompt.
  exit /b 1
)
if not exist build mkdir build
if not exist vendor\minhook\include\MinHook.h (
  echo Initialize the pinned dependency with: git submodule update --init
  exit /b 1
)
if not exist build\minhook mkdir build\minhook
cl /nologo /c /TC /O2 /W3 /MT vendor\minhook\src\buffer.c vendor\minhook\src\hook.c vendor\minhook\src\trampoline.c vendor\minhook\src\hde\hde64.c /Fobuild\minhook\
if errorlevel 1 exit /b 1
rc /nologo /fobuild\builds.res src\builds.rc
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /WX /MT /LD /Ivendor\minhook\include /DUNICODE /D_UNICODE /DWIN32_LEAN_AND_MEAN /DNOMINMAX src\bootstrap.cpp src\native_mode_probe.cpp src\graphics_mode.cpp src\export_menu.cpp src\export_settings.cpp src\native_output.cpp /Fobuild\ /link /OUT:build\VYNE.asi /IMPLIB:build\VYNE.lib build\builds.res build\minhook\buffer.obj build\minhook\hook.obj build\minhook\trampoline.obj build\minhook\hde64.obj version.lib user32.lib ole32.lib shell32.lib uuid.lib
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /WX /MT tests\diagnostics_host.cpp /Fobuild\diagnostics_host.obj /Febuild\diagnostics_host.exe
if errorlevel 1 exit /b 1
build\diagnostics_host.exe
if errorlevel 1 exit /b 1
cl /nologo /std:c++17 /EHsc /W4 /WX /MT tests\graphics_mode_test.cpp src\graphics_mode.cpp /Fobuild\ /Febuild\graphics_mode_test.exe
if errorlevel 1 exit /b 1
build\graphics_mode_test.exe
if errorlevel 1 exit /b 1
cl /nologo /utf-8 /std:c++17 /EHsc /W4 /WX /MT /Ivendor\minhook\include /DUNICODE /D_UNICODE tests\export_menu_test.cpp src\export_settings.cpp /Fobuild\ /Febuild\export_menu_test.exe /link ole32.lib shell32.lib uuid.lib
if errorlevel 1 exit /b 1
build\export_menu_test.exe
if errorlevel 1 exit /b 1
cl /nologo /utf-8 /std:c++17 /EHsc /W4 /WX /MT /DUNICODE /D_UNICODE tests\native_output_test.cpp src\native_output.cpp /Fobuild\ /Febuild\native_output_test.exe /link ole32.lib shell32.lib uuid.lib
if errorlevel 1 exit /b 1
build\native_output_test.exe
exit /b %errorlevel%
