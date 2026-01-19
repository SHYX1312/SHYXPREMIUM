# ============================================
# SHYX TWEAKS PREMIUM - Bootstrap (No Hashes)
# Repo: SHYX1312/SHYXPREMIUM
# ============================================

try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 } catch {}

$WorkingDir = "$env:ProgramData\SHYX_TWEAKS"
$ReleaseBase = "https://github.com/SHYX1312/SHYXPREMIUM/releases/latest/download"

$Assets = @(
  "SHYX_TWEAKS_PREMIUM.exe",
  "config.json",
  "fps-competitive.json",
  "battle-royale.json",
  "tactical.json"
)

function Ensure-Admin {
  $isAdmin = ([Security.Principal.WindowsPrincipal] [Security.Principal.WindowsIdentity]::GetCurrent()
  ).IsInRole([Security.Principal.WindowsBuiltInRole] "Administrator")

  if (-not $isAdmin) {
    Write-Host "[!] Admin required. Restarting elevated..." -ForegroundColor Yellow
    Start-Process powershell -Verb RunAs -ArgumentList "-NoProfile -ExecutionPolicy Bypass -Command `"irm '$ReleaseBase/bootstrap.ps1' | iex`""
    exit
  }
}

function Download-File([string]$url, [string]$outFile) {
  $ProgressPreference = "SilentlyContinue"
  Invoke-WebRequest -Uri $url -OutFile $outFile -UseBasicParsing -ErrorAction Stop
}

Ensure-Admin

New-Item -ItemType Directory -Path $WorkingDir -Force | Out-Null

Write-Host "============================================" -ForegroundColor Cyan
Write-Host "   SHYX TWEAKS PREMIUM - Bootstrap Installer" -ForegroundColor Cyan
Write-Host "============================================" -ForegroundColor Cyan
Write-Host "[*] Install dir: $WorkingDir" -ForegroundColor Gray
Write-Host "[!] No tweaks will be applied automatically." -ForegroundColor Yellow
Write-Host ""

foreach ($a in $Assets) {
  $url = "$ReleaseBase/$a"
  $dst = Join-Path $WorkingDir $a

  try {
    Write-Host "[*] Downloading: $a" -ForegroundColor Yellow
    Download-File $url $dst
    Write-Host "[+] OK: $a" -ForegroundColor Green
  } catch {
    Write-Host "[-] Failed to download: $a" -ForegroundColor Red
    Write-Host "    Check that the asset exists in your GitHub Release:" -ForegroundColor Red
    Write-Host "    $url" -ForegroundColor Red
    exit 1
  }
}

$exe = Join-Path $WorkingDir "SHYX_TWEAKS_PREMIUM.exe"
if (-not (Test-Path $exe)) {
  Write-Host "[-] Missing executable: $exe" -ForegroundColor Red
  exit 1
}

Write-Host ""
Write-Host "[*] Launching GUI..." -ForegroundColor Cyan
Start-Process -FilePath $exe
