# ============================================
# SHYX TWEAKS PREMIUM - Bootstrap Installer
# Version: 2.2.0
# ============================================

# Check for admin rights
$adminCheck = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)
if (-not $adminCheck) {
    Write-Host "[!] Administrator privileges required!" -ForegroundColor Red
    Write-Host "[*] Restarting as administrator..." -ForegroundColor Yellow
    
    $scriptPath = $MyInvocation.MyCommand.Path
    Start-Process PowerShell -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$scriptPath`"" -Verb RunAs
    exit
}

# Now running as admin
Clear-Host
Write-Host @"

╔══════════════════════════════════════════════════════════╗
║                SHYX TWEAKS PREMIUM                        ║
║           Competitive Gaming Optimization                 ║
║                  Version 2.2.0                            ║
╚══════════════════════════════════════════════════════════╝
"@ -ForegroundColor Cyan

Write-Host "[+] Running with administrator privileges" -ForegroundColor Green
Write-Host ""

# Configuration
$BaseURL = "https://files.shyxtweaks.com/premium/v2"
$WorkingDir = "$env:ProgramData\SHYX_TWEAKS"
$Executable = "SHYX_TWEAKS_PREMIUM.exe"
$Presets = @("fps-competitive.json", "battle-royale.json", "tactical.json")

# Create working directory
try {
    Write-Host "[*] Creating working directory..." -ForegroundColor Yellow
    if (-not (Test-Path $WorkingDir)) {
        New-Item -Path $WorkingDir -ItemType Directory -Force | Out-Null
    }
    Set-Location $WorkingDir
    Write-Host "[+] Directory: $WorkingDir" -ForegroundColor Green
} catch {
    Write-Host "[-] Error: $_" -ForegroundColor Red
    Pause
    exit 1
}

# Download function
function Download-File {
    param([string]$Url, [string]$Output)
    
    try {
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $Url -OutFile $Output -UseBasicParsing
        return $true
    } catch {
        return $false
    }
}

# Download main executable
Write-Host "[*] Downloading main application..." -ForegroundColor Yellow
$exeUrl = "$BaseURL/release/$Executable"
if (Download-File -Url $exeUrl -Output $Executable) {
    Write-Host "[+] Downloaded: $Executable" -ForegroundColor Green
} else {
    Write-Host "[-] Failed to download main executable" -ForegroundColor Red
    Pause
    exit 1
}

# Download presets
Write-Host "[*] Downloading presets..." -ForegroundColor Yellow
foreach ($preset in $Presets) {
    $presetUrl = "$BaseURL/presets/$preset"
    if (Download-File -Url $presetUrl -Output $preset) {
        Write-Host "[+] Downloaded: $preset" -ForegroundColor Green
    } else {
        Write-Host "[!] Failed: $preset" -ForegroundColor Yellow
    }
}

# Create default config if missing
if (-not (Test-Path "config.json")) {
    @{
        "version" = "2.2.0"
        "name" = "SHYX TWEAKS PREMIUM"
        "compatible_games" = @(
            "Fortnite", "Counter-Strike 2", "Valorant", "Apex Legends",
            "Call of Duty: Warzone", "Rainbow Six Siege", "Overwatch 2"
        )
    } | ConvertTo-Json | Out-File "config.json"
    Write-Host "[+] Created default config" -ForegroundColor Green
}

# Launch application
Write-Host ""
Write-Host "=" * 60
Write-Host "[*] Launching SHYX TWEAKS PREMIUM..." -ForegroundColor Cyan
Write-Host "[!] IMPORTANT: No tweaks are applied automatically!" -ForegroundColor Yellow
Write-Host "[!] You must select and apply tweaks manually in the GUI." -ForegroundColor Yellow
Write-Host "=" * 60
Write-Host ""

Start-Sleep -Seconds 2

try {
    $process = Start-Process -FilePath ".\$Executable" -PassThru -Wait
    if ($process.ExitCode -eq 0) {
        Write-Host "[+] Application closed successfully" -ForegroundColor Green
    }
} catch {
    Write-Host "[-] Failed to launch: $_" -ForegroundColor Red
}

Write-Host ""
Write-Host "[*] Setup complete. Files are in: $WorkingDir" -ForegroundColor Gray
Write-Host "[*] To run again, open that folder and double-click $Executable" -ForegroundColor Gray
Write-Host ""
Pause