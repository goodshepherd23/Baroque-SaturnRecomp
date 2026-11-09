# Builds Baroque.exe and the release zip: dist\Baroque-SaturnRecomp-v<version>-windows-x64.zip
# Needs a MinGW-w64 gcc (MSYS2 mingw64). Usage: powershell -File tools\package.ps1 [-Gcc C:\msys64\mingw64\bin\gcc.exe]
param([string]$Gcc = 'C:\msys64\mingw64\bin\gcc.exe')
$ErrorActionPreference = 'Stop'
$repo = Split-Path -Parent $PSScriptRoot
$version = (Get-Content -LiteralPath "$repo\kit\kit-version.txt" -Raw).Trim()
$runtimeVersion = (Get-Content -LiteralPath "$repo\kit\runtime-version.txt" -Raw).Trim()
$stage = "$repo\dist\stage"
$zip = "$repo\dist\Baroque-SaturnRecomp-v$version-windows-x64.zip"

if (Test-Path -LiteralPath $stage) { Remove-Item -LiteralPath $stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $stage | Out-Null
$env:PATH = (Split-Path -Parent $Gcc) + ';' + $env:PATH
& $Gcc -O2 -std=c11 -Wall -Wextra -municode -mwindows -static "-DKIT_VER=$version" "-DRUNTIME_VER=$runtimeVersion" `
    "$repo\launcher\baroque_launcher.c" -ladvapi32 -o "$stage\Baroque.exe"
if ($LASTEXITCODE -ne 0) { throw 'launcher build failed' }

Copy-Item -LiteralPath "$repo\kit" -Destination "$stage\kit" -Recurse
Remove-Item -LiteralPath "$stage\kit\setup.log" -ErrorAction SilentlyContinue
Copy-Item -LiteralPath "$repo\README.md", "$repo\LICENSE" -Destination $stage
Copy-Item -LiteralPath "$repo\recomplauncher.json", "$repo\prepare-baroque.cmd" -Destination $stage

if (Test-Path -LiteralPath $zip) { Remove-Item -LiteralPath $zip }
Compress-Archive -Path "$stage\*" -DestinationPath $zip
Get-Item -LiteralPath $zip | Select-Object Name, Length
