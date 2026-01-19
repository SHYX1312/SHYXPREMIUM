@echo off
setlocal EnableExtensions EnableDelayedExpansion
title SHYX TWEAKS PREMIUM - Build

echo ============================================
echo   SHYX TWEAKS PREMIUM - Build Script
echo ============================================
echo.

:: Prefer gcc for .c, fallback to g++
where gcc >nul 2>&1
if %errorlevel%==0 (
  set CC=gcc
) else (
  where g++ >nul 2>&1
  if %errorlevel%==0 (
    set CC=g++
  ) else (
    echo ERROR: gcc or g++ not found in PATH.
    echo Install MinGW-w64 and add bin to PATH.
    pause
    exit /b 1
  )
)

echo [1] Compiling with: %CC%
echo.

%CC% -o SHYX_TWEAKS_PREMIUM.exe SHYX_TWEAKS_PREMIUM.c ^
  -mwindows ^
  -lcomctl32 ^
  -s ^
  -Os

if %errorlevel% neq 0 (
  echo.
  echo ERROR: Compilation failed.
  pause
  exit /b 1
)

echo [2] Build OK: SHYX_TWEAKS_PREMIUM.exe
echo.

echo [3] Generating hashes.sha256 ...
powershell -NoProfile -ExecutionPolicy Bypass -Command ^
  "$files=@('SHYX_TWEAKS_PREMIUM.exe','config.json','fps-competitive.json','battle-royale.json','tactical.json');" ^
  "Remove-Item -Force hashes.sha256 -ErrorAction SilentlyContinue;" ^
  "foreach($f in $files){ if(Test-Path $f){ $h=(Get-FileHash $f -Algorithm SHA256).Hash.ToLower(); Add-Content hashes.sha256 ($h+'  '+$f) } }"

echo.
echo ============================================
echo   BUILD COMPLETE
echo ============================================
echo Output:
echo - SHYX_TWEAKS_PREMIUM.exe
echo - hashes.sha256
echo.
pause
