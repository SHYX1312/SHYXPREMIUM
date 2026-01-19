@echo off
setlocal
title SHYX TWEAKS PREMIUM - Build

where gcc >nul 2>&1
if errorlevel 1 (
  echo ERROR: gcc not found. Install MinGW-w64 and add gcc to PATH.
  pause
  exit /b 1
)

echo Building SHYX_TWEAKS_PREMIUM.exe ...
gcc -O2 -s -mwindows -o SHYX_TWEAKS_PREMIUM.exe SHYX_TWEAKS_PREMIUM.c -lcomctl32

if errorlevel 1 (
  echo Build failed.
  pause
  exit /b 1
)

echo Build OK: SHYX_TWEAKS_PREMIUM.exe
pause
