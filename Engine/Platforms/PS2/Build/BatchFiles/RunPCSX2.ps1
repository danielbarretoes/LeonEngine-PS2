# Launch a project's (or an engine program's) PS2 build in PCSX2, optionally building it first with LeonBuildTool.
# Usage: Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 [-Project <dir|file.lproj>] [-Configuration Development] [-Build] [-NoStage] [-StageOnly]
#        Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1 -Program TestPAL [-Build] [-NoStage] [-StageOnly]
# -StageOnly stages the config next to the ELF and returns without looking for or starting PCSX2 (Package.bat).
# Program output (printf / UE_LOG) goes to the EE console: PCSX2 log, %USERPROFILE%\Documents\PCSX2\logs\emulog.txt.
# PCSX2 path: $env:LEON_PCSX2, else PATH, else default install locations.
#
# Config staging: PCSX2's host: device is the ELF's folder, and the PS2 FPaths expects a staged layout under it
# (<ELF dir>\Engine\Config, <ELF dir>\Engine\Platforms\PS2\Config, <ELF dir>\<Project>\Config). The script copies
# the .ini files (and the .lproj) there before launching; the Binaries folder is git-ignored. With -NoStage nothing is
# copied and the game runs on its compiled defaults, which must behave the same. PCSX2 only serves files other than
# the ELF through host: with Settings > Advanced > "Enable Host Filesystem" ([EmuCore] HostFs = true in PCSX2.ini);
# without it the staged files are not read and the compiled defaults apply.
param(
    [string]$Project = "Game\ThirdPerson",
    [string]$Program = "",
    [ValidateSet("Debug", "Development", "Shipping")]
    [string]$Configuration = "Development",
    [switch]$Build,
    [switch]$NoStage,
    [switch]$StageOnly
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..\..\..")).Path

if ($Program) {
    if ($Build) {
        & cmd /c "`"$Root\Engine\Build\BatchFiles\Build.bat`" $Program PS2 $Configuration"
        if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
    }
    $ElfName = if ($Configuration -eq "Development") { "$Program.elf" } else { "$Program-PS2-$Configuration.elf" }
    $Elf = Join-Path $Root "Engine\Binaries\PS2\$ElfName"
} else {
    $ProjectPath = if ([System.IO.Path]::IsPathRooted($Project)) { $Project } else { Join-Path $Root $Project }
    if ((Get-Item $ProjectPath).PSIsContainer) {
        $ProjectFile = Get-ChildItem -Path $ProjectPath -Filter *.lproj | Select-Object -First 1
        if (-not $ProjectFile) { Write-Error "No .lproj in $ProjectPath" }
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
}
if (-not (Test-Path $Elf)) {
    Write-Error "ELF not found: $Elf (run with -Build)"
}

function Copy-Config([string]$From, [string]$To) {
    if (Test-Path $From) {
        New-Item -ItemType Directory -Force -Path $To | Out-Null
        Copy-Item -Path (Join-Path $From "*.ini") -Destination $To -Force
    }
}

$StageDir = Split-Path $Elf -Parent
$StagedEngine = Join-Path $StageDir "Engine"
if (Test-Path $StagedEngine) { Remove-Item -Recurse -Force $StagedEngine }
if (-not $Program) {
    $StagedProject = Join-Path $StageDir $ProjectName
    if (Test-Path $StagedProject) { Remove-Item -Recurse -Force $StagedProject }
}
if (-not $NoStage) {
    Copy-Config (Join-Path $Root "Engine\Config") (Join-Path $StagedEngine "Config")
    Copy-Config (Join-Path $Root "Engine\Platforms\PS2\Config") (Join-Path $StagedEngine "Platforms\PS2\Config")
    if (-not $Program) {
        Copy-Config (Join-Path $ProjectDir "Config") (Join-Path $StagedProject "Config")
        Copy-Config (Join-Path $ProjectDir "Platforms\PS2\Config") (Join-Path $StagedProject "Platforms\PS2\Config")
        Copy-Item -Path $ProjectFile -Destination (Join-Path $StagedProject (Split-Path $ProjectFile -Leaf)) -Force
    }
    Write-Host "Staged config under $StageDir"
}

if ($StageOnly) {
    exit 0
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
