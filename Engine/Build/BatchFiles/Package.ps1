# Builds and packages under each owner project's Packages\ folder (UE: RunUAT BuildCookRun -archive).
#   Win64: Game\ShooterGame\Packages\Win64\
#   PS2:   Game\<Name>\Packages\PS2\ for games; Engine\Packages\PS2\<Name>\ for engine programs.
# -NoWin64 / -NoPS2 skip a platform. -Only <Name[,Name]> packages only those artifacts (ShooterGame, TestPAL,
# GSConformance, VU1Conformance; Win64 has only ShooterGame): `Package.bat -NoWin64 -Only ShooterGame` is the game on the PS2
# alone. Each destination folder is emptied first and is git-ignored (Packages/); the others are left as they are.
#
# PS2 packaging origins (one Publish-Package path for every artifact):
#   Dev-loop (RunPCSX2 -StageOnly beside Binaries\PS2): TestPAL; GSConformance and VU1Conformance have no config to stage.
#   BuildCookRun StagedBuilds\PS2: ShooterGame (ELF at the stage root + pak / LeonCommandLine.txt).
param(
	[switch]$NoWin64,
	[switch]$NoPS2,
	[string]$Only = ""
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
# StageOnlySplat is splatted into RunPCSX2.ps1 (e.g. @{ Program = "TestPAL"; StageOnly = $true }).
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

# -Only: a comma-separated list (powershell -File passes it as one string).
$Artifacts = @("ShooterGame", "TestPAL", "GSConformance", "VU1Conformance")
$OnlyNames = @($Only -split "," | ForEach-Object { $_.Trim() } | Where-Object { $_ -ne "" })
foreach ($Name in $OnlyNames)
{
	if ($Artifacts -notcontains $Name) { Fail "-Only: unknown '$Name' (the artifacts: $($Artifacts -join ', '))" }
}

# Whether an artifact is packaged: every one without -Only, else the ones it names.
function Wants([string]$Name)
{
	return $OnlyNames.Count -eq 0 -or $OnlyNames -contains $Name
}

# Win64 packages only ShooterGame.
if (-not (Wants "ShooterGame")) { $NoWin64 = $true }

# After -Only, which can turn Win64 off: `-NoPS2 -Only TestPAL` leaves nothing.
if ($NoWin64 -and $NoPS2) { Fail "nothing to package (-NoWin64 and -NoPS2, or -Only names no artifact of the platforms left)" }

Step "setup (pinned third-party downloads)"
Invoke-Batch (Join-Path $Root "Setup.bat") ""

if (-not $NoPS2 -and -not $env:PS2DEV)
{
	# Check Docker before the long Win64 build: the PS2 toolchain runs in the pinned ps2dev image.
	& docker info *> $null
	if ($LASTEXITCODE -ne 0) { Fail "Docker is not running: start Docker Desktop (Linux containers) for the PS2 build, or run Package.bat -NoPS2" }
}

$ShooterGameDir = Join-Path $Root "Game\ShooterGame"
$ShooterGame = Join-Path $ShooterGameDir "ShooterGame.lproj"
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
	if (Wants "TestPAL")
	{
		Step "PS2: TestPAL Development"
		Publish-PS2DevLoopArtifact -Destination (Join-Path $EngineDir "Packages\PS2\TestPAL") `
			-BuildArgs "TestPAL PS2 Development" `
			-SourceDir (Join-Path $EngineDir "Binaries\PS2") `
			-Items @("TestPAL.elf", "Engine") `
			-StageOnlySplat @{ Program = "TestPAL"; StageOnly = $true }
	}

	if (Wants "ShooterGame")
	{
		# ShooterGame on the EE: BuildCookRun stages the ELF at the stage root with its PS2 cook in a pak.
		Step "PS2: ShooterGame Development (the game, with its pak)"
		Invoke-Batch (Join-Path $BatchFiles "BuildCookRun.bat") "`"-project=$ShooterGame`" -platform=PS2 -build -cook -stage -pak `"-addcmdline=-LogFrameTimes`""
		Publish-Package (Join-Path $ShooterGameDir "Packages\PS2") (Join-Path $ShooterGameDir "Saved\StagedBuilds\PS2") @("*")
	}

	if (Wants "GSConformance")
	{
		# The GS conformance scenes: no config to stage.
		Step "PS2: GSConformance Development"
		Publish-PS2DevLoopArtifact -Destination (Join-Path $EngineDir "Packages\PS2\GSConformance") `
			-BuildArgs "GSConformance PS2 Development" `
			-SourceDir (Join-Path $EngineDir "Binaries\PS2") `
			-Items @("GSConformance.elf")
	}

	if (Wants "VU1Conformance")
	{
		# VU1's microprograms against the C++ emitter (ps2-shipping N14): no config to stage.
		Step "PS2: VU1Conformance Development"
		Publish-PS2DevLoopArtifact -Destination (Join-Path $EngineDir "Packages\PS2\VU1Conformance") `
			-BuildArgs "VU1Conformance PS2 Development" `
			-SourceDir (Join-Path $EngineDir "Binaries\PS2") `
			-Items @("VU1Conformance.elf")
	}
}

Step "done"
if (-not $NoWin64)
{
	Write-Host "  Win64: Game\ShooterGame\Packages\Win64\Binaries\Win64\ShooterGame-Win64-Shipping.exe"
}
if (-not $NoPS2)
{
	$Elves = [ordered]@{
		ShooterGame = "Game\ShooterGame\Packages\PS2\ShooterGame.elf"
		TestPAL = "Engine\Packages\PS2\TestPAL\TestPAL.elf"
		GSConformance = "Engine\Packages\PS2\GSConformance\GSConformance.elf"
		VU1Conformance = "Engine\Packages\PS2\VU1Conformance\VU1Conformance.elf"
	}
	$Label = "  PS2:   "
	foreach ($Name in $Elves.Keys)
	{
		if (Wants $Name)
		{
			Write-Host "$Label$($Elves[$Name])"
			$Label = "         "
		}
	}
	if (Wants "ShooterGame")
	{
		Write-Host "         (ShooterGame: the main menu, then de_leon or de_harbor against nine bots, played with the pad)"
	}
	Write-Host "         PCSX2: enable Settings > Advanced > Enable Host Filesystem (the staged config is read through host:),"
	Write-Host "         then boot the ELF (pcsx2-qt -fastboot -elf <file>). TestPAL prints its result to the EE log."
}
exit 0
