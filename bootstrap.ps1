# ============================================
# SHYX TWEAKS PREMIUM - Bootstrap Installer
# Version: 2.3.0
# GitHub: https://github.com/SHYX1312/SHYXPREMIUM
# ============================================

# Check for admin rights
if (-NOT ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")) {
    Write-Host "[!] Administrator privileges required!" -ForegroundColor Red
    Write-Host "[*] Restarting script as administrator..." -ForegroundColor Yellow
    
    # Restart as admin
    $scriptPath = $MyInvocation.MyCommand.Path
    Start-Process powershell -ArgumentList "-NoProfile -ExecutionPolicy Bypass -File `"$scriptPath`"" -Verb RunAs
    exit
}

# Now running as admin
Clear-Host
Write-Host @"

╔══════════════════════════════════════════════════════════╗
║                SHYX TWEAKS PREMIUM                        ║
║           Competitive Gaming Optimization                 ║
║                  Version 2.3.0                            ║
║        GitHub: SHYX1312/SHYXPREMIUM                       ║
╚══════════════════════════════════════════════════════════╝
"@ -ForegroundColor Cyan

Write-Host "[+] Running with administrator privileges" -ForegroundColor Green
Write-Host ""

# Configuration - USING YOUR GITHUB
$WorkingDir = "$env:ProgramData\SHYX_TWEAKS"
$Executable = "SHYX_TWEAKS_PREMIUM.exe"
$GitHubUser = "SHYX1312"
$GitHubRepo = "SHYXPREMIUM"

# Create working directory
try {
    Write-Host "[*] Creating working directory..." -ForegroundColor Yellow
    if (-not (Test-Path $WorkingDir)) {
        New-Item -Path $WorkingDir -ItemType Directory -Force | Out-Null
        Write-Host "[+] Created: $WorkingDir" -ForegroundColor Green
    } else {
        Write-Host "[+] Using existing directory" -ForegroundColor Green
    }
    Set-Location $WorkingDir
} catch {
    Write-Host "[-] Error: $_" -ForegroundColor Red
    pause
    exit 1
}

# Download function with YOUR GitHub URLs
function Download-File {
    param(
        [string]$FileName,
        [string]$OutputName = $FileName
    )
    
    $urls = @(
        "https://github.com/$GitHubUser/$GitHubRepo/releases/latest/download/$FileName",
        "https://raw.githubusercontent.com/$GitHubUser/$GitHubRepo/main/release/$FileName",
        "https://raw.githubusercontent.com/$GitHubUser/$GitHubRepo/main/$FileName"
    )
    
    foreach ($url in $urls) {
        try {
            Write-Host "[*] Trying: $url" -ForegroundColor Gray
            $ProgressPreference = 'SilentlyContinue'
            
            # Try WebClient first (more reliable)
            try {
                $webClient = New-Object System.Net.WebClient
                $webClient.DownloadFile($url, $OutputName)
                $webClient.Dispose()
                
                if (Test-Path $OutputName -PathType Leaf) {
                    Write-Host "[+] Downloaded: $FileName" -ForegroundColor Green
                    return $true
                }
            } catch {
                # Try Invoke-WebRequest as fallback
                try {
                    Invoke-WebRequest -Uri $url -OutFile $OutputName -UseBasicParsing -ErrorAction Stop
                    if (Test-Path $OutputName) {
                        Write-Host "[+] Downloaded: $FileName" -ForegroundColor Green
                        return $true
                    }
                } catch {
                    continue
                }
            }
        } catch {
            continue
        }
    }
    
    Write-Host "[-] Failed to download: $FileName" -ForegroundColor Red
    return $false
}

# Download main executable
Write-Host "[*] Downloading main application..." -ForegroundColor Yellow
if (Download-File -FileName $Executable) {
    $fileSize = (Get-Item $Executable).Length / 1MB
    Write-Host "[+] File size: $($fileSize.ToString('0.0')) MB" -ForegroundColor Green
} else {
    Write-Host "[-] CRITICAL: Cannot download main executable" -ForegroundColor Red
    Write-Host "[*] Please visit: https://github.com/SHYX1312/SHYXPREMIUM" -ForegroundColor Yellow
    Write-Host "[*] Download the EXE manually and place it in: $WorkingDir" -ForegroundColor Yellow
    pause
    exit 1
}

# Download presets
Write-Host "[*] Downloading presets..." -ForegroundColor Yellow
$presets = @("fps-competitive.json", "battle-royale.json", "tactical.json")
foreach ($preset in $presets) {
    if (Download-File -FileName $preset) {
        Write-Host "[+] Preset: $preset" -ForegroundColor Green
    } else {
        Write-Host "[!] Missing preset: $preset" -ForegroundColor Yellow
    }
}

# Create default config if missing
if (-not (Test-Path "config.json")) {
    Write-Host "[*] Creating default configuration..." -ForegroundColor Yellow
    @'
{
  "version": "2.3.0",
  "name": "SHYX TWEAKS PREMIUM",
  "description": "Competitive Gaming Optimization",
  "author": "SHYX1312",
  "github": "https://github.com/SHYX1312/SHYXPREMIUM",
  "compatible_games": [
    "Fortnite",
    "Counter-Strike 2",
    "Valorant",
    "Apex Legends",
    "Call of Duty: Warzone",
    "Rainbow Six Siege",
    "Overwatch 2"
  ],
  "anti_cheat_safe": true
}
'@ | Out-File -FilePath "config.json" -Encoding UTF8
    Write-Host "[+] Created default config" -ForegroundColor Green
}

# Launch application
Write-Host ""
Write-Host "=" * 60
Write-Host "[*] LAUNCHING SHYX TWEAKS PREMIUM" -ForegroundColor Cyan
Write-Host "[!] IMPORTANT: No tweaks are applied automatically!" -ForegroundColor Yellow
Write-Host "[!] You must select and apply tweaks manually." -ForegroundColor Yellow
Write-Host "=" * 60
Write-Host ""

Start-Sleep -Seconds 2

try {
    if (Test-Path $Executable) {
        # Launch the executable
        Write-Host "[*] Starting application..." -ForegroundColor Yellow
        Start-Process -FilePath ".\$Executable" -Wait
        
        Write-Host "[+] Application closed" -ForegroundColor Green
    } else {
        Write-Host "[-] ERROR: Executable not found!" -ForegroundColor Red
    }
} catch {
    Write-Host "[-] Failed to launch: $_" -ForegroundColor Red
}

# Final instructions
Write-Host ""
Write-Host "=" * 60
Write-Host "[*] SETUP COMPLETE" -ForegroundColor Cyan
Write-Host "[*] Files are in: $WorkingDir" -ForegroundColor Gray
Write-Host "[*] GitHub: https://github.com/SHYX1312/SHYXPREMIUM" -ForegroundColor Gray
Write-Host "[*] To run again: Open folder and double-click $Executable" -ForegroundColor Gray
Write-Host "=" * 60
Write-Host ""

Write-Host "Press any key to exit..."
$null = $Host.UI.RawUI.ReadKey("NoEcho,IncludeKeyDown")