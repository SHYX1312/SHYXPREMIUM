# SHYX TWEAKS PREMIUM - bootstrap.ps1
# Repo: SHYX1312/SHYXPREMIUM
try { [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12 } catch {}

$WorkingDir  = "$env:ProgramData\SHYX_TWEAKS"
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

Write-Host "SHYX TWEAKS PREMIUM - Installer" -ForegroundColor Cyan
Write-Host "[*] Install dir: $WorkingDir" -ForegroundColor Gray
Write-Host "[!] Nothing is applied automatically." -ForegroundColor Yellow
Write-Host ""

foreach ($a in $Assets) {
  $url = "$ReleaseBase/$a"
  $dst = Join-Path $WorkingDir $a
  try {
    Write-Host "[*] Downloading: $a" -ForegroundColor Yellow
    Download-File $url $dst
    Write-Host "[+] OK: $a" -ForegroundColor Green
  } catch {
    Write-Host "[-] Failed: $a" -ForegroundColor Red
    Write-Host "    $url" -ForegroundColor Red
    exit 1
  }
}

$exe = Join-Path $WorkingDir "SHYX_TWEAKS_PREMIUM.exe"
if (-not (Test-Path $exe)) { Write-Host "[-] Missing EXE!" -ForegroundColor Red; exit 1 }

Start-Process -FilePath $exe
