# Baroque (SaturnRecomp) setup.
#
# Builds the SaturnRecomp runtime on this PC from the author's public source,
# with the Baroque fixes applied, and points it at YOUR disc and BIOS. Nothing
# from SaturnRecomp, the BIOS or the game is shipped in this kit.
#
# Run by Baroque.exe when needed; safe to re-run. -Root is the app folder (the
# one with kit\). Your files and everything setup downloads (game\, bios\,
# settings.ini, tools\, build\, runtime\) go in -Data, which Baroque.exe sets to
# %LOCALAPPDATA%\Baroque-SaturnRecomp so app updates leave them alone. Without
# -Data they stay in the app folder.
param(
    [Parameter(Mandatory = $true)][string]$Root,
    [string]$Data,
    [string]$Disc,
    [string]$Bios,
    [switch]$Rebuild
)
$ErrorActionPreference = 'Stop'
$ProgressPreference = 'SilentlyContinue'   # Invoke-WebRequest crawls with a progress bar on PowerShell 5.1
[Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12

$Root = (Resolve-Path -LiteralPath $Root).Path
if (-not $Data) { $Data = $Root }
New-Item -ItemType Directory -Force -Path $Data | Out-Null
$Data = (Resolve-Path -LiteralPath $Data).Path
$KitVersion   = (Get-Content -LiteralPath "$Root\kit\kit-version.txt" -Raw).Trim()   # not version.txt: Quiver owns that file
$SaturnCommit = '26c9715e5493054b8a205aa31d73d8f125fdd8f5'
$SaturnZip    = "https://codeload.github.com/sonsegajp/SaturnRecomp/zip/$SaturnCommit"
$MsysUrl      = 'https://github.com/msys2/msys2-installer/releases/download/2026-09-27/msys2-base-x86_64-20260927.sfx.exe'
$MsysSha256   = 'ad336cccfda47758b5e15cda993fbba421115cb0b126697daef1ee4dfe37209f'
$Packages     = 'mingw-w64-x86_64-gcc mingw-w64-x86_64-SDL2 mingw-w64-x86_64-vulkan-headers mingw-w64-x86_64-vulkan-loader mingw-w64-x86_64-shaderc patch'

$kit     = "$Root\kit"
$tools   = "$Data\tools"
$msys    = "$tools\msys64"
$bash    = "$msys\usr\bin\bash.exe"
$mingw   = "$msys\mingw64\bin"
$build   = "$Data\build"
$src     = "$build\SaturnRecomp"
$runtime = "$Data\runtime"
$game    = "$Data\game"

New-Item -ItemType Directory -Force -Path $tools, $build, $runtime, $game, "$Data\bios" | Out-Null
Start-Transcript -LiteralPath "$Data\setup.log" -Force | Out-Null

function Step([string]$m) { Write-Host ''; Write-Host "== $m" -ForegroundColor Cyan }
# Native tools write warnings to stderr (MSYS2 complains about some time zones);
# under 'Stop', PowerShell 5.1 would turn those into fatal errors.
function Native([scriptblock]$b) { $old = $ErrorActionPreference; $ErrorActionPreference = 'Continue'; try { & $b } finally { $ErrorActionPreference = $old } }
function Fail([string]$m) { Write-Host ''; Write-Host "SETUP FAILED: $m" -ForegroundColor Red; Stop-Transcript | Out-Null; exit 1 }

function Pick-File([string]$title, [string]$filter) {
    Add-Type -AssemblyName System.Windows.Forms
    $owner = New-Object System.Windows.Forms.Form -Property @{ TopMost = $true; ShowInTaskbar = $false }
    $dlg = New-Object System.Windows.Forms.OpenFileDialog -Property @{ Title = $title; Filter = $filter }
    try { if ($dlg.ShowDialog($owner) -eq 'OK') { return $dlg.FileName } } finally { $owner.Dispose() }
    return $null
}

# ---------------------------------------------------------------- game data
Step 'Your Saturn BIOS'
$biosDst = "$Data\bios\saturn.bin"
if (-not $Bios -and -not (Test-Path -LiteralPath $biosDst)) {
    Write-Host 'Choose your own 512 KB Sega Saturn BIOS dump (a Japanese BIOS is recommended for Baroque).'
    $Bios = Pick-File 'Choose your Sega Saturn BIOS (512 KB)' 'Saturn BIOS (*.bin)|*.bin|All files (*.*)|*.*'
    if (-not $Bios) { Fail 'No BIOS chosen.' }
}
if ($Bios) {
    if ((Get-Item -LiteralPath $Bios).Length -ne 524288) { Fail "That BIOS is not 524,288 bytes: $Bios" }
    Copy-Item -LiteralPath $Bios -Destination $biosDst -Force
}
Write-Host "BIOS: $biosDst"

function Test-BaroqueCue([string]$cue) {
    $dir = Split-Path -Parent $cue
    $files = Select-String -LiteralPath $cue -Pattern '^\s*FILE\s+"(.+)"' | ForEach-Object { $_.Matches[0].Groups[1].Value }
    if (-not $files) { throw "No FILE lines in $cue" }
    foreach ($f in $files) { if (-not (Test-Path -LiteralPath (Join-Path $dir $f))) { throw "The cue sheet needs '$f' next to it." } }
    $first = Join-Path $dir $files[0]
    $raw = (Get-Content -LiteralPath $cue -Raw) -match 'MODE1/2352|MODE2/2352'
    $fs = [IO.File]::OpenRead($first)
    try { $buf = New-Object byte[] 64; $fs.Position = $(if ($raw) { 16 } else { 0 }); [void]$fs.Read($buf, 0, 64) } finally { $fs.Close() }
    $hdr = [Text.Encoding]::ASCII.GetString($buf)
    if (-not $hdr.StartsWith('SEGA SEGASATURN')) { throw 'This is not a Sega Saturn disc image.' }
    if ($hdr.Substring(0x20, 8) -ne 'T-33901G') { throw "This disc is $($hdr.Substring(0x20, 10).Trim()), not Baroque (T-33901G)." }
}

Step 'Your Baroque disc'
$toml = "$game\game.toml"
if (-not $Disc -and (Test-Path -LiteralPath $toml)) {
    $m = Select-String -LiteralPath $toml -Pattern '^disc\s*=\s*"(.+)"'
    if ($m) { $existing = $m.Matches[0].Groups[1].Value -replace '/', '\'; if (Test-Path -LiteralPath $existing) { $Disc = $existing } }
}
if (-not $Disc) {
    $local = Get-ChildItem -LiteralPath "$Root\disc", "$Data\disc" -Filter *.cue -ErrorAction SilentlyContinue | Select-Object -First 1
    if ($local) { $Disc = $local.FullName }
}
if (-not $Disc) {
    Write-Host 'Choose the .cue file of your Baroque (Japan) disc image. The English fan translation is supported.'
    $Disc = Pick-File 'Choose your Baroque disc (.cue)' 'CUE sheet (*.cue)|*.cue'
    if (-not $Disc) { Fail 'No disc chosen.' }
}
try { Test-BaroqueCue $Disc } catch { Fail $_.Exception.Message }
$discToml = (Resolve-Path -LiteralPath $Disc).Path -replace '\\', '/'
[IO.File]::WriteAllText($toml, (Get-Content -LiteralPath "$kit\game\game.toml.in" -Raw).Replace('@DISC@', $discToml),
    (New-Object Text.UTF8Encoding($false)))
Write-Host "Disc: $Disc"

foreach ($f in 'cheats.txt', 'console.bin') {
    if (-not (Test-Path -LiteralPath "$game\$f")) { Copy-Item -LiteralPath "$kit\game\$f" -Destination "$game\$f" }
}
if (-not (Test-Path -LiteralPath "$Data\settings.ini")) { Copy-Item -LiteralPath "$kit\game\settings.ini" -Destination "$Data\settings.ini" }

$stamp = "$runtime\.kit-version"
if (-not $Rebuild -and (Test-Path -LiteralPath "$runtime\saturnwin.exe") -and (Test-Path -LiteralPath $stamp) -and
    (Get-Content -LiteralPath $stamp -Raw).Trim() -eq $KitVersion) {
    Step 'The runtime is already built for this version.'
    Stop-Transcript | Out-Null; exit 0
}

# ---------------------------------------------------------------- toolchain
Step 'Build tools (portable MSYS2, about 1 GB, kept in tools\)'
$marker = "$tools\.packages"
if (-not (Test-Path -LiteralPath "$mingw\gcc.exe") -or -not (Test-Path -LiteralPath $marker) -or
    (Get-Content -LiteralPath $marker -Raw).Trim() -ne $Packages) {
    if (-not (Test-Path -LiteralPath $bash)) {
        $sfx = "$tools\msys2-base.sfx.exe"
        Write-Host 'Downloading MSYS2 2026-09-27...'
        Invoke-WebRequest $MsysUrl -OutFile $sfx -UseBasicParsing
        if ((Get-FileHash -LiteralPath $sfx -Algorithm SHA256).Hash -ne $MsysSha256) { Remove-Item -LiteralPath $sfx; Fail 'MSYS2 download failed its checksum.' }
        Native { & $sfx -y "-o$tools" | Out-Null }
        Remove-Item -LiteralPath $sfx
        if (-not (Test-Path -LiteralPath $bash)) { Fail 'MSYS2 did not extract.' }
    }
    Write-Host 'Installing the compiler and libraries (this is the slow part)...'
    Native { & $bash -lc 'true' 2>&1 | Out-Null }
    Native { & $bash -lc 'pacman -Syu --noconfirm' 2>&1 | Out-Null }
    Native { & $bash -lc 'pacman -Syu --noconfirm' 2>&1 | Out-Null }
    Native { & $bash -lc "pacman -S --needed --noconfirm $Packages" 2>&1 | Select-Object -Last 3 | ForEach-Object { "$_" } }
    foreach ($exe in 'gcc.exe', 'glslc.exe', 'SDL2.dll') { if (-not (Test-Path -LiteralPath "$mingw\$exe")) { Fail "The build tools are missing $exe." } }
    Set-Content -LiteralPath $marker -Value $Packages
}

# ---------------------------------------------------------------- source
Step "SaturnRecomp source ($($SaturnCommit.Substring(0, 7)), from github.com/sonsegajp/SaturnRecomp)"
$zip = "$build\SaturnRecomp.zip"
$tmp = "$build\extract"
Write-Host 'Downloading...'
Invoke-WebRequest $SaturnZip -OutFile $zip -UseBasicParsing
if (Test-Path -LiteralPath $tmp) { Remove-Item -LiteralPath $tmp -Recurse -Force }
Expand-Archive -LiteralPath $zip -DestinationPath $tmp
if (Test-Path -LiteralPath $src) { Remove-Item -LiteralPath $src -Recurse -Force }
Move-Item -LiteralPath "$tmp\SaturnRecomp-$SaturnCommit" -Destination $src
Remove-Item -LiteralPath $tmp, $zip -Recurse -Force
Write-Host 'Applying the Baroque fixes...'
foreach ($p in Get-ChildItem -LiteralPath "$kit\patches" -Filter *.patch | Sort-Object Name) {
    Native { & "$msys\usr\bin\patch.exe" -p1 --forward --no-backup-if-mismatch -d $src -i $p.FullName 2>&1 | ForEach-Object { "$_" } }
    if ($LASTEXITCODE -ne 0) { Fail "Patch $($p.Name) did not apply." }
}

# ---------------------------------------------------------------- build
Step 'Building the runtime (a few minutes)'
$env:PATH = "$mingw;$env:PATH"
$env:SATURN_MINGW_BIN = $mingw
foreach ($s in 'vdp1.comp', 'vdp2.comp', 'postprocess.comp', 'game_overlay.vert', 'game_overlay.frag') {
    Native { & "$mingw\glslc.exe" "$src\runner\shaders\$s" -O -o "$src\runner\shaders\$s.spv" }
    if ($LASTEXITCODE -ne 0) { Fail "Shader $s did not compile." }
}
# The profile from the kit author's gameplay run makes the CPU interpreter
# noticeably faster. GCC files it under the build folder's absolute path.
$pgoDir = "$src\out\runtime-build\profile\" + (($src -replace ':', '~') -replace '/', '\')
New-Item -ItemType Directory -Force -Path $pgoDir | Out-Null
foreach ($g in Get-ChildItem -LiteralPath "$kit\pgo" -Filter *.gcda) {
    Copy-Item -LiteralPath $g.FullName -Destination "$pgoDir\out#runtime-build#obj#$($g.Name)"
}
Push-Location $src
try {
    Native { & powershell -NoProfile -ExecutionPolicy Bypass -File "$src\tools\build_runtime.ps1" -Profile use 2>&1 | Out-Null }
    if ($LASTEXITCODE -ne 0 -or -not (Test-Path -LiteralPath "$src\out\runtime-build\saturnwin.exe")) {
        Write-Host 'The optimized build did not match this compiler; building the standard version instead.' -ForegroundColor Yellow
        Remove-Item -LiteralPath "$src\out\runtime-build" -Recurse -Force -ErrorAction SilentlyContinue
        Native { & powershell -NoProfile -ExecutionPolicy Bypass -File "$src\tools\build_runtime.ps1" -Profile off 2>&1 | Select-Object -Last 3 | ForEach-Object { "$_" } }
    } else { Write-Host 'Optimized (PGO) build OK.' }
} finally { Pop-Location }
if (-not (Test-Path -LiteralPath "$src\out\runtime-build\saturnwin.exe")) { Fail "The runtime did not build. See $Data\setup.log." }

# ---------------------------------------------------------------- install
Step 'Installing'
New-Item -ItemType Directory -Force -Path "$runtime\shaders" | Out-Null
Copy-Item -LiteralPath "$src\out\runtime-build\saturnwin.exe" -Destination $runtime -Force
Copy-Item -LiteralPath "$mingw\SDL2.dll" -Destination $runtime -Force
Copy-Item -Path "$src\runner\shaders\*.spv" -Destination "$runtime\shaders" -Force
Set-Content -LiteralPath $stamp -Value $KitVersion
Write-Host ''
Write-Host 'Setup complete. Starting Baroque...' -ForegroundColor Green
Stop-Transcript | Out-Null
exit 0
