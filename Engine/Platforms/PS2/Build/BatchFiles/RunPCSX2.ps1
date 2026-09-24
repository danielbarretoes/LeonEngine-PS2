# Launch a project's PS2 build in PCSX2, optionally building it first with LeonBuildTool.
# Usage: Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 [-Project <dir|file.leonproject>] [-Configuration Development] [-Build]
# PCSX2 path: $env:LEON_PCSX2, else PATH, else default install locations.
param(
    [string]$Project = "Game\ThirdPerson",
    [ValidateSet("Debug", "Development", "Shipping")]
    [string]$Configuration = "Development",
    [switch]$Build
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..\..\..")).Path

$ProjectPath = if ([System.IO.Path]::IsPathRooted($Project)) { $Project } else { Join-Path $Root $Project }
if ((Get-Item $ProjectPath).PSIsContainer) {
    $ProjectFile = Get-ChildItem -Path $ProjectPath -Filter *.leonproject | Select-Object -First 1
    if (-not $ProjectFile) { Write-Error "No .leonproject in $ProjectPath" }
    $ProjectFile = $ProjectFile.FullName
} else {
    $ProjectFile = (Resolve-Path $ProjectPath).Path
}
$ProjectDir = Split-Path $ProjectFile -Parent
$ProjectName = [System.IO.Path]::GetFileNameWithoutExtension($ProjectFile)

if ($Build) {
    & cmd /c "`"$Root\Engine\Build\BatchFiles\Build.bat`" $ProjectName PS2 $Configuration `"-Project=$ProjectFile`""
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}

$ElfName = if ($Configuration -eq "Development") { "$ProjectName.elf" } else { "$ProjectName-PS2-$Configuration.elf" }
$Elf = Join-Path $ProjectDir "Binaries\PS2\$ElfName"
if (-not (Test-Path $Elf)) {
    Write-Error "ELF not found: $Elf (run with -Build, or Engine\Build\BatchFiles\Build.bat $ProjectName PS2 $Configuration -Project=$ProjectFile)"
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
