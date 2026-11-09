# Headless setup for RecompLauncher (see recomplauncher.json). Baroque.exe does not use this.
# Takes the disc (.cue, .chd or .iso) and the Saturn BIOS the player picked in the launcher and runs
# setup.ps1 with both, so nothing opens a file picker. Prints "progress <0-1> <text>" lines.
param(
    [Parameter(Mandatory = $true)][string]$Disc,
    [Parameter(Mandatory = $true)][string]$Bios,
    [Parameter(Mandatory = $true)][string]$Install,
    [Parameter(Mandatory = $true)][string]$Local
)
$ErrorActionPreference = 'Stop'
function Progress($p, $t) { Write-Host ("progress {0} {1}" -f $p, $t) }
function Fail($m) { [Console]::Error.WriteLine($m); exit 1 }

# The same data folder Baroque.exe uses: next to it when portable.txt exists.
$Data = if (Test-Path -LiteralPath (Join-Path $Install 'portable.txt')) { $Install } else { Join-Path $Local 'Baroque-SaturnRecomp' }
New-Item -ItemType Directory -Force -Path (Join-Path $Data 'disc') | Out-Null

Progress 0.02 'Checking the BIOS and disc'
if (-not (Test-Path -LiteralPath $Bios -PathType Leaf)) { Fail 'The BIOS file is missing.' }
if ((Get-Item -LiteralPath $Bios).Length -ne 524288) { Fail 'That BIOS is not 512 KB. Pick a Sega Saturn BIOS dump (.bin).' }
if (-not (Test-Path -LiteralPath $Disc -PathType Leaf)) { Fail 'The disc image is missing.' }

$sheet = $Disc
switch ([IO.Path]::GetExtension($Disc).ToLowerInvariant()) {
    { $_ -in '.cue', '.chd' } { }   # setup.ps1 reads both
    '.iso' {
        # setup.ps1 wants a cue sheet; a plain 2048-byte-sector .iso gets a one-track wrapper.
        $iso = Join-Path $Data 'disc\baroque-iso.iso'
        Remove-Item -LiteralPath $iso -ErrorAction SilentlyContinue
        try { New-Item -ItemType HardLink -Path $iso -Target $Disc | Out-Null } catch { Copy-Item -LiteralPath $Disc -Destination $iso }
        $sheet = Join-Path $Data 'disc\baroque-iso.cue'
        [IO.File]::WriteAllText($sheet, "FILE `"baroque-iso.iso`" BINARY`r`n  TRACK 01 MODE1/2048`r`n    INDEX 01 00:00:00`r`n", (New-Object Text.UTF8Encoding($false)))
    }
    default { Fail 'Pick a .cue, .chd or .iso disc image.' }
}

Progress 0.1 'Setting up and building the runtime (this takes a while the first time)'
& powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File (Join-Path $Install 'kit\setup.ps1') -Root $Install -Data $Data -Disc $sheet -Bios $Bios
if ($LASTEXITCODE -ne 0) { Fail "Setup failed (exit $LASTEXITCODE). See setup.log in $Data." }
Progress 1 'Ready'
exit 0
