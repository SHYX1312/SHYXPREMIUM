@echo off
setlocal
title SHYX TWEAKS PREMIUM - Build

echo Building SHYX_TWEAKS_PREMIUM.exe ...

where gcc >nul 2>&1
if errorlevel 1 (
  echo ERROR: gcc not found. Install MinGW-w64 and add gcc to PATH.
  echo Tip: MSYS2 UCRT64 + mingw-w64-ucrt-x86_64-gcc
  pause
  exit /b 1
)

gcc -O2 -s -mwindows -o SHYX_TWEAKS_PREMIUM.exe SHYX_TWEAKS_PREMIUM.c ^
  -lcomctl32 -lshell32 -luxtheme -lgdi32

if errorlevel 1 (
  echo Build failed.
  pause
  exit /b 1
)

echo Build OK: SHYX_TWEAKS_PREMIUM.exe
pause
