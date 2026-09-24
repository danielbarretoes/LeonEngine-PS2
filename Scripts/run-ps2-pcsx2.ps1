# Launch a built PS2 ELF in PCSX2 (optionally building it first via Docker).
# Usage: .\Scripts\run-ps2-pcsx2.ps1 [hello|lab|cube|tp] [-Build]
# PCSX2 path: $env:LEON_PCSX2, else PATH, else default install locations.
param(
    [ValidateSet("hello", "lab", "smoke", "cube", "tp", "thirdperson")]
    [string]$Target = "cube",
    [switch]$Build
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..")).Path

$Elf = switch ($Target) {
    "hello"       { "Samples\Ps2Hello\build-ps2\leon-Ps2Hello.elf" }
    "lab"         { "Projects\Ps2Lab\build-ps2\leon-Ps2Lab.elf" }
    "smoke"       { "Projects\Ps2Lab\build-ps2\leon-Ps2Lab.elf" }
    "tp"          { "Projects\Ps2ThirdPerson\build-ps2\leon-Ps2ThirdPerson.elf" }
    "thirdperson" { "Projects\Ps2ThirdPerson\build-ps2\leon-Ps2ThirdPerson.elf" }
    default       { "Projects\Ps2Cube\build-ps2\leon-Ps2Cube.elf" }
}
$Elf = Join-Path $Root $Elf

if ($Build) {
    & (Join-Path $PSScriptRoot "build-ps2-docker.ps1") $Target
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
if (-not (Test-Path $Elf)) {
    Write-Error "ELF not found: $Elf (run with -Build, or .\Scripts\build-ps2-docker.ps1 $Target)"
}

$Pcsx2 = $env:LEON_PCSX2
if (-not $Pcsx2) {
    $cmd = Get-Command pcsx2-qt.exe -ErrorAction SilentlyContinue
    if ($cmd) { $Pcsx2 = $cmd.Source }
}
if (-not $Pcsx2) {
    $Pcsx2 = @(
        "$env:ProgramFiles\PCSX2\pcsx2-qt.exe",
        "${env:ProgramFiles(x86)}\PCSX2\pcsx2-qt.exe",
        "$env:LOCALAPPDATA\Programs\PCSX2\pcsx2-qt.exe"
    ) | Where-Object { Test-Path $_ } | Select-Object -First 1
}
if (-not $Pcsx2) {
    Write-Error "PCSX2 not found. Install it (winget install PCSX2Team.PCSX2) or set LEON_PCSX2."
}

Write-Host "PCSX2: $Pcsx2"
Write-Host "ELF:   $Elf"
Start-Process -FilePath $Pcsx2 -ArgumentList @("-fastboot", "-elf", "`"$Elf`"")
