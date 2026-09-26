# Builds and packages under each owner project's Packages\ folder (UE: RunUAT BuildCookRun -archive).
#   Win64: Game\ShooterGame\Packages\Win64\
#   PS2:   Game\<Name>\Packages\PS2\ for games; Engine\Packages\PS2\<Name>\ for engine programs.
# -NoWin64 / -NoPS2 skip a platform. Each destination folder is emptied first and is git-ignored (Packages/).
#
# PS2 packaging origins (one Publish-Package path for every artifact):
#   Dev-loop (RunPCSX2 -StageOnly beside Binaries\PS2): ThirdPerson, TestPAL; GSConformance has no config to stage.
#   BuildCookRun StagedBuilds\PS2: ShooterGame (ELF at the stage root + pak / LeonCommandLine.txt).
param(
	[switch]$NoWin64,
	[switch]$NoPS2
)

$ErrorActionPreference = "Stop"
$Root = (Resolve-Path (Join-Path $PSScriptRoot "..\..\..")).Path
$BatchFiles = Join-Path $Root "Engine\Build\BatchFiles"
$RunPCSX2 = Join-Path $Root "Engine\Platforms\PS2\Build\BatchFiles\RunPCSX2.ps1"

function Fail([string]$Message)
{
	Write-Host "Package: $Message" -ForegroundColor Red
	exit 1
}

function Step([string]$Message)
{
	Write-Host ""
	Write-Host "Package: $Message" -ForegroundColor Cyan
}

# Runs a batch file with arguments and fails the package when it fails.
function Invoke-Batch([string]$Batch, [string]$Arguments)
{
	& cmd /c "`"$Batch`" $Arguments"
	if ($LASTEXITCODE -ne 0) { Fail "$(Split-Path $Batch -Leaf) $Arguments failed ($LASTEXITCODE)" }
}

# Replaces Destination with a copy of the listed items of Source.
function Copy-Package([string]$Source, [string[]]$Items, [string]$Destination)
{
	if (Test-Path $Destination) { Remove-Item -Recurse -Force $Destination }
	New-Item -ItemType Directory -Force -Path $Destination | Out-Null
	foreach ($Item in $Items)
	{
		$Path = Join-Path $Source $Item
		if (-not (Test-Path $Path)) { Fail "missing $Path" }
		Copy-Item -Recurse -Force -Path $Path -Destination $Destination
	}
}

# Single publish path: empties Destination and copies Items from SourceDir into it.
function Publish-Package([string]$Destination, [string]$SourceDir, [string[]]$Items)
{
	Copy-Package $SourceDir $Items $Destination
}

# Dev-loop PS2: Build.bat, optional RunPCSX2 -StageOnly, then Publish-Package.
# StageOnlySplat is splatted into RunPCSX2.ps1 (e.g. @{ Project = $Path; StageOnly = $true }).
function Publish-PS2DevLoopArtifact(
	[string]$Destination,
	[string]$BuildArgs,
	[string]$SourceDir,
	[string[]]$Items,
	[hashtable]$StageOnlySplat = $null
)
{
	Invoke-Batch (Join-Path $BatchFiles "Build.bat") $BuildArgs
	if ($null -ne $StageOnlySplat)
	{
		& $RunPCSX2 @StageOnlySplat
		if ($LASTEXITCODE -ne 0) { Fail "staging config for $Destination failed" }
	}
	Publish-Package $Destination $SourceDir $Items
}

if ($NoWin64 -and $NoPS2) { Fail "nothing to package (-NoWin64 and -NoPS2)" }

# Legacy repo-root Packages\ is no longer used; drop a leftover so it cannot be confused with the new layout.
$LegacyPackages = Join-Path $Root "Packages"
if (Test-Path $LegacyPackages)
{
	Step "removing legacy root Packages\"
	Remove-Item -Recurse -Force $LegacyPackages
}

Step "setup (pinned third-party downloads)"
Invoke-Batch (Join-Path $Root "Setup.bat") ""

if (-not $NoPS2 -and -not $env:PS2DEV)
{
	# Check Docker before the long Win64 build: the PS2 toolchain runs in the pinned ps2dev image.
	& docker info *> $null
	if ($LASTEXITCODE -ne 0) { Fail "Docker is not running: start Docker Desktop (Linux containers) for the PS2 build, or run Package.bat -NoPS2" }
}

$ShooterGameDir = Join-Path $Root "Game\ShooterGame"
$ThirdPersonDir = Join-Path $Root "Game\ThirdPerson"
$ShooterGame = Join-Path $ShooterGameDir "ShooterGame.lproj"
$ThirdPerson = Join-Path $ThirdPersonDir "ThirdPerson.lproj"
$EngineDir = Join-Path $Root "Engine"

if (-not $NoWin64)
{
	Step "Win64: ShooterGame Shipping (build, cook, stage, pak)"
	Invoke-Batch (Join-Path $BatchFiles "BuildCookRun.bat") "`"-project=$ShooterGame`" -platform=Win64 -configuration=Shipping -build -cook -stage -pak"
	# StagedBuilds\Win64\ShooterGame\ -> Game\ShooterGame\Packages\Win64\ (no nested project name).
	$StagedProject = Join-Path $ShooterGameDir "Saved\StagedBuilds\Win64\ShooterGame"
	Publish-Package (Join-Path $ShooterGameDir "Packages\Win64") $StagedProject @("*")
}

if (-not $NoPS2)
{
	Step "PS2: ThirdPerson Development"
	Publish-PS2DevLoopArtifact -Destination (Join-Path $ThirdPersonDir "Packages\PS2") `
		-BuildArgs "ThirdPerson PS2 Development `"-Project=$ThirdPerson`"" `
		-SourceDir (Join-Path $ThirdPersonDir "Binaries\PS2") `
		-Items @("ThirdPerson.elf", "Engine", "ThirdPerson") `
		-StageOnlySplat @{ Project = $ThirdPerson; StageOnly = $true }

	Step "PS2: TestPAL Development"
	Publish-PS2DevLoopArtifact -Destination (Join-Path $EngineDir "Packages\PS2\TestPAL") `
		-BuildArgs "TestPAL PS2 Development" `
		-SourceDir (Join-Path $EngineDir "Binaries\PS2") `
		-Items @("TestPAL.elf", "Engine") `
		-StageOnlySplat @{ Program = "TestPAL"; StageOnly = $true }

	# ShooterGame on the EE: BuildCookRun stages the ELF at the stage root with its PS2 cook in a pak.
	Step "PS2: ShooterGame Development (the game, with its pak)"
	Invoke-Batch (Join-Path $BatchFiles "BuildCookRun.bat") "`"-project=$ShooterGame`" -platform=PS2 -build -cook -stage -pak `"-addcmdline=-LogFrameTimes`""
	Publish-Package (Join-Path $ShooterGameDir "Packages\PS2") (Join-Path $ShooterGameDir "Saved\StagedBuilds\PS2") @("*")

	# The GS conformance scenes: no config to stage.
	Step "PS2: GSConformance Development"
	Publish-PS2DevLoopArtifact -Destination (Join-Path $EngineDir "Packages\PS2\GSConformance") `
		-BuildArgs "GSConformance PS2 Development" `
		-SourceDir (Join-Path $EngineDir "Binaries\PS2") `
		-Items @("GSConformance.elf")
}

Step "done"
if (-not $NoWin64)
{
	Write-Host "  Win64: Game\ShooterGame\Packages\Win64\Binaries\Win64\ShooterGame-Win64-Shipping.exe"
}
if (-not $NoPS2)
{
	Write-Host "  PS2:   Game\ThirdPerson\Packages\PS2\ThirdPerson.elf"
	Write-Host "         Game\ShooterGame\Packages\PS2\ShooterGame.elf"
	Write-Host "         Engine\Packages\PS2\TestPAL\TestPAL.elf"
	Write-Host "         Engine\Packages\PS2\GSConformance\GSConformance.elf"
	Write-Host "         (ShooterGame: de_leon against nine bots, played with the pad; its frame times are in the EE log)"
	Write-Host "         PCSX2: enable Settings > Advanced > Enable Host Filesystem (the staged config is read through host:),"
	Write-Host "         then boot the ELF (pcsx2-qt -fastboot -elf <file>). TestPAL prints its result to the EE log."
}
exit 0
