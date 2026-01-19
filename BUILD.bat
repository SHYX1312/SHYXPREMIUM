@echo off
chcp 65001 >nul
title SHYX TWEAKS PREMIUM - Builder

echo ============================================
echo    SHYX TWEAKS PREMIUM - C++ Build Script
echo ============================================
echo.

:: Check for MinGW
where g++ >nul 2>&1
if %errorlevel% neq 0 (
    echo ERROR: MinGW (g++) is not installed or not in PATH!
    echo.
    echo Please install MinGW from:
    echo https://sourceforge.net/projects/mingw/
    echo.
    echo Or use MSVC from Visual Studio Build Tools.
    pause
    exit /b 1
)

echo [1] Compiling C++ source code...
echo This will create a native Windows .exe with no dependencies!
echo.

:: Compile with MinGW
g++ -o SHYX_TWEAKS_PREMIUM.exe SHYX_TWEAKS_PREMIUM.c ^
    -mwindows ^
    -lcomctl32 ^
    -lwininet ^
    -s ^
    -Os ^
    -static

if %errorlevel% neq 0 (
    echo.
    echo ERROR: Compilation failed!
    echo.
    echo Alternative: Use Visual Studio (MSVC):
    echo cl SHYX_TWEAKS_PREMIUM.c ^/FeSHYX_TWEAKS_PREMIUM.exe ^
    echo    user32.lib gdi32.lib comctl32.lib shell32.lib advapi32.lib
    pause
    exit /b 1
)

:: Check file size
for %%F in (SHYX_TWEAKS_PREMIUM.exe) do set size=%%~zF
set /a sizeMB=size/1024/1024

echo [2] Build successful!
echo.
echo File: SHYX_TWEAKS_PREMIUM.exe
echo Size: %size% bytes (%sizeMB% MB)
echo.
echo [3] Creating supporting files...
echo.

:: Create config.json
echo { > config.json
echo   "version": "2.2.0", >> config.json
echo   "name": "SHYX TWEAKS PREMIUM" >> config.json
echo } >> config.json

:: Create presets directory
if not exist presets mkdir presets

:: Create preset files
echo {"name":"FPS Competitive","tweaks":["Ultimate Performance","CPU Priority","Game Mode"]} > presets\fps-competitive.json
echo {"name":"Battle Royale","tweaks":["CPU Priority","Game Mode","GPU Scheduling"]} > presets\battle-royale.json
echo {"name":"Tactical","tweaks":["Ultimate Performance","CPU Priority","HPET Disable"]} > presets\tactical.json

echo [4] Creating hash for verification...
python -c "import hashlib; h=hashlib.sha256(); h.update(open('SHYX_TWEAKS_PREMIUM.exe','rb').read()); print(h.hexdigest()+'  SHYX_TWEAKS_PREMIUM.exe')" > hashes.sha256 2>nul

if %errorlevel% neq 0 (
    powershell -Command "Get-FileHash SHYX_TWEAKS_PREMIUM.exe -Algorithm SHA256 | Format-List | Out-File hashes.sha256"
)

echo.
echo ============================================
echo    BUILD COMPLETE!
echo ============================================
echo.
echo Files created:
echo - SHYX_TWEAKS_PREMIUM.exe (Standalone executable)
echo - config.json
echo - presets\*.json
echo - hashes.sha256
echo.
echo To test, run: SHYX_TWEAKS_PREMIUM.exe
echo (Right-click -> Run as Administrator)
echo.
echo For distribution, package with bootstrap.ps1
echo.
pause