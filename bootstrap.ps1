# ============================================
# SHYX TWEAKS PREMIUM - Bootstrap Installer
# Downloads .exe + config + presets from GitHub Releases and launches GUI
# ============================================

# Force TLS 1.2 for older Windows
try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 } catch {}

# ---- CONFIG ----
$WorkingDir = "$env:ProgramData\SHYX_TWEAKS"
$Executable = "SHYX_TWEAKS_PREMIUM.exe"

$GitHubUser = "SHYX1312"
$GitHubRepo = "SHYXPREMIUM"

# Prefer Releases "latest" (so users always get newest)
$ReleaseBase = "https://github.com/$GitHubUser/$GitHubRepo/releases/latest/download"

# Files to pull from release assets
$MainFiles = @(
    $Executable,
    "config.json",
    "fps-competitive.json",
    "battle-royale.json",
    "tactical.json"
)

$HashFile = "hashes.sha256" # optional but recommended to upload as an asset

# ---- ADMIN CHECK ----
$IsAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")

if (-not $IsAdmin) {
    Write-Host "[!] Administrator privileges required." -ForegroundColor Red
    Write-Host "[*] Restarting as administrator..." -ForegroundColor Yellow
    Start-Process powershell -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -Command `"irm '$ReleaseBase/bootstrap.ps1' | iex`""
    exit
}

# ---- SETUP DIR ----
New-Item -ItemType Directory -Path $WorkingDir -Force | Out-Null
Set-Location $WorkingDir

function Download-File {
    param(
        [Parameter(Mandatory=$true)][string]$Url,
        [Parameter(Mandatory=$true)][string]$OutFile
    )
    $ProgressPreference = "SilentlyContinue"
    Invoke-WebRequest -Uri $Url -OutFile $OutFile -UseBasicParsing -ErrorAction Stop
}

function Get-ExpectedHashFromFile {
    param(
        [string]$HashPath,
        [string]$FileName
    )
    if (-not (Test-Path $HashPath)) { return $null }

    $lines = Get-Content $HashPath -ErrorAction SilentlyContinue
    foreach ($line in $lines) {
        # expected format: "<sha256>  <filename>"
        if ($line -match "^[a-fA-F0-9]{64}\s+\*?(.+)$") {
            $hash = $line.Split(" ")[0].Trim()
            $name = $line.Substring($line.IndexOf(" ")).Trim()
            $name = $name.TrimStart("*").Trim()
            if ($name -ieq $FileName) { return $hash }
        }
    }
    return $null
}

function Verify-Hash {
    param(
        [string]$FilePath,
        [string]$ExpectedHash
    )
    if (-not $ExpectedHash) { return $true } # skip if not available
    $actual = (Get-FileHash -Algorithm SHA256 -Path $FilePath).Hash.ToLower()
    return ($actual -eq $ExpectedHash.ToLower())
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "   SHYX TWEAKS PREMIUM - Bootstrap Installer" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "[+] Running as Administrator" -ForegroundColor Green
Write-Host "[*] Working dir: $WorkingDir" -ForegroundColor Gray
Write-Host ""

# ---- TRY DOWNLOAD hashes.sha256 (optional) ----
$HashPath = Join-Path $WorkingDir $HashFile
try {
    Write-Host "[*] Attempting to download hash manifest: $HashFile" -ForegroundColor Yellow
    Download-File -Url "$ReleaseBase/$HashFile" -OutFile $HashPath
    Write-Host "[+] Hash manifest downloaded." -ForegroundColor Green
} catch {
    Write-Host "[!] hashes.sha256 not found in release assets (hash verification will be skipped)." -ForegroundColor Yellow
}

# ---- DOWNLOAD MAIN FILES ----
foreach ($f in $MainFiles) {
    $dest = Join-Path $WorkingDir $f
    $url  = "$ReleaseBase/$f"

    try {
        Write-Host "[*] Downloading: $f" -ForegroundColor Yellow
        Download-File -Url $url -OutFile $dest

        $expected = Get-ExpectedHashFromFile -HashPath $HashPath -FileName $f
        if (-not (Verify-Hash -FilePath $dest -ExpectedHash $expected)) {
            throw "SHA256 mismatch for $f"
        }

        Write-Host "[+] OK: $f" -ForegroundColor Green
    } catch {
        Write-Host "[-] Failed: $f" -ForegroundColor Red
        Write-Host "    $_" -ForegroundColor Red
        Write-Host "[!] Make sure $f exists as a Release Asset in: releases/latest/download/" -ForegroundColor Yellow
        exit 1
    }
}

# ---- LAUNCH EXE ----
$ExePath = Join-Path $WorkingDir $Executable
if (-not (Test-Path $ExePath)) {
    Write-Host "[-] Executable missing: $ExePath" -ForegroundColor Red
    exit 1
}

Write-Host ""
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "[*] Launching SHYX TWEAKS PREMIUM..." -ForegroundColor Cyan
Write-Host "[!] Nothing is applied automatically." -ForegroundColor Yellow
Write-Host "============================================" -ForegroundColor Cyan
Write-Host ""

Start-Process -FilePath $ExePath
