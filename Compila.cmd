@echo off
setlocal
cd /d "%~dp0"
where gcc >nul 2>&1 || goto :strumenti
where windres >nul 2>&1 || goto :strumenti
for /f "delims=" %%T in ('gcc -dumpmachine') do set "ORDINE_TARGET=%%T"
if /i not "%ORDINE_TARGET%"=="x86_64-w64-mingw32" goto :strumenti
if not exist "build" mkdir "build"
windres --codepage=65001 -i "InOrdine.rc" -O coff -o "build\risorse.o"
if errorlevel 1 exit /b 1
gcc -std=c11 -O2 -finput-charset=UTF-8 -mwindows -o "build\InOrdine.exe" "src\main.c" "winmain.c" "build\risorse.o" -lgdiplus -lcomctl32 -lshell32 -lole32 -lgdi32 -luser32 -lkernel32 -ladvapi32 -lcomdlg32
if errorlevel 1 exit /b 1
echo Creato "%CD%\build\InOrdine.exe".
exit /b 0
:strumenti
echo Per compilare servono gcc e windres della toolchain MinGW-w64 x64 nel PATH.
exit /b 1
